#ifndef CHESS_UCI_H
#define CHESS_UCI_H
#include "game.h"
#include "engine.h"
#include <iostream>

struct UCISettings {
    const std::string name = "Chess Engine";
    const std::string author = "Tim Swan";
    bool debugMode = false;
};

class UCISession {
    Game game;
    Engine engine;
    UCISettings uciSettings;

public:
    void cmdUCI() const;
    void cmdDebug(bool option);
    void cmdIsReady();
    void cmdSetOption(std::string name);
    void cmdSetOption(std::string name, std::string value);
    void cmdUCINewGame();
};

#endif //CHESS_UCI_H