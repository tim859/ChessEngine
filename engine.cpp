#include "engine.h"
#include <random>
#include <thread>
#include <iostream>

void Engine::reset() {

}

Move Engine::generateEngineMove(Game& game) {
    // call searchMoves with a simulated gamestate that mirrors the current gamestate
    auto simulatedGameState = game.getCurrentGameState();
    auto* simulatedGameStateHistory = new std::vector<GameState>{simulatedGameState};
    bestMove = game.generateAllLegalMoves(simulatedGameState).front();
    movesSearched = 0;
    alphaBetaSearch(game, simulatedGameState, simulatedGameStateHistory, minusInfinity, infinity, 5, 5);
    std::cout << "moves searched: " << movesSearched << std::endl;
    return bestMove;
}

int Engine::evaluateBoardPosition(const GameState& gameState) const {
    const auto evaluation = countMaterial(gameState, Piece::Colour::WHITE) - countMaterial(gameState, Piece::Colour::BLACK);;
    const auto perspective = gameState.moveColour == Piece::Colour::WHITE ? 1 : -1;
    //std::cout << "evaluation: " << evaluation << std::endl;
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

int Engine::alphaBetaSearch(Game& game, GameState& gameState, std::vector<GameState>* gameStateHistory, int alpha, const int beta, const int depthLeft, const int initialDepth) {
    if (depthLeft == 0)
        // TODO: implement quiescent function
            return evaluateBoardPosition(gameState);

    auto moves = game.generateAllLegalMoves(gameState);
    if (moves.empty()) {
        if (game.checkIsKingInCheck(gameState, gameState.moveColour))
            return minusInfinity;
        return 0;
    }

    orderMoves(game, gameState, moves);

    for (const auto& move : moves) {
        // engine will always promote a pawn to a queen for the time being
        const auto pawnPromotionPiece = Piece(Piece::Type::QUEEN, gameState.moveColour);
        game.movePiece(gameState, move, gameStateHistory, &pawnPromotionPiece);
        const int evaluation = -alphaBetaSearch(game, gameState, gameStateHistory, -beta, -alpha, depthLeft - 1, initialDepth);
        game.undoLastMove(gameState, gameStateHistory);
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

void Engine::orderMoves(const Game& game, const GameState& gameState, std::vector<Move>& moves) const {
    for (auto& move : moves) {
        auto moveScoreGuess = 0;
        const auto movePiece = gameState.boardPosition[move.startSquare.y][move.startSquare.x].value();
        const auto movePieceValue = pieceValues.at(movePiece.type);

        // check if a piece will be captured on this move
        if (gameState.boardPosition[move.endSquare.y][move.endSquare.x]) {
            const auto capturePiece = gameState.boardPosition[move.endSquare.y][move.endSquare.x].value();

            // prioritise capturing opponents most valuable pieces with our least valuable pieces
            moveScoreGuess = 10 * pieceValues.at(capturePiece.type) - movePieceValue;
        }

        // promoting a pawn is likely to be good
        if (game.checkForPawnPromotionOnNextMove(gameState, move))
            moveScoreGuess += pieceValues.at(Piece::Type::QUEEN);

        // penalise moving pieces to a square under attack by an opponent pawn
        if (game.checkIsSquareUnderAttackByPawn(gameState, move.endSquare, gameState.moveColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE))
            moveScoreGuess -= movePieceValue;

        move.score = moveScoreGuess;
    }

    // sort moves by score value, highest first
    std::ranges::stable_sort(moves, std::greater{}, &Move::score);
}
