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
bool waitingForPawnPromotionChoice = false;
Piece* pawnPromotionPiece;
sf::Vector2i pawnPromotionSquare;

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

    // game is over or it is not the players turn
    if (game.getCurrentGameOverType() != GameTypes::GameOverType::CONTINUE || engineTurn)
        return;

    if (const auto* mouseButtonPressed = event->getIf<sf::Event::MouseButtonPressed>())
    {
        if (mouseButtonPressed->button == sf::Mouse::Button::Left)
        {
            mousePosition = {mouseButtonPressed->position.x, mouseButtonPressed->position.y};
            if (const sf::Vector2i startSquare = boardView.getSquare(mousePosition.x, mousePosition.y); game.pickupPieceFromBoard(game.getCurrentGameState(), startSquare))
                boardView.pickupPieceFromBoard(startSquare, game.generateLegalMovesForSquare(game.getCurrentGameState(), startSquare));
        }
    }

    if (const auto* mouseButtonReleased = event->getIf<sf::Event::MouseButtonReleased>()) {
        if (mouseButtonReleased->button == sf::Mouse::Button::Left)
        {
            mousePosition = {mouseButtonReleased->position.x, mouseButtonReleased->position.y};

            // use the location of the players mouse press to determine which type of piece they want to promote their pawn to
            // the logic is mapped to a column of four squares in the same way as the drawing is in drawWindow()
            if (waitingForPawnPromotionChoice) {
                const float direction = game.getCurrentGameState().moveColour == Piece::Colour::WHITE ? 1 : -1;
                if (boardView.getSquare(mousePosition.x, mousePosition.y) == pawnPromotionSquare)
                    pawnPromotionPiece = new Piece(Piece::Type::QUEEN, game.getCurrentGameState().moveColour);
                if (boardView.getSquare(mousePosition.x, mousePosition.y) == sf::Vector2i(pawnPromotionSquare.x, pawnPromotionSquare.y + direction))
                    pawnPromotionPiece = new Piece(Piece::Type::ROOK, game.getCurrentGameState().moveColour);
                if (boardView.getSquare(mousePosition.x, mousePosition.y) == sf::Vector2i(pawnPromotionSquare.x, pawnPromotionSquare.y + (direction * 2)))
                    pawnPromotionPiece = new Piece(Piece::Type::KNIGHT, game.getCurrentGameState().moveColour);
                if (boardView.getSquare(mousePosition.x, mousePosition.y) == sf::Vector2i(pawnPromotionSquare.x, pawnPromotionSquare.y + (direction * 3)))
                    pawnPromotionPiece = new Piece(Piece::Type::BISHOP, game.getCurrentGameState().moveColour);

                // make sure pawnPromotionPiece is pointing to a valid piece, otherwise the player could click anywhere on the board, and it would still set waitingForPawnPromotionChoice to false
                if (pawnPromotionPiece)
                    waitingForPawnPromotionChoice = false;
            }

            // endSquare uses the mouse position to determine which square the piece is moving to, this is accurate unless the player is promoting a pawn to a rook, a knight or a bishop, in which case the mouse won't be on the end square
            const auto endSquare = boardView.getSquare(mousePosition.x, mousePosition.y);

            // check ahead of making the move if the move will promote a pawn, need to get the player choice of pawn promotion before the move is made
            if (!waitingForPawnPromotionChoice && !pawnPromotionPiece) {
                if (game.getCurrentGameState().selectedPieceStartSquare) {
                    if (game.checkForPawnPromotionOnNextMove(game.getCurrentGameState(), Move(game.getCurrentGameState().selectedPieceStartSquare.value(), endSquare))) {
                        pawnPromotionSquare = endSquare;
                        waitingForPawnPromotionChoice = true;
                    }
                }
            }

            // don't move the piece if the player still needs to decide which piece to promote the pawn to
            if (waitingForPawnPromotionChoice)
                return;

            GameTypes::MoveType moveType;
            if (pawnPromotionPiece) {
                // promoting a pawn requires passing in the pawnPromotionSquare as the endSquare because the endSquare is based on mouse position and that won't be accurate if the player selected a promotion piece other than the queen
                moveType = game.placePieceOnBoard(game.getCurrentGameState(), pawnPromotionSquare, game.getCurrentGameStateHistory(), pawnPromotionPiece);
                boardView.placePieceOnBoard(moveType != GameTypes::MoveType::NONE, pawnPromotionSquare);
            }
            else {
                // standard move (not promoting a pawn)
                moveType = game.placePieceOnBoard(game.getCurrentGameState(), endSquare, game.getCurrentGameStateHistory(), nullptr);
                boardView.placePieceOnBoard(moveType != GameTypes::MoveType::NONE, endSquare);
            }
            // free memory to avoid memory leak
            delete pawnPromotionPiece;
            pawnPromotionPiece = nullptr;
            audio.playSoundOnMove(moveType);

            if (moveType != GameTypes::MoveType::NONE)
                engineTurn = true;
        }
    }

    if (const auto* mouseButtonMoved = event->getIf<sf::Event::MouseMoved>())
        mousePosition = {mouseButtonMoved->position.x, mouseButtonMoved->position.y};
}

