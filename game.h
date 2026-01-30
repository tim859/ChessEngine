#ifndef CHESS_GAME_H
#define CHESS_GAME_H
#include <array>
#include <optional>
#include <string>
#include <vector>

struct Vector2Int {
    int x;
    int y;

    bool operator==(const Vector2Int& other) const {
        return x == other.x && y == other.y;
    }

    bool operator<(const Vector2Int& other) const {
        if (x != other.x)
            return x < other.x;
        return y < other.y;
    }

    Vector2Int operator+(const Vector2Int& other) const {
        return { x + other.x, y + other.y };
    }

    Vector2Int& operator+=(const Vector2Int& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
};

namespace GameTypes {
    enum class MoveType {NONE, MOVESELF, CAPTURE, CASTLE, PROMOTEPAWN, CHECK, GAMEOVER};
    enum class GameOverType {CONTINUE, STALEMATE, TFRDRAW, FIFTYMOVEDRAW, WHITEWINBYCHECKMATE, BLACKWINBYCHECKMATE, WHITEWINBYRESIGN, BLACKWINBYRESIGN};
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
    Vector2Int startSquare;
    Vector2Int endSquare;
    int score = 0;
};

struct GameState {
    std::optional<Piece> selectedPiece;
    std::optional<Vector2Int> selectedPieceStartSquare;
    // the 2d array that all the positions of the pieces will be held in
    // remember that while the array will hold piece positions as [row][column], vector2's hold positions as (x, y) aka [column][row]
    // therefore whenever you use a vector2 to find a position in the 2d array you need to index it using [vector.y][vector.x]
    // also remember that the origin ([0][0] for the array and (0, 0) for vectors) is the top left corner of the board for both the array and vectors
    std::array<std::array<std::optional<Piece>, 8>, 8> boardPosition;
    Piece::Colour moveColour = Piece::Colour::WHITE;
    int fullMoveCounter = 0;
    int halfMoveCounter = 0;
    int halfMovesSinceLastActiveMove = 0;
    int movesSinceEnPassant = 0;
    // {white queenside, white kingside}
    std::array<bool, 2> whiteCastleRights = {true, true};
    // {black queenside, black kingside}
    std::array<bool, 2> blackCastleRights = {true, true};
    std::optional<Vector2Int> enPassantSquare;
    GameTypes::GameOverType gameOverType = GameTypes::GameOverType::CONTINUE;

    void reset() {
        selectedPiece = std::nullopt;
        selectedPieceStartSquare = std::nullopt;
        boardPosition = {};
        moveColour = Piece::Colour::WHITE;
        fullMoveCounter = 0;
        halfMoveCounter = 0;
        halfMovesSinceLastActiveMove = 0;
        movesSinceEnPassant = 0;
        whiteCastleRights = {true, true};
        blackCastleRights = {true, true};
        enPassantSquare = std::nullopt;
        gameOverType = GameTypes::GameOverType::CONTINUE;
    }

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
    std::vector<GameState>* currentGameStateHistory;

    // -------------------- castling positions --------------------

    const Vector2Int whiteKingStartSquare = Vector2Int(4, 7);
    const Vector2Int whiteQueensideRookStartSquare = Vector2Int(0, 7);
    const Vector2Int whiteKingsideRookStartSquare = Vector2Int(7, 7);
    const Vector2Int blackKingStartSquare = Vector2Int(4, 0);
    const Vector2Int blackQueensideRookStartSquare = Vector2Int(0, 0);
    const Vector2Int blackKingsideRookStartSquare = Vector2Int(7, 0);

public:
    Game();
    // -------------------- getters --------------------

    [[nodiscard]] GameState& getCurrentGameState() {return currentGameState;}
    [[nodiscard]] std::vector<GameState>* getCurrentGameStateHistory() const {return currentGameStateHistory;}
    [[nodiscard]] std::array<std::array<std::optional<Piece>, 8>, 8> getCurrentBoardPosition() const {return currentGameState.boardPosition;}
    [[nodiscard]] std::optional<Vector2Int> getCurrentSelectedPieceStartSquare() const {return currentGameState.selectedPieceStartSquare;}
    [[nodiscard]] std::optional<Piece> getCurrentSelectedPiece() const {return currentGameState.selectedPiece;}
    [[nodiscard]] GameTypes::GameOverType getCurrentGameOverType() const {return currentGameState.gameOverType;}
    [[nodiscard]] std::vector<Vector2Int> generateLegalMovesForSquare(const GameState& gameState, Vector2Int startSquare) const;

    // -------------------- application functions (modify the game state) --------------------

    void reset();
    bool populateGameStateFromFEN(GameState& gameState, std::vector<GameState>* gameStateHistory, const std::string& fen) const;
    // TODO: remove pickupPieceFromBoard and placePieceOnBoard, all the logic they contained has been moved to movePiece and they now do almost nothing
    bool pickupPieceFromBoard(GameState& gameState, Vector2Int startSquare) const;
    GameTypes::MoveType placePieceOnBoard(GameState& gameState, Vector2Int endSquare, std::vector<GameState>* gameStateHistory, const Piece* pawnPromotionChoice) const;
    GameTypes::MoveType movePiece(GameState& gameState, const Move& move, std::vector<GameState>* gameStateHistory, const Piece* pawnPromotionChoice) const;
    void undoLastMove(GameState& gameState, std::vector<GameState>* gameStateHistory) const;
    void updateCastlingRights(GameState& gameState, Move move) const;
    void castleRook(GameState& gameState, int rook) const;

    // -------------------- validation functions (do not modify the game state) --------------------

    [[nodiscard]] bool checkIsMoveValid(const GameState& gameState, const Move& move) const;
    [[nodiscard]] bool checkIsMoveLegal(const GameState& gameState, const Move& move) const;
    [[nodiscard]] bool checkIsMovePathClearForSliders(const GameState& gameState, const Move& move) const;
    [[nodiscard]] bool checkIsMoveValidForKing(const GameState& gameState, const Move& move) const;
    [[nodiscard]] int checkForCastle(const GameState& gameState, const Move& move) const;
    [[nodiscard]] bool checkIsMoveValidForPawn(const GameState& gameState, const Move& move) const;
    [[nodiscard]] bool checkForPawnDoublePush(const GameState& gameState, const Move& move) const;
    [[nodiscard]] bool checkForEnPassantTake(const GameState& gameState, const Move &move) const;
    [[nodiscard]] bool checkIsSquareUnderAttack(const GameState& gameState, Vector2Int square, Piece::Colour enemyColour) const;
    [[nodiscard]] bool checkIsSquareUnderAttackByPawn(const GameState& gameState, Vector2Int square, Piece::Colour enemyColour) const;
    [[nodiscard]] bool checkIsKingInCheck(const GameState& gameState, Piece::Colour kingColour) const;
    [[nodiscard]] bool checkForPawnPromotionOnLastMove(const GameState& gameState) const;
    [[nodiscard]] bool checkForPawnPromotionOnNextMove(const GameState& gameState, const Move& move) const;
    [[nodiscard]] std::vector<Move> generateAllLegalMoves(const GameState& gameState) const;
};

#endif //CHESS_GAME_H