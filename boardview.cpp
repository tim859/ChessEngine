#include "boardview.h"
#include <iostream>

BoardView::BoardView() {
    // initialise and cache the 12 textures needed for the 12 pieces/sprites
    const std::string pieceColours[2]{"Light", "Dark"};
    const std::string pieceTypes[6]{"King", "Queen", "Rook", "Bishop", "Knight", "Pawn"};
    for (int colour = 0; colour < 2; ++colour) {
        for (int type = 0; type < 6; ++type) {
            std::string pieceFilePath = "assets/";
            pieceFilePath += pieceColours[colour] + pieceTypes[type] + ".png";

            if (!pieceTextures[colour][type].loadFromFile(pieceFilePath)) {
                std::cerr << "Failed to load " << pieceFilePath << std::endl;
            }
        }
    }
}

void BoardView::drawBoard(sf::RenderWindow& window) const {
    for (int row = 0; row < 8; ++row) {
        for (int column = 0; column < 8; ++column) {
            sf::RectangleShape square({squareSize, squareSize});
            square.setPosition({column * squareSize, row * squareSize});

            // combine the different highlighted squares together into one vector
            std::vector<HighlightedSquare> highlightedSquares;
            if (currentMoveHighlightedSquare)
                highlightedSquares.emplace_back(currentMoveHighlightedSquare.value());
            highlightedSquares.insert(highlightedSquares.end(), previousMoveHighlightedSquares.begin(), previousMoveHighlightedSquares.end());
            highlightedSquares.insert(highlightedSquares.end(), validMoveHighlightedSquares.begin(), validMoveHighlightedSquares.end());

            // check to see if the square should be highlighted a different colour
            bool isHighlightedSquare = false;
            for (const auto highlightedSquare : highlightedSquares) {
                if (highlightedSquare.position.x == column && highlightedSquare.position.y == row) {
                    switch (highlightedSquare.highlightType) {
                        case HighlightedSquare::HighlightType::STARTMOVE:
                            square.setFillColor(startMoveSquareHighlight);
                            break;
                        case HighlightedSquare::HighlightType::STOPMOVE:
                            square.setFillColor(stopMoveSquareHighlight);
                            break;
                        case HighlightedSquare::HighlightType::VALIDMOVE:
                            square.setFillColor(validMoveSquareHighlight);
                            break;
                        case HighlightedSquare::HighlightType::VALIDMOVEALT:
                            square.setFillColor(alternateValidMoveSquareHighlight);
                            break;
                    }
                    isHighlightedSquare = true;
                }
            }

            if (!isHighlightedSquare)
                square.setFillColor(((row + column) % 2 == 0) ? lightSquareColour : darkSquareColour);

            window.draw(square);
        }
    }
}

void BoardView::drawPieces(sf::RenderWindow& window, const std::array<std::array<std::optional<Piece>, 8>, 8>& piecePositions, const std::optional<sf::Vector2<int>> excludedSquare) const {
    for (int row = 0; row < 8; ++row) {
        for (int column = 0; column < 8; ++column) {

            // check there is a piece on the current square and check the current square is not excluded
            if (!piecePositions[row][column] || sf::Vector2(column, row) == excludedSquare)
                continue;

            // get the texture that matches the piece and use it to initialise a sprite for the piece
            sf::Sprite sprite(GetPieceTexture(*piecePositions[row][column]));
            sprite.setPosition({column * squareSize,row * squareSize});
            sprite.scale({pieceScale, pieceScale});
            window.draw(sprite);
        }
    }
}

void BoardView::drawSelectedPiece(sf::RenderWindow &window, const Piece piece, const int mouseX, const int mouseY) const {
    sf::Sprite sprite(GetPieceTexture(piece));
    sprite.setPosition({static_cast<float>(mouseX) - draggedPieceOffset, static_cast<float>(mouseY) - draggedPieceOffset});
    sprite.scale({pieceScale, pieceScale});
    window.draw(sprite);
}

void BoardView::drawPawnPromotionPieces(sf::RenderWindow &window, const Piece::Colour pieceColour, const sf::Vector2f pawnPromotionSquare) const {
    auto currentSquare = pawnPromotionSquare;
    for (const auto& pieceType : pawnPromotionPieces) {
        sf::Sprite sprite(GetPieceTexture(Piece(pieceType, pieceColour)));
        sprite.setPosition({currentSquare.x * squareSize, currentSquare.y * squareSize});
        sprite.scale({pieceScale, pieceScale});
        window.draw(sprite);
        // draw the pieces towards the centre of the screen
        if (pieceColour == Piece::Colour::WHITE)
            ++currentSquare.y;
        else
            --currentSquare.y;
    }
}

void BoardView::pickupPieceFromBoard(const sf::Vector2<int> startSquare, const std::vector<sf::Vector2<int>>& validMovableSquares) {
    currentMoveHighlightedSquare = HighlightedSquare(startSquare, HighlightedSquare::HighlightType::STARTMOVE);
    // add the valid movable squares to the list of valid move highlighted squares
    // to avoid solid blocks of colour on the board, sequential valid moves in the horizontal or vertical directions are broken up into two alternating colours
    for (const auto& square : validMovableSquares) {
        // check whether the square is horizontally or vertically aligned with the start square
        if (square.x == startSquare.x || square.y == startSquare.y) {
            // use manhattan distance from the start square to determine if the square is alternating or not
            if (const auto distanceFromStart = std::abs(square.x - startSquare.x) + std::abs(square.y - startSquare.y); distanceFromStart % 2 == 1) {
                validMoveHighlightedSquares.emplace_back(square, HighlightedSquare::HighlightType::VALIDMOVEALT);
                continue;
            }
        }
        // square is not horizontally or vertically aligned and/or is not alternating
        validMoveHighlightedSquares.emplace_back(square, HighlightedSquare::HighlightType::VALIDMOVE);
    }
}

void BoardView::placePieceOnBoard(const bool pieceMoved, sf::Vector2<int> endSquare) {
    validMoveHighlightedSquares.clear();
    if (pieceMoved) {
        // stop highlighting the last moves squares
        previousMoveHighlightedSquares.clear();
        // highlight this moves squares
        previousMoveHighlightedSquares.emplace_back(currentMoveHighlightedSquare.value());
        previousMoveHighlightedSquares.emplace_back(endSquare, HighlightedSquare::HighlightType::STOPMOVE);
    }
    currentMoveHighlightedSquare = std::nullopt;
}

sf::Vector2<int> BoardView::getSquare(const int mouseX, const int mouseY) const {
    // compute the square of the board that the mouse cursor is over
    return {std::clamp(static_cast<int>(floor(mouseX / squareSize)), 0, 7), std::clamp(static_cast<int>(floor(mouseY / squareSize)), 0, 7)};
}

const sf::Texture& BoardView::GetPieceTexture(const Piece piece) const {
    // retrieve and return the cached texture that matches the colour and type of the piece
    return pieceTextures[(piece.colour == Piece::Colour::WHITE) ? 0 : 1][static_cast<int>(piece.type)];
}