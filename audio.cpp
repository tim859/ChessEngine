#include "audio.h"
#include <iostream>

Audio::Audio() : gameStartSound(gameStartBuffer), moveSelfSound(moveSelfBuffer), captureSound(captureBuffer),
                 castleSound(castleBuffer), promoteSound(promoteBuffer), moveCheckSound(moveCheckBuffer), gameEndSound(gameEndBuffer) {
    if (!gameStartBuffer.loadFromFile("assets/game-start.mp3"))
        std::cerr << "Could not load game-start.mp3 into sound buffer" << std::endl;

    if (!moveSelfBuffer.loadFromFile("assets/move-self.mp3"))
        std::cerr << "Could not load move-self.mp3 into sound buffer" << std::endl;

    if (!captureBuffer.loadFromFile("assets/capture.mp3"))
        std::cerr << "Could not load capture.mp3 into sound buffer" << std::endl;

    if (!castleBuffer.loadFromFile("assets/castle.mp3"))
        std::cerr << "Could not load castle.mp3 into sound buffer" << std::endl;

    if (!promoteBuffer.loadFromFile("assets/promote.mp3"))
        std::cerr << "Could not load promote.mp3 into sound buffer" << std::endl;

    if (!moveCheckBuffer.loadFromFile("assets/move-check.mp3"))
        std::cerr << "Could not load move-check.mp3 into sound buffer" << std::endl;

    if (!gameEndBuffer.loadFromFile("assets/game-end.mp3"))
        std::cerr << "Could not load game-end.mp3 into sound buffer" << std::endl;
}

void Audio::playSoundOnStart() {
    gameStartSound.play();
}

void Audio::playSoundOnMove(const GameTypes::MoveType moveType) {
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
            promoteSound.play();
            break;
        case GameTypes::MoveType::CHECK:
            moveCheckSound.play();
            break;
        case GameTypes::MoveType::GAMEOVER:
            gameEndSound.play();
            break;
    }
}
