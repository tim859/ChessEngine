#include "engine.h"
#include <random>
#include <thread>
#include <iostream>

Move Engine::generateEngineMove(Game& game) {
    // call searchMoves with a simulated gamestate that mirrors the current gamestate
    auto simulatedGameState = game.getCurrentGameState();
    auto* simulatedGameStateHistory = new std::vector<GameState>{simulatedGameState};
    searchMoves(game, simulatedGameState, simulatedGameStateHistory, 1);
    return *bestMove;
}

int Engine::evaluateBoardPosition(const GameState& gameState) const {
    const auto evaluation = countMaterial(gameState, Piece::Colour::WHITE) - countMaterial(gameState, Piece::Colour::BLACK);
    const auto perspective = gameState.moveColour == Piece::Colour::WHITE ? 1 : -1;
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
    return material;
}

int Engine::searchMoves(Game& game, GameState& gameState, std::vector<GameState>* gameStateHistory, const int depth) {
    if (depth == 0)
        return evaluateBoardPosition(gameState);

    const auto moves = game.generateAllLegalMoves(gameState);
    if (moves.empty()) {
        if (game.checkIsKingInCheck(gameState, gameState.moveColour))
            return -std::numeric_limits<int>::infinity();
        return 0;
    }

    int bestEvaluation = -std::numeric_limits<int>::infinity();

    for (const auto& move : moves) {
        game.movePiece(gameState, move, gameStateHistory);
        int evaluation = -searchMoves(game, gameState, gameStateHistory, depth - 1);
        if (evaluation > bestEvaluation) {
            bestEvaluation = evaluation;
            *bestMove = move;
        }
        game.undoLastMove(gameState, gameStateHistory);
    }
    return bestEvaluation;
}
