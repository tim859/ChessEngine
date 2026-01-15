#include "boardview.h"
#include "game.h"
#include "engine.h"
#include "audio.h"
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Audio.hpp>
#include <thread>
#include <iostream>

sf::Vector2 mousePosition = {0, 0};
Game game;
BoardView boardView;
Engine engine;
Audio audio;
sf::RenderWindow window(sf::VideoMode({1200, 1200}), "Chess");
const int squareSize = 150;
auto stalemate = false;
auto checkmate = false;
auto tFRDraw = false;
auto fiftyMoveDraw = false;
auto engineTurn = false;
auto engineThinking = false;

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

    // no user input processed if game is already over
    if (stalemate || checkmate || engineTurn)
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
            boardView.placePieceOnBoard(moveType != Game::MoveType::NONE, boardView.getSquare(mouseButtonReleased->position.x, mouseButtonReleased->position.y));
            if (moveType != Game::MoveType::NONE)
                engineTurn = true;

            // play corresponding audio file of the move
            audio.playSound(moveType);

            // check for game over conditions
            switch (moveType) {
                case Game::MoveType::STALEMATE:
                    stalemate = true;
                    break;
                case Game::MoveType::CHECKMATE:
                    checkmate = true;
                    break;
                case Game::MoveType::TFRDRAW:
                    tFRDraw = true;
                    break;
                case Game::MoveType::FIFTYMOVEDRAW:
                    fiftyMoveDraw = true;
                    break;
            }
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
    if (stalemate) {
        window.draw(gameOverOverlay);
        gameOverText.setString("STALEMATE");
        window.draw(gameOverText);
    }
    if (checkmate) {
        window.draw(gameOverOverlay);
        gameOverText.setString("CHECKMATE");
        window.draw(gameOverText);
    }
    if (tFRDraw) {
        window.draw(gameOverOverlay);
        gameOverText.setString("DRAW BY THREEFOLD REPETITION");
        window.draw(gameOverText);
    }
    if (fiftyMoveDraw) {
        window.draw(gameOverOverlay);
        gameOverText.setString("DRAW BY 50 MOVE RULE");
        window.draw(gameOverText);
    }
    window.display();
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
        if (engineTurn) {
            if (!engineThinking) {
                const auto legalMoves = game.generateAllLegalMoves(game.getCurrentGameState());
                std::thread engineThread([legalMoves]() mutable {
                    engine.generateEngineMove(legalMoves);
                });
            }
            break;
        }

        // check all the windows events that were triggered since the last iteration of the loop
        while (const std::optional event = window.pollEvent())
        {
            processUserInput(event);
        }
        drawWindow();
    }
}