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

class Engine {
    const std::map<Piece::Type, int> pieceValues = {{Piece::Type::PAWN, 100}, {Piece::Type::KNIGHT, 320}, {Piece::Type::BISHOP, 330}, {Piece::Type::ROOK, 500}, {Piece::Type::QUEEN, 900}, {Piece::Type::KING, 20000}};
    const int minusInfinity = -999999;
    const int infinity = 999999;
    Move bestMove = {};
    int movesSearched = 0;

public:
    void reset();
    Move generateEngineMove(const Game& game, const EngineSearchSettings& engineSearchSettings, const std::stop_token& stopToken);
    [[nodiscard]] std::vector<std::pair<Move, std::uint64_t>> generatePerftDivide(const Game& game, int depth) const;

private:
    [[nodiscard]] int evaluateBoardPosition(const GameState& gameState) const;
    [[nodiscard]] int countMaterial(const GameState& gameState, Piece::Colour pieceColour) const;
    [[nodiscard]] std::uint64_t perft(Game& game, int depth) const;
    [[nodiscard]] std::vector<Move> generatePerftMoves(const Game& game) const;
    int alphaBetaSearch(Game& game, int alpha, int beta, int depthLeft, int initialDepth, const std::stop_token& stopToken);
    void orderMoves (const Game& game, std::vector<Move>& moves) const;
};

#endif //CHESS_ENGINE_H
