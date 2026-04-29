#include "ucisession.h"

#include <charconv>
#include <unordered_map>

std::string getLowercase(const std::string &string) {
    std::string lowercaseString;
    for (const auto& character : string) {
        lowercaseString += std::tolower(static_cast<unsigned char>(character));
    }
    return lowercaseString;
}

// get all tokens between startTokenIndex (exclusive) and endTokenIndex (exclusive) as one token/string
std::string getCombinedTokens(const std::vector<std::string> &tokens, const size_t startTokenIndex, const size_t endTokenIndex) {
    auto combinedTokens = tokens[startTokenIndex + 1];
    auto i = startTokenIndex + 2;
    while (i < endTokenIndex) {
        combinedTokens += " " + tokens[i];
        ++i;
    }
    return combinedTokens;
}

bool validateUCIMove(const std::string &move) {
    // algebraic move must have a length of either 4 or 5 characters
    if (move.size() != 4 && move.size() != 5)
        return false;

    // first character must be lowercase letter a to h, second character must be digit 1 to 8, third character must be lowercase letter a to h, fourth character must be digit 1 to 8
    if (move[0] < 'a' || move[0] > 'h' || move[1] < '1' || move[1] > '8' || move[2] < 'a' || move[2] > 'h' || move[3] < '1' || move[3] > '8')
        return false;

    // if move has 5 characters then the fifth character must be lowercase letter q, r, b or n
    if (move.size() == 5 && move[4] != 'q' && move[4] != 'r' && move[4] != 'b' && move[4] != 'n')
        return false;

    return true;
}

Move convertUCIMoveToGameStateMove(const std::string &uciMove) {
    // failsafe to prevent out of bounds errors, but this function should only ever be called right after checking with validateUCIMove anyway
    if (!validateUCIMove(uciMove)) {
        std::cerr << "Warning! Attempt to convert UCI move to GameState move failed as UCI move failed validation." << std::endl;
        return Move({0, 0}, {0, 0});
    }
    auto move = Move(Vector2Int(uciMove[0] - 'a', 7 - (uciMove[1] - '1')), Vector2Int(uciMove[2] - 'a', 7 - (uciMove[3] - '1')));
    if (uciMove.length() == 5) {
        switch (uciMove[4]) {
            case 'q':
                move.promotionPieceType = Piece::Type::QUEEN;
                break;
            case 'r':
                move.promotionPieceType = Piece::Type::ROOK;
                break;
            case 'b':
                move.promotionPieceType = Piece::Type::BISHOP;
                break;
            case 'n':
                move.promotionPieceType = Piece::Type::KNIGHT;
                break;
        }
    }
    return move;
}

