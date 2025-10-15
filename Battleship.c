// Lab9Main.c
// Runs on MSPM0G3507
// Lab 9 ECE319K
// Your name
// Last Modified: 12/26/2024

#include <stdio.h>
#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "../inc/ST7735.h"
#include "../inc/Clock.h"
#include "../inc/LaunchPad.h"
#include "../inc/TExaS.h"
#include "../inc/Timer.h"
#include "../inc/ADC1.h"
#include "../inc/DAC5.h"
#include "../inc/Arabic.h"
#include "SmallFont.h"
#include "LED.h"
#include "Switch.h"
#include "Sound.h"
#include "images/images.h"
// ****note to ECE319K students****
// the data sheet says the ADC does not work when clock is 80 MHz
// however, the ADC seems to work on my boards at 80 MHz
// I suggest you try 80MHz, but if it doesn't work, switch to 40MHz
void PLL_Init(void){ // set phase lock loop (PLL)
  // Clock_Init40MHz(); // run this line for 40MHz
  Clock_Init80MHz(0);   // run this line for 80MHz
}

uint32_t M=1;
uint32_t Random32(void){
  M = 1664525*M+1013904223;
  return M;
}

uint32_t Random(uint32_t n){
  return (Random32()>>16)%n;
}

uint8_t TExaS_LaunchPadLogicPB27PB26(void){
  return (0x80|((GPIOB->DOUT31_0>>26)&0x03));
}

#define LEFT 0x1
#define RIGHT 0x4
#define DOWN 0x2
#define UP 0x8
#define ROTATE 0x10
#define FIRE 0x80

// languages
typedef enum {
  English, 
  Spanish, 
} Language_t;

// game states
typedef enum 
{
  LANGUAGE_SELECT, 
  PLAYER_ONE_SHIP_SELECT,
  PLAYER_TWO_SHIP_SELECT,
  PLAYER_ONE_PLAY,
  PLAYER_TWO_PLAY,
  GAME_OVER
} GameState_t;

// status of the players
typedef enum
{
  SELECT,
  PLAY
} Status_t;

// players
typedef struct {
  int32_t shipsAlive;
  int32_t shipsDead;
  Status_t status;       // selecting where to place ships or selecting where to fire shots
  int32_t grid[16][11];  // each players grid 
  int32_t bitmapData[16][11];
  int32_t placingShip;
} Player_t;

// ships
typedef struct {
  int32_t size; // 2 for 2Long, 3 for 3Long, etc

  uint32_t r;
  uint32_t rPrev;

  uint32_t shipX;
  uint32_t shipY;

  uint32_t shipXsize; // always 10
  uint32_t shipYsize; // 10* the normal size

  uint32_t prevShipX;
  uint32_t prevShipY;
  const unsigned short* Length[4]; // rotation of the battleship sprites
} Ship_t;

// Game structure
typedef struct {
  GameState_t state;       // current game state
  uint32_t time;           // game time in ms
  Language_t language;     // current language
  Player_t players[2];     // array of players
  uint8_t semaphore;       // synchronization flag for main loop
} Game_t;

// Phrase states
typedef enum
{
  START_GAME, 
  PLAYER1_WINS, 
  PLAYER2_WINS, 
  SCORE, 
  READY, 
  GO
} phrase_t;

const char StartGame_English[] = "Start Game";
const char StartGame_Spanish[] = "Iniciar Juego";
const char Player1Wins_English[] = "Player 1 Wins!";
const char Player1Wins_Spanish[] = "¡Jugador 1 Gana!";
const char Player2Wins_English[] = "Player 2 Wins!";
const char Player2Wins_Spanish[] = "¡Jugador 2 Gana!";
const char Score_English[] = "Score:";
const char Score_Spanish[] = "Puntaje:";
const char Ready_English[] = "Ready...";
const char Ready_Spanish[] = "Listo...";
const char Go_English[] = "GO!";
const char Go_Spanish[] = "¡YA!";

const char *Phrases[6][2]={
  {StartGame_English, StartGame_Spanish},
  {Player1Wins_English, Player1Wins_Spanish},
  {Player2Wins_English, Player2Wins_Spanish},
  {Score_English, Score_Spanish},
  {Ready_English, Ready_Spanish},
  {Go_English, Go_Spanish}
};

// Variables
Game_t Game;  // Main game structure

Ship_t placingShip;

uint32_t screenX = 160;
uint32_t screenY = 110;

uint32_t inputCounter = 0; 
uint32_t inputRate = 5;    // Only process inputs every 5 interrupt cycles

