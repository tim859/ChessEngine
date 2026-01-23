#include "ucisession.h"

// get all tokens between startTokenIndex (inclusive) and endTokenIndex (exclusive) as one token/string
std::string getCombinedTokens(const std::vector<std::string>& tokens, const int startTokenIndex, const int endTokenIndex) {
    auto token = tokens[startTokenIndex];
    auto i = startTokenIndex + 1;
    while (i < endTokenIndex) {
        token += " " + tokens[i];
        ++i;
    }
    return token;
}

std::string getLowercase(const std::string& string) {
    std::string lowercaseString;
    for (const auto& character : string) {
        lowercaseString += std::tolower(static_cast<unsigned char>(character));
    }
    return lowercaseString;
}

int main() {
    UCISession uciSession;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty())
            break;

        std::size_t currentIndex = 0;
        std::vector<std::string> tokens;
        std::string currentToken;

        // tokenise the line, the first token will be the uci command and the tokens after will be its modifiers/values
        while (currentIndex < line.size()) {
            if (line[currentIndex] == ' ') {
                if (!currentToken.empty()) {
                    tokens.push_back(currentToken);
                    currentToken.clear();
                }
            }
            else
                currentToken += line[currentIndex];
            ++currentIndex;
        }
        if (!currentToken.empty())
            tokens.push_back(currentToken);

        // ########## UCI Command Parsing ##########

        if (auto firstToken = getLowercase(tokens[0]); firstToken == "uci")
            uciSession.cmdUCI();

        else if (tokens.size() > 1 && firstToken == "debug") {
            if (getLowercase(tokens[1]) == "on")
                uciSession.cmdDebug(true);
            else if (getLowercase(tokens[1]) == "off")
                uciSession.cmdDebug(false);
            else
                std::cerr << R"(You must specify "debug on" or "debug off")" << std::endl;
        }

        else if (firstToken == "isready")
            uciSession.cmdIsReady();

        // setoption command requires format "setoption name <id> [value <x>]"
        else if (firstToken == "setoption") {
            // check there are enough tokens for "setoption", "name" and <id>
            if (tokens.size() < 3) {
                std::cerr << "\"setoption\" requires: setoption name <id> [value <x>]" << std::endl;
                continue;
            }
            // check "name" is the token immediately after "setoption"
            if (getLowercase(tokens[1]) != "name") {
                std::cerr << R"("setoption" must be immediately followed by "name".)" << std::endl;
                continue;
            }

            // find "value" if present, search from index 3, since index 2 is start of <id>
            auto valueIndex = 0;
            if (tokens.size() > 3) {
                for (auto i = 3; i < tokens.size(); ++i) {
                    if (getLowercase(tokens[i]) == "value") {
                        valueIndex = i;
                        break;
                    }
                }
            }

            if (valueIndex == 0)
                // "value" is not present so all tokens after "name" are <id>
                uciSession.cmdSetOption(getCombinedTokens(tokens, 2, static_cast<int>(tokens.size())));
            else {
                // "value" is present, make sure "value" is not the last token as <x> must come after it
                if (valueIndex + 1 == tokens.size()) {
                    std::cerr << "There must be a value after \"value\".\n";
                    continue;
                }

                // "value" is present with at least one token after it
                uciSession.cmdSetOption(getCombinedTokens(tokens, 2, valueIndex), getCombinedTokens(tokens, valueIndex + 1, static_cast<int>(tokens.size())));
            }
        }

        else if (firstToken == "register") {
            // ignore
        }

        else if (firstToken == "ucinewgame") {
            uciSession.cmdUCINewGame();
        }

        else if (firstToken == "position") {

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