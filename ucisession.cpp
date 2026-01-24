#include "ucisession.h"

void UCISession::cmdUCI() const {
    std::cout << "id name " << uciSettings.name << std::endl;
    std::cout << "id author " << uciSettings.author << std::endl;
    std::cout << "uciok" << std::endl;
}

void UCISession::cmdDebug(const bool option) {
    uciSettings.debugMode = option;
}

void UCISession::cmdIsReady() const {
    std::cout << "readyok" << std::endl;
}

void UCISession::cmdSetOption(const std::string& name) const {

}

void UCISession::cmdSetOption(const std::string& name, const std::string& value) const {

}

void UCISession::cmdUCINewGame() {
    engine.reset();
    game.reset();
}

void UCISession::cmdPositionStartpos(const std::vector<std::string>& moves) {
    game.populateGameStateFromFEN(game.getCurrentGameState(), game.getCurrentGameStateHistory(), uciSettings.startPosFen);
    if (!moves.empty())
        applyMovesToGameState(moves);
}

void UCISession::cmdPositionFen(const std::string& fen, const std::vector<std::string>& moves) {
    game.populateGameStateFromFEN(game.getCurrentGameState(), game.getCurrentGameStateHistory(), fen);
    if (!moves.empty())
        applyMovesToGameState(moves);
}

void UCISession::applyMovesToGameState(const std::vector<std::string>& moves) {
    for (const auto& algebraicMove : moves) {
        // convert the chess move to the internal Vector2 based move system
        const auto move = Move(Vector2Int(algebraicMove[0] - 'a', 7 - (algebraicMove[1] - '1')), Vector2Int(algebraicMove[2] - 'a', 7 - (algebraicMove[3] - '1')));

        Piece* promotionPiece = nullptr;
        if (algebraicMove.length() == 5) {
            switch (algebraicMove[4]) {
                case 'q': promotionPiece = new Piece(Piece::Type::QUEEN, game.getCurrentGameState().moveColour); break;
                case 'r': promotionPiece = new Piece(Piece::Type::ROOK, game.getCurrentGameState().moveColour); break;
                case 'b': promotionPiece = new Piece(Piece::Type::BISHOP, game.getCurrentGameState().moveColour); break;
                case 'n': promotionPiece = new Piece(Piece::Type::KNIGHT, game.getCurrentGameState().moveColour); break;
            }
        }
        game.movePiece(game.getCurrentGameState(), move, game.getCurrentGameStateHistory(), promotionPiece);
        delete promotionPiece;
    }
}