uint32_t resetScreen = 0;

// games  engine runs at 30Hz
void TIMG12_IRQHandler(void){uint32_t pos,msg;
  if((TIMG12->CPU_INT.IIDX) == 1){ // this will acknowledge
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)

    uint32_t input = Switch_In();

    inputCounter++;
    if(inputCounter >= inputRate) {
      inputCounter = 0; // Reset counter

      switch (Game.state) {
      case LANGUAGE_SELECT:
        if(input == LEFT){ // left : english
          Game.language = English;
          Game.state = PLAYER_ONE_SHIP_SELECT;
          resetScreen = 1; // resets the screen for the next state
        }
        if(input == RIGHT){ // right : spansih
          Game.language = Spanish;
          Game.state = PLAYER_ONE_SHIP_SELECT;
          resetScreen = 1; // resets the screen for the next state
        }
        break;

      case PLAYER_ONE_SHIP_SELECT:
        if(resetScreen == 1){
          ST7735_FillScreen(ST7735_BLACK);
          resetScreen = 0;
        }

        if(Game.players[0].shipsAlive == 4){
          Game.state = PLAYER_TWO_SHIP_SELECT;
          resetScreen = 1;
        }
        if(Game.players[0].placingShip == 0){
          Game.players[0].placingShip = 1;
        
          placingShip.r = 0;
          placingShip.rPrev = 0;

          placingShip.size = Game.players[0].shipsAlive + 2;

          placingShip.shipX = 0;
          placingShip.shipY = 10 * placingShip.size; 

          placingShip.shipXsize = 10;
          placingShip.shipYsize = 10 * placingShip.size;
          
          placingShip.prevShipX = placingShip.shipXsize;
          placingShip.prevShipY = placingShip.shipYsize;

          
          if(placingShip.size == 2){
            placingShip.Length[0] = TwoLongBattleShip;
            placingShip.Length[1] = TwoLongBattleShip90;
            placingShip.Length[2] = TwoLongBattleShipFlip;
            placingShip.Length[3] = TwoLongBattleShip90Flip;        
          }
          else if(placingShip.size == 3){
            placingShip.Length[0] = ThreeLongBattleShip;
            placingShip.Length[1] = ThreeLongBattleShip90;
            placingShip.Length[2] = ThreeLongBattleShipFlip;
            placingShip.Length[3] = ThreeLongBattleShip90Flip;
          }
          else if(placingShip.size == 4){
            placingShip.Length[0] = FourLongBattleShip;
            placingShip.Length[1] = FourLongBattleShip90;
            placingShip.Length[2] = FourLongBattleShipFlip;
            placingShip.Length[3] = FourLongBattleShip90Flip;
          }
          else{
              placingShip.Length[0] = FiveLongBattleShip;
              placingShip.Length[1] = FiveLongBattleShip90;
              placingShip.Length[2] = FiveLongBattleShipFlip;
              placingShip.Length[3] = FiveLongBattleShip90Flip;
          }
          
          // create ship struct
        }
        // Left
        if(input == LEFT){
          if(placingShip.shipX >= 10) {
              placingShip.shipX -= 10;
          } else {
              placingShip.shipX = 0;
          }
          // Clock_Delay1ms(75);
        }

        // Right
        if(input == RIGHT){
          // Define width based on rotation
          uint32_t current_width = (placingShip.r % 2 == 0) ? placingShip.shipXsize : placingShip.shipYsize;
          
          placingShip.shipX += 10;
          if(placingShip.shipX > screenX - current_width) {
              placingShip.shipX = screenX - current_width;
          }
          // Clock_Delay1ms(75);
        }

        // Up
        if(input == UP){
          placingShip.shipY -= 10;
          // Different minimum Y based on rotation
          uint32_t min_y = (placingShip.r % 2 == 0) ? (placingShip.size*10) : 10;
          
          if(placingShip.shipY < min_y) {
              placingShip.shipY = min_y;
          }
          // Clock_Delay1ms(75);
        }

        // Down
        if(input == DOWN){
          placingShip.shipY += 10;
          if(placingShip.shipY > screenY) {
              placingShip.shipY = screenY;
          }
          // Clock_Delay1ms(75);
        }

        if(input == ROTATE){
          if((placingShip.r % 2) == 0){
            if((placingShip.shipX < 150)){
              placingShip.r++;
            }
          }
          else if((placingShip.r % 2) == 1){
            if((placingShip.shipY > 10)){
              placingShip.r++;
            }
          }
          if((placingShip.r >= 4)){
            placingShip.r = 0;
          }
          // Clock_Delay1ms(100); // delay 50 msec
        }
        if(input == 0x80){ // place
          // Calculate the grid coordinates from pixel coordinates
          uint32_t gridX = (placingShip.shipX) / 10;
          uint32_t gridY = placingShip.shipY / 10;

          if(placingShip.r % 2 == 1){ // horizontal
            for(uint32_t x = 0; x < placingShip.size; x++){
              Game.players[0].bitmapData[((gridX-1)+(x))][(gridY-1)] = 1;
            }
          }

          if(placingShip.r % 2 == 0){ // horizontal
            for(uint32_t x = 0; x < placingShip.size; x++){

              Game.players[0].bitmapData[(gridX-1)][((gridY-1)-(x))] = 1;
            }
          }
          // Update game state
          Game.players[0].placingShip = 0;
          Game.players[0].shipsAlive += 1;
          Game.players[0].shipsDead -= 1;
        }

        break;
      case PLAYER_TWO_SHIP_SELECT:
        ST7735_FillScreen(ST7735_BLACK);

        break;
      case PLAYER_ONE_PLAY:
        ST7735_FillScreen(ST7735_BLACK);

        break;
      case PLAYER_TWO_PLAY:
        ST7735_FillScreen(ST7735_BLACK);

        break;
      case GAME_OVER:
        ST7735_FillScreen(ST7735_BLACK);

        break;
    }
  }
// game engine goes here
    // 1) sample slide pot
    // 2) read input switches
    // 3) move sprites
    // 4) start sounds
    // 5) set semaphore
    // NO LCD OUTPUT IN INTERRUPT SERVICE ROUTINES
    Game.semaphore = 1;
    GPIOB->DOUTTGL31_0 = GREEN; // toggle PB27 (minimally intrusive debugging)
  }
}


