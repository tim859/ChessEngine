#include "engine.h"

#include <algorithm>
#include <random>
#include <iostream>

void Engine::reset() {

}

Move Engine::generateEngineMove(const Game& game, const EngineSearchSettings& engineSearchSettings, const std::stop_token& stopToken) {
    // TODO: implement all the engine search settings into the search
    auto simulatedGame = game;
    const auto allLegalMoves = game.generateAllLegalMoves(simulatedGame.getCurrentGameState());
    if (allLegalMoves.empty())
        return Move({0, 0}, {0, 0});

    // seed bestMove with any legal move so we always have something to return,
    // even if depth 1 is interrupted before completing
    bestMove = allLegalMoves.front();
    Move bestMoveFromLastCompletedIteration = bestMove;
    positionsEvaluated = 0;

    const int maxSearchDepth = engineSearchSettings.depth.value_or(6);

    // iterative deepening: search progressively deeper, using the best move from the previous
    // iteration as the first move tried in the next iteration. this dramatically improves
    // alpha-beta cutoffs because the previous iteration's best is very likely still best.
    // it also gives anytime behaviour: if stop is requested partway through, we still have
    // a fully-searched best move from the last completed iteration to return.
    for (int currentDepth = 1; currentDepth <= maxSearchDepth; ++currentDepth) {
        if (stopToken.stop_requested())
            break;

        const int evaluation = search(simulatedGame, minusInfinity, infinity, currentDepth, currentDepth, 0, stopToken);

        // if the iteration was cut short, its bestMove update is unreliable; revert to the previous one
        if (stopToken.stop_requested()) {
            bestMove = bestMoveFromLastCompletedIteration;
            break;
        }

        bestMoveFromLastCompletedIteration = bestMove;
        std::cout << "depth " << currentDepth << " complete, evaluation: " << evaluation << ", positions evaluated so far: " << positionsEvaluated << std::endl;

        // forced mate detected — searching deeper cannot improve the outcome
        if (evaluation >= infinity - 1000 || evaluation <= minusInfinity + 1000)
            break;
    }

    std::cout << "total positions evaluated: " << positionsEvaluated << std::endl;
    std::cout << "total transpositions: " << transpositions << std::endl;
    return bestMove;
}

std::vector<std::pair<Move, std::uint64_t>> Engine::generatePerftDivide(const Game& game, const int depth) const {
    // "divide" output is perft split by root move so external tools can bisect exactly where counts diverge.
    std::vector<std::pair<Move, std::uint64_t>> divide;
    // use a copy instead of the live session
    auto simulatedGame = game;
    // promotion moves need to be expanded before we start counting, otherwise promotion positions will be undercounted.
    const auto moves = generatePerftMoves(simulatedGame);

    divide.reserve(moves.size());
    for (const auto& move : moves) {
        // play one legal root move, count the subtree below it, then restore the snapshot for the next root move.
        const auto moveDelta = simulatedGame.movePiece(simulatedGame.getCurrentGameState(), move);
        const std::uint64_t nodes = perft(simulatedGame, depth - 1);
        simulatedGame.undoLastMove(simulatedGame.getCurrentGameState(), moveDelta);
        // keep the original move paired with its subtree size so UCISession can print "move: nodes" lines.
        divide.emplace_back(move, nodes);
    }
    return divide;
}

int Engine::evaluateBoardPosition(const GameState& gameState) const {
    int evaluation = countMaterial(gameState, Piece::Colour::WHITE) - countMaterial(gameState, Piece::Colour::BLACK);

    const float endgameWeight = calculateEndgameWeight(gameState);
    const int whiteKingScore = evaluateKingPositionsEndgame(gameState, Piece::Colour::WHITE, endgameWeight);
    const int blackKingScore = evaluateKingPositionsEndgame(gameState, Piece::Colour::BLACK, endgameWeight);

    evaluation += whiteKingScore - blackKingScore;

    const int perspective = gameState.moveColour == Piece::Colour::WHITE ? 1 : -1;
    return evaluation * perspective;
}


