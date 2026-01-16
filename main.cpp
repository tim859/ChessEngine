#include "boardview.h"
#include "game.h"
#include "engine.h"
#include "audio.h"
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Text.hpp>
#include <thread>
#include <mutex>
#include <iostream>

sf::Vector2 mousePosition = {0, 0};
Game game;
BoardView boardView;
Engine engine;
Audio audio;
sf::RenderWindow window(sf::VideoMode({1200, 1200}), "Chess");
const int squareSize = 150;
auto engineTurn = false;
auto engineThinking = false;
std::jthread engineThread;
std::mutex engineMoveMutex;
std::optional<Move> pendingEngineMove;

// -------------------- ui --------------------

sf::RectangleShape gameOverOverlay(sf::Vector2f(window.getSize().x, window.getSize().y));
sf::RectangleShape pawnPromotionOverlay(sf::Vector2f(squareSize, squareSize * 4));

const sf::Font calibriFont("assets/calibri.ttf");
sf::Text gameOverText(calibriFont);

void processUserInput(const std::optional<sf::Event> &event) {
    // "close requested" event: we close the window
    if (event->is<sf::Event::Closed>())
        window.close();

    // prevent window resizing to ensure pixel maths is not broken
    if (event->is<sf::Event::Resized>())
        window.setSize({squareSize * 8, squareSize * 8});

    // // game is over so no further processing needed
    if (game.getCurrentGameOverType() != GameTypes::GameOverType::CONTINUE || engineTurn)
        return;

    // update the stored mouse position whenever the left mouse button is pressed or released or the mouse is moved
    if (const auto* mouseButtonPressed = event->getIf<sf::Event::MouseButtonPressed>())
    {
        if (mouseButtonPressed->button == sf::Mouse::Button::Left)
        {
            mousePosition = {mouseButtonPressed->position.x, mouseButtonPressed->position.y};
            if (game.getCurrentPawnPendingPromotionSquare()) {
                const auto pawnPendingPromotionSquare = game.getCurrentPawnPendingPromotionSquare().value();
                const float direction = game.getCurrentPawnPendingPromotionColour() == Piece::Colour::WHITE ? 1 : -1;
                // get the selected promotion piece and send it to game
                if (boardView.getSquare(mousePosition.x, mousePosition.y) == pawnPendingPromotionSquare)
                    game.promotePawn(game.getCurrentGameState(), Piece::Type::QUEEN);
                if (boardView.getSquare(mousePosition.x, mousePosition.y) == sf::Vector2<int>(pawnPendingPromotionSquare.x, pawnPendingPromotionSquare.y + direction))
                    game.promotePawn(game.getCurrentGameState(), Piece::Type::ROOK);
                if (boardView.getSquare(mousePosition.x, mousePosition.y) == sf::Vector2<int>(pawnPendingPromotionSquare.x, pawnPendingPromotionSquare.y + (direction * 2)))
                    game.promotePawn(game.getCurrentGameState(), Piece::Type::KNIGHT);
                if (boardView.getSquare(mousePosition.x, mousePosition.y) == sf::Vector2<int>(pawnPendingPromotionSquare.x, pawnPendingPromotionSquare.y + (direction * 3)))
                    game.promotePawn(game.getCurrentGameState(), Piece::Type::BISHOP);
                return;
            }
            if (const sf::Vector2<int> startSquare = boardView.getSquare(mouseButtonPressed->position.x, mouseButtonPressed->position.y); game.pickupPieceFromBoard(startSquare))
                boardView.pickupPieceFromBoard(startSquare, game.generateLegalMovesForSquare(game.getCurrentGameState(), startSquare));
        }
    }

    if (const auto* mouseButtonMoved = event->getIf<sf::Event::MouseMoved>())
        mousePosition = {mouseButtonMoved->position.x, mouseButtonMoved->position.y};

    if (const auto* mouseButtonReleased = event->getIf<sf::Event::MouseButtonReleased>()) {
        if (mouseButtonReleased->button == sf::Mouse::Button::Left)
        {
            mousePosition = {mouseButtonReleased->position.x, mouseButtonReleased->position.y};
            const auto moveType = game.placePieceOnBoard(boardView.getSquare(mouseButtonReleased->position.x, mouseButtonReleased->position.y));
            boardView.placePieceOnBoard(moveType != GameTypes::MoveType::NONE, boardView.getSquare(mouseButtonReleased->position.x, mouseButtonReleased->position.y));

            if (moveType != GameTypes::MoveType::NONE)
                engineTurn = true;

            // play corresponding audio file of the move
            audio.playSound(moveType);
        }
    }
}

