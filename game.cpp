#include "game.h"
#include <algorithm>
#include <iostream>
#include <map>

Game::Game() {
    previousGameStatesFrequency[currentGameState] = 1;
}

std::vector<sf::Vector2<int>> Game::generateLegalMovesForSquare(const GameState &gameState, const sf::Vector2<int> startSquare) const {
    std::vector<sf::Vector2<int>> validMovableSquares;
    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            if (const auto move = Move(startSquare, sf::Vector2(file, rank)); checkIsMoveLegal(gameState, move))
                validMovableSquares.emplace_back(file, rank);
        }
    }
    return validMovableSquares;
}

// contains very little validation, invalid/incomplete fen strings will cause exceptions and/or undefined behaviour
void Game::populateCurrentGameStateWithFen(const std::string& fen) {
    // TODO: pass in the gamestate so the function can be used on any gamestate instead of just the current one
    // TODO: will need to reset/clear the passed in gamestate at the beginning of the function as well

    // map that stores the symbols of fen notation that denote pieces and their corresponding pieces
    std::map<char, Piece::Type> pieceTypeFromSymbol = {{'k', Piece::Type::KING}, {'q', Piece::Type::QUEEN}, {'r', Piece::Type::ROOK},
        {'b', Piece::Type::BISHOP}, {'n', Piece::Type::KNIGHT}, {'p', Piece::Type::PAWN}};

    auto row = 0, column = 0, firstSpaceIndex = 0;
    for (auto i = 0; i < fen.length(); ++i) {
        // slash character means go to the next row and reset the column
        if (fen[i] == '/') {
            ++row;
            column = 0;
        }
        // a digit means skip that amount of squares
        else if (std::isdigit(fen[i]))
            column += (fen[i] - '0');
        else if (std::isalpha(fen[i])) {
            // use the fen letter to get the type, use the case of the fen letter to get the colour and then instantiate a new Piece object with those attributes at the corresponding square
            currentGameState.boardPosition[row][column] = Piece(pieceTypeFromSymbol[std::tolower(fen[i])], std::isupper(fen[i]) ? Piece::Colour::WHITE : Piece::Colour::BLACK);
            ++column;
        }
        else if (fen[i] == ' ') {
            firstSpaceIndex = i;
            break;
        }
        else
            std::cerr << "fen string contains invalid character" << std::endl;
    }

    // get each of the 5 pieces of game state information that fen contains after the piece positions
    std::array<std::string, 5> fenInfo;
    auto fenIndex = firstSpaceIndex;
    for (auto& i : fenInfo) {
        // avoid putting the space in the string
        ++fenIndex;
        std::string gameStateInfo;
        while (fenIndex < fen.length() && fen[fenIndex] != ' ') {
            gameStateInfo += fen[fenIndex];
            ++fenIndex;
        }
        i = gameStateInfo;
    }

    // parse the extracted fen information and apply it to the game state
    // 1: move colour
    currentGameState.moveColour = fenInfo[0] == "w" ? Piece::Colour::WHITE : Piece::Colour::BLACK;
    // 2: castling rights
    currentGameState.whiteCastleRights = {false, false};
    currentGameState.blackCastleRights = {false, false};
    if (fenInfo[1] != "-") {
        for (const auto& character : fenInfo[1]) {
            switch (character) {
                case 'Q':
                    currentGameState.whiteCastleRights[0] = true;
                    break;
                case 'K':
                    currentGameState.whiteCastleRights[1] = true;
                    break;
                case 'q':
                    currentGameState.blackCastleRights[0] = true;
                    break;
                case 'k':
                    currentGameState.blackCastleRights[1] = true;
                    break;
                default:
                    std::cerr << "invalid character found in castling section of fen string" << std::endl;
            }
        }
    }
    // 3: enpassant square
    if (fenInfo[2] != "-") {
        // convert standard chess notation to programs zero indexed vector2 based way of storing piece positions e.g. e3 becomes (4, 5)
        currentGameState.enPassantSquare = sf::Vector2(fenInfo[2][0] - 'a', 7 - (fenInfo[2][1] - '1'));
    }
    // 4: half move counter
    currentGameState.halfMoveCounter = std::stoi(fenInfo[3]);
    // 5: full move counter
    currentGameState.fullMoveCounter = std::stoi(fenInfo[4]);
}

