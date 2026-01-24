#ifndef CHESS_UCI_H
#define CHESS_UCI_H
#include "game.h"
#include "engine.h"
#include <iostream>

struct UCISettings {
    const std::string name = "Chess Engine";
    const std::string author = "Tim Swan";
    const std::string startPosFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w QKqk - 0 0";
    bool debugMode = false;
};

class UCISession {
    Game game;
    Engine engine;
    UCISettings uciSettings;

public:
    void cmdUCI() const;
    void cmdDebug(bool option);
    void cmdIsReady() const;
    void cmdSetOption(const std::string& name) const;
    void cmdSetOption(const std::string& name, const std::string& value) const;
    void cmdUCINewGame();
    void cmdPositionStartpos(const std::vector<std::string>& moves);
    void cmdPositionFen(const std::string& fen , const std::vector<std::string>& moves);

private:
    void applyMovesToGameState(const std::vector<std::string>& moves);
};

#endif //CHESS_UCI_H