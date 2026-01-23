#include "ucisession.h"

void UCISession::cmdUCI() const {
    std::cout << "id name " << uciSettings.name << std::endl;
    std::cout << "id author " << uciSettings.author << std::endl;
    std::cout << "uciok" << std::endl;
}

void UCISession::cmdDebug(const bool option) {
    uciSettings.debugMode = option;
}

void UCISession::cmdIsReady() {
    std::cout << "readyok" << std::endl;
}

void UCISession::cmdSetOption(std::string name) {

}

void UCISession::cmdSetOption(const std::string name, const std::string value) {

}

void UCISession::cmdUCINewGame() {
    engine.reset();
    game.reset();
}