bool Game::pickupPieceFromBoard(const sf::Vector2<int> startSquare) {
    // check there is a piece at the start square
    if (!currentGameState.boardPosition[startSquare.y][startSquare.x])
        return false;

    selectedPieceStartSquare = startSquare;
    selectedPiece = currentGameState.boardPosition[startSquare.y][startSquare.x];

    // check the piece is the same colour as the colour of whose turn it is
    if (selectedPiece->colour != currentGameState.moveColour) {
        selectedPieceStartSquare = std::nullopt;
        selectedPiece = std::nullopt;
        return false;
    }
    return true;
}

GameTypes::MoveType Game::placePieceOnBoard(const sf::Vector2<int> endSquare) {
    auto moveType = GameTypes::MoveType::NONE;

    // ensure the piece and the piece start square will be valid for all the functions that need them and get called from this function
    if (!selectedPieceStartSquare)
        return moveType;
    if (!currentGameState.boardPosition[selectedPieceStartSquare.value().y][selectedPieceStartSquare.value().x])
        return moveType;

    // determine if piece can move to this square and move it if so
    if (checkIsMoveLegal(currentGameState, Move(selectedPieceStartSquare.value(), endSquare))) {
        moveType = movePiece(currentGameState, Move(selectedPieceStartSquare.value(), endSquare));

        // check to see if a pawn has reached the other side and can be promoted
        if (checkForPawnPromotion(currentGameState)) {
            currentGameState.pawnPendingPromotionSquare = endSquare;
            currentGameState.pawnPendingPromotionColour = currentGameState.moveColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE;
        }

        // record current game state position
        if (previousGameStatesFrequency.contains(currentGameState)) {
            ++previousGameStatesFrequency[currentGameState];
            // check for threefold repetition draw
            if (previousGameStatesFrequency[currentGameState] >= 3) {
                moveType = GameTypes::MoveType::GAMEOVER;
                currentGameState.gameOverType = GameTypes::GameOverType::TFRDRAW;
            }
        }
        else
            previousGameStatesFrequency[currentGameState] = 1;

        // check for 50 moves since a piece capture or a pawn moving aka the 50 move draw
        if (moveType == GameTypes::MoveType::CAPTURE || currentGameState.boardPosition[endSquare.y][endSquare.x].value().type == Piece::Type::PAWN)
            currentGameState.halfMovesSinceLastActiveMove = 0;
        else {
            ++currentGameState.halfMovesSinceLastActiveMove;
            if (currentGameState.halfMovesSinceLastActiveMove >= 50) {
                moveType = GameTypes::MoveType::GAMEOVER;
                currentGameState.gameOverType = GameTypes::GameOverType::FIFTYMOVEDRAW;
            }
        }

        // check for stalemate and checkmate
        if (generateAllLegalMoves(currentGameState).empty()) {
            moveType = GameTypes::MoveType::GAMEOVER;
            if (checkIsKingInCheck(currentGameState, currentGameState.moveColour)) {
                if (currentGameState.moveColour == Piece::Colour::WHITE)
                    currentGameState.gameOverType = GameTypes::GameOverType::BLACKWINBYCHECKMATE;
                else
                    currentGameState.gameOverType = GameTypes::GameOverType::WHITEWINBYCHECKMATE;
            }
            else
                currentGameState.gameOverType = GameTypes::GameOverType::STALEMATE;
        }
    }
    selectedPieceStartSquare = std::nullopt;
    selectedPiece = std::nullopt;
    return moveType;
}

void Game::promotePawn(GameState& gameState, const Piece::Type pieceType) const {
    if (!gameState.pawnPendingPromotionSquare || !gameState.pawnPendingPromotionColour)
        return;
    if (!gameState.boardPosition[gameState.pawnPendingPromotionSquare.value().y][gameState.pawnPendingPromotionSquare.value().x])
        return;
    gameState.boardPosition[gameState.pawnPendingPromotionSquare.value().y][gameState.pawnPendingPromotionSquare.value().x] = Piece(pieceType, gameState.pawnPendingPromotionColour.value());
    gameState.pawnPendingPromotionSquare = std::nullopt;
    gameState.pawnPendingPromotionColour = std::nullopt;
}

