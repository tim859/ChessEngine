#include "engine.h"
#include <random>
#include <thread>

Move Engine::generateEngineMove(std::vector<Move> legalMoves) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::random_device random;
    std::mt19937 generator(random());
    std::uniform_int_distribution<> range(0, legalMoves.size());
    const auto moveIndex = range(generator);
    return legalMoves[moveIndex];
}
