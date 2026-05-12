#include "game.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <ranges>

ZobristHashKeys Game::generateZobristHashKeys() {
    // set up rng
    constexpr uint64_t zobristSeed = 42;
    std::mt19937_64 rng(zobristSeed);

    // initialise hash keys
    ZobristHashKeys zobristHashKeys;
    for (auto i = 0; i < 64; ++i) {
        for (auto j = 0; j < 6; ++j) {
            for (auto k = 0; k < 2; ++k)
                zobristHashKeys.boardHash[i][j][k] = rng();
        }
    }
    for (auto i = 0; i < 8; ++i)
        zobristHashKeys.enPassantFileHash[i] = rng();
    for (auto i = 0; i < 4; ++i)
        zobristHashKeys.castlingRightsHash[i] = rng();
    zobristHashKeys.turnHash = rng();

    return zobristHashKeys;
}

std::vector<Vector2Int> Game::generateLegalMovesForSquare(const GameState& gameState,  const Vector2Int startSquare) const {
    std::vector<Vector2Int> validMovableSquares;
    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            if (const auto move = Move(startSquare, Vector2Int(file, rank)); isMoveLegal(gameState, move))
                validMovableSquares.emplace_back(file, rank);
        }
    }
    return validMovableSquares;
}

void Game::reset() {
    currentGameState.reset();
    currentGameStateHistory.clear();
}

bool Game::populateGameStateFromFEN(GameState& gameState, std::vector<GameState>& gameStateHistory, const std::string& fen) const {
    gameState.reset();

    // tokenise the fen string so the 6 pieces of information can be handled individually
    std::vector<std::string> fenTokens;
    std::string currentToken;
    for (const auto& character : fen) {
        if (character == ' ') {
            if (!currentToken.empty()) {
                fenTokens.push_back(currentToken);
                currentToken.clear();
            }
        }
        else
            currentToken += character;
    }
    if (!currentToken.empty())
        fenTokens.push_back(currentToken);

    if (fenTokens.size() != 6) {
        std::cerr << "FEN must have exactly 6 fields" << std::endl;
        return false;
    }

    // ---------- 1. board position ----------

    auto row = 0, column = 0;
    for (const auto& character : fenTokens[0]) {
        if (row > 7 || column > 8) {
            std::cerr << "FEN board position invalid" << std::endl;
            return false;
        }

        // required to avoid undefined behaviour with std::isdigit, std::isalpha, std::isupper, std::islower
        const auto unsignedCharacter = static_cast<unsigned char>(character);
        // slash means go to the next row and reset the column
        if (character == '/') {
            if (column != 8) {
                std::cerr << "FEN board position invalid" << std::endl;
                return false;
            }
            ++row;
            column = 0;
        }
        // digit means skip that amount of squares
        else if (std::isdigit(unsignedCharacter)) {
            if (character - '0' < 1 || character - '0' > 8) {
                std::cerr << "FEN board position contains invalid digit" << std::endl;
                return false;
            }
            column += character - '0';
        }
        // letter denotes type, case denotes colour
        else if (std::isalpha(unsignedCharacter)) {
            if (column > 7) {
                std::cerr << "FEN board position invalid" << std::endl;
                return false;
            }
            const auto letter = static_cast<char>(std::tolower(unsignedCharacter));
            Piece::Type pieceType;

            switch (letter) {
                case 'k': pieceType = Piece::Type::KING; break;
                case 'q': pieceType = Piece::Type::QUEEN; break;
                case 'r': pieceType = Piece::Type::ROOK; break;
                case 'b': pieceType = Piece::Type::BISHOP; break;
                case 'n': pieceType = Piece::Type::KNIGHT; break;
                case 'p': pieceType = Piece::Type::PAWN; break;
                default:
                    std::cerr << "FEN board position contains invalid character" << std::endl;
                    return false;
            }
            gameState.boardPosition[row][column] = Piece(pieceType, std::isupper(unsignedCharacter) ? Piece::Colour::WHITE : Piece::Colour::BLACK);
            ++column;
        }
        else {
            std::cerr << "FEN board position contains invalid character" << std::endl;
            return false;
        }
    }
    if (row != 7 || column != 8) {
        std::cerr << "FEN board position invalid" << std::endl;
        return false;
    }

    // ---------- 2. move colour ----------

    if (fenTokens[1] == "w")
        gameState.moveColour = Piece::Colour::WHITE;
    else if (fenTokens[1] == "b")
        gameState.moveColour = Piece::Colour::BLACK;
    else {
        std::cerr << "FEN move colour invalid" << std::endl;
        return false;
    }

    // ---------- 3. castling rights ----------

    gameState.castlingRights = {false, false, false, false};
    if (fenTokens[2] != "-") {
        for (const auto& character : fenTokens[2]) {
            switch (character) {
                case 'Q': gameState.castlingRights[0] = true; break;
                case 'K': gameState.castlingRights[1] = true; break;
                case 'q': gameState.castlingRights[2] = true; break;
                case 'k': gameState.castlingRights[3] = true; break;
                default:
                    std::cerr << "FEN castling rights invalid" << std::endl;
                    return false;
            }
        }
    }

    // ---------- 4. enpassant square ----------

    if (fenTokens[3] != "-") {
        // must be 2 characters exactly
        if (fenTokens[3].length() != 2) {
            std::cerr << "FEN enpassant square invalid" << std::endl;
            return false;
        }
        // first character must be a lowercase letter a to h
        if (!std::islower(static_cast<unsigned char>(fenTokens[3][0])) || fenTokens[3][0] < 'a' || fenTokens[3][0] > 'h') {
            std::cerr << "FEN enpassant square invalid" << std::endl;
            return false;
        }
        // second character must be 3 or 6
        if (fenTokens[3][1] != '3' && fenTokens[3][1] != '6') {
            std::cerr << "FEN enpassant square invalid" << std::endl;
            return false;
        }

        // convert standard chess notation to programs zero indexed vector2 based way of storing piece positions e.g. e3 becomes (4, 5)
        gameState.enPassantSquare = Vector2Int(fenTokens[3][0] - 'a', 7 - (fenTokens[3][1] - '1'));
    }

    // ---------- 5. half move counter ----------

    // std::stoi throws if the first character it parses is not a digit, so make sure it is before using std::stoi
    if (!std::isdigit(static_cast<unsigned char>(fenTokens[4][0]))) {
        std::cerr << "FEN half move count invalid" << std::endl;
        return false;
    }
    // pos will count the number of characters that std::stoi parses
    size_t pos = 0;
    int halfMoveCount;
    try {
        halfMoveCount = std::stoi(fenTokens[4], &pos);
    }
    catch (const std::out_of_range&) {
        std::cerr << "FEN half move count too large" << std::endl;
        return false;
    }
    // if pos is the same as the number of characters in the token, then every character in the token was a digit
    if (pos == fenTokens[4].length())
        gameState.halfMoveCounter = halfMoveCount;
    else {
        std::cerr << "FEN half move count invalid" << std::endl;
        return false;
    }

    // ---------- 6. full move counter ----------

    // same strategy as 5.
    if (!std::isdigit(static_cast<unsigned char>(fenTokens[5][0]))) {
        std::cerr << "FEN full move count invalid" << std::endl;
        return false;
    }
    pos = 0;
    int fullMoveCount;
    try {
        fullMoveCount = std::stoi(fenTokens[5], &pos);
    }
    catch (const std::out_of_range&) {
        std::cerr << "FEN full move count too large" << std::endl;
        return false;
    }
    if (pos == fenTokens[5].length())
        gameState.fullMoveCounter = fullMoveCount;
    else {
        std::cerr << "FEN full move count invalid" << std::endl;
        return false;
    }

    gameState.zobristHash = generateZobristHash(gameState);
    gameStateHistory.emplace_back(gameState);
    return true;
}