int Engine::evaluateKingPositionsEndgame(const GameState& gameState, const Piece::Colour friendlyColour, const float endgameWeight) const {
    auto evaluation = 0;
    // find the squares of both king pieces
    Vector2Int friendlyKing = {8, 8};
    Vector2Int enemyKing = {8, 8};
    for (auto rank = 0; rank < 8; ++rank) {
        for (auto file = 0; file < 8; ++file) {
            if (gameState.boardPosition[rank][file]) {
                if (gameState.boardPosition[rank][file]->type == Piece::Type::KING) {
                    if (gameState.boardPosition[rank][file]->colour == friendlyColour) {
                        friendlyKing.x = file;
                        friendlyKing.y = rank;
                    }
                    else {
                        enemyKing.x = file;
                        enemyKing.y = rank;
                    }
                }
            }
        }
    }
    if (friendlyKing == Vector2Int{8, 8} || enemyKing == Vector2Int{8, 8})
        return 0;

    // calculate distance of the enemy king from the centre
    // favour positions where the enemy king is forced away from the centre as this makes it easier to checkmate in endgame
    evaluation += std::max(3 - enemyKing.x, enemyKing.x - 4) + std::max(3 - enemyKing.y, enemyKing.y - 4);

    // calculate distance between kings
    // incentivise moving friendly king closer to enemy king to cut off escape routes and assist with checkmate
    evaluation += 14 - (std::abs(friendlyKing.x - enemyKing.x) + std::abs(friendlyKing.y - enemyKing.y));

    // the closer to endgame the more this evaluation will be weighted
    return static_cast<int>(static_cast<float>(evaluation) * 10.0f * endgameWeight);
}

int Engine::countMaterial(const GameState& gameState, const Piece::Colour pieceColour) const {
    auto material = 0;
    for (auto rank = 0; rank < 8; ++rank) {
        for (auto file = 0; file < 8; ++file) {
            if (gameState.boardPosition[rank][file]) {
                if (gameState.boardPosition[rank][file]->colour == pieceColour)
                    material += pieceValues[static_cast<int>(gameState.boardPosition[rank][file]->type)];
            }
        }
    }
    return material;
}

float Engine::calculateEndgameWeight(const GameState& gameState) const {
    int phase = 0;

    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            if (!gameState.boardPosition[rank][file])
                continue;

            switch (gameState.boardPosition[rank][file]->type) {
            case Piece::Type::QUEEN:
                phase += 4; break;
            case Piece::Type::ROOK:
                phase += 2; break;
            case Piece::Type::BISHOP:
                phase += 1; break;
            case Piece::Type::KNIGHT:
                phase += 1; break;
            default:
                break;
            }
        }
    }

    return std::clamp(1.0f - static_cast<float>(phase) / 24.0f, 0.0f, 1.0f);
}

void Engine::orderMoves(const Game& game, std::vector<Move>& moves) const {
    for (auto& move : moves) {
        auto moveScoreGuess = 0;
        const auto movePiece = game.getCurrentGameState().boardPosition[move.startSquare.y][move.startSquare.x].value();
        const auto movePieceValue = pieceValues[static_cast<int>(movePiece.type)];

        // check if a piece will be captured on this move
        if (game.getCurrentGameState().boardPosition[move.endSquare.y][move.endSquare.x]) {
            const auto capturePiece = game.getCurrentGameState().boardPosition[move.endSquare.y][move.endSquare.x].value();

            // prioritise capturing opponents most valuable pieces with our least valuable pieces
            moveScoreGuess = 10 * pieceValues[static_cast<int>(capturePiece.type)] - movePieceValue;
        }

        // promoting a pawn is likely to be good
        if (game.checkForPawnPromotionOnNextMove(game.getCurrentGameState(), move))
            moveScoreGuess += pieceValues[static_cast<int>(Piece::Type::QUEEN)];

        // penalise moving pieces to a square under attack by an opponent pawn
        if (game.isSquareUnderAttackByPawn(game.getCurrentGameState(), move.endSquare, game.getCurrentGameState().moveColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE))
            moveScoreGuess -= movePieceValue;

        move.score = moveScoreGuess;
    }

    // sort moves by score value, highest first
    std::ranges::stable_sort(moves, std::greater{}, &Move::score);
}

