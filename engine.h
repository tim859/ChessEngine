#ifndef CHESS_ENGINE_H
#define CHESS_ENGINE_H
#include "game.h"
#include <map>
#include <thread>

struct EngineSearchSettings {
    std::vector<Move> searchMoves;
    bool ponder = false;
    bool infinite = false;
    std::optional<int> wtime, btime, winc, binc, movestogo, depth, nodes, mate, movetime, perft;
};

struct TTEntry
{
    uint64_t hashKey;
    Move bestMove;
    int evaluation;
    int16_t depth;
    enum class Flag : uint8_t {EXACT, LOWERBOUND, UPPERBOUND} flag;
};

class Engine {
    const std::array<int, 6> pieceValues = {20000, 900, 500, 330, 320, 100};
    static constexpr int minusInfinity = -999999;
    static constexpr int infinity = 999999;
    Move bestMove = {};
    int positionsEvaluated = 0;
    int transpositions = 0;

    // transposition table attributes
    static constexpr int mateThreshold = infinity - 1000;
    static constexpr size_t ttSize = 1 << 20;
    static constexpr uint64_t ttMask = ttSize - 1;
    std::vector<TTEntry> transpositionTable = std::vector<TTEntry>(ttSize);

public:
    void reset();
    Move generateEngineMove(const Game& game, const EngineSearchSettings& engineSearchSettings, const std::stop_token& stopToken);
    [[nodiscard]] std::vector<std::pair<Move, std::uint64_t>> generatePerftDivide(const Game& game, int depth) const;

private:
    [[nodiscard]] int evaluateBoardPosition(const GameState& gameState) const;
    [[nodiscard]] int evaluateKingPositionsEndgame(const GameState& gameState, Piece::Colour friendlyColour, float endgameWeight) const;
    [[nodiscard]] int countMaterial(const GameState& gameState, Piece::Colour pieceColour) const;
    [[nodiscard]] float calculateEndgameWeight(const GameState& gameState) const;
    void orderMoves (const Game& game, std::vector<Move>& moves) const;
    int search(Game& game, int alpha, int beta, int depthLeft, int initialDepth, int plyFromRoot, const std::stop_token& stopToken);
    int quiescenceSearch(Game& game, int alpha, int beta, int plyFromRoot);
    void storeTTEntry(uint64_t hashKey, const Move& entryBestMove, int evaluation, int depth, TTEntry::Flag flag, int plyFromRoot);

    // performance testing
    [[nodiscard]] std::uint64_t perft(Game& game, int depth) const;
    [[nodiscard]] std::vector<Move> generatePerftMoves(const Game& game) const;
};

#endif //CHESS_ENGINE_H