bool Game::pickupPieceFromBoard(GameState& gameState, const Vector2Int startSquare) const {
    // check there is a piece at the start square
    if (!gameState.boardPosition[startSquare.y][startSquare.x])
        return false;

    gameState.selectedPieceStartSquare = startSquare;
    gameState.selectedPiece = gameState.boardPosition[startSquare.y][startSquare.x];

    // check the piece is the same colour as the colour of whose turn it is
    if (gameState.selectedPiece->colour != gameState.moveColour) {
        gameState.selectedPieceStartSquare = std::nullopt;
        gameState.selectedPiece = std::nullopt;
        return false;
    }
    return true;
}

// TODO: this function is only used for ChessGUI, not ChessUCI. It can be only used from main.cpp currently, all other classes that want to move pieces have to manually call checkIsMoveLegal() first.
// TODO: it needs to be rewritten to be more clear about what its actually doing and to make it available to other classes if necessary.
GameTypes::MoveType Game::placePieceOnBoard(GameState& gameState, const Vector2Int endSquare, std::vector<GameState>& gameStateHistory, const Piece* pawnPromotionChoice) const {
    auto moveType = GameTypes::MoveType::NONE;

    // ensure the piece and the piece start square will be valid for all the functions that need them and get called from this function
    if (!gameState.selectedPieceStartSquare)
        return moveType;
    if (!gameState.boardPosition[gameState.selectedPieceStartSquare.value().y][gameState.selectedPieceStartSquare.value().x])
        return moveType;

    // determine if piece can move to this square and move it if so
    auto move = Move(gameState.selectedPieceStartSquare.value(), endSquare);
    // moveDelta is populated only when the move was legal and applied. it carries everything
    // needed to derive moveType post-hoc (capture / castle / promotion attribution).
    std::optional<MoveDelta> moveDelta;
    if (isMoveLegal(gameState, move)) {
        if (pawnPromotionChoice)
            move.promotionPieceType = pawnPromotionChoice->type;
        // GUI code maintains the full gameStateHistory for threefold repetition;
        // push the pre-move snapshot before mutating gameState.
        gameStateHistory.emplace_back(gameState);
        moveDelta = movePiece(gameState, move);
    }

    // derive moveType from the delta. the order matters: castling and promotion are more specific
    // than "regular capture" and override it, and CHECK overrides everything below.
    if (moveDelta) {
        if (moveDelta->castleType != GameTypes::CastleType::NOCASTLE)
            moveType = GameTypes::MoveType::CASTLE;
        else if (moveDelta->wasPromotion)
            moveType = GameTypes::MoveType::PROMOTEPAWN;
        else if (moveDelta->capturedPiece)
            moveType = GameTypes::MoveType::CAPTURE;
        else
            moveType = GameTypes::MoveType::MOVESELF;
    }

    if (isKingInCheck(gameState, gameState.moveColour))
        moveType = GameTypes::MoveType::CHECK;

    // check for stalemate and checkmate
    if (generateAllLegalMoves(gameState).empty()) {
        moveType = GameTypes::MoveType::GAMEOVER;
        if (isKingInCheck(gameState, gameState.moveColour)) {
            if (gameState.moveColour == Piece::Colour::WHITE)
                gameState.gameOverType = GameTypes::GameOverType::BLACKWINBYCHECKMATE;
            else
                gameState.gameOverType = GameTypes::GameOverType::WHITEWINBYCHECKMATE;
        }
        else
            gameState.gameOverType = GameTypes::GameOverType::STALEMATE;
    }

    // check for how many times this gamestate (specifically board position) has appeared, if it is 3 or more then the game is drawn by threefold repetition
    auto gameStateFrequency = 0;
    for (const auto& previousGameState : gameStateHistory) {
        if (gameState == previousGameState) {
            ++gameStateFrequency;
            if (gameStateFrequency >= 3) {
                moveType = GameTypes::MoveType::GAMEOVER;
                gameState.gameOverType = GameTypes::GameOverType::TFRDRAW;
            }
        }
    }

    // check for 50 full moves/100 half moves since a piece capture or a pawn moving aka the 50 move draw.
    // note: the moveType variable can be overridden to CHECK/GAMEOVER above, so we read attribution
    // from the moveDelta directly rather than from moveType. wasPromotion counts as a pawn move
    // even though the post-move endSquare no longer holds a pawn.
    const auto& endSquarePiece = gameState.boardPosition[move.endSquare.y][move.endSquare.x];
    const bool wasCapture = moveDelta && moveDelta->capturedPiece;
    const bool wasPawnMove = moveDelta && (moveDelta->wasPromotion
                                           || (endSquarePiece && endSquarePiece->type == Piece::Type::PAWN));
    if (wasCapture || wasPawnMove)
        gameState.halfMovesSinceLastActiveMove = 0;
    else {
        ++gameState.halfMovesSinceLastActiveMove;
        if (gameState.halfMovesSinceLastActiveMove >= 100) {
            moveType = GameTypes::MoveType::GAMEOVER;
            gameState.gameOverType = GameTypes::GameOverType::FIFTYMOVEDRAW;
        }
    }

    gameState.selectedPieceStartSquare = std::nullopt;
    gameState.selectedPiece = std::nullopt;
    return moveType;
}