// Correct the array type to be pointers to const unsigned short
const unsigned short* TwoLong[4] = {TwoLongBattleShip, TwoLongBattleShip90, TwoLongBattleShipFlip, TwoLongBattleShip90Flip};
const unsigned short* ThreeLong[4] = {ThreeLongBattleShip, ThreeLongBattleShip90, ThreeLongBattleShipFlip, ThreeLongBattleShip90Flip};
const unsigned short* FourLong[4] = {FourLongBattleShip, FourLongBattleShip90, FourLongBattleShipFlip, FourLongBattleShip90Flip};
const unsigned short* FiveLong[4] = {FiveLongBattleShip, FiveLongBattleShip90, FiveLongBattleShipFlip, FiveLongBattleShip90Flip};

uint32_t sizeCorresponding[4] = {
  2,
  2,
  2,
  2,
  3,
  3,
  3,
  3,
  4,
  4,
  4,
  4,
  5,
  5,
  5,
  5,
};
/*
  [0] = empty,
  [1] = size of 2, rotation = 0;
  [2] = size of 2, rotation = 1;
  [3] = size of 2, rotation = 2;
  [4] = size of 2, rotation = 3;
  
  [5] = size of 3, rotation = 0;
  [6] = size of 3, rotation = 1;
  [7] = size of 3, rotation = 2;
  [8] = size of 3, rotation = 3;
  
  [9] = size of 4, rotation = 0;
  [10] = size of 4, rotation = 1;
  [11] = size of 4, rotation = 2;
  [12] = size of 4, rotation = 3;
  
  [13] = size of 5, rotation = 0;
  [14] = size of 5, rotation = 1;
  [15] = size of 5, rotation = 2;
  [16] = size of 5, rotation = 3;
*/

void Game_Init(void){
  Game.state = LANGUAGE_SELECT;
  Game.time = 0;
  Game.semaphore = 0;

  // grids for both players
  for(int x = 0; x < 16; x ++){
    for(int y = 0; y < 11; y ++){
      Game.players[0].grid[x][y] = 0;
      Game.players[1].grid[x][y] = 0;
    }
  }

  // player 1
  Game.players[0].shipsAlive = 0;
  Game.players[0].shipsDead = 4;
  Game.players[0].status = SELECT;

  // player 2
  Game.players[1].shipsAlive = 0;
  Game.players[1].shipsDead = 4;
  Game.players[1].status = SELECT;
}