std::vector<Move> Game::generateAllLegalMoves(const GameState &gameState) const {
    // generate all possible pseudo legal moves first then filter by actual legal moves
    // this reduces computation as unnecessarily simulating game states is more expensive than unnecessarily checking for pseudo legal moves
    std::vector<Move> validMoves;
    for (auto startSquareRank = 0; startSquareRank < 8; ++startSquareRank) {
        for (auto startSquareFile = 0; startSquareFile < 8; ++startSquareFile) {
            if (gameState.boardPosition[startSquareRank][startSquareFile].has_value()) {
                if (gameState.boardPosition[startSquareRank][startSquareFile].value().colour == gameState.moveColour) {
                    for (auto endSquareRank = 0; endSquareRank < 8; ++endSquareRank) {
                        for (auto endSquareFile = 0; endSquareFile < 8; ++endSquareFile) {
                            if (const auto move = Move(sf::Vector2(startSquareFile, startSquareRank), sf::Vector2(endSquareFile, endSquareRank)); checkIsMoveValid(gameState, move))
                                validMoves.emplace_back(move);
                        }
                    }
                }
            }
        }
    }
    std::vector<Move> legalMoves;
    for (const auto& move : validMoves) {
        if (checkIsMoveLegal(gameState, move))
            legalMoves.emplace_back(move);
    }
    return legalMoves;
}

GameTypes::MoveType Game::movePiece(GameState& gameState, const Move move) const {
    auto moveType = GameTypes::MoveType::NONE;
    const auto movePiece = gameState.boardPosition[move.startSquare.y][move.startSquare.x].value();

    // ---------- en passant ---------------

    // check for pawn double push and record the intermediate square it skipped over (enPassantSquare) and the square it is now on (enPassantPawnSquare)
    if (checkForPawnDoublePush(gameState, move)) {
        const int forwardStep = (movePiece.colour == Piece::Colour::WHITE) ? -1 : 1;
        gameState.enPassantSquare = sf::Vector2(move.endSquare.x, move.endSquare.y - forwardStep);
        gameState.movesSinceEnPassant = 0;
    }

    // check for enpassant take and if returns true, remove the appropriate pawn from the board
    if (checkForEnPassantTake(gameState, move)) {
        // colour of the pawn to be taken will be the opposite of the current move colour
        // therefore we can get the forward direction of the other colour and use it to find the square with the pawn to be taken on it
        const auto enemyForwardStep = gameState.moveColour == Piece::Colour::WHITE ? 1 : -1;
        gameState.boardPosition[gameState.enPassantSquare->y + enemyForwardStep][gameState.enPassantSquare->x] = std::nullopt;
        moveType = GameTypes::MoveType::CAPTURE;
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
        moveType = GameTypes::MoveType::CASTLE;
    }
    updateCastlingRights(gameState, move);

    if (moveType == GameTypes::MoveType::NONE) {
        if (gameState.boardPosition[move.endSquare.y][move.endSquare.x])
            moveType = GameTypes::MoveType::CAPTURE;
        else
            moveType = GameTypes::MoveType::MOVESELF;
    }

    // remove the piece from the start square
    gameState.boardPosition[move.startSquare.y][move.startSquare.x] = std::nullopt;
    // move the piece to the end square, any enemy piece that was on the end square will be overwritten/taken
    gameState.boardPosition[move.endSquare.y][move.endSquare.x] = movePiece;
    gameState.moveColour = gameState.moveColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE;
    ++gameState.halfMoveCounter;
    if (gameState.halfMoveCounter % 2 == 0)
        ++gameState.fullMoveCounter;
    return moveType;
}

