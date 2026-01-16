#include "engine.h"
#include <random>
#include <thread>
#include <iostream>

Move Engine::generateEngineMove(const std::vector<Move> &legalMoves) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::random_device random;
    std::mt19937 generator(random());
    std::uniform_int_distribution<> range(0, legalMoves.size() - 1);
    const auto moveIndex = range(generator);
    return legalMoves[moveIndex];
}
