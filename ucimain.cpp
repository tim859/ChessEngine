#include "ucisession.h"

int main() {
    UCISession uciSession;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty())
            continue;

        std::vector<std::string> tokens;
        std::string currentToken;

        // tokenise the input, tokens are separated by whitespace
        for (const auto& character : line) {
            if (character == ' ') {
                if (!currentToken.empty()) {
                    tokens.push_back(currentToken);
                    currentToken.clear();
                }
            }
            else
                currentToken += character;
        }
        if (!currentToken.empty())
            tokens.push_back(currentToken);

        const auto checkUCIKeyword = [](const std::string& token) {
            return token == "uci" || token == "debug" || token == "isready"
            || token == "setoption" || token == "ucinewgame" || token =="go"
            || token == "ponderhit" || token == "position" || token == "quit"
            || token == "register" || token == "stop";
        };

        // find position of the first keyword
        auto it = tokens.begin();
        while (it != tokens.end() && !checkUCIKeyword(*it)) {
            ++it;
        }
        // erase all tokens before the first keyword
        tokens.erase(tokens.begin(), it);
        if (tokens.empty()) {
            std::cout << "no valid commands found" << std::endl;
            continue;
        }

        // ########## UCI command validation and application ##########

        const auto firstToken = uciSession.getLowercase(tokens[0]);
        if (firstToken == "uci")
            uciSession.cmdUCI();

        else if (firstToken == "debug") {
            if (!uciSession.validateCmdDebug(tokens)) {
                std::cout << R"("debug" command requires specifier of "on" or "off")" << std::endl;
                continue;
            }
            uciSession.cmdDebug(tokens);
        }

        else if (firstToken == "isready")
            uciSession.cmdIsReady();

        else if (firstToken == "setoption") {
            if (!uciSession.validateCmdSetOption(tokens)) {
                std::cout << R"("setoption" requires: "setoption name <id> [value <x>]")" << std::endl;
                continue;
            }
            uciSession.cmdSetOption(tokens);
        }

        else if (firstToken == "register") {
            // ignore
        }

        else if (firstToken == "ucinewgame")
            uciSession.cmdUCINewGame();

        else if (firstToken == "position") {
            if (uciSession.validateCmdPosition(tokens))
                uciSession.cmdPosition(tokens);
        }

        else if (firstToken == "go") {
        }

        else if (firstToken == "stop") {

        }

        else if (firstToken == "ponderhit") {

        }

        else if (firstToken == "quit")
            return 0;
    }
}