void Game::updateCastlingRights(GameState& gameState, const Move move) const {
    if (!gameState.boardPosition[move.startSquare.y][move.startSquare.x])
        return;
    const auto movePiece = gameState.boardPosition[move.startSquare.y][move.startSquare.x].value();
    const auto endSquareHasPiece = gameState.boardPosition[move.endSquare.y][move.endSquare.x].has_value();

    struct SideCastlingData {
        // [0]=queenside, [1]=kingside
        std::array<bool, 2>& castleRights;
        sf::Vector2<int> queensideRookStartSquare;
        sf::Vector2<int> kingsideRookStartSquare;
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
        sf::Vector2<int> startSquare;
        sf::Vector2<int> endSquare;
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

bool Game::checkIsMoveValid(const GameState& gameState, const Move move) const {
    // universal checks carried out for all pieces

    // make sure start square contains a piece
    if (!gameState.boardPosition[move.startSquare.y][move.startSquare.x])
        return false;
    const auto movePiece = gameState.boardPosition[move.startSquare.y][move.startSquare.x].value();

    // check start and end square are in different locations and check the end position is on the board
    if (move.startSquare == move.endSquare || move.endSquare.x < 0 || move.endSquare.x > 7 || move.endSquare.y < 0 || move.endSquare.y > 7)
        return false;

    // check end square is either empty or contains an enemy piece
    if (gameState.boardPosition[move.endSquare.y][move.endSquare.x]) {
        if (gameState.boardPosition[move.endSquare.y][move.endSquare.x]->colour == movePiece.colour)
            return false;
    }

    const sf::Vector2 moveVector = {move.endSquare.x - move.startSquare.x, move.endSquare.y - move.startSquare.y};
    const auto absMoveVector = sf::Vector2(std::abs(moveVector.x), std::abs(moveVector.y));

    switch (movePiece.type) {
        case Piece::Type::QUEEN:
            return checkIsMovePathClearForSliders(gameState, move);
        case Piece::Type::ROOK:
            return (moveVector.x == 0 xor moveVector.y == 0) && checkIsMovePathClearForSliders(gameState, move);
        case Piece::Type::BISHOP:
            return std::abs(moveVector.x) == std::abs(moveVector.y) ? checkIsMovePathClearForSliders(gameState, move) : false;
        case Piece::Type::KNIGHT:
            return absMoveVector == sf::Vector2(2, 1) || absMoveVector == sf::Vector2(1, 2);
        case Piece::Type::KING:
            return checkIsMoveValidForKing(gameState, move);
        case Piece::Type::PAWN:
            return checkIsMoveValidForPawn(gameState, move);
    }
    return false;
}

bool Game::checkIsMoveLegal(const GameState& gameState, const Move move) const {
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
        sf::Vector2 stepVector = {1, 0};
        if (move.endSquare.x - move.startSquare.x < 0)
            stepVector.x = -1;

        // check none of the squares in between the startSquare (inclusive) and the endSquare are under attack
        sf::Vector2<int> currentSquare = move.startSquare;
        while (currentSquare != move.endSquare) {
            if (checkIsSquareUnderAttack(gameState, currentSquare, gameState.moveColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE))
                return false;
            currentSquare += stepVector;
        }
    }
    return true;
}

bool Game::checkIsMovePathClearForSliders(const GameState& gameState, const Move move) const {
    const sf::Vector2 moveVector = {move.endSquare.x - move.startSquare.x, move.endSquare.y - move.startSquare.y};

    // check to make sure the move makes geometric sense for sliding pieces
    // i.e. is the end piece legally reachable from the start piece horizontally, vertically or diagonally
    if (moveVector.x != 0 && moveVector.y != 0 && std::abs(moveVector.x) != std::abs(moveVector.y))
        return false;

    // next we need to find out which one of the 8 possible directions that the move is going in. The direction of the vector can be computed by:
    // stepRow = sign(moveVector.y), stepColumn = sign(moveVector.x) where sign(x) is -1 if x < 0, 0 if x == 0, +1 if x > 0
    sf::Vector2 stepVector = {0, 0};
    if (moveVector.x > 0)
        stepVector.x += 1;
    else if (moveVector.x < 0)
        stepVector.x -= 1;

    if (moveVector.y > 0)
        stepVector.y += 1;
    else if (moveVector.y < 0)
        stepVector.y -= 1;

    // the step vector is then repeatedly added on from the start square until we reach the end square, if no piece is found while traversing then the move path is clear
    sf::Vector2<int> currentSquare = move.startSquare + stepVector;
    while (currentSquare != move.endSquare) {
        if (gameState.boardPosition[currentSquare.y][currentSquare.x])
            return false;
        currentSquare += stepVector;
    }
    return true;
}

bool Game::checkIsMoveValidForKing(const GameState& gameState, const Move move) const {
    const sf::Vector2 moveVector = {move.endSquare.x - move.startSquare.x, move.endSquare.y - move.startSquare.y};

    // normal king move
    if (const auto absMoveVector = sf::Vector2(std::abs(moveVector.x), std::abs(moveVector.y)); std::max(absMoveVector.x, absMoveVector.y) == 1)
        return true;

    // castling
    return checkForCastle(gameState, move) > 0;
}

int Game::checkForCastle(const GameState& gameState, const Move move) const {
    const bool isWhite = gameState.moveColour == Piece::Colour::WHITE;
    const auto& castleRights = isWhite ? gameState.whiteCastleRights : gameState.blackCastleRights;
    const sf::Vector2 kingStartSquare = isWhite ? whiteKingStartSquare : blackKingStartSquare;
    const int startRow = isWhite ? 7 : 0;

    struct CastleOption {
        int rightsIndex{};
        sf::Vector2<int> kingTarget;
        int rookIndex{};
    };

    const CastleOption castleOptions[2] = {
        // queenside
        {0, {2, startRow}, isWhite ? 1 : 3},
        // kingside
        {1, {6, startRow}, isWhite ? 2 : 4}};

    if (move.startSquare != kingStartSquare)
        return 0;

    // for both the queenside and the kingside of the king we check
    for (const auto& castleOption : castleOptions) {
        // if the king has the right to castle on that side, if the king will end up on the correct square and if the squares between the king and the rook are clear
        if (castleRights[castleOption.rightsIndex] && move.endSquare == castleOption.kingTarget && checkIsMovePathClearForSliders(gameState, move)) {
            // if all these conditions are met then the king is allowed to castle/move two squares
            // returns 0 if no castle, return 1 to 4 to specify which rook needs to move as well
            return castleOption.rookIndex;
        }
    }
    return 0;
}

bool Game::checkIsMoveValidForPawn(const GameState& gameState, const Move move) const {
    if (!gameState.boardPosition[move.startSquare.y][move.startSquare.x])
        return false;
    const auto movePiece = gameState.boardPosition[move.startSquare.y][move.startSquare.x].value();
    const sf::Vector2 moveVector = {move.endSquare.x - move.startSquare.x, move.endSquare.y - move.startSquare.y};

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

bool Game::checkForPawnDoublePush(const GameState& gameState, const Move move) const {
    const sf::Vector2 moveVector = {move.endSquare.x - move.startSquare.x, move.endSquare.y - move.startSquare.y};
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

bool Game::checkForEnPassantTake(const GameState& gameState, const Move move) const {
    if (!gameState.enPassantSquare)
        return false;

    const auto enemyForwardStep = gameState.moveColour == Piece::Colour::WHITE ? 1 : -1;
    const auto enPassantPawnSquare = sf::Vector2(gameState.enPassantSquare.value().x, gameState.enPassantSquare.value().y + enemyForwardStep);
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

bool Game::checkIsSquareUnderAttack(const GameState& gameState, const sf::Vector2<int> square, const Piece::Colour enemyColour) const {
    constexpr std::array pieceDirections = {sf::Vector2(1, 1), sf::Vector2(-1, -1), sf::Vector2(1, -1), sf::Vector2(-1, 1), sf::Vector2(1, 0), sf::Vector2(0, 1), sf::Vector2(-1, 0), sf::Vector2(0, -1)};
    // -------------------- check for pawn attack --------------------

    // get forward step of friendly colour
    int forwardStep = 1;
    if (enemyColour == Piece::Colour::BLACK)
        forwardStep = -1;
    // check for enemy pawns on the two diagonally forward squares
    if (const auto squareOne = sf::Vector2(square.x + 1, square.y + forwardStep); squareOne.x < 8 && squareOne.y >= 0 && squareOne.y < 8) {
        if (gameState.boardPosition[squareOne.y][squareOne.x]) {
            if (gameState.boardPosition[squareOne.y][squareOne.x]->colour == enemyColour && gameState.boardPosition[squareOne.y][squareOne.x]->type == Piece::Type::PAWN)
                return true;
        }
    }
    if (const auto squareTwo = sf::Vector2(square.x - 1, square.y + forwardStep); squareTwo.x >= 0 && squareTwo.y >= 0 && squareTwo.y < 8) {
        if (gameState.boardPosition[squareTwo.y][squareTwo.x]) {
            if (gameState.boardPosition[squareTwo.y][squareTwo.x]->colour == enemyColour && gameState.boardPosition[squareTwo.y][squareTwo.x]->type == Piece::Type::PAWN)
                return true;
        }
    }

    // -------------------- check for knight attack --------------------

    constexpr std::array knightVectors = {sf::Vector2(2, 1), sf::Vector2(-2, -1), sf::Vector2(2, -1), sf::Vector2(-2, 1), sf::Vector2(1, 2), sf::Vector2(-1, -2), sf::Vector2(1, -2), sf::Vector2(-1, 2)};
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

bool Game::checkIsKingInCheck(const GameState& gameState, const Piece::Colour kingColour) const {
    // find the king
    for (auto rank = 0; rank < 8; ++rank) {
        for (auto file = 0; file < 8; ++file) {
            if (gameState.boardPosition[rank][file]) {
                if (gameState.boardPosition[rank][file]->colour == kingColour && gameState.boardPosition[rank][file]->type == Piece::Type::KING) {
                    // check whether the king is currently under attack
                    const auto enemyColour = kingColour == Piece::Colour::WHITE ? Piece::Colour::BLACK : Piece::Colour::WHITE;
                    return checkIsSquareUnderAttack(gameState, sf::Vector2(file, rank), enemyColour);
                }
            }
        }
    }
    // should never happen during a normal game but if somehow a king wasn't found on the board
    std::cerr << "no king was found on the board of this colour!" << std::endl;
    return false;
}

bool Game::checkForPawnPromotion(const GameState &gameState) const {
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
