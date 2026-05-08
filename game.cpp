#include "game.h"
#include <algorithm>
#include <iostream>
#include <ranges>

std::vector<Vector2Int> Game::generateLegalMovesForSquare(const GameState& gameState,  const Vector2Int startSquare) const {
    std::vector<Vector2Int> validMovableSquares;
    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            if (const auto move = Move(startSquare, Vector2Int(file, rank)); checkIsMoveLegal(gameState, move))
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

    gameState.whiteCastleRights = {false, false};
    gameState.blackCastleRights = {false, false};
    if (fenTokens[2] != "-") {
        for (const auto& character : fenTokens[2]) {
            switch (character) {
                case 'Q': gameState.whiteCastleRights[0] = true; break;
                case 'K': gameState.whiteCastleRights[1] = true; break;
                case 'q': gameState.blackCastleRights[0] = true; break;
                case 'k': gameState.blackCastleRights[1] = true; break;
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
    if (checkIsMoveLegal(gameState, move)) {
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
        if (moveDelta->castleRookIndex > 0)
            moveType = GameTypes::MoveType::CASTLE;
        else if (moveDelta->wasPromotion)
            moveType = GameTypes::MoveType::PROMOTEPAWN;
        else if (moveDelta->capturedPiece)
            moveType = GameTypes::MoveType::CAPTURE;
        else
            moveType = GameTypes::MoveType::MOVESELF;
    }

    if (checkIsKingInCheck(gameState, gameState.moveColour))
        moveType = GameTypes::MoveType::CHECK;

    // check for stalemate and checkmate
    if (generateAllLegalMoves(gameState).empty()) {
        moveType = GameTypes::MoveType::GAMEOVER;
        if (checkIsKingInCheck(gameState, gameState.moveColour)) {
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
    const bool wasCapture = moveDelta && moveDelta->capturedPiece.has_value();
    const bool wasPawnMove = moveDelta && (moveDelta->wasPromotion
                                           || (endSquarePiece.has_value() && endSquarePiece->type == Piece::Type::PAWN));
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
    // capturedPiece is filled in by the EP / regular-capture branches below.
    MoveDelta moveDelta;
    moveDelta.move = move;
    moveDelta.previousEnPassantSquare = gameState.enPassantSquare;
    moveDelta.previousMovesSinceEnPassant = gameState.movesSinceEnPassant;
    moveDelta.previousWhiteCastleRights = gameState.whiteCastleRights;
    moveDelta.previousBlackCastleRights = gameState.blackCastleRights;
    // en passant can override this below
    moveDelta.capturedPieceSquare = move.endSquare;

    // ---------- en passant ---------------

    // check for pawn double push and record the intermediate square it skipped over (enPassantSquare) and the square it is now on (enPassantPawnSquare)
    if (checkForPawnDoublePush(gameState, move)) {
        const int forwardStep = (movePiece.colour == Piece::Colour::WHITE) ? -1 : 1;
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
    }
    else if (gameState.boardPosition[move.endSquare.y][move.endSquare.x]) {
        // regular capture - the piece on the end square will be overwritten when we place the moving piece there.
        // capturedPieceSquare is already move.endSquare from the default above.
        moveDelta.capturedPiece = gameState.boardPosition[move.endSquare.y][move.endSquare.x];
    }

    // allow one move before en passant is no longer available
    if (gameState.enPassantSquare) {
        if (gameState.movesSinceEnPassant == 0)
            ++gameState.movesSinceEnPassant;
        else {
            gameState.enPassantSquare = std::nullopt;
            gameState.movesSinceEnPassant = 0;
        }
    }

    // ----------------- castling --------------------

    // check for castle and if returns true move the rook on the board
    // 0 = no rook needs castling, 1 = white queenside rook, 2 = white kingside rook, 3 = black queenside rook, 4 = black kingside rook
    if (const auto rookIndex = checkForCastle(gameState, move); rookIndex > 0) {
        castleRook(gameState, rookIndex);
        moveDelta.castleRookIndex = rookIndex;
    }
    updateCastlingRights(gameState, move);

    // ----------------- moving the piece --------------------

    // if a pawn is being promoted, place the requested promotion piece on the end square
    if (checkForPawnPromotionOnNextMove(gameState, move) && move.promotionPieceType) {
        gameState.boardPosition[move.endSquare.y][move.endSquare.x] = Piece(*move.promotionPieceType, movePiece.colour);
        moveDelta.wasPromotion = true;
    }
    // otherwise place the move piece on the end square, overwriting/capturing any enemy piece already there
    else
        gameState.boardPosition[move.endSquare.y][move.endSquare.x] = movePiece;

    // remove the move piece from the start square
    gameState.boardPosition[move.startSquare.y][move.startSquare.x] = std::nullopt;
    // toggle move colour
    gameState.moveColour = gameState.moveColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE;
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
    if (moveDelta.castleRookIndex > 0) {
        // {rookStartSquare, rookEndSquare} keyed by castleRookIndex - 1, mirroring castleRook's table.
        constexpr std::array<std::pair<Vector2Int, Vector2Int>, 4> castleRookEndpoints{{
            {Vector2Int{0, 7}, Vector2Int{3, 7}},  // white queenside
            {Vector2Int{7, 7}, Vector2Int{5, 7}},  // white kingside
            {Vector2Int{0, 0}, Vector2Int{3, 0}},  // black queenside
            {Vector2Int{7, 0}, Vector2Int{5, 0}}   // black kingside
        }};
        const auto rookArrayIndex = moveDelta.castleRookIndex - 1;
        const auto& [rookStart, rookEnd] = castleRookEndpoints[rookArrayIndex];
        gameState.boardPosition[rookStart.y][rookStart.x] = gameState.boardPosition[rookEnd.y][rookEnd.x];
        gameState.boardPosition[rookEnd.y][rookEnd.x] = std::nullopt;
    }

    // restore enpassant and castling rights states.
    gameState.enPassantSquare = moveDelta.previousEnPassantSquare;
    gameState.movesSinceEnPassant = moveDelta.previousMovesSinceEnPassant;
    gameState.whiteCastleRights = moveDelta.previousWhiteCastleRights;
    gameState.blackCastleRights = moveDelta.previousBlackCastleRights;
}

void Game::updateCastlingRights(GameState& gameState, const Move &move) const {
    if (!gameState.boardPosition[move.startSquare.y][move.startSquare.x])
        return;
    const auto movePiece = gameState.boardPosition[move.startSquare.y][move.startSquare.x].value();
    const auto endSquareHasPiece = gameState.boardPosition[move.endSquare.y][move.endSquare.x].has_value();

    struct SideCastlingData {
        // [0]=queenside, [1]=kingside
        std::array<bool, 2>& castleRights;
        Vector2Int queensideRookStartSquare;
        Vector2Int kingsideRookStartSquare;
        Piece::Colour pieceColour;
    };

    const std::array<SideCastlingData, 2> sides =
    {{{gameState.whiteCastleRights, whiteQueensideRookStartSquare, whiteKingsideRookStartSquare, Piece::Colour::WHITE},
        {gameState.blackCastleRights, blackQueensideRookStartSquare, blackKingsideRookStartSquare, Piece::Colour::BLACK}}};

    // for both sides of the board (black and white) we check
    for (auto& side : sides) {
        // initial check - no point doing lots of comparisons if that side has no castling rights
        if (side.castleRights == std::array{false, false})
            continue;

        // if this side moved it's king, or it's rook, update the castling rights
        if (movePiece.colour == side.pieceColour) {
            if (movePiece.type == Piece::Type::KING) {
                side.castleRights = std::array{false, false};
            } else if (movePiece.type == Piece::Type::ROOK) {
                if (move.startSquare == side.queensideRookStartSquare)
                    side.castleRights[0] = false;
                if (move.startSquare == side.kingsideRookStartSquare)
                    side.castleRights[1] = false;
            }
        }

        // if an enemy piece moved onto a rook start square on this side, the rook was taken (if it wasn't already), update castling rights
        if (endSquareHasPiece) {
            if (move.endSquare == side.queensideRookStartSquare)
                side.castleRights[0] = false;
            if (move.endSquare == side.kingsideRookStartSquare)
                side.castleRights[1] = false;
        }
    }
}

// 1 = white queenside rook, 2 = white kingside rook, 3 = black queenside rook, 4 = black kingside rook
void Game::castleRook(GameState& gameState, const int rook) const {
    struct CastleRookData {
        Vector2Int startSquare;
        Vector2Int endSquare;
        Piece::Colour rookColour;
    };

    constexpr std::array rookMoves{
        // white queenside
        CastleRookData{{0, 7}, {3, 7}, Piece::Colour::WHITE},
        // white kingside
        CastleRookData{{7, 7}, {5, 7}, Piece::Colour::WHITE},
        // black queenside
        CastleRookData{{0, 0}, {3, 0}, Piece::Colour::BLACK},
        // black kingside
        CastleRookData{{7, 0}, {5, 0}, Piece::Colour::BLACK}};

    const CastleRookData& castleRookData = rookMoves[rook-1];

    gameState.boardPosition[castleRookData.startSquare.y][castleRookData.startSquare.x] = std::nullopt;
    gameState.boardPosition[castleRookData.endSquare.y][castleRookData.endSquare.x] = Piece(Piece::Type::ROOK, castleRookData.rookColour);
}

bool Game::checkIsMoveValid(const GameState& gameState, const Move& move) const {
    // universal checks carried out for all pieces

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
            return checkIsMovePathClearForSliders(gameState, move);
        case Piece::Type::ROOK:
            return (moveVector.x == 0 xor moveVector.y == 0) && checkIsMovePathClearForSliders(gameState, move);
        case Piece::Type::BISHOP:
            return std::abs(moveVector.x) == std::abs(moveVector.y) ? checkIsMovePathClearForSliders(gameState, move) : false;
        case Piece::Type::KNIGHT:
            return absMoveVector == Vector2Int(2, 1) || absMoveVector == Vector2Int(1, 2);
        case Piece::Type::KING:
            return checkIsMoveValidForKing(gameState, move);
        case Piece::Type::PAWN:
            return checkIsMoveValidForPawn(gameState, move);
    }
    return false;
}

bool Game::checkIsMoveLegal(const GameState& gameState, const Move& move) const {
    if (!checkIsMoveValid(gameState, move))
        return false;

    // simulate the board position if the move was to be made
    auto simulatedGameState = gameState;
    movePiece(simulatedGameState, move);

    // disallow moves that would leave the king in check
    if (checkIsKingInCheck(simulatedGameState, gameState.moveColour))
        return false;

    // don't allow castling if the king is in check, or if it would move the king through squares under attack
    if (checkForCastle(gameState, move) > 0) {
        // get direction of move
        Vector2Int stepVector = {1, 0};
        if (move.endSquare.x - move.startSquare.x < 0)
            stepVector.x = -1;

        // check none of the squares in between the startSquare (inclusive) and the endSquare are under attack
        Vector2Int currentSquare = move.startSquare;
        while (currentSquare != move.endSquare) {
            if (checkIsSquareUnderAttack(gameState, currentSquare, gameState.moveColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE))
                return false;
            currentSquare += stepVector;
        }
    }
    return true;
}

bool Game::checkIsMovePathClearForSliders(const GameState& gameState, const Move& move) const {
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

bool Game::checkIsMoveValidForKing(const GameState& gameState, const Move& move) const {
    const Vector2Int moveVector = {move.endSquare.x - move.startSquare.x, move.endSquare.y - move.startSquare.y};

    // normal king move
    if (const auto absMoveVector = Vector2Int(std::abs(moveVector.x), std::abs(moveVector.y)); std::max(absMoveVector.x, absMoveVector.y) == 1)
        return true;

    // castling
    return checkForCastle(gameState, move) > 0;
}

int Game::checkForCastle(const GameState& gameState, const Move& move) const {
    const bool isWhite = gameState.moveColour == Piece::Colour::WHITE;
    const auto& castleRights = isWhite ? gameState.whiteCastleRights : gameState.blackCastleRights;
    const Vector2Int kingStartSquare = isWhite ? whiteKingStartSquare : blackKingStartSquare;
    const int startRow = isWhite ? 7 : 0;
    const auto rookColour = isWhite ? Piece::Colour::WHITE : Piece::Colour::BLACK;

    struct CastleOption {
        int rightsIndex{};
        Vector2Int kingTarget;
        Vector2Int rookStartSquare;
        std::array<Vector2Int, 3> squaresToCheck;
        int squaresToCheckCount{};
        int rookIndex{};
    };

    const CastleOption castleOptions[2] = {
        // queenside
        {0, {2, startRow}, {0, startRow}, {{{1, startRow}, {2, startRow}, {3, startRow}}}, 3, isWhite ? 1 : 3},
        // kingside
        {1, {6, startRow}, {7, startRow}, {{{5, startRow}, {6, startRow}, {0, 0}}}, 2, isWhite ? 2 : 4}};

    if (move.startSquare != kingStartSquare)
        return 0;

    // for both the queenside and the kingside of the king we check
    for (const auto& castleOption : castleOptions) {
        if (!castleRights[castleOption.rightsIndex] || move.endSquare != castleOption.kingTarget)
            continue;

        const auto& rookSquare = gameState.boardPosition[castleOption.rookStartSquare.y][castleOption.rookStartSquare.x];
        if (!rookSquare || rookSquare->type != Piece::Type::ROOK || rookSquare->colour != rookColour)
            continue;

        auto pathClear = true;
        for (int i = 0; i < castleOption.squaresToCheckCount; ++i) {
            const auto& square = castleOption.squaresToCheck[i];
            if (gameState.boardPosition[square.y][square.x]) {
                pathClear = false;
                break;
            }
        }

        // if all these conditions are met then the king is allowed to castle/move two squares
        // returns 0 if no castle, return 1 to 4 to specify which rook needs to move as well
        if (pathClear)
            return castleOption.rookIndex;
    }
    return 0;
}

bool Game::checkIsMoveValidForPawn(const GameState& gameState, const Move& move) const {
    if (!gameState.boardPosition[move.startSquare.y][move.startSquare.x])
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
    return moveVector.x == 0 && moveVector.y == (forwardStep * 2) && move.startSquare.y == startingRow && !enemyOnEndSquare && checkIsMovePathClearForSliders(gameState, move);
}

bool Game::checkForEnPassantTake(const GameState& gameState, const Move& move) const {
    if (!gameState.enPassantSquare)
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

bool Game::checkIsSquareUnderAttack(const GameState& gameState, const Vector2Int square, const Piece::Colour enemyColour) const {
    constexpr std::array pieceDirections = {Vector2Int(1, 1), Vector2Int(-1, -1), Vector2Int(1, -1), Vector2Int(-1, 1), Vector2Int(1, 0), Vector2Int(0, 1), Vector2Int(-1, 0), Vector2Int(0, -1)};

    // -------------------- check for pawn attack --------------------

    if (checkIsSquareUnderAttackByPawn(gameState, square, enemyColour))
        return true;

    // -------------------- check for knight attack --------------------

    constexpr std::array knightVectors = {Vector2Int(2, 1), Vector2Int(-2, -1), Vector2Int(2, -1), Vector2Int(-2, 1), Vector2Int(1, 2), Vector2Int(-1, -2), Vector2Int(1, -2), Vector2Int(-1, 2)};
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
            // if there is a piece on the current square
            if (gameState.boardPosition[currentSquare.y][currentSquare.x]) {
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

bool Game::checkIsSquareUnderAttackByPawn(const GameState& gameState, const Vector2Int square, const Piece::Colour enemyColour) const {
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

bool Game::checkIsKingInCheck(const GameState& gameState, const Piece::Colour kingColour) const {
    // find the king
    for (auto rank = 0; rank < 8; ++rank) {
        for (auto file = 0; file < 8; ++file) {
            if (gameState.boardPosition[rank][file]) {
                if (gameState.boardPosition[rank][file]->colour == kingColour && gameState.boardPosition[rank][file]->type == Piece::Type::KING) {
                    // check whether the king is currently under attack
                    const auto enemyColour = kingColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE;
                    return checkIsSquareUnderAttack(gameState, Vector2Int(file, rank), enemyColour);
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

std::vector<Move> Game::generateAllLegalMoves(const GameState& gameState, const bool capturesOnly) const {
    // generate all possible pseudo legal moves first then filter by actual legal moves
    // this reduces computation as unnecessarily simulating game states is more expensive than unnecessarily checking for pseudo legal moves
    std::vector<Move> validMoves;
    for (auto startSquareRank = 0; startSquareRank < 8; ++startSquareRank) {
        for (auto startSquareFile = 0; startSquareFile < 8; ++startSquareFile) {
            if (gameState.boardPosition[startSquareRank][startSquareFile].has_value() && gameState.boardPosition[startSquareRank][startSquareFile].value().colour == gameState.moveColour) {
                for (auto endSquareRank = 0; endSquareRank < 8; ++endSquareRank) {
                    for (auto endSquareFile = 0; endSquareFile < 8; ++endSquareFile) {
                        if (capturesOnly) {
                            const auto& endSquare = gameState.boardPosition[endSquareRank][endSquareFile];
                            const bool occupiedByEnemy = endSquare.has_value() && endSquare->colour != gameState.moveColour;
                            const bool isEnPassantTarget = gameState.enPassantSquare && endSquareFile == gameState.enPassantSquare->x && endSquareRank == gameState.enPassantSquare->y;
                            if (!occupiedByEnemy && !isEnPassantTarget)
                                continue;
                        }
                        if (const auto move = Move(Vector2Int(startSquareFile, startSquareRank), Vector2Int(endSquareFile, endSquareRank)); checkIsMoveValid(gameState, move))
                            validMoves.emplace_back(move);
                    }
                }
            }
        }
    }
    std::vector<Move> legalMoves;
    for (const auto& move : validMoves) {
        if (!checkIsMoveLegal(gameState, move))
            continue;

        const auto isCapture = gameState.boardPosition[move.endSquare.y][move.endSquare.x].has_value() || checkForEnPassantTake(gameState, move);
        if (capturesOnly && !isCapture)
            continue;

        legalMoves.emplace_back(move);
    }
    return legalMoves;
}