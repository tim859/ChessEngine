#ifndef CHESS_GAME_H
#define CHESS_GAME_H
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <SFML/System/Vector2.hpp>

namespace GameTypes {
    enum class MoveType {NONE, MOVESELF, CAPTURE, CASTLE, PROMOTEPAWN, GAMEOVER};
    enum class GameOverType {CONTINUE, STALEMATE, TFRDRAW, FIFTYMOVEDRAW, WHITEWINBYCHECKMATE, BLACKWINBYCHECKMATE, WHITEWINBYRESIGN, BLACKWINBYRESIGN};
}

namespace sf {
    inline bool operator<(const Vector2<int>& first, const Vector2<int>& second) {
        if (first.x != second.x) return first.x < second.x;
        return first.y < second.y;
    }

    inline bool operator==(const Vector2<int>& first, const Vector2<int>& second) {
        return first.x == second.x && first.y == second.y;
    }
}

struct Piece {
    enum class Type {KING, QUEEN, ROOK, BISHOP, KNIGHT, PAWN};
    enum class Colour {WHITE, BLACK};

    Type type;
    Colour colour;

    bool operator==(const Piece& other) const {
        return type == other.type && colour == other.colour;
    }

    bool operator!=(const Piece& other) const {
        return !(*this == other);
    }

    bool operator<(const Piece& other) const {
        if (type != other.type) return type < other.type;
        return colour < other.colour;
    }
};

struct Move {
    sf::Vector2<int> startSquare;
    sf::Vector2<int> endSquare;

    Move(const sf::Vector2<int> newStartPosition, const sf::Vector2<int> newEndPosition) : startSquare(newStartPosition), endSquare(newEndPosition) {}
};

// TODO: replace as many instances of std::optional<> as possible with pointers or smart pointers, they're just better
struct GameState {
    std::optional<Piece> selectedPiece;
    std::optional<sf::Vector2<int>> selectedPieceStartSquare;
    // the 2d array that all the positions of the pieces will be held in
    // remember that while the array will hold piece positions as [row][column], vector2's hold positions as (x, y) aka [column][row]
    // therefore whenever you use a vector2 to find a position in the 2d array you need to index it using [vector.y][vector.x]
    // also remember that the origin ([0][0] for the array and (0, 0) for vectors) is the top left corner of the board for both the array and vectors
    std::array<std::array<std::optional<Piece>, 8>, 8> boardPosition;
    Piece::Colour moveColour = Piece::Colour::WHITE;
    int fullMoveCounter = 0;
    int halfMoveCounter = 0;
    int halfMovesSinceLastActiveMove = 0;
    // {white queenside, white kingside}
    std::array<bool, 2> whiteCastleRights = {true, true};
    // {black queenside, black kingside}
    std::array<bool, 2> blackCastleRights = {true, true};
    std::optional<sf::Vector2<int>> enPassantSquare;
    //std::optional<sf::Vector2<int>> enPassantPawnSquare;
    int movesSinceEnPassant = 0;
    std::optional<sf::Vector2<int>> pawnPendingPromotionSquare;
    std::optional<Piece::Colour> pawnPendingPromotionColour;
    GameTypes::GameOverType gameOverType = GameTypes::GameOverType::CONTINUE;

    bool operator==(const GameState& other) const {
        return boardPosition == other.boardPosition;
    }

    bool operator<(const GameState& other) const {
        if (boardPosition != other.boardPosition) return boardPosition < other.boardPosition;
        if (moveColour != other.moveColour) return moveColour < other.moveColour;
        if (whiteCastleRights != other.whiteCastleRights) return whiteCastleRights < other.whiteCastleRights;
        if (blackCastleRights != other.blackCastleRights) return blackCastleRights < other.blackCastleRights;
        if (enPassantSquare != other.enPassantSquare) return enPassantSquare < other.enPassantSquare;
        return false;
    }
};

class Game {
    // -------------------- game state members

    GameState currentGameState;
    std::map<GameState, int>* currentGameStateHistory;

    // -------------------- castling positions --------------------