void drawWindow() {
    window.clear();

    // -------------------------- always drawn -----------------------

    boardView.drawBoard(window);
    boardView.drawPieces(window, game.getCurrentBoardPosition(), game.getSelectedPieceStartSquare());

    // ------------------------- conditionally drawn

    if (game.getSelectedPiece())
        boardView.drawSelectedPiece(window, game.getSelectedPiece().value(), mousePosition.x, mousePosition.y);
    if (game.getCurrentPawnPendingPromotionSquare() && game.getCurrentPawnPendingPromotionColour()) {
        int pawnPromotionOverlayY;
        if (game.getCurrentPawnPendingPromotionColour().value() == Piece::Colour::WHITE) {
            pawnPromotionOverlayY = 0;
            pawnPromotionOverlay.setFillColor(sf::Color(0, 0, 0, 200));
        }
        else {
            pawnPromotionOverlayY = squareSize * 4;
            pawnPromotionOverlay.setFillColor(sf::Color(255, 255, 255, 140));
        }
        // account for the side of the board that the overlay will be displayed on
        pawnPromotionOverlay.setPosition(static_cast<sf::Vector2f>(sf::Vector2(game.getCurrentPawnPendingPromotionSquare().value().x * squareSize, pawnPromotionOverlayY)));
        window.draw(pawnPromotionOverlay);

        boardView.drawPawnPromotionPieces(window, game.getCurrentPawnPendingPromotionColour().value(), static_cast<sf::Vector2f>(game.getCurrentPawnPendingPromotionSquare().value()));
    }
    if (game.getCurrentGameOverType() != GameTypes::GameOverType::CONTINUE) {
        switch (game.getCurrentGameOverType()) {
            case GameTypes::GameOverType::STALEMATE:
                gameOverText.setString("STALEMATE");
                break;
            case GameTypes::GameOverType::TFRDRAW:
                gameOverText.setString("DRAW BY THREEFOLD REPETITION");
                break;
            case GameTypes::GameOverType::FIFTYMOVEDRAW:
                gameOverText.setString("DRAW BY 50 MOVE RULE");
                break;
            case GameTypes::GameOverType::WHITEWINBYCHECKMATE:
                gameOverText.setString("WHITE WINS BY CHECKMATE");
                break;
            case GameTypes::GameOverType::BLACKWINBYCHECKMATE:
                gameOverText.setString("BLACK WINS BY CHECKMATE");
                break;
            case GameTypes::GameOverType::WHITEWINBYRESIGN:
                gameOverText.setString("WHITE WINS BY RESIGNATION");
                break;
            case GameTypes::GameOverType::BLACKWINBYRESIGN:
                gameOverText.setString("BLACK WINS BY RESIGNATION");
                break;
        }
        window.draw(gameOverOverlay);
        window.draw(gameOverText);
    }
    window.display();
}

void processEngineMove() {
    // game is over so no processing needed
    if (game.getCurrentGameOverType() != GameTypes::GameOverType::CONTINUE)
        return;

    if (!engineThinking) {
        auto legalMoves = game.generateAllLegalMoves(game.getCurrentGameState());
        if (legalMoves.empty()) {
            // the game is either drawn or lost for the engine
            std::cout << "no legal moves left for engine" << std::endl;
            return;
        }

        engineThinking = true;
        // spawn a new worker thread that will allow the engine to spend time thinking about the move without freezing the main thread and therefore the program
        engineThread = std::jthread([legalMoves = std::move(legalMoves)] {
            Move move = engine.generateEngineMove(legalMoves);
            std::scoped_lock lock(engineMoveMutex);
            pendingEngineMove = move;
        });
    }
    else {
        // safely (threadwise) get the value in pendingEngineMove
        std::optional<Move> move;
        {
            std::scoped_lock lock(engineMoveMutex);
            move = pendingEngineMove;
            if (move) {
                pendingEngineMove.reset();
            }
        }

        // if there was a valid move in pendingEngineMove, the worker thread is killed and the move is applied on the board
        if (move) {
            if (engineThread.joinable()) {
                engineThread.join();
            }
            engineThinking = false;

            game.pickupPieceFromBoard(move->startSquare);
            boardView.pickupPieceFromBoard(move->startSquare, game.generateLegalMovesForSquare(game.getCurrentGameState(), move->startSquare));

            const auto moveType = game.placePieceOnBoard(move->endSquare);
            boardView.placePieceOnBoard(moveType != GameTypes::MoveType::NONE, move->endSquare);

            audio.playSound(moveType);
            engineTurn = false;
        }
    }
}

int main() {
    gameOverOverlay.setFillColor(sf::Color(0, 0, 0, 150));
    gameOverText.setFillColor(sf::Color::White);
    gameOverText.setCharacterSize(120);
    gameOverText.setPosition({squareSize * 2, squareSize * 3});

    // standard chess starting position fen string
    game.populateCurrentGameStateWithFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w QKqk - 0 0");

    // run the program as long as the window is open
    while (window.isOpen())
    {
        if (engineTurn)
            processEngineMove();

        // check all the windows events that were triggered since the last iteration of the loop
        while (const std::optional event = window.pollEvent())
        {
            processUserInput(event);
        }
        drawWindow();
    }
}