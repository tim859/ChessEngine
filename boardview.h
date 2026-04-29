#ifndef CHESS_BOARD_H
#define CHESS_BOARD_H
#include <SFML/Graphics.hpp>

#include "game.h"
#include <vector>

class BoardView {
    const float squareSize = 150.0f;
    const float pieceScale = 2.3f;
    const float draggedPieceOffset = 75.0f;
    const sf::Color darkSquareColour = sf::Color(161, 111, 90);
    const sf::Color lightSquareColour = sf::Color(235, 210, 184);
    const sf::Color startMoveSquareHighlight = sf::Color(202, 163, 97);
    const sf::Color stopMoveSquareHighlight = sf::Color(216, 199, 112);
    const sf::Color validMoveSquareHighlight = sf::Color(176, 39, 48);
    const sf::Color alternateValidMoveSquareHighlight = sf::Color(222, 61, 76);
    std::array<std::array<sf::Texture, 6>, 2> pieceTextures;
    std::array<Piece::Type, 4> pawnPromotionPieces = {Piece::Type::QUEEN, Piece::Type::ROOK, Piece::Type::KNIGHT, Piece::Type::BISHOP};

    struct HighlightedSquare {
        enum class HighlightType {STARTMOVE, STOPMOVE, VALIDMOVE, VALIDMOVEALT};

        Vector2Int position;
        HighlightType highlightType;

        HighlightedSquare(const Vector2Int newPosition, const HighlightType newColourType) : position(newPosition), highlightType(newColourType) {}
    };

    std::optional<HighlightedSquare> currentMoveHighlightedSquare;
    std::vector<HighlightedSquare> previousMoveHighlightedSquares;
    std::vector<HighlightedSquare> validMoveHighlightedSquares;

public:
    BoardView();
    void drawBoard(sf::RenderWindow& window) const;
    void drawPieces(sf::RenderWindow& window, const std::array<std::array<std::optional<Piece>, 8>, 8>& piecePositions, std::optional<Vector2Int> excludedSquare) const;
    void drawSelectedPiece(sf::RenderWindow& window, Piece piece, int mouseX, int mouseY) const;
    void drawPawnPromotionPieces(sf::RenderWindow& window, Piece::Colour pieceColour, Vector2Int pawnPromotionSquare) const;
    void pickupPieceFromBoard(Vector2Int startSquare, const std::vector<Vector2Int>& validMovableSquares);
    void placePieceOnBoard(bool pieceMoved, Vector2Int endSquare);
    [[nodiscard]] Vector2Int getSquare(int mouseX, int mouseY) const;

private:
    const sf::Texture& GetPieceTexture(Piece piece) const;
};

#endif //CHESS_BOARD_H