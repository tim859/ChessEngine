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

        else if (tokens.size() >= 2 && firstToken == "debug") {
            if (getLowercase(tokens[1]) == "on")
                uciSession.cmdDebug(true);
            else if (getLowercase(tokens[1]) == "off")
                uciSession.cmdDebug(false);
            else
                std::cerr << R"(you must specify "debug on" or "debug off")" << std::endl;
        }

        else if (firstToken == "isready")
            uciSession.cmdIsReady();

        // setoption command requires format "setoption name <id> [value <x>]"
        else if (firstToken == "setoption") {
            // check there are enough tokens for "setoption", "name" and <id>
            if (tokens.size() <= 2) {
                std::cerr << "\"setoption\" requires: setoption name <id> [value <x>]" << std::endl;
                continue;
            }
            // check "name" is the token immediately after "setoption"
            if (getLowercase(tokens[1]) != "name") {
                std::cerr << R"("setoption" must be followed by "name")" << std::endl;
                continue;
            }

            // find "value" if present, search from index 3, since index 2 is start of <id>
            auto valueIndex = 0;
            if (tokens.size() >= 4) {
                for (size_t i = 3; i < tokens.size(); ++i) {
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
                    std::cerr << "there must be a value after \"value\"" << std::endl;
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

        else if (tokens.size() > 1 && firstToken == "position") {
            std::vector<std::string> moves;

            if (getLowercase(tokens[1]) == "startpos") {
                if (tokens.size() == 2)
                    // allow "position startpos"
                    uciSession.cmdPositionStartpos(moves);
                else if (tokens.size() >= 3 && getLowercase(tokens[2]) == "moves") {
                    // allow "position startpos moves"
                    // allow "position startpos moves <move1> ... <movei>"
                    for (size_t i = 3; i < tokens.size(); ++i)
                        moves.push_back(tokens[i]);
                    uciSession.cmdPositionStartpos(moves);
                }
                else
                    std::cerr << R"("position startpos" configurations allowed are: "position startpos", "position startpos moves", "position startpos moves <move1> ... <movei>")" << std::endl;
            }
            else if (getLowercase(tokens[1]) == "fen") {
                // check for at least 8 tokens, "position fen" and then 6 tokens for the fen string
                if (tokens.size() < 8) {
                    std::cerr << R"("position fen" must be followed by a 6 field FEN)" << std::endl;
                    continue;
                }

                // construct the fen string
                std::string fen = tokens[2];
                for (size_t i = 3; i < 8; ++i)
                    fen += " " + tokens[i];

                if (tokens.size() == 8 || (tokens.size() == 9 && getLowercase(tokens[8]) == "moves"))
                    // allow "position fen <fenstring>"
                    // allow "position fen <fenstring> moves"
                    uciSession.cmdPositionFen(fen, moves);
                else if (tokens.size() >= 10 && getLowercase(tokens[8]) == "moves") {
                    // allow "position fen <fenstring> moves <move1> ... <movei>"
                    for (size_t i = 9; i < tokens.size(); ++i)
                        moves.push_back(tokens[i]);
                    uciSession.cmdPositionFen(fen, moves);
                }
                else
                    std::cerr << R"("position fen" configurations allowed are: "position fen <fenstring>", "position fen <fenstring> moves", "position fen <fenstring> moves <move1> ... <movei>")" << std::endl;
            }
            else
                std::cerr << R"("position" requires "position [fen <fenstring> | startpos ]  [moves] [<move1> ... <movei>]")" << std::endl;
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