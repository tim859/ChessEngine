#include "ucisession.h"

#include <algorithm>
#include <charconv>

bool UCISession::validateCmdDebug(const std::vector<std::string>& tokens) const {
    if (tokens.size() < 2)
        return false;

    for (size_t i = 1; i < tokens.size(); ++i) {
        if (tokens[i] == "on" || tokens[i] == "false")
            return true;
    }
    return false;
}

// "setoption name <id> [value <x>]"
bool UCISession::validateCmdSetOption(const std::vector<std::string> &tokens) const {
    if (tokens.size() < 3)
        return false;

    // use tokens.size() as a null value
    auto nameTokenIndex = tokens.size();
    auto valueTokenIndex = tokens.size();

    // loop serves two purposes, ensure the keyword tokens are not duplicated and
    // find the indexes of the keyword tokens for further validation while ignoring garbage tokens that don't get in the way
    for (size_t i = 1; i < tokens.size(); ++i) {
        const auto lowercaseToken = getLowercase(tokens[i]);
        if (lowercaseToken == "name") {
            // ensure "name" token is not duplicated
            if (nameTokenIndex != tokens.size())
                return false;
            nameTokenIndex = i;
        }
        else if (lowercaseToken == "value") {
            // ensure "value" token is not duplicated
            if (valueTokenIndex != tokens.size())
                return false;
            valueTokenIndex = i;
        }
    }

    // ensure there is a "name" token
    if (nameTokenIndex == tokens.size())
        return false;

    // if there is a "value" token
    if (valueTokenIndex != tokens.size()) {
        // ensure "value" comes after "name", ensure there is at least one token between "name" and "value" and ensure "value" is not the last token
        if (valueTokenIndex - nameTokenIndex < 2 || valueTokenIndex == tokens.size() - 1)
            return false;
    }
    return true;
}

// "position [fen <fenstring> | startpos ]  moves <move1> .... <movei>"
bool UCISession::validateCmdPosition(const std::vector<std::string> &tokens) const {
    if (tokens.size() < 2)
        return false;

    // TODO: could use a map to reduce repeated code

    auto startPosTokenIndex = tokens.size();
    auto fenTokenIndex = tokens.size();
    auto moveTokenIndex = tokens.size();
    for (size_t i = 1; i < tokens.size(); ++i) {
        const auto lowercaseToken = getLowercase(tokens[i]);
        if (lowercaseToken == "startpos") {
            if (startPosTokenIndex != tokens.size())
                return false;
            startPosTokenIndex = i;
        }
        else if (lowercaseToken == "fen") {
            if (fenTokenIndex != tokens.size())
                return false;
            fenTokenIndex = i;
        }
        else if (lowercaseToken == "moves") {
            if (moveTokenIndex != tokens.size())
                return false;
            moveTokenIndex = i;
        }
    }

    // ensure either "startpos" or "fen" is found but not both
    if (startPosTokenIndex == tokens.size() == (fenTokenIndex == tokens.size()))
        return false;

    if (fenTokenIndex != tokens.size()) {
        if (tokens.size() < fenTokenIndex + 7)
            return false;

        // extract the fen string
        std::string fen;
        for (size_t i = fenTokenIndex + 1; i < fenTokenIndex + 7; ++i)
            fen += " " + tokens[i];

        // validate fen string using validation in game.populateGameStateFromFEN
        if (GameState emptyGameState; !game.populateGameStateFromFEN(emptyGameState, nullptr, fen))
            return false;
    }

    if (moveTokenIndex != tokens.size()) {
        // ensure "moves" token is after "startpos" token if "startpos" was used
        if (startPosTokenIndex != tokens.size()) {
            if (moveTokenIndex < startPosTokenIndex)
                return false;
        }
        // ensure "moves" token is after "fen" token and there are at least 6 tokens between them if "fen" was used
        else {
            if (moveTokenIndex - fenTokenIndex < 7)
                return false;
        }

        // ensure "moves" is not the last token
        if (moveTokenIndex == tokens.size() - 1)
            return false;

        // validate all moves
        for (size_t i = moveTokenIndex + 1; i < tokens.size(); ++i) {
            // TODO: this needs to validate the algebraic moves in the context of the fen position, simply checking for letters and digits is not enough
            // it needs to reject illegal moves for that position as well
            if (!validateAlgebraicMove(tokens[i]))
                return false;
        }
    }
    return true;
}

// TODO: allow and remove unrecognised tokens instead of refusing to continue parsing
bool UCISession::validateCmdGo(const std::vector<std::string> &tokens) const {
    // allow "go" command by itself
    if (tokens.size() == 1)
        return true;

    // helper lambda to check if a token is a keyword
    const auto checkGoKeyword = [](const std::string& token) {
        return token == "searchmoves" || token == "wtime"
        || token == "btime" || token == "winc" || token =="binc"
        || token == "movestogo" || token == "depth" || token == "nodes"
        || token == "mate" || token == "movetime";
    };

    for (size_t i = 1; i < tokens.size(); ++i) {
        if (tokens[i] == "ponder" || tokens[i] == "infinite")
            continue;

        if (checkGoKeyword(tokens[i])) {
            if (tokens[i] == "searchmoves") {
                ++i;
                while (!checkGoKeyword(tokens[i])) {
                    if (!validateAlgebraicMove(tokens[i])) {
                        std::cout << "invalid move in \"go searchmoves\" command" << std::endl;
                        return false;
                    }
                    ++i;
                }
            }
            else {
                ++i;
                if (int value; !stringToInt(tokens[i], value)) {
                    std::cout << "invalid integer in \"go\" command" << std::endl;
                    return false;
                }
            }
        }
        else {
            std::cout << "invalid subcommand in \"go\" command" << std::endl;
            return false;
        }
    }
    return true;
}