void Game_Draw(void){
  switch (Game.state) {
    case LANGUAGE_SELECT:
      //ST7735_FillScreen(ST7735_BLACK);
      ST7735_SetCursor(1, 1);
      ST7735_OutString("Select Language");
      ST7735_SetCursor(1, 3);
      ST7735_OutString("Left: English");
      ST7735_SetCursor(1, 4);
      ST7735_OutString("Right: Espanol");
      break;

    case PLAYER_ONE_SHIP_SELECT:
      //ST7735_FillScreen(ST7735_BLACK);

      // make sure input changed so that there's a reason to redraw the grid

      // clear grid position

      // print out previous grid

      for(int x = 0; x < 16; x ++){
        for(int y = 0; y < 11; y ++){
          if(Game.players[0].bitmapData[x][y] != 0){
            uint32_t realVal = Game.players[0].bitmapData[x][y] - 1;
            uint32_t r = (realVal % 4);
            uint32_t sizex = sizeCorresponding[(realVal - r)/4];
            if(sizex == 2){
              if(r == 0){
                ST7735_DrawBitmap(x*10, y*10, TwoLong[r], 10, sizex*10);
              }
              else {
                ST7735_DrawBitmap(x*10, y*10, TwoLong[r], sizex*10, 10);
              }
            }
            if(sizex == 3){
              if(r == 0){
                ST7735_DrawBitmap(x*10, y*10, ThreeLong[r], 10, sizex*10);
              }
              else {
                ST7735_DrawBitmap(x*10, y*10, ThreeLong[r], sizex*10, 10);
              }
            }
            if(sizex == 4){
              if(r == 0){
                ST7735_DrawBitmap(x*10, y*10, FourLong[r], 10, sizex*10);
              }
              else {
                ST7735_DrawBitmap(x*10, y*10, FourLong[r], sizex*10, 10);
              }
            }
            if(sizex == 5){
              if(r == 0){
                ST7735_DrawBitmap(x*10, y*10, FiveLong[r], 10, sizex*10);
              }
              else {
                ST7735_DrawBitmap(x*10, y*10, FiveLong[r], sizex*10, 10);
              }
            }
          }
        }
      }

      if((placingShip.prevShipX != placingShip.shipX) || (placingShip.prevShipY != placingShip.shipY)) {
      // Erase the old ship by drawing a black rectangle over it
      if((placingShip.r%2) == 0){
        ST7735_FillRect(placingShip.prevShipX, placingShip.prevShipY-placingShip.shipYsize, placingShip.shipXsize, placingShip.shipYsize+1, ST7735_BLACK);
      }
      if((placingShip.r%2) == 1){
        if(placingShip.shipX > placingShip.prevShipX){
          ST7735_FillRect(placingShip.prevShipX-placingShip.shipXsize, placingShip.prevShipY-placingShip.shipXsize+1, placingShip.shipYsize, placingShip.shipXsize, ST7735_BLACK); // if ship x is greater than prev shipx
        }
        if(placingShip.shipX < placingShip.prevShipX){
          ST7735_FillRect(placingShip.prevShipX+placingShip.shipXsize, placingShip.prevShipY-placingShip.shipXsize+1, placingShip.shipYsize, placingShip.shipXsize, ST7735_BLACK); // if shipx is less than prevshipx
        }
        if(placingShip.shipY < placingShip.prevShipY){
          ST7735_FillRect(placingShip.prevShipX, placingShip.prevShipY-placingShip.shipXsize+1, placingShip.shipYsize, placingShip.shipXsize, ST7735_BLACK); // if shipx is less than prevshipx
        }
        if(placingShip.shipY > placingShip.prevShipY){
          ST7735_FillRect(placingShip.prevShipX, placingShip.prevShipY-placingShip.shipXsize+1, placingShip.shipYsize, placingShip.shipXsize, ST7735_BLACK); // if shipx is less than prevshipx
        }
      }
      // Draw the ship at the new position

      // Odd
      if((placingShip.r%2) == 1){
        ST7735_DrawBitmap(placingShip.shipX, placingShip.shipY, placingShip.Length[placingShip.r], placingShip.shipYsize, placingShip.shipXsize);
      }
      // Even
      if((placingShip.r%2) == 0){
        ST7735_DrawBitmap(placingShip.shipX, placingShip.shipY, placingShip.Length[placingShip.r], placingShip.shipXsize, placingShip.shipYsize);
      }
      
      // Update previous position
      placingShip.prevShipX = placingShip.shipX;
      placingShip.prevShipY = placingShip.shipY;
    }

    if(placingShip.r != placingShip.rPrev)
    {
      if((placingShip.r%2) == 0){
        ST7735_FillRect(placingShip.prevShipX, placingShip.prevShipY-placingShip.shipXsize+1, placingShip.shipYsize, placingShip.shipXsize, ST7735_BLACK);
      }
      
      if((placingShip.r%2) == 1){
        ST7735_FillRect(placingShip.prevShipX, placingShip.prevShipY-placingShip.shipYsize, placingShip.shipXsize, placingShip.shipYsize+1, ST7735_BLACK);
      }
      // Odd
      if((placingShip.r%2) == 1){
        ST7735_DrawBitmap(placingShip.shipX, placingShip.shipY, placingShip.Length[placingShip.r], placingShip.shipYsize, placingShip.shipXsize);
      }
      // Even
      if((placingShip.r%2) == 0){
        ST7735_DrawBitmap(placingShip.shipX, placingShip.shipY, placingShip.Length[placingShip.r], placingShip.shipXsize, placingShip.shipYsize);
      }

      placingShip.rPrev = placingShip.r;
    }
    // Clock_Delay1ms(150); // delay 50 msec

      break;
    case PLAYER_TWO_SHIP_SELECT:
      ST7735_FillScreen(ST7735_BLUE);

      break;
    case PLAYER_ONE_PLAY:
      ST7735_FillScreen(ST7735_BLACK);

      break;
    case PLAYER_TWO_PLAY:
      ST7735_FillScreen(ST7735_BLACK);

      break;
    case GAME_OVER:
      ST7735_FillScreen(ST7735_BLACK);

      break;
  }
}


