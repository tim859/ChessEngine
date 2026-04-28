#include "ucisession.h"

#include <algorithm>
#include <charconv>

void UCISession::uci() const {
    std::cout << "id name " << uciSettings.name << std::endl;
    std::cout << "id author " << uciSettings.author << std::endl;
    std::cout << "uciok" << std::endl;
}

void UCISession::debug(const bool debugCommand) {
    if (debugCommand)
        uciSettings.debugMode = true;
    else
        uciSettings.debugMode = false;
}

void UCISession::isReady() const {
    std::cout << "readyok" << std::endl;
}

bool UCISession::setOption(const SetOptionCommand& setOptionCommand) const {
    return false;
}

void UCISession::uciNewGame() {
    engine.reset();
    game.reset();
}

bool UCISession::position(const PositionCommand& positionCommand) {
    if (!game.populateGameStateFromFEN(game.getCurrentGameState(), game.getCurrentGameStateHistory(), positionCommand.fen))
        return false;

    for (const auto& move : positionCommand.moves) {
        if (!game.checkIsMoveLegal(game.getCurrentGameState(), move))
            return false;
        game.movePiece(game.getCurrentGameState(), move, game.getCurrentGameStateHistory());
    }
    return true;
}

void UCISession::go(const GoCommand& goCommand) {
    stop();
    const Game gameSnapshot = game;
    const EngineSearchSettings& engineSearchSettings = goCommand;

    engineThread = std::jthread([this, gameSnapshot, engineSearchSettings](const std::stop_token& stopToken) {
        Move move = engine.generateEngineMove(gameSnapshot, engineSearchSettings, stopToken);
        std::scoped_lock lock(engineMoveMutex);
        pendingEngineMove = move;
    });
}

void UCISession::stop() {
    // stop the engine search
    if (engineThread.joinable()) {
        engineThread.request_stop();
        engineThread.join();
        // return the best move found so far
        if (pendingEngineMove) {
            std::scoped_lock lock(engineMoveMutex);
            std::cout << "bestmove " << convertGameStateMoveToUCIMove(*pendingEngineMove) << std::endl;
            pendingEngineMove = std::nullopt;
        }
        else {
            // purely for debugging, delete later
            std::cerr << "no bestmove found" << std::endl;
        }
    }
}

std::string UCISession::convertGameStateMoveToUCIMove(const Move& move) const {
    std::string uciMove;
    uciMove += static_cast<char>('a' + move.startSquare.x);
    uciMove += static_cast<char>('1' + (7 - move.startSquare.y));
    uciMove += static_cast<char>('a' + move.endSquare.x);
    uciMove += static_cast<char>('1' + (7 - move.endSquare.y));

    if (move.promotionPieceType) {
        switch (*move.promotionPieceType) {
            case Piece::Type::QUEEN:
                uciMove += 'q';
                break;
            case Piece::Type::ROOK:
                uciMove += 'r';
                break;
            case Piece::Type::BISHOP:
                uciMove += 'b';
                break;
            case Piece::Type::KNIGHT:
                uciMove += 'n';
                break;
            default:
                break;
        }
    }
    return uciMove;
}