MoveDelta Game::movePiece(GameState& gameState, const Move& move) const {
    const auto movePiece = gameState.boardPosition[move.startSquare.y][move.startSquare.x].value();

    // record everything needed to reverse this move, before any state mutates.
    // capturedPiece is filled in by the enpassant / regular-capture branches below.
    MoveDelta moveDelta;
    moveDelta.move = move;
    moveDelta.previousEnPassantSquare = gameState.enPassantSquare;
    moveDelta.previousMovesSinceEnPassant = gameState.movesSinceEnPassant;
    moveDelta.previousCastlingRights = gameState.castlingRights;
    // en passant can override this below
    moveDelta.capturedPieceSquare = move.endSquare;
    moveDelta.previousZobristHash = gameState.zobristHash;

    // ---------- en passant ---------------

    // XOR out the old enpassant file if it was playable
    if (isEnPassantPlayable(gameState))
        gameState.zobristHash ^= zobristHashKeys.enPassantFileHash[gameState.enPassantSquare->x];

    // check for pawn double push and record the intermediate square it skipped over (enPassantSquare) and the square it is now on (enPassantPawnSquare)
    if (checkForPawnDoublePush(gameState, move)) {
        const int forwardStep = movePiece.colour == Piece::Colour::WHITE ? -1 : 1;
        gameState.enPassantSquare = Vector2Int(move.endSquare.x, move.endSquare.y - forwardStep);
        gameState.movesSinceEnPassant = 0;
    }

    // capture detection happens BEFORE the moving piece overwrites the destination, so we can
    // still see what was there. en passant captures live on a different square from move.endSquare,
    // so capturedPieceSquare is tracked separately for correct undo.
    if (checkForEnPassantTake(gameState, move)) {
        // colour of the pawn to be taken will be the opposite of the current move colour
        // therefore we can get the forward direction of the other colour and use it to find the square with the pawn to be taken on it
        const auto enemyForwardStep = gameState.moveColour == Piece::Colour::WHITE ? 1 : -1;
        const auto capturedSquare = Vector2Int(gameState.enPassantSquare->x, gameState.enPassantSquare->y + enemyForwardStep);
        moveDelta.capturedPiece = gameState.boardPosition[capturedSquare.y][capturedSquare.x];
        moveDelta.capturedPieceSquare = capturedSquare;
        gameState.boardPosition[capturedSquare.y][capturedSquare.x] = std::nullopt;

        // XOR out the pawn on the captured square. read the colour from moveDelta.capturedPiece
        // (already saved above) rather than re-reading boardPosition - the square was just cleared,
        // so dereferencing the optional there would be UB.
        gameState.zobristHash ^= zobristHashKeys.boardHash[capturedSquare.y * 8 + capturedSquare.x][static_cast<int>(Piece::Type::PAWN)][moveDelta.capturedPiece->colour == Piece::Colour::WHITE ? 0 : 1];
    }
    else if (gameState.boardPosition[move.endSquare.y][move.endSquare.x]) {
        const auto capturedPiece = gameState.boardPosition[move.endSquare.y][move.endSquare.x];
        // regular capture - the piece on the end square will be overwritten when we place the moving piece there.
        // capturedPieceSquare is already move.endSquare from the default above.
        moveDelta.capturedPiece = capturedPiece;

        // XOR out the piece on the captured square
        gameState.zobristHash ^= zobristHashKeys.boardHash[move.endSquare.y * 8 + move.endSquare.x][static_cast<int>(capturedPiece->type)][capturedPiece->colour == Piece::Colour::WHITE ? 0 : 1];
    }

    // allow one move before enpassant is no longer available
    if (gameState.enPassantSquare) {
        if (gameState.movesSinceEnPassant == 0)
            ++gameState.movesSinceEnPassant;
        else {
            gameState.enPassantSquare = std::nullopt;
            gameState.movesSinceEnPassant = 0;
        }
    }

    // ----------------- castling --------------------

    // check for castle and if so move the rook on the board. checkForCastle returns CastleType::NOCASTLE
    // when the move isn't a castle, otherwise the specific WHITE/BLACK QUEENSIDE/KINGSIDE variant.
    if (const auto castleType = checkForCastle(gameState, move); castleType != GameTypes::CastleType::NOCASTLE) {
        castleRook(gameState, castleType);
        moveDelta.castleType = castleType;

        Vector2Int rookStartSquare{};
        Vector2Int rookEndSquare{};
        switch (castleType) {
        case GameTypes::CastleType::WHITEQUEENSIDE:
            rookStartSquare = whiteQueensideRookStartSquare;
            rookEndSquare = whiteQueensideRookEndSquare;
            break;
        case GameTypes::CastleType::WHITEKINGSIDE:
            rookStartSquare = whiteKingsideRookStartSquare;
            rookEndSquare = whiteKingsideRookEndSquare;
            break;
        case GameTypes::CastleType::BLACKQUEENSIDE:
            rookStartSquare = blackQueensideRookStartSquare;
            rookEndSquare = blackQueensideRookEndSquare;
            break;
        case GameTypes::CastleType::BLACKKINGSIDE:
            rookStartSquare = blackKingsideRookStartSquare;
            rookEndSquare = blackKingsideRookEndSquare;
            break;
        }

        auto rookColour = 0;
        if (castleType == GameTypes::CastleType::BLACKQUEENSIDE || castleType == GameTypes::CastleType::BLACKKINGSIDE)
            rookColour = 1;

        // XOR out the rook on its start square
        gameState.zobristHash ^= zobristHashKeys.boardHash[rookStartSquare.y * 8 + rookStartSquare.x][static_cast<int>(Piece::Type::ROOK)][rookColour];
        // XOR in the rook on its end square
        gameState.zobristHash ^= zobristHashKeys.boardHash[rookEndSquare.y * 8 + rookEndSquare.x][static_cast<int>(Piece::Type::ROOK)][rookColour];
    }

    // -------------------- update castling rights --------------------

    const auto endSquareHasPiece = gameState.boardPosition[move.endSquare.y][move.endSquare.x];

    struct SideCastlingData {
        // indices into gameState.castlingRights for this side's queenside and kingside flags.
        // ordering of castlingRights is {WQ, WK, BQ, BK}.
        int queensideIndex;
        int kingsideIndex;
        Vector2Int queensideRookStartSquare;
        Vector2Int kingsideRookStartSquare;
        Piece::Colour pieceColour;
    };

    constexpr std::array<SideCastlingData, 2> sides = {{
        {0, 1, whiteQueensideRookStartSquare, whiteKingsideRookStartSquare, Piece::Colour::WHITE},
        {2, 3, blackQueensideRookStartSquare, blackKingsideRookStartSquare, Piece::Colour::BLACK}
    }};

    // for both sides of the board (black and white) we check
    for (const auto& side : sides) {
        // initial check - no point doing lots of comparisons if that side has no castling rights
        if (!gameState.castlingRights[side.queensideIndex] && !gameState.castlingRights[side.kingsideIndex])
            continue;

        // if this side moved its king, or its rook, update the castling rights
        if (movePiece.colour == side.pieceColour) {
            if (movePiece.type == Piece::Type::KING) {
                gameState.castlingRights[side.queensideIndex] = false;
                gameState.castlingRights[side.kingsideIndex] = false;
            } else if (movePiece.type == Piece::Type::ROOK) {
                if (move.startSquare == side.queensideRookStartSquare)
                    gameState.castlingRights[side.queensideIndex] = false;
                if (move.startSquare == side.kingsideRookStartSquare)
                    gameState.castlingRights[side.kingsideIndex] = false;
            }
        }

        // if an enemy piece moved onto a rook start square on this side, the rook was taken (if it wasn't already), update castling rights
        if (endSquareHasPiece) {
            if (move.endSquare == side.queensideRookStartSquare)
                gameState.castlingRights[side.queensideIndex] = false;
            if (move.endSquare == side.kingsideRookStartSquare)
                gameState.castlingRights[side.kingsideIndex] = false;
        }
    }

    // XOR the castling-rights key for any flag that changed during this move. XOR is self-inverse,
    // so a single toggle correctly handles both true->false (right lost) and false->true (which
    // doesn't happen in normal play, but the code is symmetric and would do the right thing).
    for (int i = 0; i < 4; ++i) {
        if (moveDelta.previousCastlingRights[i] != gameState.castlingRights[i])
            gameState.zobristHash ^= zobristHashKeys.castlingRightsHash[i];
    }

    // ----------------- move the piece --------------------

    // XOR out the moving piece on the start square
    gameState.zobristHash ^= zobristHashKeys.boardHash[move.startSquare.y * 8 + move.startSquare.x][static_cast<int>(movePiece.type)][movePiece.colour == Piece::Colour::WHITE ? 0 : 1];

    // if a pawn is being promoted, place the requested promotion piece on the end square
    if (checkForPawnPromotionOnNextMove(gameState, move) && move.promotionPieceType) {
        gameState.boardPosition[move.endSquare.y][move.endSquare.x] = Piece(*move.promotionPieceType, movePiece.colour);
        moveDelta.wasPromotion = true;

        // XOR in the promoted piece on the end square
        gameState.zobristHash ^= zobristHashKeys.boardHash[move.endSquare.y * 8 + move.endSquare.x][static_cast<int>(*move.promotionPieceType)][movePiece.colour == Piece::Colour::WHITE ? 0 : 1];
    }
    // otherwise place the move piece on the end square, overwriting/capturing any enemy piece already there
    else {
        gameState.boardPosition[move.endSquare.y][move.endSquare.x] = movePiece;

        // XOR in the moving piece on the end square
        gameState.zobristHash ^= zobristHashKeys.boardHash[move.endSquare.y * 8 + move.endSquare.x][static_cast<int>(movePiece.type)][movePiece.colour == Piece::Colour::WHITE ? 0 : 1];
    }

    // remove the move piece from the start square
    gameState.boardPosition[move.startSquare.y][move.startSquare.x] = std::nullopt;
    // toggle move colour
    gameState.moveColour = gameState.moveColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE;
    // XOR the turn
    gameState.zobristHash ^= zobristHashKeys.turnHash;
    // XOR in the new en passant file if it is playable (must be done after the move colour has changed)
    if (isEnPassantPlayable(gameState))
        gameState.zobristHash ^= zobristHashKeys.enPassantFileHash[gameState.enPassantSquare->x];
    // update move counters
    ++gameState.halfMoveCounter;
    if (gameState.halfMoveCounter % 2 == 0)
        ++gameState.fullMoveCounter;

    return moveDelta;
}

