#ifndef CHESS_ENGINE_H
#define CHESS_ENGINE_H

#include "game.h"

class Engine {
public:
    Move generateEngineMove(const std::vector<Move> &legalMoves);
};

#endif //CHESS_ENGINE_H