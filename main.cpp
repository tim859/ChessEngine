#include "boardview.h"
#include "game.h"
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Audio.hpp>
#include <iostream>

int main()
{
    sf::Vector2 mousePosition = {0, 0};
    Game game;
    BoardView boardView;
    sf::RenderWindow window(sf::VideoMode({1200, 1200}), "Chess");
    const int squareSize = 150;
    auto stalemate = false;
    auto checkmate = false;
    auto tFRDraw = false;
    auto fiftyMoveDraw = false;

    // -------------------- audio --------------------
    sf::SoundBuffer moveSelfBuffer("assets/move-self.mp3");
    sf::Sound moveSelfSound(moveSelfBuffer);
    sf::SoundBuffer captureBuffer("assets/capture.mp3");
    sf::Sound captureSound(captureBuffer);
    sf::SoundBuffer castleBuffer("assets/castle.mp3");
    sf::Sound castleSound(castleBuffer);

    // -------------------- ui --------------------

    sf::RectangleShape gameOverOverlay(sf::Vector2f(window.getSize().x, window.getSize().y));
    gameOverOverlay.setFillColor(sf::Color(0, 0, 0, 150));
    sf::RectangleShape pawnPromotionOverlay(sf::Vector2f(squareSize, squareSize * 4));

    const sf::Font calibriFont("assets/calibri.ttf");

    sf::Text stalemateText(calibriFont);
    stalemateText.setString("STALEMATE");
    stalemateText.setFillColor(sf::Color::White);
    stalemateText.setCharacterSize(120);
    stalemateText.setPosition({squareSize * 2, squareSize * 3});

    sf::Text checkmateText(calibriFont);
    checkmateText.setString("CHECKMATE");
    checkmateText.setFillColor(sf::Color::White);
    checkmateText.setCharacterSize(120);
    checkmateText.setPosition({squareSize * 2, squareSize * 3});

    sf::Text tFRDrawText(calibriFont);
    tFRDrawText.setString("DRAW BY THREEFOLD REPETITION");
    tFRDrawText.setFillColor(sf::Color::White);
    tFRDrawText.setCharacterSize(120);
    tFRDrawText.setPosition({squareSize * 2, squareSize * 3});

    sf::Text fiftyMoveDrawText(calibriFont);
    fiftyMoveDrawText.setString("DRAW BY 50 MOVE RULE");
    fiftyMoveDrawText.setFillColor(sf::Color::White);
    fiftyMoveDrawText.setCharacterSize(120);
    fiftyMoveDrawText.setPosition({squareSize * 2, squareSize * 3});

    // standard chess starting position fen string
    game.populateCurrentGameStateWithFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w QKqk - 0 0");

    // fen string for testing pawn promotion
    //game.populateCurrentGameStateWithFen("/PPPPPPPP/8/8/8/8/pppppppp/");

    // run the program as long as the window is open
    while (window.isOpen())
    {
        // check all the windows events that were triggered since the last iteration of the loop
        while (const std::optional event = window.pollEvent())
        {
            // "close requested" event: we close the window
            if (event->is<sf::Event::Closed>())
                window.close();

            // prevent window resizing to ensure pixel maths is not broken
            if (event->is<sf::Event::Resized>())
                window.setSize({squareSize * 8, squareSize * 8});

            if (stalemate || checkmate)
                break;

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
                        if (boardView.getSquare(mousePosition.x, mousePosition.y) == static_cast<sf::Vector2f>(pawnPendingPromotionSquare))
                            game.promotePawn(game.getCurrentGameState(), Piece::Type::QUEEN);
                        if (boardView.getSquare(mousePosition.x, mousePosition.y) == sf::Vector2f(pawnPendingPromotionSquare.x, pawnPendingPromotionSquare.y + direction))
                            game.promotePawn(game.getCurrentGameState(), Piece::Type::ROOK);
                        if (boardView.getSquare(mousePosition.x, mousePosition.y) == sf::Vector2f(pawnPendingPromotionSquare.x, pawnPendingPromotionSquare.y + (direction * 2)))
                            game.promotePawn(game.getCurrentGameState(), Piece::Type::KNIGHT);
                        if (boardView.getSquare(mousePosition.x, mousePosition.y) == sf::Vector2f(pawnPendingPromotionSquare.x, pawnPendingPromotionSquare.y + (direction * 3)))
                            game.promotePawn(game.getCurrentGameState(), Piece::Type::BISHOP);
                        break;
                    }
                    game.pickupPieceFromBoard(static_cast<sf::Vector2<int>>(boardView.getSquare(mouseButtonPressed->position.x, mouseButtonPressed->position.y)));
                }
            }

            if (const auto* mouseButtonMoved = event->getIf<sf::Event::MouseMoved>())
                mousePosition = {mouseButtonMoved->position.x, mouseButtonMoved->position.y};

            if (const auto* mouseButtonReleased = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mouseButtonReleased->button == sf::Mouse::Button::Left)
                {
                    mousePosition = {mouseButtonReleased->position.x, mouseButtonReleased->position.y};
                    const auto moveType = game.placePieceOnBoard(static_cast<sf::Vector2<int>>(boardView.getSquare(mouseButtonReleased->position.x, mouseButtonReleased->position.y)));
                    // play corresponding audio file of the move
                    switch (moveType) {
                        case Game::MoveType::MOVESELF:
                            moveSelfSound.play();
                            break;
                        case Game::MoveType::CAPTURE:
                            captureSound.play();
                            break;
                        case Game::MoveType::CASTLE:
                            castleSound.play();
                            break;
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
        window.clear();

        // -------------------------- always drawn -----------------------

        boardView.drawBoard(window, game.getHighlightedSquares());
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
            window.draw(stalemateText);
        }
        if (checkmate) {
            window.draw(gameOverOverlay);
            window.draw(checkmateText);
        }
        if (tFRDraw) {
            window.draw(gameOverOverlay);
            window.draw(tFRDrawText);
        }
        if (fiftyMoveDraw) {
            window.draw(gameOverOverlay);
            window.draw(fiftyMoveDrawText);
        }
        window.display();
    }
}