int Engine::search(Game& game, int alpha, const int beta, const int depthLeft, const int initialDepth, const int plyFromRoot, const std::stop_token& stopToken) {
    if (stopToken.stop_requested())
        return alpha;

    // -------------------- Transposition Table Probe --------------------
    const auto hash = game.getCurrentGameState().zobristHash;
    // fast lookup to find possible match in the transposition table
    const auto& entry = transpositionTable[hash & ttMask];
    // confirm the found entry is an exact match
    const auto ttHit = entry.hashKey == hash;
    Move ttMove{};
    if (ttHit)
        ttMove = entry.bestMove;

    // only use the tt entry's value if we are not at the root
    if (ttHit && depthLeft != initialDepth && entry.depth >= depthLeft) {
        auto ttEvaluation = entry.evaluation;
        // reverse the checkmate distance adjustment that was applied at store time
        if (ttEvaluation > mateThreshold)
            ttEvaluation -= plyFromRoot;
        else if (ttEvaluation < -mateThreshold)
            ttEvaluation += plyFromRoot;

        if (entry.flag == TTEntry::Flag::EXACT || (entry.flag == TTEntry::Flag::LOWERBOUND && ttEvaluation >= beta) || (entry.flag == TTEntry::Flag::UPPERBOUND && ttEvaluation <= alpha))
            return ttEvaluation;
    }

    // -------------------- Depth = 0, Terminal Checks --------------------
    if (depthLeft == 0)
        return quiescenceSearch(game, alpha, beta, plyFromRoot);

    auto moves = game.generateAllLegalMoves(game.getCurrentGameState());
    if (moves.empty()) {
        if (game.isKingInCheck(game.getCurrentGameState(), game.getCurrentGameState().moveColour))
            return minusInfinity + plyFromRoot;
        return 0;
    }

    orderMoves(game, moves);

    // -------------------- Move Ordering Hook: TT Move First --------------------
    if (ttHit) {
        const auto it = std::ranges::find_if(moves, [&ttMove](const Move& move) {
            return move.startSquare == ttMove.startSquare
                && move.endSquare   == ttMove.endSquare
                && move.promotionPieceType == ttMove.promotionPieceType;
        });
        if (it != moves.end() && it != moves.begin())
            std::rotate(moves.begin(), it, it + 1);
    }

    // -------------------- Main Loop (Negamax + Alpha Beta Pruning) --------------------
    const auto originalAlpha = alpha;
    Move localBestMove{};

    for (auto& move : moves) {
        if (stopToken.stop_requested())
            return alpha;

        // engine will always promote a pawn to a queen for the time being
        if (game.checkForPawnPromotionOnNextMove(game.getCurrentGameState(), move))
            move.promotionPieceType = Piece::Type::QUEEN;
        const auto moveDelta = game.movePiece(game.getCurrentGameState(), move);
        const int evaluation = -search(game, -beta, -alpha, depthLeft - 1, initialDepth, plyFromRoot + 1, stopToken);
        game.undoLastMove(game.getCurrentGameState(), moveDelta);
        ++positionsEvaluated;

        if (evaluation > alpha) {
            alpha = evaluation;
            localBestMove = move;
            if (depthLeft == initialDepth)
                bestMove = move;
        }

        // the last move was too good, the opponent won't allow this position to be reached (by playing a different move earlier on)
        // skip remaining moves/prune branch
        if (evaluation >= beta) {
            storeTTEntry(hash, localBestMove, beta, depthLeft, TTEntry::Flag::LOWERBOUND, plyFromRoot);
            return beta;
        }
    }
    storeTTEntry(hash, localBestMove, alpha, depthLeft, alpha > originalAlpha ? TTEntry::Flag::EXACT : TTEntry::Flag::UPPERBOUND, plyFromRoot);
    return alpha;
}

