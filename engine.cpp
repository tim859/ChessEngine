#include "engine.h"
#include <random>
#include <iostream>

void Engine::reset() {

}

Move Engine::generateEngineMove(const Game& game, const EngineSearchSettings& engineSearchSettings, const std::stop_token& stopToken) {
    // TODO: implement all the engine search settings into the search
    // TODO: allow the search to be cancelled early and return the best move found so far
    // call searchMoves with a simulated gamestate that mirrors the current gamestate
    auto simulatedGame = game;
    bestMove = game.generateAllLegalMoves(simulatedGame.getCurrentGameState()).front();
    movesSearched = 0;
    alphaBetaSearch(simulatedGame ,minusInfinity, infinity, 5, 5);
    std::cout << "moves searched: " << movesSearched << std::endl;
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
        simulatedGame.movePiece(simulatedGame.getCurrentGameState(), move, simulatedGame.getCurrentGameStateHistory());
        const std::uint64_t nodes = perft(simulatedGame, depth - 1);
        simulatedGame.undoLastMove(simulatedGame.getCurrentGameState(), simulatedGame.getCurrentGameStateHistory());
        // keep the original move paired with its subtree size so UCISession can print "move: nodes" lines.
        divide.emplace_back(move, nodes);
    }
    return divide;
}

int Engine::evaluateBoardPosition(const GameState& gameState) const {
    const auto evaluation = countMaterial(gameState, Piece::Colour::WHITE) - countMaterial(gameState, Piece::Colour::BLACK);;
    const auto perspective = gameState.moveColour == Piece::Colour::WHITE ? 1 : -1;
    // std::cout << "evaluation: " << evaluation << std::endl;
    return evaluation * perspective;
}

int Engine::countMaterial(const GameState& gameState, const Piece::Colour pieceColour) const {
    auto material = 0;
    for (auto rank = 0; rank < 8; ++rank) {
        for (auto file = 0; file < 8; ++file) {
            if (gameState.boardPosition[rank][file]) {
                if (gameState.boardPosition[rank][file]->colour == pieceColour)
                    material += pieceValues.at(gameState.boardPosition[rank][file]->type);
            }
        }
    }
    //std::cout << "material: " << material << std::endl;
    return material;
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
        game.movePiece(game.getCurrentGameState(), move, game.getCurrentGameStateHistory());
        nodes += perft(game, depth - 1);
        game.undoLastMove(game.getCurrentGameState(), game.getCurrentGameStateHistory());
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

int Engine::alphaBetaSearch(Game& game, int alpha, const int beta, const int depthLeft, const int initialDepth) {
    if (depthLeft == 0)
        // TODO: implement quiescent function
        return evaluateBoardPosition(game.getCurrentGameState());

    auto moves = game.generateAllLegalMoves(game.getCurrentGameState());
    if (moves.empty()) {
        if (game.checkIsKingInCheck(game.getCurrentGameState(), game.getCurrentGameState().moveColour))
            return minusInfinity;
        return 0;
    }

    orderMoves(game, moves);

    for (auto& move : moves) {
        // engine will always promote a pawn to a queen for the time being
        move.promotionPieceType = Piece::Type::QUEEN;
        game.movePiece(game.getCurrentGameState(), move, game.getCurrentGameStateHistory());
        const int evaluation = -alphaBetaSearch(game, -beta, -alpha, depthLeft - 1, initialDepth);
        game.undoLastMove(game.getCurrentGameState(), game.getCurrentGameStateHistory());
        ++movesSearched;

        if (evaluation >= beta)
            return beta;

        if (evaluation > alpha) {
            alpha = evaluation;
            if (depthLeft == initialDepth) {
                bestMove = move;
                std::cout << "Move: " << move.startSquare.x << "," << move.startSquare.y << " -> " << move.endSquare.x << "," << move.endSquare.y << " Evaluation: " << evaluation << std::endl;
            }
        }
    }
    return alpha;
}

void Engine::orderMoves(const Game& game, std::vector<Move>& moves) const {
    for (auto& move : moves) {
        auto moveScoreGuess = 0;
        const auto movePiece = game.getCurrentGameState().boardPosition[move.startSquare.y][move.startSquare.x].value();
        const auto movePieceValue = pieceValues.at(movePiece.type);

        // check if a piece will be captured on this move
        if (game.getCurrentGameState().boardPosition[move.endSquare.y][move.endSquare.x]) {
            const auto capturePiece = game.getCurrentGameState().boardPosition[move.endSquare.y][move.endSquare.x].value();

            // prioritise capturing opponents most valuable pieces with our least valuable pieces
            moveScoreGuess = 10 * pieceValues.at(capturePiece.type) - movePieceValue;
        }

        // promoting a pawn is likely to be good
        if (game.checkForPawnPromotionOnNextMove(game.getCurrentGameState(), move))
            moveScoreGuess += pieceValues.at(Piece::Type::QUEEN);

        // penalise moving pieces to a square under attack by an opponent pawn
        if (game.checkIsSquareUnderAttackByPawn(game.getCurrentGameState(), move.endSquare, game.getCurrentGameState().moveColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE))
            moveScoreGuess -= movePieceValue;

        move.score = moveScoreGuess;
    }

    // sort moves by score value, highest first
    std::ranges::stable_sort(moves, std::greater{}, &Move::score);
}
