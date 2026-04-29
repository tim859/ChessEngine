#ifndef CHESS_AUDIO_H
#define CHESS_AUDIO_H
#include "game.h"
#include <SFML/Audio.hpp>

class Audio {
    sf::SoundBuffer gameStartBuffer;
    sf::Sound gameStartSound;
    sf::SoundBuffer moveSelfBuffer;
    sf::Sound moveSelfSound;
    sf::SoundBuffer captureBuffer;
    sf::Sound captureSound;
    sf::SoundBuffer castleBuffer;
    sf::Sound castleSound;
    sf::SoundBuffer promoteBuffer;
    sf::Sound promoteSound;
    sf::SoundBuffer moveCheckBuffer;
    sf::Sound moveCheckSound;
    sf::SoundBuffer gameEndBuffer;
    sf::Sound gameEndSound;

public:
    Audio();
    void playSoundOnStart();
    void playSoundOnMove(GameTypes::MoveType moveType);
};

#endif //CHESS_AUDIO_H