void Game::undoLastMove(GameState& gameState, const MoveDelta& moveDelta) const {
    const auto& move = moveDelta.move;

    // toggle move colour back first so any colour-dependent restoration below sees the original mover's colour.
    gameState.moveColour = gameState.moveColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE;

    // reverse the move counters. fullMoveCounter was incremented if the post-move halfMoveCounter is even.
    if (gameState.halfMoveCounter % 2 == 0)
        --gameState.fullMoveCounter;
    --gameState.halfMoveCounter;

    // restore the piece on the start square. for promotions the piece on endSquare is the promoted piece, so put a pawn back instead of copying.
    if (moveDelta.wasPromotion)
        gameState.boardPosition[move.startSquare.y][move.startSquare.x] = Piece(Piece::Type::PAWN, gameState.moveColour);
    else
        gameState.boardPosition[move.startSquare.y][move.startSquare.x] = gameState.boardPosition[move.endSquare.y][move.endSquare.x];

    // clear the end square, for regular captures the captured-piece restore below will refill it.
    gameState.boardPosition[move.endSquare.y][move.endSquare.x] = std::nullopt;

    // restore captured piece. for regular captures capturedPieceSquare == endSquare, for en passant it's one rank away.
    if (moveDelta.capturedPiece)
        gameState.boardPosition[moveDelta.capturedPieceSquare.y][moveDelta.capturedPieceSquare.x] = moveDelta.capturedPiece;

    // undo the castling rook move - castleRook moved the rook from its corner to its castle-end square; reverse it.
    if (moveDelta.castleType != GameTypes::CastleType::NOCASTLE) {
        // {rookStartSquare, rookEndSquare} indexed parallel to GameTypes::CastleType (minus the NOCASTLE entry),
        // mirroring castleRook's rookMoves table.
        constexpr std::array<std::pair<Vector2Int, Vector2Int>, 4> castleRookEndpoints{{
            {whiteQueensideRookStartSquare, whiteQueensideRookEndSquare},
            {whiteKingsideRookStartSquare, whiteKingsideRookEndSquare},
            {blackQueensideRookStartSquare, blackQueensideRookEndSquare},
            {blackKingsideRookStartSquare, blackKingsideRookEndSquare}
        }};
        const auto rookArrayIndex = static_cast<int>(moveDelta.castleType) - 1;
        const auto& [rookStart, rookEnd] = castleRookEndpoints[rookArrayIndex];
        gameState.boardPosition[rookStart.y][rookStart.x] = gameState.boardPosition[rookEnd.y][rookEnd.x];
        gameState.boardPosition[rookEnd.y][rookEnd.x] = std::nullopt;
    }

    // restore enpassant and castling rights states.
    gameState.enPassantSquare = moveDelta.previousEnPassantSquare;
    gameState.movesSinceEnPassant = moveDelta.previousMovesSinceEnPassant;
    gameState.castlingRights = moveDelta.previousCastlingRights;
    gameState.zobristHash = moveDelta.previousZobristHash;
}

void Game::castleRook(GameState& gameState, const GameTypes::CastleType castleType) const {
    struct CastleRookData {
        Vector2Int startSquare;
        Vector2Int endSquare;
        Piece::Colour rookColour;
    };

    // moves the rook from its corner to the square next to where the king lands.
    // indexed parallel to GameTypes::CastleType so that static_cast<int>(castleType) - 1 selects the right entry.
    // (the - 1 is because NOCASTLE = 0 sits at the front of the enum and isn't represented in this table.)
    constexpr std::array rookMoves{
        CastleRookData{whiteQueensideRookStartSquare, whiteQueensideRookEndSquare, Piece::Colour::WHITE},
        CastleRookData{whiteKingsideRookStartSquare, whiteKingsideRookEndSquare, Piece::Colour::WHITE},
        CastleRookData{blackQueensideRookStartSquare, blackQueensideRookEndSquare, Piece::Colour::BLACK},
        CastleRookData{blackKingsideRookStartSquare, blackKingsideRookEndSquare, Piece::Colour::BLACK}
    };

    // NOCASTLE is the caller's responsibility to filter out before calling castleRook.
    const auto& rookData = rookMoves[static_cast<int>(castleType) - 1];

    gameState.boardPosition[rookData.startSquare.y][rookData.startSquare.x] = std::nullopt;
    gameState.boardPosition[rookData.endSquare.y][rookData.endSquare.x] = Piece(Piece::Type::ROOK, rookData.rookColour);
}

bool Game::isMoveValid(const GameState& gameState, const Move& move) const {
    // make sure start square contains a piece
    if (!gameState.boardPosition[move.startSquare.y][move.startSquare.x])
        return false;
    const auto movePiece = gameState.boardPosition[move.startSquare.y][move.startSquare.x].value();

    // check start and end square are in different locations and check the end position is on the board
    if (move.startSquare == move.endSquare || move.endSquare.x < 0 || move.endSquare.x > 7 || move.endSquare.y < 0 || move.endSquare.y > 7)
        return false;

    // check end square is either empty or contains an enemy piece.
    // kings are never capturable in chess; mate must be represented by "no legal replies while in check",
    // not by allowing a move onto the enemy king's square.
    if (gameState.boardPosition[move.endSquare.y][move.endSquare.x]) {
        if (gameState.boardPosition[move.endSquare.y][move.endSquare.x]->type == Piece::Type::KING)
            return false;
        if (gameState.boardPosition[move.endSquare.y][move.endSquare.x]->colour == movePiece.colour)
            return false;
    }

    const Vector2Int moveVector = {move.endSquare.x - move.startSquare.x, move.endSquare.y - move.startSquare.y};
    const auto absMoveVector = Vector2Int(std::abs(moveVector.x), std::abs(moveVector.y));

    switch (movePiece.type) {
        case Piece::Type::QUEEN:
            return isMovePathClearForSliders(gameState, move);
        case Piece::Type::ROOK:
            return (moveVector.x == 0 xor moveVector.y == 0) && isMovePathClearForSliders(gameState, move);
        case Piece::Type::BISHOP:
            return std::abs(moveVector.x) == std::abs(moveVector.y) ? isMovePathClearForSliders(gameState, move) : false;
        case Piece::Type::KNIGHT:
            return absMoveVector == Vector2Int(2, 1) || absMoveVector == Vector2Int(1, 2);
        case Piece::Type::KING:
            return isMoveValidForKing(gameState, move);
        case Piece::Type::PAWN:
            return isMoveValidForPawn(gameState, move);
    }
    return false;
}

bool Game::isMoveLegal(const GameState& gameState, const Move& move) const {
    if (!isMoveValid(gameState, move))
        return false;

    // simulate the board position if the move was to be made
    auto simulatedGameState = gameState;
    movePiece(simulatedGameState, move);

    // disallow moves that would leave the king in check
    if (isKingInCheck(simulatedGameState, gameState.moveColour))
        return false;

    // don't allow castling if the king is in check, or if it would move the king through squares under attack
    if (checkForCastle(gameState, move) != GameTypes::CastleType::NOCASTLE) {
        // get direction of move
        Vector2Int stepVector = {1, 0};
        if (move.endSquare.x - move.startSquare.x < 0)
            stepVector.x = -1;

        // check none of the squares in between the startSquare (inclusive) and the endSquare are under attack
        Vector2Int currentSquare = move.startSquare;
        while (currentSquare != move.endSquare) {
            if (isSquareUnderAttack(gameState, currentSquare, gameState.moveColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE, std::nullopt))
                return false;
            currentSquare += stepVector;
        }
    }
    return true;
}

bool Game::isMovePathClearForSliders(const GameState& gameState, const Move& move) const {
    const Vector2Int moveVector = {move.endSquare.x - move.startSquare.x, move.endSquare.y - move.startSquare.y};

    // check to make sure the move makes geometric sense for sliding pieces
    // i.e. is the end piece legally reachable from the start piece horizontally, vertically or diagonally
    if (moveVector.x != 0 && moveVector.y != 0 && std::abs(moveVector.x) != std::abs(moveVector.y))
        return false;

    // next we need to find out which one of the 8 possible directions that the move is going in. The direction of the vector can be computed by:
    // stepRow = sign(moveVector.y), stepColumn = sign(moveVector.x) where sign(x) is -1 if x < 0, 0 if x == 0, +1 if x > 0
    Vector2Int stepVector = {0, 0};
    if (moveVector.x > 0)
        stepVector.x += 1;
    else if (moveVector.x < 0)
        stepVector.x -= 1;

    if (moveVector.y > 0)
        stepVector.y += 1;
    else if (moveVector.y < 0)
        stepVector.y -= 1;

    // the step vector is then repeatedly added on from the start square until we reach the end square, if no piece is found while traversing then the move path is clear
    Vector2Int currentSquare = move.startSquare + stepVector;
    while (currentSquare != move.endSquare) {
        if (gameState.boardPosition[currentSquare.y][currentSquare.x])
            return false;
        currentSquare += stepVector;
    }
    return true;
}

