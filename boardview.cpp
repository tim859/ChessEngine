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

void BoardView::drawBoard(sf::RenderWindow& window, const std::vector<HighlightedSquare>& highlightedSquares) const {
    for (int row = 0; row < 8; ++row) {
        for (int column = 0; column < 8; ++column) {
            sf::RectangleShape square({squareSize, squareSize});
            square.setPosition({column * squareSize, row * squareSize});

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

sf::Vector2f BoardView::getSquare(const int mouseX, const int mouseY) const {
    // calculation that computes the square of the board that the mouse cursor is over
    return {floor(mouseX / squareSize), floor(mouseY / squareSize)};
}

const sf::Texture& BoardView::GetPieceTexture(const Piece piece) const {
    // retrieve and return the cached texture that matches the colour and type of the piece
    return pieceTextures[(piece.colour == Piece::Colour::WHITE) ? 0 : 1][static_cast<int>(piece.type)];
}