    const sf::Vector2<int> whiteKingStartSquare = sf::Vector2(4, 7);
    const sf::Vector2<int> whiteQueensideRookStartSquare = sf::Vector2(0, 7);
    const sf::Vector2<int> whiteKingsideRookStartSquare = sf::Vector2(7, 7);
    const sf::Vector2<int> blackKingStartSquare = sf::Vector2(4, 0);
    const sf::Vector2<int> blackQueensideRookStartSquare = sf::Vector2(0, 0);
    const sf::Vector2<int> blackKingsideRookStartSquare = sf::Vector2(7, 0);

public:
    Game();
    // -------------------- getters --------------------

    [[nodiscard]] GameState& getCurrentGameState() {return currentGameState;}
    [[nodiscard]] std::map<GameState, int>* getCurrentGameStateHistory() const {return currentGameStateHistory;}
    [[nodiscard]] std::array<std::array<std::optional<Piece>, 8>, 8> getCurrentBoardPosition() const {return currentGameState.boardPosition;}
    [[nodiscard]] std::optional<sf::Vector2<int>> getCurrentSelectedPieceStartSquare() const {return currentGameState.selectedPieceStartSquare;}
    [[nodiscard]] std::optional<Piece> getCurrentSelectedPiece() const {return currentGameState.selectedPiece;}
    [[nodiscard]] std::optional<sf::Vector2<int>> getCurrentPawnPendingPromotionSquare() const {return currentGameState.pawnPendingPromotionSquare;}
    [[nodiscard]] std::optional<Piece::Colour> getCurrentPawnPendingPromotionColour() const {return currentGameState.pawnPendingPromotionColour;}
    [[nodiscard]] GameTypes::GameOverType getCurrentGameOverType() const {return currentGameState.gameOverType;}
    [[nodiscard]] std::vector<sf::Vector2<int>> generateLegalMovesForSquare(const GameState& gameState, sf::Vector2<int> startSquare) const;

    // -------------------- public application functions (modify the game state) --------------------

    void populateCurrentGameStateWithFen(const std::string& fen);
    bool pickupPieceFromBoard(GameState& gameState, sf::Vector2<int> startSquare) const;
    GameTypes::MoveType placePieceOnBoard(GameState& gameState, sf::Vector2<int> endSquare, std::map<GameState, int>*) const;
    void promotePawn(GameState& gameState, Piece::Type pieceType) const;
    [[nodiscard]] std::vector<Move> generateAllLegalMoves(const GameState& gameState) const;

private:
    // -------------------- private application functions (modify the game state) --------------------

    GameTypes::MoveType movePiece(GameState& gameState, Move move) const;
    void updateCastlingRights(GameState& gameState, Move move) const;
    void castleRook(GameState& gameState, int rook) const;

    // -------------------- validation functions (do not modify the game state) --------------------

    [[nodiscard]] bool checkIsMoveValid(const GameState& gameState, Move move) const;
    [[nodiscard]] bool checkIsMoveLegal(const GameState& gameState, Move move) const;
    [[nodiscard]] bool checkIsMovePathClearForSliders(const GameState& gameState, Move move) const;
    [[nodiscard]] bool checkIsMoveValidForKing(const GameState& gameState, Move move) const;
    [[nodiscard]] int checkForCastle(const GameState& gameState, Move move) const;
    [[nodiscard]] bool checkIsMoveValidForPawn(const GameState& gameState, Move move) const;
    [[nodiscard]] bool checkForPawnDoublePush(const GameState& gameState, Move move) const;
    [[nodiscard]] bool checkForEnPassantTake(const GameState& gameState, Move move) const;
    [[nodiscard]] bool checkIsSquareUnderAttack(const GameState& gameState, sf::Vector2<int> square, Piece::Colour enemyColour) const;
    [[nodiscard]] bool checkIsKingInCheck(const GameState& gameState, Piece::Colour kingColour) const;
    [[nodiscard]] bool checkForPawnPromotion(const GameState& gameState) const;
};

#endif //CHESS_GAME_H