bool Game::isMoveValidForKing(const GameState& gameState, const Move& move) const {
    const Vector2Int moveVector = {move.endSquare.x - move.startSquare.x, move.endSquare.y - move.startSquare.y};

    // normal king move
    if (const auto absMoveVector = Vector2Int(std::abs(moveVector.x), std::abs(moveVector.y)); std::max(absMoveVector.x, absMoveVector.y) == 1)
        return true;

    // castling
    return checkForCastle(gameState, move) != GameTypes::CastleType::NOCASTLE;
}

GameTypes::CastleType Game::checkForCastle(const GameState& gameState, const Move& move) const {
    using GameTypes::CastleType;

    // every castle is a king move from its home square, so bail early if the start square doesn't match.
    // this short-circuit also means every other check below is per-side rather than per-piece.
    const bool isWhite = gameState.moveColour == Piece::Colour::WHITE;
    const Vector2Int kingStartSquare = isWhite ? whiteKingStartSquare : blackKingStartSquare;
    if (move.startSquare != kingStartSquare)
        return CastleType::NOCASTLE;

    // group everything the per-castle check needs into one struct so the loop body stays uniform.
    // emptySquares is sized 3 because queenside checks 3 squares (b/c/d-file) and kingside checks 2 (f/g-file);
    // emptySquaresCount tells us how many entries are real for a given option.
    struct CastleOption {
        int rightsIndex;                            // index into gameState.castlingRights {WQ, WK, BQ, BK}
        Vector2Int kingTarget;                      // square the king must be moving to (c-file or g-file on the home row)
        Vector2Int rookStartSquare;                 // square the rook must still be sitting on
        std::array<Vector2Int, 3> emptySquares;     // squares between king and rook that must be empty
        int emptySquaresCount;                      // 3 for queenside, 2 for kingside (third slot is unused)
        CastleType resultIfLegal;                   // returned when all the conditions hold for this option
    };

    const int homeRow = isWhite ? 7 : 0;
    const std::array<CastleOption, 2> castleOptions = {{
        // queenside: king e -> c, rook a -> d. b/c/d-files between them must be empty.
        {
            isWhite ? 0 : 2,
            {2, homeRow},
            {0, homeRow},
            {{{1, homeRow}, {2, homeRow}, {3, homeRow}}},
            3,
            isWhite ? CastleType::WHITEQUEENSIDE : CastleType::BLACKQUEENSIDE
        },
        // kingside: king e -> g, rook h -> f. f/g-files between them must be empty.
        {
            isWhite ? 1 : 3,
            {6, homeRow},
            {7, homeRow},
            {{{5, homeRow}, {6, homeRow}, {0, 0}}},
            2,
            isWhite ? CastleType::WHITEKINGSIDE : CastleType::BLACKKINGSIDE
        }
    }};

    const auto rookColour = isWhite ? Piece::Colour::WHITE : Piece::Colour::BLACK;

    for (const auto& option : castleOptions) {
        // 1. the side must still have castling rights for this castle, and the king must be heading to the right square.
        if (!gameState.castlingRights[option.rightsIndex] || move.endSquare != option.kingTarget)
            continue;

        // 2. the rook must still be sitting on its starting square. updateCastlingRights would normally clear
        //    the rights when a rook moves or is captured, but the board check is a safety net.
        const auto& rookSquare = gameState.boardPosition[option.rookStartSquare.y][option.rookStartSquare.x];
        if (!rookSquare || rookSquare->type != Piece::Type::ROOK || rookSquare->colour != rookColour)
            continue;

        // 3. all squares between king and rook must be empty.
        bool pathClear = true;
        for (int i = 0; i < option.emptySquaresCount; ++i) {
            const auto& square = option.emptySquares[i];
            if (gameState.boardPosition[square.y][square.x]) {
                pathClear = false;
                break;
            }
        }

        // note: this function does NOT check whether the king starts in check or passes through attacked squares.
        // those are legality concerns handled in isMoveLegal, not validity concerns handled here.
        if (pathClear)
            return option.resultIfLegal;
    }

    return CastleType::NOCASTLE;
}

bool Game::isMoveValidForPawn(const GameState& gameState, const Move& move) const {
    if (!gameState.boardPosition[move.startSquare.y][move.startSquare.x])
        return false;
    // make sure the end square is on the board
    if (move.endSquare.x < 0 || move.endSquare.x > 7 || move.endSquare.y < 0 || move.endSquare.y > 7)
        return false;
    const auto movePiece = gameState.boardPosition[move.startSquare.y][move.startSquare.x].value();
    const Vector2Int moveVector = {move.endSquare.x - move.startSquare.x, move.endSquare.y - move.startSquare.y};

    if (moveVector.y == 0 || std::abs(moveVector.x) > 1 || std::abs(moveVector.y) > 2)
        return false;

    // moving one step forward for black is equivalent to the vector (0, 1), while for white it is (0, -1)
    // similarly the pawn starting row for black is y = 1, while for white it is y = 6
    int forwardStep = 1;
    bool enemyOnEndSquare = false;

    if (movePiece.colour == Piece::Colour::WHITE)
        forwardStep = -1;

    // already checked for friendly pieces on end square when doing universal checks so if there is a piece it has to be an enemy
    if (gameState.boardPosition[move.endSquare.y][move.endSquare.x])
        enemyOnEndSquare = true;

    // single push forward
    // requires no horizontal movement, 1 forward step of vertical movement and no enemy on the destination square
    if (moveVector.x == 0 && moveVector.y == forwardStep && !enemyOnEndSquare)
        return true;

    // double push forward
    if (checkForPawnDoublePush(gameState, move))
        return true;

    // taking an enemy piece normally
    // requires there to be an enemy piece on the end square, 1 unit of horizontal movement and 1 forward step of vertical movement
    if (gameState.boardPosition[move.endSquare.y][move.endSquare.x]) {
        if (std::abs(moveVector.x) == 1 && moveVector.y == forwardStep)
            return true;
    }

    // en passant
    if (checkForEnPassantTake(gameState, move))
        return true;
    return false;
}

bool Game::checkForPawnDoublePush(const GameState& gameState, const Move& move) const {
    if (!gameState.boardPosition[move.startSquare.y][move.startSquare.x])
        return false;
    const auto movePiece = *gameState.boardPosition[move.startSquare.y][move.startSquare.x];
    if (movePiece.type != Piece::Type::PAWN || movePiece.colour != gameState.moveColour)
        return false;

    const Vector2Int moveVector = {move.endSquare.x - move.startSquare.x, move.endSquare.y - move.startSquare.y};
    int forwardStep = 1;
    int startingRow = 1;
    bool enemyOnEndSquare = false;
    if (gameState.moveColour == Piece::Colour::WHITE) {
        forwardStep = -1;
        startingRow = 6;
    }
    if (gameState.boardPosition[move.endSquare.y][move.endSquare.x])
        enemyOnEndSquare = true;

    // double push forward
    // requires no horizontal movement, 2 forward steps of vertical movement, pawn must be on the starting row, there must not be an enemy on the destination square
    // and there must not a piece on the intermediate square between the start and end square
    return moveVector.x == 0 && moveVector.y == (forwardStep * 2) && move.startSquare.y == startingRow && !enemyOnEndSquare && isMovePathClearForSliders(gameState, move);
}

