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
    // ---------- validate uci commands ----------

    [[nodiscard]] bool validateCmdDebug(const std::vector<std::string>& tokens) const;
    [[nodiscard]] bool validateCmdSetOption(const std::vector<std::string>& tokens) const;
    [[nodiscard]] bool validateCmdPosition(const std::vector<std::string>& tokens) const;
    [[nodiscard]] bool validateCmdGo(const std::vector<std::string>& tokens) const;

    // ---------- apply uci commands ----------

    void cmdUCI() const;
    void cmdDebug(const std::vector<std::string>& tokens);
    void cmdIsReady() const;
    void cmdSetOption(const std::vector<std::string>& tokens) const;
    void cmdUCINewGame();
    void cmdPosition(const std::vector<std::string>& tokens);
    void cmdGo(const std::vector<std::string>& tokens);

    // ---------- helper functions ----------

    [[nodiscard]] std::string getLowercase(const std::string& string) const;

private:
    [[nodiscard]] std::string getCombinedTokens(const std::vector<std::string>& tokens, size_t startTokenIndex, size_t endTokenIndex) const;
    bool stringToInt(const std::string& string, int& value) const;
    [[nodiscard]] bool validateUCIMove(const std::string& move) const;
    void applyMovesToGameState(const std::vector<std::string>& moves);
};

#endif //CHESS_UCI_H