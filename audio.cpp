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

void Audio::playSound(const GameTypes::MoveType moveType) {
    switch (moveType) {
        case GameTypes::MoveType::NONE:
            break;
        case GameTypes::MoveType::MOVESELF:
            moveSelfSound.play();
            break;
        case GameTypes::MoveType::CAPTURE:
            captureSound.play();
            break;
        case GameTypes::MoveType::CASTLE:
            castleSound.play();
            break;
        case GameTypes::MoveType::PROMOTEPAWN:
            moveSelfSound.play();
            break;
        case GameTypes::MoveType::GAMEOVER:
            break;
    }
}
