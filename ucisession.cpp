#include "ucisession.h"

#include <algorithm>
#include <charconv>

UCISession::UCISession() {
    completionThread = std::jthread([this](const std::stop_token& stopToken) {
        monitorSearchCompletion(stopToken);
    });
}

UCISession::~UCISession() {
    requestEngineStop();
    completionThread.request_stop();
    searchStateChanged.notify_all();

    if (completionThread.joinable())
        completionThread.join();

    std::jthread remainingEngineThread;
    {
        std::scoped_lock lock(searchMutex);
        remainingEngineThread = std::move(engineThread);
        pendingEngineMove.reset();
    }

    if (remainingEngineThread.joinable())
        remainingEngineThread.join();
}

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
    requestEngineStop();
    waitForSearchToBecomeIdle();
    engine.reset();
    game.reset();
}

bool UCISession::position(const PositionCommand& positionCommand) {
    requestEngineStop();
    waitForSearchToBecomeIdle();

    // check for position and move validity on a fresh game before changing the actual game
    Game updatedGame;
    if (!updatedGame.populateGameStateFromFEN(updatedGame.getCurrentGameState(), updatedGame.getCurrentGameStateHistory(), positionCommand.fen))
        return false;
    for (const auto& move : positionCommand.moves) {
        if (!updatedGame.checkIsMoveLegal(updatedGame.getCurrentGameState(), move))
            return false;
        updatedGame.movePiece(updatedGame.getCurrentGameState(), move, updatedGame.getCurrentGameStateHistory());
    }

    // copy the updated game to game
    game = updatedGame;
    return true;
}

void UCISession::go(const GoCommand& goCommand) {
    requestEngineStop();
    waitForSearchToBecomeIdle();

    if (goCommand.perft) {
        // autoperft sends "go perft <depth>" and expects divide-style output followed by a final node count line.
        std::uint64_t totalNodes = 0;
        // ask the engine for the subtree size below each root move so we can stream standard divide output.
        for (const auto& moveNodePair : engine.generatePerftDivide(game, *goCommand.perft)) {
            const Move& move = moveNodePair.first;
            const std::uint64_t nodes = moveNodePair.second;
            std::cout << convertGameStateMoveToUCIMove(move) << ": " << nodes << "\n";
            // autoperft also parses the final total, so accumulate the root counts as we print them.
            totalNodes += nodes;
        }
        std::cout << "Nodes searched: " << totalNodes << std::endl;
        return;
    }

    // search uses a snapshot so later UCI commands cannot mutate the position out from under the worker thread.
    const Game gameSnapshot = game;
    const EngineSearchSettings engineSearchSettings = goCommand;

    {
        std::scoped_lock lock(searchMutex);
        pendingEngineMove.reset();
        engineThread = std::jthread([this, gameSnapshot, engineSearchSettings](const std::stop_token& stopToken) {
            // the engine thread only computes the move and stores it; ucisession is responsible for printing it.
            Move move = engine.generateEngineMove(gameSnapshot, engineSearchSettings, stopToken);
            {
                std::scoped_lock lock(searchMutex);
                pendingEngineMove = move;
            }
            searchStateChanged.notify_all();
        });
    }
}

void UCISession::stop() {
    requestEngineStop();
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

void UCISession::monitorSearchCompletion(const std::stop_token& stopToken) {
    std::unique_lock lock(searchMutex);

    while (!stopToken.stop_requested()) {
        searchStateChanged.wait(lock, stopToken, [this] {
            return pendingEngineMove.has_value();
        });

        if (stopToken.stop_requested())
            break;

        const Move completedMove = *pendingEngineMove;
        pendingEngineMove.reset();

        std::jthread completedEngineThread = std::move(engineThread);
        lock.unlock();

        if (completedEngineThread.joinable())
            completedEngineThread.join();

        std::cout << "bestmove " << convertGameStateMoveToUCIMove(completedMove) << std::endl;
        searchStateChanged.notify_all();
        lock.lock();
    }
}

void UCISession::requestEngineStop() {
    {
        std::scoped_lock lock(searchMutex);
        if (engineThread.joinable())
            engineThread.request_stop();
    }
    searchStateChanged.notify_all();
}

void UCISession::waitForSearchToBecomeIdle() {
    std::unique_lock lock(searchMutex);
    searchStateChanged.wait(lock, [this] {
        return !engineThread.joinable() && !pendingEngineMove.has_value();
    });
}
