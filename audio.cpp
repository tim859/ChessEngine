#include "audio.h"
#include <iostream>

Audio::Audio() : moveSelfSound(moveSelfBuffer), captureSound(captureBuffer), castleSound(castleBuffer) {
    if (!moveSelfBuffer.loadFromFile("assets/move-self.mp3"))
        std::cerr << "Could not load move-self.mp3 into sound buffer" << std::endl;

    if (!captureBuffer.loadFromFile("assets/capture.mp3"))
        std::cerr << "Could not load capture.mp3 into sound buffer" << std::endl;

    if (!castleBuffer.loadFromFile("assets/castle.mp3"))
        std::cerr << "Could not load castle.mp3 into sound buffer" << std::endl;
}

void Audio::playSound(const Game::MoveType moveType) {
    switch (moveType) {
        case Game::MoveType::NONE:
            break;
        case Game::MoveType::MOVESELF:
            moveSelfSound.play();
            break;
        case Game::MoveType::CAPTURE:
            captureSound.play();
            break;
        case Game::MoveType::CASTLE:
            castleSound.play();
            break;
        case Game::MoveType::TFRDRAW:
            moveSelfSound.play();
            break;
        case Game::MoveType::FIFTYMOVEDRAW:
            moveSelfSound.play();
            break;
        case Game::MoveType::PROMOTEPAWN:
            moveSelfSound.play();
            break;
        case Game::MoveType::STALEMATE:
            moveSelfSound.play();
            break;
        case Game::MoveType::CHECKMATE:
            moveSelfSound.play();
            break;
    }
}
