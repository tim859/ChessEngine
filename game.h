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

// -------------------- castling positions --------------------

inline constexpr auto whiteKingStartSquare = Vector2Int(4, 7);
inline constexpr auto whiteQueensideRookStartSquare = Vector2Int(0, 7);
inline constexpr auto whiteQueensideRookEndSquare = Vector2Int(3, 7);
inline constexpr auto whiteKingsideRookStartSquare = Vector2Int(7, 7);
inline constexpr auto whiteKingsideRookEndSquare = Vector2Int(5, 7);

inline constexpr auto blackKingStartSquare = Vector2Int(4, 0);
inline constexpr auto blackQueensideRookStartSquare = Vector2Int(0, 0);
inline constexpr auto blackQueensideRookEndSquare = Vector2Int(3, 0);
inline constexpr auto blackKingsideRookStartSquare = Vector2Int(7, 0);
inline constexpr auto blackKingsideRookEndSquare = Vector2Int(5, 0);

namespace GameTypes {
    enum class MoveType {NONE, MOVESELF, CAPTURE, CASTLE, PROMOTEPAWN, CHECK, GAMEOVER};
    enum class GameOverType {CONTINUE, STALEMATE, TFRDRAW, FIFTYMOVEDRAW, WHITEWINBYCHECKMATE, BLACKWINBYCHECKMATE, WHITEWINBYRESIGN, BLACKWINBYRESIGN};
    enum class CastleType {NOCASTLE, WHITEQUEENSIDE, WHITEKINGSIDE, BLACKQUEENSIDE, BLACKKINGSIDE};
}


struct Piece {
    enum class Type {KING, QUEEN, ROOK, BISHOP, KNIGHT, PAWN};
    enum class Colour {WHITE, BLACK};

    Type type;
    Colour colour;

    Piece(const Type type, const Colour colour) : type(type), colour(colour) {}

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
    std::optional<Piece::Type> promotionPieceType;
    int score = 0;
};

// records the minimum information needed to reverse a single movePiece() call.
// search uses this for cheap make/unmake instead of snapshotting the whole gameState.
struct MoveDelta {
    Move move{};
    std::optional<Piece> capturedPiece;
    Vector2Int capturedPieceSquare{};
    bool wasPromotion = false;
    GameTypes::CastleType castleType = GameTypes::CastleType::NOCASTLE;
    std::optional<Vector2Int> previousEnPassantSquare;
    int previousMovesSinceEnPassant = 0;
    std::array<bool, 4> previousCastlingRights{};
    uint64_t previousZobristHash = 0;
};

struct GameState {
    std::optional<Piece> selectedPiece;
    std::optional<Vector2Int> selectedPieceStartSquare;
    // the 2d array that all the positions of the pieces will be held in
    // remember that while the array will hold piece positions as [row][column]/[rank][file], vector2's hold positions as (x, y) aka [column][row]/[file][rank]
    // therefore whenever you use a vector2 to find a position in the 2d array you need to index it using [vector.y][vector.x]
    // also remember that the origin ([0][0] for the array and (0, 0) for vectors) is the top left corner of the board for both the array and vectors
    std::array<std::array<std::optional<Piece>, 8>, 8> boardPosition;
    Piece::Colour moveColour = Piece::Colour::WHITE;
    int fullMoveCounter = 0;
    int halfMoveCounter = 0;
    int halfMovesSinceLastActiveMove = 0;
    int movesSinceEnPassant = 0;
    // {white queenside, white kingside, black queenside, black kingside
    std::array<bool, 4> castlingRights = {true, true, true, true};
    std::optional<Vector2Int> enPassantSquare;
    GameTypes::GameOverType gameOverType = GameTypes::GameOverType::CONTINUE;
    // zobrist hash that will contain a full board state in one 64-bit number
    uint64_t zobristHash = 0;

    void reset() {
        selectedPiece = std::nullopt;
        selectedPieceStartSquare = std::nullopt;
        boardPosition = {};
        moveColour = Piece::Colour::WHITE;
        fullMoveCounter = 0;
        halfMoveCounter = 0;
        halfMovesSinceLastActiveMove = 0;
        movesSinceEnPassant = 0;
        castlingRights = {true, true, true, true};
        enPassantSquare = std::nullopt;
        gameOverType = GameTypes::GameOverType::CONTINUE;
        zobristHash = 0;
    }

    bool operator==(const GameState& other) const {
        return boardPosition == other.boardPosition;
    }

    bool operator<(const GameState& other) const {
        if (boardPosition != other.boardPosition) return boardPosition < other.boardPosition;
        if (moveColour != other.moveColour) return moveColour < other.moveColour;
        if (castlingRights != other.castlingRights) return castlingRights < other.castlingRights;
        if (enPassantSquare != other.enPassantSquare) return enPassantSquare < other.enPassantSquare;
        return false;
    }
};