int main(void){
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  ADCinit();     //PB18 = ADC1 channel 5, slidepot
  Switch_Init(); // initialize switches
  LED_Init();    // initialize LED
  Sound_Init();  // initialize sound
  ST7735_InitPrintf();
  ST7735_SetRotation(1);
  
  ST7735_FillScreen(ST7735_BLACK);

  Game_Init(); // initiallizes the game

  TimerG12_IntArm(80000000/30, 2);

  __enable_irq();


  while(1){
    // Wait for semaphore from the Timer ISR
    while(!Game.semaphore) {};
    
    // Clear semaphore
    Game.semaphore = 0;
    Game_Draw();
  }


}

int main2(void){ // main2
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  Switch_Init();
  ST7735_InitPrintf();
  ST7735_SetRotation(1);
  
  ST7735_FillScreen(ST7735_BLACK);

  uint32_t r = 0;
  uint32_t rPrev = 0;

  uint32_t shipX = 0;
  uint32_t shipY = 20;

  uint32_t shipXsize = 10;
  uint32_t shipYsize = 20;

  uint32_t prevShipX = shipX;
  uint32_t prevShipY = shipY;

  uint32_t screenX = 160;
  uint32_t screenY = 110; // leaves 18 pixels at the bottom for a score
  
  while(1){
    //SmallFont_OutVertical(t,104,6); // top left
    
    // Only erase and redraw if position changed
    if((prevShipX != shipX) || (prevShipY != shipY)) {
      // Erase the old ship by drawing a black rectangle over it
      if((r%2) == 0){
        ST7735_FillRect(prevShipX, prevShipY-shipYsize, shipXsize, shipYsize+1, ST7735_BLACK);
      }
      if((r%2) == 1){
        if(shipX > prevShipX){
          ST7735_FillRect(prevShipX-shipXsize, prevShipY-shipXsize+1, shipYsize, shipXsize, ST7735_BLACK); // if ship x is greater than prev shipx
        }
        if(shipX < prevShipX){
          ST7735_FillRect(prevShipX+shipXsize, prevShipY-shipXsize+1, shipYsize, shipXsize, ST7735_BLACK); // if shipx is less than prevshipx
        }
        if(shipY < prevShipY){
          ST7735_FillRect(prevShipX, prevShipY-shipXsize+1, shipYsize, shipXsize, ST7735_BLACK); // if shipx is less than prevshipx
        }
        if(shipY > prevShipY){
          ST7735_FillRect(prevShipX, prevShipY-shipXsize+1, shipYsize, shipXsize, ST7735_BLACK); // if shipx is less than prevshipx
        }
      }
      // Draw the ship at the new position

      // Odd
      if((r%2) == 1){
        ST7735_DrawBitmap(shipX, shipY, TwoLong[r], shipYsize, shipXsize);
      }
      // Even
      if((r%2) == 0){
        ST7735_DrawBitmap(shipX, shipY, TwoLong[r], shipXsize, shipYsize);
      }
      
      // Update previous position
      prevShipX = shipX;
      prevShipY = shipY;
    }

    if(r != rPrev)
    {
      if((r%2) == 0){
        ST7735_FillRect(prevShipX, prevShipY-shipXsize+1, shipYsize, shipXsize, ST7735_BLACK);
      }
      
      if((r%2) == 1){
        ST7735_FillRect(prevShipX, prevShipY-shipYsize, shipXsize, shipYsize+1, ST7735_BLACK);
      }
      // Odd
      if((r%2) == 1){
        ST7735_DrawBitmap(shipX, shipY, TwoLong[r], shipYsize, shipXsize);
      }
      // Even
      if((r%2) == 0){
        ST7735_DrawBitmap(shipX, shipY, TwoLong[r], shipXsize, shipYsize);
      }

      rPrev = r;
    }    
    // Clock_Delay1ms(150); // delay 50 msec

    // Left
    uint32_t input = Switch_In();

    // Left
    if(input == LEFT){
        if(shipX >= 10) {
            shipX -= 10;
        } else {
            shipX = 0;
        }
        Clock_Delay1ms(150);
    }

    // Right
    if(input == RIGHT){
        // Define width based on rotation
        uint32_t current_width = (r % 2 == 0) ? shipXsize : shipYsize;
        
        shipX += 10;
        if(shipX > screenX - current_width) {
            shipX = screenX - current_width;
        }
        Clock_Delay1ms(150);
    }

    // Up
    if(input == UP){
        shipY -= 10;
        // Different minimum Y based on rotation
        uint32_t min_y = (r % 2 == 0) ? 20 : 10;
        
        if(shipY < min_y) {
            shipY = min_y;
        }
        Clock_Delay1ms(150);
    }

    // Down
    if(input == DOWN){
        shipY += 10;
        if(shipY > screenY) {
            shipY = screenY;
        }
        Clock_Delay1ms(150);
    }

    if(input == ROTATE){
      if((r % 2) == 0){
        if((shipX < 150)){
          r++;
        }
      }
      else if((r % 2) == 1){
        if((shipY > 10)){
          r++;
        }
      }
      if((r >= 4)){
        r = 0;
      }

      Clock_Delay1ms(150); // delay 50 msec
    }

    if(input == FIRE){
      ST7735_FillScreen(ST7735_BLACK);
      Clock_Delay1ms(75);
    }
  }
}

