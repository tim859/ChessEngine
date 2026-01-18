#ifndef CHESS_ENGINE_H
#define CHESS_ENGINE_H

#include "game.h"

class Engine {
    const std::map<Piece::Type, int> pieceValues = {{Piece::Type::PAWN, 100}, {Piece::Type::KNIGHT, 320}, {Piece::Type::BISHOP, 330}, {Piece::Type::ROOK, 500}, {Piece::Type::QUEEN, 900}, {Piece::Type::KING, 20000}};
    const int minusInfinity = -999999;
    const int infinity = 999999;
    Move bestMove = {};
    int movesSearched = 0;

public:
    Move generateEngineMove(Game& game);

private:
    [[nodiscard]] int evaluateBoardPosition(const GameState& gameState) const;
    [[nodiscard]] int countMaterial(const GameState& gameState, Piece::Colour pieceColour) const;
    int alphaBetaSearch(Game& game, GameState& gameState, std::vector<GameState>* gameStateHistory, int alpha, int beta, int depthLeft, int initialDepth);
};

#endif //CHESS_ENGINE_H