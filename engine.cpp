#include "engine.h"
#include <random>
#include <thread>
#include <iostream>

Move Engine::generateEngineMove(Game& game) {
    // call searchMoves with a simulated gamestate that mirrors the current gamestate
    auto simulatedGameState = game.getCurrentGameState();
    auto* simulatedGameStateHistory = new std::vector<GameState>{simulatedGameState};
    bestMove = game.generateAllLegalMoves(simulatedGameState).front();
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

int Engine::alphaBetaSearch(Game& game, GameState& gameState, std::vector<GameState>* gameStateHistory, int alpha, int beta, int depthLeft, const int initalDepth) {
    if (depthLeft == 0)
        // TODO: implement quiescent function
            return evaluateBoardPosition(gameState);

    const auto moves = game.generateAllLegalMoves(gameState);
    if (moves.empty()) {
        if (game.checkIsKingInCheck(gameState, gameState.moveColour))
            return minusInfinity;
        return 0;
    }

    for (const auto& move : moves) {
        // engine will always promote a pawn to a queen for the time being
        const auto pawnPromotionPiece = Piece(Piece::Type::QUEEN, gameState.moveColour);
        game.movePiece(gameState, move, gameStateHistory, &pawnPromotionPiece);
        const int evaluation = -alphaBetaSearch(game, gameState, gameStateHistory, -beta, -alpha, depthLeft - 1, initalDepth);
        game.undoLastMove(gameState, gameStateHistory);
        ++movesSearched;

        if (evaluation >= beta)
            return beta;

        if (evaluation > alpha) {
            alpha = evaluation;
            if (depthLeft == initalDepth) {
                bestMove = move;
                std::cout << "Move: " << move.startSquare.x << "," << move.startSquare.y << " -> " << move.endSquare.x << "," << move.endSquare.y << " Evaluation: " << evaluation << std::endl;
            }
        }
    }
    return alpha;
}

// bestMove = move;
// ++bestMoveChanged;
// std::cout << "Move: " << move.startSquare.x << "," << move.startSquare.y << " -> " << move.endSquare.x << "," << move.endSquare.y << " Evaluation: " << evaluation << std::endl;