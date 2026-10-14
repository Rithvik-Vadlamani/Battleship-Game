# Embedded Battleship

A two-player Battleship game in C for the TI MSPM0G3507 microcontroller. It uses an ST7735 LCD and physical controls for ship placement and turn-based play, with English and Spanish text.

My project work included programming the game controller and designing and soldering custom boards for the buttons, LCD, LEDs, and speaker.

## How the game works

The controller moves through six states:

1. Language selection
2. Player one ship placement
3. Player two ship placement
4. Player one turn
5. Player two turn
6. Game over

The source defines each player's grid and ship state, tracks game timing, and updates the display as players place ships and take turns.

## Where to look

[Battleship.c](Battleship.c) contains the game state definitions, player and ship data structures, language strings, and game logic. Start with `GameState_t`, `Player_t`, and `Game_t` to see how the controller is organized.

## Repository scope

This repository contains the main C source file. It references course support libraries, hardware drivers, and image assets that are not included here, so it is a source sample rather than a complete standalone build.

The original setup uses an MSPM0G3507 board, an ST7735 display, and the ECE 319K support files. Reproducing it requires those dependencies and the matching hardware connections.

[My other projects](https://github.com/Rithvik-Vadlamani) · [LinkedIn](https://www.linkedin.com/in/rithvik-vadlamani/)
