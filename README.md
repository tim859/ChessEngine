The first part of this project is a chess game written in C++. It uses the SFML graphics library for rendering. 
All the standard features of chess are implemented including castling, enpassant, pawn promotion, check and gameover/win conditions (checkmate, draw by threefold repetition, draw by 50 move rule e.t.c.)

Class descriptions:
game is the main API for the core game functionality, all game info and logic is contained in here. If another part of the program needs information about the game or wants to change something, the function for it is available in game.h.
boardview contains the core rendering logic, it draws the board, the pieces and handles extra ui stuff like highlighting available squares that pieces can move to when they are selected by the player.
audio hooks up all the sound effects, other classes just need to pass a MoveType into audio.playSoundOnMove and it will play the appropriate sound effect.
main wires everything together.
ucimain/uci when they are written will allow the program to be compatible with the UCI (Universal Chess Interface) protocol aside from being a standalone application.

The second part of this project is an engine that should hopefully be pretty capable one day. Currently it's in the beginning stages and uses these algorithms:
negamax search, alpha beta pruning and move ordering.

Readme will be updated  when there's more.