bool Game::checkForEnPassantTake(const GameState& gameState, const Move& move) const {
    if (!gameState.enPassantSquare)
        return false;
    if (!gameState.boardPosition[move.startSquare.y][move.startSquare.x])
        return false;
    const auto movePiece = *gameState.boardPosition[move.startSquare.y][move.startSquare.x];
    if (movePiece.type != Piece::Type::PAWN || movePiece.colour != gameState.moveColour)
        return false;

    const auto enemyForwardStep = gameState.moveColour == Piece::Colour::WHITE ? 1 : -1;
    const auto enPassantPawnSquare = Vector2Int(gameState.enPassantSquare.value().x, gameState.enPassantSquare.value().y + enemyForwardStep);
    // check that the pawn is trying to move to the enpassant square and then check that the pawn is directly next to the enpassant pawn (the pawn that just moved 2 spaces last turn)
    if (move.endSquare == gameState.enPassantSquare && std::abs(move.startSquare.x - enPassantPawnSquare.x) == 1 && move.startSquare.y == enPassantPawnSquare.y) {
        // check to make sure the enpassant pawn square does have a piece on it
        if (gameState.boardPosition[enPassantPawnSquare.y][enPassantPawnSquare.x]) {
            // if that piece is a pawn of the opposite colour then an enpassant take can be made
            if (gameState.boardPosition[enPassantPawnSquare.y][enPassantPawnSquare.x]->type == Piece::Type::PAWN && gameState.boardPosition[enPassantPawnSquare.y][enPassantPawnSquare.x]->colour != gameState.moveColour)
                return true;
        }
    }
    return false;
}

bool Game::isSquareUnderAttack(const GameState& gameState, const Vector2Int square, const Piece::Colour enemyColour, const std::optional<Vector2Int> ignoredSquare) const {
    // -------------------- check for pawn attack --------------------

    if (isSquareUnderAttackByPawn(gameState, square, enemyColour))
        return true;

    // -------------------- check for knight attack --------------------

    for (const auto& vector : knightVectors) {
        const auto& possibleKnightSquare = square + vector;
        // make sure the square being checked is on the board
        if (possibleKnightSquare.x < 0 || possibleKnightSquare.x > 7 || possibleKnightSquare.y < 0 || possibleKnightSquare.y > 7)
            continue;

        // if square has a piece on it
        if (gameState.boardPosition[possibleKnightSquare.y][possibleKnightSquare.x]) {
            // if that piece is an enemy knight
            if (const auto piece = *gameState.boardPosition[possibleKnightSquare.y][possibleKnightSquare.x]; piece.colour == enemyColour && piece.type == Piece::Type::KNIGHT)
                return true;
        }
    }

    // -------------------- check for king attack --------------------

    for (const auto& direction : pieceDirections) {
        const auto& possibleKingSquare = square + direction;
        // make sure the square being checked is on the board
        if (possibleKingSquare.x < 0 || possibleKingSquare.x > 7 || possibleKingSquare.y < 0 || possibleKingSquare.y > 7)
            continue;

        // if square has a piece on it
        if (gameState.boardPosition[possibleKingSquare.y][possibleKingSquare.x]) {
            // if that piece is an enemy king
            if (const auto piece = *gameState.boardPosition[possibleKingSquare.y][possibleKingSquare.x]; piece.colour == enemyColour && piece.type == Piece::Type::KING)
                return true;
        }
    }

    // -------------------- check for sliders attack --------------------

    for (const auto& direction : pieceDirections) {
        auto currentSquare = square + direction;
        while (currentSquare.x >= 0 && currentSquare.x < 8 && currentSquare.y >= 0 && currentSquare.y < 8) {
            // a piece on the current square blocks further attackers along this ray, UNLESS it's the
            // ignored square - in which case we walk past it as if empty. this is used to discount the
            // king's own square when checking whether a destination square would be attacked after the king moves.
            // important: the "skip past ignored" path must fall through to currentSquare += direction below;
            // a continue here would loop forever because the same square would keep matching "has a piece".
            if (gameState.boardPosition[currentSquare.y][currentSquare.x] && currentSquare != ignoredSquare) {
                if (const auto piece = *gameState.boardPosition[currentSquare.y][currentSquare.x]; piece.colour == enemyColour) {
                    if (piece.type == Piece::Type::QUEEN)
                        return true;
                    // find out if we travelled straight or diagonally to get here, the calculation will result in 1 == straight movement, 2 = diagonal movement
                    const bool movedStraight = std::abs(direction.x) + std::abs(direction.y) == 1;
                    if (piece.type == Piece::Type::ROOK && movedStraight)
                        return true;
                    if (piece.type == Piece::Type::BISHOP && !movedStraight)
                        return true;
                }
                break;
            }
            currentSquare += direction;
        }
    }
    return false;
}

bool Game::isSquareUnderAttackByPawn(const GameState& gameState, const Vector2Int square, const Piece::Colour enemyColour) const {
    // get forward step of friendly colour
    int forwardStep = 1;
    if (enemyColour == Piece::Colour::BLACK)
        forwardStep = -1;

    // check for enemy pawns on the two diagonally forward squares
    if (const auto squareOne = Vector2Int(square.x + 1, square.y + forwardStep); squareOne.x < 8 && squareOne.y >= 0 && squareOne.y < 8) {
        if (gameState.boardPosition[squareOne.y][squareOne.x]) {
            if (gameState.boardPosition[squareOne.y][squareOne.x]->colour == enemyColour && gameState.boardPosition[squareOne.y][squareOne.x]->type == Piece::Type::PAWN)
                return true;
        }
    }
    if (const auto squareTwo = Vector2Int(square.x - 1, square.y + forwardStep); squareTwo.x >= 0 && squareTwo.y >= 0 && squareTwo.y < 8) {
        if (gameState.boardPosition[squareTwo.y][squareTwo.x]) {
            if (gameState.boardPosition[squareTwo.y][squareTwo.x]->colour == enemyColour && gameState.boardPosition[squareTwo.y][squareTwo.x]->type == Piece::Type::PAWN)
                return true;
        }
    }
    return false;
}

bool Game::isKingInCheck(const GameState& gameState, const Piece::Colour kingColour) const {
    // find the king
    for (auto rank = 0; rank < 8; ++rank) {
        for (auto file = 0; file < 8; ++file) {
            if (gameState.boardPosition[rank][file]) {
                if (gameState.boardPosition[rank][file]->colour == kingColour && gameState.boardPosition[rank][file]->type == Piece::Type::KING) {
                    // check whether the king is currently under attack
                    const auto enemyColour = kingColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE;
                    return isSquareUnderAttack(gameState, Vector2Int(file, rank), enemyColour, std::nullopt);
                }
            }
        }
    }
    return false;
}

