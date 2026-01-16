#ifndef CHESS_AUDIO_H
#define CHESS_AUDIO_H
#include "game.h"
#include <SFML/Audio.hpp>

class Audio {
    sf::SoundBuffer moveSelfBuffer;
    sf::Sound moveSelfSound;
    sf::SoundBuffer captureBuffer;
    sf::Sound captureSound;
    sf::SoundBuffer castleBuffer;
    sf::Sound castleSound;

public:
    Audio();
    void playSound(GameTypes::MoveType moveType);
};

#endif //CHESS_AUDIO_H