void UCISession::cmdUCI() const {
    std::cout << "id name " << uciSettings.name << std::endl;
    std::cout << "id author " << uciSettings.author << std::endl;
    std::cout << "uciok" << std::endl;
}

void UCISession::cmdDebug(const std::vector<std::string>& tokens) {
    for (const auto& token : tokens) {
        if (token == "on") {
            uciSettings.debugMode = true;
            return;
        }
        if (token == "off") {
            uciSettings.debugMode = false;
            return;
        }
    }
}

void UCISession::cmdIsReady() const {
    std::cout << "readyok" << std::endl;
}

void UCISession::cmdSetOption(const std::vector<std::string>& tokens) const {
    auto nameTokenIndex = tokens.size();
    auto valueTokenIndex = tokens.size();
    for (size_t i = 1; i < tokens.size(); ++i) {
        if (getLowercase(tokens[i]) == "name")
            nameTokenIndex = i;
        else if (getLowercase(tokens[i]) == "value")
            valueTokenIndex = i;
    }

    // if "value" token not found
    if (valueTokenIndex == tokens.size()) {
        const auto name = getCombinedTokens(tokens, nameTokenIndex, tokens.size());
        // TODO: apply changes to options that just need a name here
        return;
    }

    // "value" token was found
    const auto name = getCombinedTokens(tokens, nameTokenIndex, valueTokenIndex);
    const auto value = getCombinedTokens(tokens, valueTokenIndex, tokens.size());
    // TODO: apply changes to options that need a name and a value here
}

void UCISession::cmdUCINewGame() {
    engine.reset();
    game.reset();
}

void UCISession::cmdPosition(const std::vector<std::string>& tokens) {
    std::vector<std::string> moves;
    if (getLowercase(tokens[1]) == "startpos") {
        if (tokens.size() >= 4) {
            for (size_t i = 3; i < tokens.size(); ++i) {
                moves.push_back(tokens[i]);
            }
        }
        game.populateGameStateFromFEN(game.getCurrentGameState(), game.getCurrentGameStateHistory(), uciSettings.startPosFen);
        if (!moves.empty())
            applyMovesToGameState(moves);
    }
    else if (getLowercase(tokens[1]) == "fen") {
        // extract the fen string
        std::string fen = tokens[2];
        for (size_t i = 3; i < 8; ++i)
            fen += " " + tokens[i];

        if (tokens.size() >= 10) {
            for (size_t i = 9; i < tokens.size(); ++i) {
                moves.push_back(tokens[i]);
            }
        }
        game.populateGameStateFromFEN(game.getCurrentGameState(), game.getCurrentGameStateHistory(), fen);
        if (!moves.empty())
            applyMovesToGameState(moves);
    }
    else
        std::cerr << "uciSession.validateCmdPosition() failed to properly validate position command" << std::endl;
}

void UCISession::cmdGo(const std::vector<std::string> &tokens) {
}

std::string UCISession::getLowercase(const std::string &string) const {
    std::string lowercaseString;
    for (const auto& character : string) {
        lowercaseString += std::tolower(static_cast<unsigned char>(character));
    }
    return lowercaseString;
}

// get all tokens between startTokenIndex (exclusive) and endTokenIndex (exclusive) as one token/string
std::string UCISession::getCombinedTokens(const std::vector<std::string> &tokens, const size_t startTokenIndex, const size_t endTokenIndex) const {
    auto token = tokens[startTokenIndex];
    auto i = startTokenIndex + 1;
    while (i < endTokenIndex) {
        token += " " + tokens[i];
        ++i;
    }
    return token;
}

bool UCISession::validateAlgebraicMove(const std::string &move) const {
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

bool UCISession::stringToInt(const std::string &string, int &value) const {
    std::from_chars_result result = std::from_chars(string.data(), string.data() + string.size(), value);
    return result.ec == std::errc{} && result.ptr == string.data() + string.size();
}

void UCISession::applyMovesToGameState(const std::vector<std::string>& moves) {
    for (const auto& algebraicMove : moves) {
        // convert the chess move to the internal Vector2 based move system
        const auto move = Move(Vector2Int(algebraicMove[0] - 'a', 7 - (algebraicMove[1] - '1')), Vector2Int(algebraicMove[2] - 'a', 7 - (algebraicMove[3] - '1')));

        const Piece* promotionPiece = nullptr;
        if (algebraicMove.length() == 5) {
            switch (algebraicMove[4]) {
                case 'q': promotionPiece = new Piece(Piece::Type::QUEEN, game.getCurrentGameState().moveColour); break;
                case 'r': promotionPiece = new Piece(Piece::Type::ROOK, game.getCurrentGameState().moveColour); break;
                case 'b': promotionPiece = new Piece(Piece::Type::BISHOP, game.getCurrentGameState().moveColour); break;
                case 'n': promotionPiece = new Piece(Piece::Type::KNIGHT, game.getCurrentGameState().moveColour); break;
            }
        }
        game.movePiece(game.getCurrentGameState(), move, game.getCurrentGameStateHistory(), promotionPiece);
        delete promotionPiece;
    }
}
