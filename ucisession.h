#ifndef CHESS_UCI_H
#define CHESS_UCI_H
#include "game.h"
#include "engine.h"
#include <iostream>
#include <mutex>
#include <thread>

struct UCISettings {
    const std::string name = "Chess Engine";
    const std::string author = "Tim Swan";
    bool debugMode = false;
};

struct SetOptionCommand {
    std::string name, value;
};

struct PositionCommand {
    std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w QKqk - 0 0";
    std::vector<Move> moves;
};

// use alias instead of a new struct as they both contain exactly the same properties
using GoCommand = EngineSearchSettings;

class UCISession {
    Game game;
    Engine engine;
    UCISettings uciSettings;
    std::jthread engineThread;
    std::jthread completionThread;
    std::mutex engineMoveMutex;
    std::optional<Move> pendingEngineMove;

public:
    void uci() const;
    void debug(bool debugCommand);
    void isReady() const;
    [[nodiscard]] bool setOption(const SetOptionCommand& setOptionCommand) const;
    void uciNewGame();
    bool position(const PositionCommand& positionCommand);
    void go(const GoCommand& goCommand);
    void stop();

private:
    [[nodiscard]] std::string convertGameStateMoveToUCIMove(const Move& move) const;
};

#endif //CHESS_UCI_H