// use main3 to test switches and LEDs
int main3(void){ // main3
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  Switch_Init(); // initialize switches
  LED_Init(); // initialize LED
  while(1){
    // write code to test switches and LEDs
    
  }
}
// use main4 to test sound outputs
int main4(void){ uint32_t last=0,now;
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  Switch_Init(); // initialize switches
  LED_Init(); // initialize LED
  Sound_Init();  // initialize sound
  TExaS_Init(ADC0,6,0); // ADC1 channel 6 is PB20, TExaS scope
  __enable_irq();
  while(1){
    now = Switch_In(); // one of your buttons
    if((last == 0)&&(now == 1)){
      Sound_Shoot(); // call one of your sounds
    }
    if((last == 0)&&(now == 2)){
      Sound_Killed(); // call one of your sounds
    }
    if((last == 0)&&(now == 4)){
      Sound_Explosion(); // call one of your sounds
    }
    if((last == 0)&&(now == 8)){
      Sound_Fastinvader1(); // call one of your sounds
    }
    // modify this to test all your sounds
  }
}

// ALL ST7735 OUTPUT MUST OCCUR IN MAIN
int main5(void){ // final main
  __disable_irq();
  PLL_Init(); // set bus speed
  LaunchPad_Init();
  ST7735_InitPrintf();
    //note: if you colors are weird, see different options for
    // ST7735_InitR(INITR_REDTAB); inside ST7735_InitPrintf()
  ST7735_FillScreen(ST7735_BLACK);
  ADCinit();     //PB18 = ADC1 channel 5, slidepot
  Switch_Init(); // initialize switches
  LED_Init();    // initialize LED
  Sound_Init();  // initialize sound
  TExaS_Init(0,0,&TExaS_LaunchPadLogicPB27PB26); // PB27 and PB26
    // initialize interrupts on TimerG12 at 30 Hz
  TimerG12_IntArm(80000000/30,2);
  // initialize all data structures
  __enable_irq();

  while(1){
    // wait for semaphore
       // clear semaphore
       // update ST7735R
    // check for end game or level switch
  }
}