int Engine::quiescenceSearch(Game& game, int alpha, const int beta, const int plyFromRoot) {

    const auto legalMoves = game.generateAllLegalMoves(game.getCurrentGameState());

    // handle checkmate and stalemate
    if (legalMoves.empty()) {
        if (game.isKingInCheck(game.getCurrentGameState(), game.getCurrentGameState().moveColour))
            // checkmate found, use plyFromRoot to prioritise faster mates when winning and slower mates when losing
            return minusInfinity + plyFromRoot;
        // stalemate found
        return 0;
    }

    const auto sideToMoveInCheck = game.isKingInCheck(game.getCurrentGameState(), game.getCurrentGameState().moveColour);

    // static evaluation (stand pat)
    auto evaluation = evaluateBoardPosition(game.getCurrentGameState());
    if (!sideToMoveInCheck) {
        if (evaluation >= beta)
            return beta;
        alpha = std::max(alpha, evaluation);
    }

    // if in check then must search every move to find every possible evasion
    // if not in check then can rely on stand pat and only search captures/tactical moves
    auto moves = sideToMoveInCheck ? legalMoves : game.generateAllLegalMoves(game.getCurrentGameState(), true);
    orderMoves(game, moves);

    // same negamax recursive search with alpha beta pruning as the one in the main search function
    for (auto& move : moves) {
        const auto moveDelta = game.movePiece(game.getCurrentGameState(), move);
        evaluation = -quiescenceSearch(game, -beta, -alpha, plyFromRoot + 1);
        game.undoLastMove(game.getCurrentGameState(), moveDelta);

        if (evaluation >= beta)
            return beta;
        alpha = std::max(alpha, evaluation);
    }

    return alpha;
}

void Engine::storeTTEntry(const uint64_t hashKey, const Move& entryBestMove, int evaluation, const int depth, const TTEntry::Flag flag, const int plyFromRoot) {
    if (evaluation > mateThreshold)
        evaluation += plyFromRoot;
    else if (evaluation < -mateThreshold)
        evaluation -= plyFromRoot;

    transpositionTable[hashKey & ttMask] = TTEntry{hashKey, entryBestMove, evaluation, static_cast<int16_t>(depth), flag};
    ++transpositions;
}

std::uint64_t Engine::perft(Game& game, const int depth) const {
    // perft counts legal move tree size only; it should not evaluate positions or apply search heuristics.
    // depth 0 means "the current position itself is one leaf node".
    if (depth == 0)
        return 1;

    // generate the exact legal children from this position, including all promotion variants.
    const auto moves = generatePerftMoves(game);
    // once we are one ply away from the leaves, the node count is just the number of legal moves.
    if (depth == 1)
        return moves.size();

    std::uint64_t nodes = 0;
    for (const auto& move : moves) {
        // standard recursive perft flow: make move, count descendants, then undo before trying the next sibling.
        const auto moveDelta = game.movePiece(game.getCurrentGameState(), move);
        nodes += perft(game, depth - 1);
        game.undoLastMove(game.getCurrentGameState(), moveDelta);
    }
    return nodes;
}

std::vector<Move> Engine::generatePerftMoves(const Game& game) const {
    // perft must treat each promotion piece as a separate legal move, whereas the search currently defaults promotions to queens.
    static const std::array promotionPieceTypes = {
        Piece::Type::QUEEN,
        Piece::Type::ROOK,
        Piece::Type::BISHOP,
        Piece::Type::KNIGHT
    };

    std::vector<Move> perftMoves;
    for (const auto& move : game.generateAllLegalMoves(game.getCurrentGameState())) {
        if (game.checkForPawnPromotionOnNextMove(game.getCurrentGameState(), move)) {
            // a single legal promotion square expands into four separate UCI/legal moves: q, r, b and n.
            for (const auto promotionPieceType : promotionPieceTypes) {
                auto promotionMove = move;
                promotionMove.promotionPieceType = promotionPieceType;
                perftMoves.emplace_back(promotionMove);
            }
        }
        // non-promotion moves can be forwarded unchanged.
        else
            perftMoves.emplace_back(move);
    }
    return perftMoves;
}