struct ZobristHashKeys
{
    // boardHash is accessed with [piece square index][piece type][piece colour]
    // piece square index: rank * 8 + file
    // piece type: static_cast<int>(Piece::Type) (casts the Piece::Type enum to its core int values)
    // piece colour: 0 = white, 1 = black
    std::array<std::array<std::array<uint64_t, 2>, 6>, 64> boardHash = {};
    // only need the file, rank is always 3 or 6 depending on colour
    std::array<uint64_t, 8> enPassantFileHash = {};
    // white queenside, white kingside, black queenside, black kingside
    std::array<uint64_t, 4> castlingRightsHash = {};
    uint64_t turnHash = 0;
};

class Game {
    GameState currentGameState;
    std::vector<GameState> currentGameStateHistory;

    [[nodiscard]] static ZobristHashKeys generateZobristHashKeys();
    inline static const ZobristHashKeys zobristHashKeys = generateZobristHashKeys();

public:
    // -------------------- getters --------------------

    [[nodiscard]] GameState& getCurrentGameState() {return currentGameState;}
    [[nodiscard]] const GameState& getCurrentGameState() const {return currentGameState; }
    [[nodiscard]] std::vector<GameState>& getCurrentGameStateHistory() {return currentGameStateHistory;}
    [[nodiscard]] const std::vector<GameState>& getCurrentGameStateHistory() const { return currentGameStateHistory; }
    [[nodiscard]] std::array<std::array<std::optional<Piece>, 8>, 8> getCurrentBoardPosition() const {return currentGameState.boardPosition;}
    [[nodiscard]] std::optional<Vector2Int> getCurrentSelectedPieceStartSquare() const {return currentGameState.selectedPieceStartSquare;}
    [[nodiscard]] std::optional<Piece> getCurrentSelectedPiece() const {return currentGameState.selectedPiece;}
    [[nodiscard]] GameTypes::GameOverType getCurrentGameOverType() const {return currentGameState.gameOverType;}
    [[nodiscard]] std::vector<Vector2Int> generateLegalMovesForSquare(const GameState& gameState, Vector2Int startSquare) const;

    // -------------------- application functions (modify the game state) --------------------

    void reset();
    bool populateGameStateFromFEN(GameState& gameState, std::vector<GameState>& gameStateHistory, const std::string& fen) const;
    bool pickupPieceFromBoard(GameState& gameState, Vector2Int startSquare) const;
    GameTypes::MoveType placePieceOnBoard(GameState& gameState, Vector2Int endSquare, std::vector<GameState>& gameStateHistory, const Piece* pawnPromotionChoice) const;
    MoveDelta movePiece(GameState& gameState, const Move& move) const;
    void undoLastMove(GameState& gameState, const MoveDelta& moveDelta) const;
    void updateCastlingRights(GameState& gameState, const Move &move) const;
    void castleRook(GameState& gameState, GameTypes::CastleType castleType) const;

    // -------------------- validation/helper functions (do not modify the game state) --------------------

    [[nodiscard]] bool isMoveValid(const GameState& gameState, const Move& move) const;
    [[nodiscard]] bool isMoveLegal(const GameState& gameState, const Move& move) const;
    [[nodiscard]] bool isMovePathClearForSliders(const GameState& gameState, const Move& move) const;
    [[nodiscard]] bool isMoveValidForKing(const GameState& gameState, const Move& move) const;
    [[nodiscard]] GameTypes::CastleType checkForCastle(const GameState& gameState, const Move& move) const;
    [[nodiscard]] bool isMoveValidForPawn(const GameState& gameState, const Move& move) const;
    [[nodiscard]] bool checkForPawnDoublePush(const GameState& gameState, const Move& move) const;
    [[nodiscard]] bool checkForEnPassantTake(const GameState& gameState, const Move &move) const;
    [[nodiscard]] bool isSquareUnderAttack(const GameState& gameState, Vector2Int square, Piece::Colour enemyColour) const;
    [[nodiscard]] bool isSquareUnderAttackByPawn(const GameState& gameState, Vector2Int square, Piece::Colour enemyColour) const;
    [[nodiscard]] bool isKingInCheck(const GameState& gameState, Piece::Colour kingColour) const;
    [[nodiscard]] bool checkForPawnPromotionOnLastMove(const GameState& gameState) const;
    [[nodiscard]] bool checkForPawnPromotionOnNextMove(const GameState& gameState, const Move& move) const;
    [[nodiscard]] std::vector<Move> generateAllLegalMoves(const GameState& gameState, bool capturesOnly = false) const;
    [[nodiscard]] uint64_t generateZobristHash(const GameState& gameState) const;
    [[nodiscard]] bool isEnPassantPlayable(const GameState& gameState) const;
};

#endif //CHESS_GAME_H