void processEngineMove() {
    // game is over so no processing needed
    if (game.getCurrentGameOverType() != GameTypes::GameOverType::CONTINUE)
        return;

    if (!engineThinking) {
        if (const auto legalMoves = game.generateAllLegalMoves(game.getCurrentGameState()); legalMoves.empty()) {
            // the game is either drawn or lost for the engine
            std::cout << "no legal moves left for engine" << std::endl;
            return;
        }

        engineThinking = true;
        // spawn a new worker thread that will allow the engine to spend time thinking about the move without freezing the main thread and therefore the program
        engineThread = std::jthread([] {
            Move move = engine.generateEngineMove(game);
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

            game.pickupPieceFromBoard(game.getCurrentGameState(), move->startSquare);
            boardView.pickupPieceFromBoard(move->startSquare, game.generateLegalMovesForSquare(game.getCurrentGameState(), move->startSquare));

            // for now, engine will always promote a pawn to a queen
            const auto piece = Piece(Piece::Type::QUEEN, game.getCurrentGameState().moveColour);
            const auto moveType = game.placePieceOnBoard(game.getCurrentGameState(), move->endSquare, game.getCurrentGameStateHistory(), &piece);
            boardView.placePieceOnBoard(moveType != GameTypes::MoveType::NONE, move->endSquare);

            audio.playSoundOnMove(moveType);
            engineTurn = false;
        }
    }
}

void drawWindow() {
    window.clear();

    // -------------------------- always drawn -----------------------

    boardView.drawBoard(window);
    boardView.drawPieces(window, game.getCurrentBoardPosition(), game.getCurrentSelectedPieceStartSquare());

    // ------------------------- conditionally drawn

    if (game.getCurrentSelectedPiece() && !waitingForPawnPromotionChoice)
        boardView.drawSelectedPiece(window, game.getCurrentSelectedPiece().value(), mousePosition.x, mousePosition.y);
    if (waitingForPawnPromotionChoice) {
        int pawnPromotionOverlayY;
        // account for the side of the board that the pawn promotion menu should be displayed on
        if (game.getCurrentGameState().moveColour == Piece::Colour::WHITE) {
            pawnPromotionOverlayY = 0;
            pawnPromotionOverlay.setFillColor(sf::Color(0, 0, 0, 200));
        }
        else {
            pawnPromotionOverlayY = squareSize * 4;
            pawnPromotionOverlay.setFillColor(sf::Color(255, 255, 255, 140));
        }
        pawnPromotionOverlay.setPosition(static_cast<sf::Vector2f>(sf::Vector2(pawnPromotionSquare.x * squareSize, pawnPromotionOverlayY)));
        window.draw(pawnPromotionOverlay);

        boardView.drawPawnPromotionPieces(window, game.getCurrentGameState().moveColour, static_cast<sf::Vector2f>(pawnPromotionSquare));
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

int main() {
    gameOverOverlay.setFillColor(sf::Color(0, 0, 0, 150));
    gameOverText.setFillColor(sf::Color::White);
    gameOverText.setCharacterSize(120);
    gameOverText.setPosition({squareSize * 2, squareSize * 3});

    // standard chess starting position fen string
    game.populateCurrentGameStateWithFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w QKqk - 0 0");

    // testing pawn promotion fen string
    // game.populateCurrentGameStateWithFen("8/PPPPPPPP/8/8/8/8/pppppppp/8 w - - 0 0");

    audio.playSoundOnStart();

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