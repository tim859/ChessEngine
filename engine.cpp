#include "engine.h"
#include <random>

Move Engine::generateEngineMove(std::vector<Move> legalMoves) {
    std::random_device random;
    std::mt19937 generator(random());
    std::uniform_int_distribution<> range(0, legalMoves.size());
    const auto moveIndex = range(generator);
    return legalMoves[moveIndex];
}