bool Game::checkForPawnPromotionOnLastMove(const GameState& gameState) const {
    for (auto rank = 0; rank < 8; rank += 7) {
        for (auto file = 0; file < 8; ++file) {
            if (gameState.boardPosition[rank][file]) {
                if (gameState.boardPosition[rank][file]->type == Piece::Type::PAWN) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool Game::checkForPawnPromotionOnNextMove(const GameState& gameState, const Move& move) const {
    const auto& piece = gameState.boardPosition[move.startSquare.y][move.startSquare.x];
    if (piece->type != Piece::Type::PAWN)
        return false;
    if (piece->colour == Piece::Colour::WHITE)
        return move.endSquare.y == 0;
    return move.endSquare.y == 7;
}

struct PinInfo
{
    Vector2Int pinSquare;
    Vector2Int pinDirection;

    PinInfo(const Vector2Int pinSquare, const Vector2Int pinDirection) : pinSquare(pinSquare), pinDirection(pinDirection) {}
};

struct CheckerInfo
{
    int numCheckers = 0;
    std::optional<Vector2Int> checkerSquare;
    std::array<bool, 64> checkRay{};
};

std::vector<Move> Game::generateAllLegalMoves(const GameState& gameState, bool capturesOnly) const {
    Vector2Int kingSquare{};
    std::array<std::optional<PinInfo>, 8> pins;

    // find the king square
    for (auto rank = 0; rank < 8; ++rank) {
        for (auto file = 0; file < 8; ++file) {
            if (gameState.boardPosition[rank][file] && gameState.boardPosition[rank][file]->type == Piece::Type::KING && gameState.boardPosition[rank][file]->colour == gameState.moveColour)
                kingSquare = Vector2Int(file, rank);
        }
    }

    // find all the pinned pieces by raycasting from the king square
    for (auto i = 0; i < 8; ++i) {
        const auto direction = pieceDirections[i];
        auto currentSquare = kingSquare + direction;
        std::optional<Vector2Int> candidatePin;
        // while we are still on the board
        while (currentSquare.x >= 0 && currentSquare.x < 8 && currentSquare.y >= 0 && currentSquare.y < 8) {
            // if there is a piece on the current square
            if (gameState.boardPosition[currentSquare.y][currentSquare.x]) {
                // if the current square contains a friendly piece
                if (gameState.boardPosition[currentSquare.y][currentSquare.x]->colour == gameState.moveColour) {
                    // second friendly piece in a row found, no pin
                    if (candidatePin)
                        break;
                    // the piece could be pinned, we need to keep walking the board in this direction to find out
                    candidatePin = Vector2Int(currentSquare.x, currentSquare.y);
                }
                // the current square contains an enemy piece
                else {
                    if (candidatePin) {
                        // find out if we travelled straight or diagonally to get here, the calculation will result in 1 == straight movement, 2 = diagonal movement
                        // aka straight movement = true, diagonal movement = false
                        const auto movedStraight = std::abs(direction.x) + std::abs(direction.y) == 1;

                        // check if the enemy piece is a slider attacking in the direction of the king
                        if (gameState.boardPosition[currentSquare.y][currentSquare.x]->type == Piece::Type::QUEEN ||
                            (gameState.boardPosition[currentSquare.y][currentSquare.x]->type == Piece::Type::ROOK && movedStraight) ||
                            (gameState.boardPosition[currentSquare.y][currentSquare.x]->type == Piece::Type::BISHOP && !movedStraight)) {
                            // pin found
                            pins[i] = PinInfo(*candidatePin, direction);
                            break;
                        }
                    }
                    // no pin because either no friendly piece between the enemy piece and the king
                    // or the enemy piece was not a slider attacking in the direction of the king
                    break;
                }
            }
            currentSquare += direction;
        }
    }

    // check evasion
    CheckerInfo checkerInfo;

    // pawns
    // get forward step of friendly king
    auto forwardStep = 1;
    if (gameState.moveColour == Piece::Colour::WHITE)
        forwardStep = -1;

    // check for enemy pawns on the two diagonally forward squares
    for (auto i = -1; i < 2; i += 2) {
        const auto candidateSquare = Vector2Int(kingSquare.x + i, kingSquare.y + forwardStep);
        if (candidateSquare.x >= 0 && candidateSquare.x < 8 && candidateSquare.y >= 0 && candidateSquare.y < 8) {
            if (gameState.boardPosition[candidateSquare.y][candidateSquare.x]) {
                if (gameState.boardPosition[candidateSquare.y][candidateSquare.x]->type == Piece::Type::PAWN && gameState.boardPosition[candidateSquare.y][candidateSquare.x]->colour != gameState.moveColour) {
                    if (++checkerInfo.numCheckers == 2)
                        goto twoCheckersFound;
                    checkerInfo.checkerSquare = candidateSquare;
                }
            }
        }
    }
    // knights
    for (const auto& knightVector : knightVectors) {
        const auto& candidateSquare = kingSquare + knightVector;
        // make sure the square being checked is on the board
        if (candidateSquare.x < 0 || candidateSquare.x > 7 || candidateSquare.y < 0 || candidateSquare.y > 7)
            continue;

        if (gameState.boardPosition[candidateSquare.y][candidateSquare.x]) {
            if (gameState.boardPosition[candidateSquare.y][candidateSquare.x]->type == Piece::Type::KNIGHT && gameState.boardPosition[candidateSquare.y][candidateSquare.x]->colour != gameState.moveColour) {
                if (++checkerInfo.numCheckers == 2)
                    goto twoCheckersFound;
                checkerInfo.checkerSquare = candidateSquare;
            }
        }
    }
    // sliders
    for (const auto& direction : pieceDirections) {
        auto currentSquare = kingSquare + direction;
        std::vector<int> checkRayBuffer;
        while (currentSquare.x >= 0 && currentSquare.x < 8 && currentSquare.y >= 0 && currentSquare.y < 8) {
            if (gameState.boardPosition[currentSquare.y][currentSquare.x]) {
                if (const auto piece = *gameState.boardPosition[currentSquare.y][currentSquare.x]; piece.colour != gameState.moveColour) {
                    // find out if we travelled straight or diagonally to get here, the calculation will result in 1 == straight movement, 2 = diagonal movement
                    const bool movedStraight = std::abs(direction.x) + std::abs(direction.y) == 1;
                    if (piece.type == Piece::Type::QUEEN || (piece.type == Piece::Type::ROOK && movedStraight) || (piece.type == Piece::Type::BISHOP && !movedStraight)) {
                        if (++checkerInfo.numCheckers == 2)
                            goto twoCheckersFound;
                        checkerInfo.checkerSquare = currentSquare;
                        for (const auto& squareIndex : checkRayBuffer)
                            checkerInfo.checkRay[squareIndex] = true;
                    }
                }
                // friendly piece found
                break;
            }
            // no piece on this square
            checkRayBuffer.emplace_back(currentSquare.y * 8 + currentSquare.x);
            currentSquare += direction;
        }
    }

    // label to skip unnecessary computation if two checkers are found
    twoCheckersFound:

    std::array<bool, 64> allowedDestinations{};
    if (checkerInfo.numCheckers == 0)
        // no check, every destination allowed
        allowedDestinations.fill(true);
    else if (checkerInfo.numCheckers == 1) {
        // in case a double pushed pawn that can be taken by enpassant is the only checker
        if (gameState.enPassantSquare) {
            const int enemyForwardStep = gameState.moveColour == Piece::Colour::WHITE ? 1 : -1;
            const Vector2Int enPassantPawnSquare(gameState.enPassantSquare->x,gameState.enPassantSquare->y + enemyForwardStep);
            if (enPassantPawnSquare == checkerInfo.checkerSquare)
                allowedDestinations[gameState.enPassantSquare->y * 8 + gameState.enPassantSquare->x] = true;
        }
        // single check, only moves that capture the checker or block the check ray are allowed
        // checking piece
        allowedDestinations[checkerInfo.checkerSquare->y * 8 + checkerInfo.checkerSquare->x] = true;
        // checking ray
        for (auto i = 0; i < 64; ++i) {
            if (checkerInfo.checkRay[i])
                allowedDestinations[i] = true;
        }
    }
    // else two or more checkers, only king moves that move out of check are allowed, mask stays all false

    std::vector<Move> moves;
    for (auto rank = 0; rank < 8; ++rank) {
        for (auto file = 0; file < 8; ++file) {
            if (gameState.boardPosition[rank][file] && gameState.boardPosition[rank][file]->colour == gameState.moveColour) {
                const auto moveStartSquare = Vector2Int(file, rank);

                // in double check, only king moves can be legal - skip all other pieces' generation
                if (checkerInfo.numCheckers >= 2 && gameState.boardPosition[rank][file]->type != Piece::Type::KING)
                    continue;

                // check whether this piece is pinned and get the pin direction if so
                auto pinned = false;
                Vector2Int pinDirection{};
                for (const auto& pin : pins) {
                    if (pin && pin->pinSquare == moveStartSquare) {
                        pinned = true;
                        pinDirection = pin->pinDirection;
                        break;
                    }
                }

                auto addSliderMovesAlongRayIfLegal = [&](const Vector2Int direction) {
                    // use cross product of 2d vectors to ensure move direction is parallel to the pin direction
                    if (pinned && direction.x * pinDirection.y != direction.y * pinDirection.x)
                        return;
                    auto currentSquare = Vector2Int(file + direction.x, rank + direction.y);
                    while (currentSquare.x >= 0 && currentSquare.x < 8 && currentSquare.y >= 0 && currentSquare.y < 8) {
                        const Move move(moveStartSquare, currentSquare);
                        if (!isMoveValid(gameState, move))
                            break;
                        if (!allowedDestinations[currentSquare.y * 8 + currentSquare.x]) {
                            currentSquare += direction;
                            continue;
                        }
                        moves.emplace_back(move);
                        // stop when we hit any piece (capture or friendly blocker)
                        if (gameState.boardPosition[currentSquare.y][currentSquare.x])
                            break;
                        currentSquare += direction;
                    }
                };

                // generate all possible moves for each piece, accounting for the piece being pinned if applicable
                switch (gameState.boardPosition[rank][file]->type) {
                case Piece::Type::PAWN:
                    {
                        const int forwardStep = (gameState.moveColour == Piece::Colour::WHITE) ? -1 : 1;

                        auto addPawnMoveIfLegal = [&](const int dx, const int dy)
                        {
                            // use cross product of 2d vectors to ensure move direction is parallel to the pin direction
                            if (pinned && dx * pinDirection.y != dy * pinDirection.x)
                                return;
                            const int destX = file + dx;
                            const int destY = rank + dy;
                            if (destX < 0 || destX > 7 || destY < 0 || destY > 7)
                                return;
                            const Move move(moveStartSquare, Vector2Int(destX, destY));
                            if (!allowedDestinations[move.endSquare.y * 8 + move.endSquare.x])
                                return;
                            if (isMoveValid(gameState, move)) {
                                if (gameState.enPassantSquare && move.endSquare == *gameState.enPassantSquare) {
                                    // EP can expose a horizontal discovered check that the pin table missed,
                                    // so fall back to a full simulation just for this one case.
                                    GameState simulated = gameState;
                                    movePiece(simulated, move);
                                    if (isKingInCheck(simulated, gameState.moveColour))
                                        // illegal, would expose king
                                        return;
                                }
                                moves.emplace_back(move);
                            }
                        };

                        // single push
                        addPawnMoveIfLegal(0,  forwardStep);
                        // double push
                        addPawnMoveIfLegal(0,  2 * forwardStep);
                        // diagonal forward-right capture
                        addPawnMoveIfLegal(1,  forwardStep);
                        // diagonal forward-left capture
                        addPawnMoveIfLegal(-1, forwardStep);
                        break;
                    }
                case Piece::Type::KNIGHT:
                    // knights cannot move if pinned
                    if (pinned)
                        break;
                    for (const auto& knightVector : knightVectors) {
                        const auto move = Move(moveStartSquare, moveStartSquare + knightVector);
                        if (move.endSquare.x < 0 || move.endSquare.x >= 8 || move.endSquare.y < 0 || move.endSquare.y >= 8)
                            continue;
                        if (!allowedDestinations[move.endSquare.y * 8 + move.endSquare.x])
                            continue;
                        if (isMoveValid(gameState, move))
                            moves.emplace_back(move);
                    }
                    break;
                case Piece::Type::BISHOP:
                    for (auto i = 4; i < 8; ++i)
                        addSliderMovesAlongRayIfLegal(pieceDirections[i]);
                    break;
                case Piece::Type::ROOK:
                    for (auto i = 0; i < 4; ++i)
                        addSliderMovesAlongRayIfLegal(pieceDirections[i]);
                    break;
                case Piece::Type::QUEEN:
                    for (auto i = 0; i < 8; ++i)
                        addSliderMovesAlongRayIfLegal(pieceDirections[i]);
                    break;
                case Piece::Type::KING:
                    // ordinary moves
                    for (const auto& direction : pieceDirections) {
                        const auto move = Move(moveStartSquare, Vector2Int(file + direction.x, rank + direction.y));
                        if (isMoveValid(gameState, move) && !isSquareUnderAttack(gameState, move.endSquare, gameState.moveColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE, move.startSquare))
                            moves.emplace_back(move);
                    }
                    // cannot castle if the king is in check
                    if (checkerInfo.numCheckers > 0)
                        break;
                    // castling moves - try both queenside (king to c-file = 2) and kingside (king to g-file = 6).
                    // checkForCastle handles the structural conditions (castling rights still held, rook still on its
                    // starting square, path between king and rook empty). this block layers in the attack-safety
                    // conditions: the king must not start in check, must not pass through an attacked square, and
                    // must not end on an attacked square. kingSquare is passed to isSquareUnderAttack as the
                    // ignore-square hint so sliders can find the king's path through the now-vacated start square.
                    const auto enemyColour = gameState.moveColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE;
                    for (const int destinationFile : {2, 6}) {
                        const auto castleMove = Move(moveStartSquare, Vector2Int(destinationFile, kingSquare.y));
                        if (checkForCastle(gameState, castleMove) == GameTypes::CastleType::NOCASTLE)
                            continue;

                        // walk every square the king travels through, inclusive of both endpoints. for kingside
                        // (e -> g) that's e/f/g; for queenside (e -> c) that's e/d/c. note: the b-file square on
                        // queenside is NOT checked here - the rook crosses it but rooks may safely pass through
                        // attacked squares while castling, only the king's path matters.
                        const int step = destinationFile > kingSquare.x ? 1 : -1;
                        bool pathSafe = true;
                        for (int currentFile = kingSquare.x; ; currentFile += step) {
                            if (isSquareUnderAttack(gameState, Vector2Int(currentFile, kingSquare.y), enemyColour, kingSquare)) {
                                pathSafe = false;
                                break;
                            }
                            if (currentFile == destinationFile)
                                break;
                        }

                        if (pathSafe)
                            moves.emplace_back(castleMove);
                    }
                    break;
                }
            }
        }
    }
    if (capturesOnly) {
        std::vector<Move> captures;
        for (const auto& move : moves) {
            if (gameState.boardPosition[move.endSquare.y][move.endSquare.x] || checkForEnPassantTake(gameState, move))
                captures.push_back(move);
        }
        return captures;
    }
    return moves;
}

uint64_t Game::generateZobristHash(const GameState& gameState) const {
    uint64_t hash = 0;
    for (auto rank = 0; rank < 8; ++rank) {
        for (auto file = 0; file < 8; ++file) {
            if (gameState.boardPosition[rank][file])
                hash ^= zobristHashKeys.boardHash[rank * 8 + file][static_cast<int>(gameState.boardPosition[rank][file]->type)][gameState.boardPosition[rank][file]->colour == Piece::Colour::WHITE ? 0 : 1];
        }
    }

    if (isEnPassantPlayable(gameState))
        hash ^= zobristHashKeys.enPassantFileHash[gameState.enPassantSquare->x];

    for (auto i = 0; i < 4; ++i) {
        if (gameState.castlingRights[i])
            hash ^= zobristHashKeys.castlingRightsHash[i];
    }

    if (gameState.moveColour == Piece::Colour::BLACK)
        hash ^= zobristHashKeys.turnHash;

    return hash;
}

/* Is there at least one pawn directly to the side of the pawn that has just double pushed and is it/are they the opposite colour of that double pushed pawn? */
bool Game::isEnPassantPlayable(const GameState& gameState) const {
    if (gameState.enPassantSquare) {
        const int enemyForwardStep = gameState.moveColour == Piece::Colour::WHITE ? 1 : -1;
        const int captureRank = gameState.enPassantSquare->y + enemyForwardStep;
        const int enPassantFile = gameState.enPassantSquare->x;

        const auto hasFriendlyPawnAt = [&](const int file) {
            if (file < 0 || file > 7)
                return false;
            const auto& square = gameState.boardPosition[captureRank][file];
            return square && square->type == Piece::Type::PAWN && square->colour == gameState.moveColour;
        };

        if (hasFriendlyPawnAt(enPassantFile - 1) || hasFriendlyPawnAt(enPassantFile + 1))
            return true;
    }
    return false;
}