bool stringToInt(const std::string &string, int &value) {
    std::from_chars_result result = std::from_chars(string.data(), string.data() + string.size(), value);
    return result.ec == std::errc{} && result.ptr == string.data() + string.size();
}

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

        // ########## UCI command and value parser ##########

        // NOTE: the parser is designed to ignore as much garbage as possible for maximal resilience.
        // checks are focused on ensuring the right keywords are there with enough tokens after or inbetween them.
        // the content of the tokens is validated in uciSession, the validation here is focused purely on token structure

        const auto firstToken = getLowercase(tokens[0]);

        if (firstToken == "uci")
            uciSession.uci();

        else if (firstToken == "debug") {
            const std::string debugFailOutput = R"("debug" command requires specifier of "on" or "off")";
            if (tokens.size() < 2) {
                std::cout << debugFailOutput << std::endl;
                continue;
            }

            for (size_t i = 1; i < tokens.size(); ++i) {
                if (getLowercase(tokens[i]) == "on") {
                    uciSession.debug(true);
                    break;
                }
                if (getLowercase(tokens[i]) == "off") {
                    uciSession.debug(false);
                    break;
                }
                if (i + 1 == tokens.size()) {
                    std::cout << debugFailOutput << std::endl;
                    break;
                }
            }
        }

        else if (firstToken == "isready")
            uciSession.isReady();

        else if (firstToken == "setoption") {
            const std::string setOptionFailOutput = R"("setoption" command requires: "setoption name <id> [value <x>]")";
            if (tokens.size() < 3) {
                std::cout << setOptionFailOutput << std::endl;
                continue;
            }

            // find indexes of "name" (required) and "value" (optional)
            std::optional<int> nameTokenIndex;
            std::optional<int> valueTokenIndex;
            for (size_t i = 1; i < tokens.size(); ++i) {
                if (getLowercase(tokens[i]) == "name") {
                    nameTokenIndex = i;
                }
                else if (getLowercase(tokens[i]) == "value") {
                    valueTokenIndex = i;
                }
            }

            // ensure "name" was found and that it is not the final token
            if (!nameTokenIndex || *nameTokenIndex + 1 == tokens.size()) {
                std::cout << setOptionFailOutput << std::endl;
                continue;
            }

            SetOptionCommand setOptionCommand;

            // if "value" was found
            if (valueTokenIndex) {
                // ensure "value" comes after "name", ensure there is at least one token between "name" and "value" and ensure "value" is not the last token
                if (*valueTokenIndex - *nameTokenIndex < 2 || *valueTokenIndex + 1 == tokens.size()) {
                    std::cout << setOptionFailOutput << std::endl;
                    continue;
                }
                setOptionCommand.name = getCombinedTokens(tokens, *nameTokenIndex, *valueTokenIndex);
                setOptionCommand.value = getCombinedTokens(tokens, *valueTokenIndex, tokens.size());
            }
            else
                setOptionCommand.name = getCombinedTokens(tokens, *nameTokenIndex, tokens.size());

            if (!uciSession.setOption(setOptionCommand))
                std::cout << setOptionFailOutput << std::endl;
        }

        else if (firstToken == "register") {
            // ignore
        }

        else if (firstToken == "ucinewgame")
            uciSession.uciNewGame();

        else if (firstToken == "position") {
            const std::string positionFailOutput = R"("position" command requires: "position [startpos | fen <fenstring>] [moves <move1> .... <movei>]")";
            if (tokens.size() < 2) {
                std::cout << positionFailOutput << std::endl;
                continue;
            }

            std::optional<size_t> fenTokenIndex;
            std::optional<size_t> startPosTokenIndex;
            std::optional<size_t> movesTokenIndex;
            for (size_t i = 1; i < tokens.size(); ++i) {
                if (getLowercase(tokens[i]) == "fen")
                    fenTokenIndex = i;
                else if (getLowercase(tokens[i]) == "startpos")
                    startPosTokenIndex = i;
                else if (getLowercase(tokens[i]) == "moves")
                    movesTokenIndex = i;
            }

            // ensure either "startpos" or "fen" is found but not both and not neither (aka XOR startpos and fen)
            if (startPosTokenIndex.has_value() == fenTokenIndex.has_value()) {
                std::cout << positionFailOutput << std::endl;
                continue;
            }

            // positionCommand.fen contains the fen starting position by default
            PositionCommand positionCommand;
            if (fenTokenIndex) {
                // ensure there are enough tokens after the "fen" keyword to parse the fen string
                if (tokens.size() < *fenTokenIndex + 7) {
                    std::cout << positionFailOutput << std::endl;
                    continue;
                }

                // extract the fen string
                auto fen = tokens[*fenTokenIndex + 1];
                for (size_t i = *fenTokenIndex + 2; i < *fenTokenIndex + 7; ++i)
                    fen += " " + tokens[i];
                positionCommand.fen = fen;
            }

            if (movesTokenIndex) {
                // ensure "moves" token is after "startpos" token if "startpos" was used
                if (startPosTokenIndex) {
                    if (*movesTokenIndex < *startPosTokenIndex) {
                        std::cout << positionFailOutput << std::endl;
                        continue;
                    }
                }
                // ensure "moves" token is after "fen" token and there are at least 6 tokens between them if "fen" was used
                else {
                    if (*movesTokenIndex - *fenTokenIndex < 7) {
                        std::cout << positionFailOutput << std::endl;
                        continue;
                    }
                }
                // ensure "moves" is not the last token and therefore there is at least one possible move
                if (*movesTokenIndex + 1 == tokens.size()) {
                    std::cout << positionFailOutput << std::endl;
                    continue;
                }
                // all tokens after the "moves" keyword are treated as potential moves
                for (size_t i = *movesTokenIndex + 1; i < tokens.size(); ++i) {
                    // validate that the token complies with the uci notation standard
                    // if an invalid move is found, only moves before the invalid move will be processed
                    if (!validateUCIMove(tokens[i]))
                        break;
                    // convert the uci move to a move compatible with the programs internal architecture and push it to the vector
                    positionCommand.moves.push_back(convertUCIMoveToGameStateMove(tokens[i]));
                }
                // ensure at least 1 valid move followed the "moves" keyword
                if (positionCommand.moves.empty()) {
                    std::cout << positionFailOutput << std::endl;
                    continue;
                }
            }

            if (!uciSession.position(positionCommand))
                std::cout << positionFailOutput << std::endl;
        }

        else if (firstToken == "go") {
            GoCommand goCommand;
            if (tokens.size() < 2) {
                uciSession.go(goCommand);
                continue;
            }

            // map for looking up the relevant std::optional<int> to modify based on the go subcommand found
            static const std::unordered_map<std::string, std::optional<int> GoCommand::*> goCommandIntFields = {
                {"perft", &GoCommand::perft},
                {"wtime", &GoCommand::wtime},
                {"btime", &GoCommand::btime},
                {"winc", &GoCommand::winc},
                {"binc", &GoCommand::binc},
                {"movestogo", &GoCommand::movestogo},
                {"depth", &GoCommand::depth},
                {"nodes", &GoCommand::nodes},
                {"mate", &GoCommand::mate},
                {"movetime", &GoCommand::movetime}};

            // helper lambda for checking whether a token is a valid go subcommand
            static const auto isSubcommand = [](const std::string& token) {
                return token == "searchmoves" || token == "ponder"  || token == "infinite" || goCommandIntFields.contains(token);
            };

            bool failedValidation = false;
            // helper lambda that cuts down on repeatedly writing the same thing
            const auto failValidation = [&] {
                std::cout << R"("go" command requires: "go [searchmoves <move1> ... <movei>] [ponder] [perft <x>] [wtime <x>] [btime <x>] [winc <x>] [binc <x>] [movestogo <x>] [depth <x>] [nodes <x>] [mate <x>] [movetime <x>] [infinite]")" << std::endl;
                failedValidation = true;
            };

            for (size_t i = 1; i < tokens.size(); ++i) {
                if (getLowercase(tokens[i]) == "ponder")
                    goCommand.ponder = true;

                else if (getLowercase(tokens[i]) == "infinite")
                    goCommand.infinite = true;

                else if (getLowercase(tokens[i]) == "searchmoves") {
                    // ensure "searchmoves" is not the last token and is not immediately followed by another subcommand
                    if (i + 1 == tokens.size() || isSubcommand(getLowercase(tokens[i + 1]))) {
                        failValidation();
                        break;
                    }

                    // clear searchMoves in case searchMoves is a duplicated token
                    goCommand.searchMoves.clear();
                    // all tokens after the "searchmoves" subcommand are treated as potential moves until the next subcommand is found
                    for (++i; i < tokens.size(); ++i) {
                        // moves will be processed until a subcommand or an invalid move is found
                        if (isSubcommand(getLowercase(tokens[i])) || !validateUCIMove(tokens[i]))
                            break;
                        goCommand.searchMoves.push_back(convertUCIMoveToGameStateMove(tokens[i]));
                    }
                    --i;

                    // ensure at least 1 valid move followed the "searchmoves" subcommand
                    if (goCommand.searchMoves.empty()) {
                        failValidation();
                        break;
                    }
                }

                // if the current token can be found in the map of goCommandIntFields
                else if (auto goIterator = goCommandIntFields.find(getLowercase(tokens[i])); goIterator != goCommandIntFields.end()) {
                    // ensure it is not at the end of the list
                    if (i + 1 == tokens.size()) {
                        failValidation();
                        break;
                    }
                    int value;
                    // ensure the token directly after this one is a valid integer
                    // increment i as that token (the integer) does not need to be rexamined by the outside loop
                    if (!stringToInt(tokens[++i], value)) {
                        failValidation();
                        break;
                    }
                    // use the iterators place in the map to set the corresponding value of goCommand
                    goCommand.*goIterator->second = value;
                }
                // non-subcommand tokens will be ignored
            }
            if (!failedValidation)
                uciSession.go(goCommand);
        }

        else if (firstToken == "stop") {
            uciSession.stop();
        }

        else if (firstToken == "ponderhit") {
            // ignore for now, might implement later
        }

        else if (firstToken == "quit")
            return 0;
    }
}
