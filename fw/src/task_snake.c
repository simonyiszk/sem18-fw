/*! *******************************************************************************************************
* Copyright (c) 2026 T. Szilagyi
*
* All rights reserved
*
* \file task_snake.c
*
* \brief Snake game
*
* \author T. Szilagyi
*
**********************************************************************************************************/

//--------------------------------------------------------------------------------------------------------/
// Include files
//--------------------------------------------------------------------------------------------------------/
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "types.h"
#include "tasks.h"
#include "lcd.h"
#include "buttons.h"
#include "housekeeping.h"

// Own include
#include "task_snake.h"


//--------------------------------------------------------------------------------------------------------/
// Definitions
//--------------------------------------------------------------------------------------------------------/

#define SNAKE_CELL_SIZE            (12u)

#define GRID_PIXEL_WIDTH           (SNAKE_GRID_WIDTH * SNAKE_CELL_SIZE)
#define GRID_PIXEL_HEIGHT          (SNAKE_GRID_HEIGHT * SNAKE_CELL_SIZE)

#define GRID_OFFSET_X              ((LCD_WIDTH - GRID_PIXEL_WIDTH) / 2u)
#define GRID_OFFSET_Y              ((LCD_HEIGHT - GRID_PIXEL_HEIGHT) / 2u)

#define SNAKE_MAX_LENGTH           (SNAKE_GRID_WIDTH * SNAKE_GRID_HEIGHT)

#define SCORE_TEXT_X               (170u)
#define SCORE_TEXT_Y               (2u)

#define OVERLAY_BOX_X              (40u)
#define OVERLAY_BOX_Y              (35u)
#define OVERLAY_BOX_W              (160u)
#define OVERLAY_BOX_H              (65u)

#define ARRAY_SIZE(x)              (sizeof(x) / sizeof((x)[0]))

//--------------------------------------------------------------------------------------------------------/
// Types
//--------------------------------------------------------------------------------------------------------/

typedef enum
{
  GAME_STATE_MAIN_MENU = 0,
  GAME_STATE_IN_GAME,
  GAME_STATE_PAUSED,
  GAME_STATE_GAME_OVER,
  GAME_STATE_WIN
} E_GAME_STATE;

typedef enum
{
  DIRECTION_UP = 0,
  DIRECTION_DOWN,
  DIRECTION_LEFT,
  DIRECTION_RIGHT
} E_DIRECTION;

typedef struct
{
  I16 i16X;
  I16 i16Y;
} S_POSITION;

typedef struct
{
  E_GAME_STATE eState;

  S_POSITION asSnake[SNAKE_MAX_LENGTH];
  U16 u16SnakeLength;

  S_POSITION asFood[SNAKE_MAX_FOOD];
  U8 u8FoodCount;

  E_DIRECTION eDirection;
  E_DIRECTION eNextDirection;

  U32 u32LastMoveTick;

  U16 u16Score;
} S_GAME_CONTEXT;

static S_GAME_CONTEXT g_sGame;

//--------------------------------------------------------------------------------------------------------/
// Static function declarations
//--------------------------------------------------------------------------------------------------------/

static void Game_Init(S_GAME_CONTEXT* psGame);
static void Game_Reset(S_GAME_CONTEXT* psGame);
static void Game_Update(S_GAME_CONTEXT* psGame);

static void Graphics_DrawMainMenu(void);
static void Graphics_DrawOverlay(const char* pcTitle);
static void Graphics_DrawScore(U16 u16OldScore, U16 u16NewScore);
static void Graphics_DrawCell(I16 i16GridX, I16 i16GridY, U16 u16Color);
static void Graphics_DrawSnakeHead(I16 i16GridX, I16 i16GridY);
static void Graphics_DrawSnakeBody(I16 i16GridX, I16 i16GridY);
static void Graphics_DrawFood(I16 i16GridX, I16 i16GridY);
static void Graphics_DrawEmpty(I16 i16GridX, I16 i16GridY);
static void Graphics_DrawFullGame(const S_GAME_CONTEXT* psGame);

static void Input_UpdateDirection(S_GAME_CONTEXT* psGame);

static BOOL Snake_IsOppositeDirection(E_DIRECTION eA, E_DIRECTION eB);
static BOOL Snake_IsOccupied(const S_GAME_CONTEXT* psGame, I16 i16X, I16 i16Y);
static BOOL Snake_IsFoodAt(const S_GAME_CONTEXT* psGame, I16 i16X, I16 i16Y, U8* pu8Index);
static void Snake_SpawnFood(S_GAME_CONTEXT* psGame);
static void Snake_RemoveFood(S_GAME_CONTEXT* psGame, U8 u8Index);

static void UIntToString(U16 u16Value, char* pcBuffer);

//--------------------------------------------------------------------------------------------------------/
// Static functions
//--------------------------------------------------------------------------------------------------------/


void Task_Snake_Init(void)
{
  Game_Init(&g_sGame);

  LCD_Clear(SNAKE_BACKGROUND_COLOR);
  Graphics_DrawMainMenu();
}

void Task_Snake_Cycle(void)
{
  Game_Update(&g_sGame);
}

static void Game_Init(S_GAME_CONTEXT* psGame)
{
  memset(psGame, 0, sizeof(S_GAME_CONTEXT));

  psGame->eState = GAME_STATE_MAIN_MENU;
  Graphics_DrawMainMenu();
}

static void Game_Reset(S_GAME_CONTEXT* psGame)
{
  I16 i16CenterX;
  I16 i16CenterY;

  LCD_Clear(SNAKE_BACKGROUND_COLOR);

  memset(psGame->asSnake, 0, sizeof(psGame->asSnake));
  memset(psGame->asFood, 0, sizeof(psGame->asFood));

  psGame->eState = GAME_STATE_IN_GAME;
  psGame->u16SnakeLength = SNAKE_INITIAL_LENGTH;
  psGame->u16Score = 0u;
  psGame->u8FoodCount = 0u;

  psGame->eDirection = DIRECTION_RIGHT;
  psGame->eNextDirection = DIRECTION_RIGHT;

  i16CenterX = (I16)(SNAKE_GRID_WIDTH / 2u);
  i16CenterY = (I16)(SNAKE_GRID_HEIGHT / 2u);

  psGame->asSnake[0].i16X = i16CenterX;
  psGame->asSnake[0].i16Y = i16CenterY;

  psGame->asSnake[1].i16X = i16CenterX - 1;
  psGame->asSnake[1].i16Y = i16CenterY;

  psGame->asSnake[2].i16X = i16CenterX - 2;
  psGame->asSnake[2].i16Y = i16CenterY;

  Snake_SpawnFood(psGame);

  psGame->u32LastMoveTick = HAL_GetTick();

  LCD_Clear(SNAKE_BACKGROUND_COLOR);
  Graphics_DrawFullGame(psGame);
}

static void Game_Update(S_GAME_CONTEXT* psGame)
{
  U32 u32Now;

  switch(psGame->eState)
  {
    case GAME_STATE_MAIN_MENU:
    {
      if(BUTTON_PRESSED == Buttons_GetEvent(BUTTON_SW4_PUSH))
      {
        Game_Reset(psGame);
      }

      if(BUTTON_PRESSED == Buttons_GetEvent(BUTTON_SW3))
      {
        Tasks_EndTask();
      }

      break;
    }

    case GAME_STATE_IN_GAME:
    {
      I16 i16NewX;
      I16 i16NewY;
      U16 u16Index;
      BOOL bGrow;
      U8 u8FoodIndex;
      U16 u16OldScore;

      if(BUTTON_PRESSED == Buttons_GetEvent(BUTTON_SW4_PUSH))
      {
        psGame->eState = GAME_STATE_PAUSED;
        Graphics_DrawOverlay("Paused");
        return;
      }

      if(BUTTON_PRESSED == Buttons_GetEvent(BUTTON_SW3))
      {
        Tasks_EndTask();
	  }

      Input_UpdateDirection(psGame);

      u32Now = HAL_GetTick();

      if((u32Now - psGame->u32LastMoveTick) < SNAKE_MOVE_INTERVAL_MS)
      {
        return;
      }

      psGame->u32LastMoveTick = u32Now;
      psGame->eDirection = psGame->eNextDirection;

      i16NewX = psGame->asSnake[0].i16X;
      i16NewY = psGame->asSnake[0].i16Y;

      switch(psGame->eDirection)
      {
        case DIRECTION_UP:
          i16NewY--;
          break;

        case DIRECTION_DOWN:
          i16NewY++;
          break;

        case DIRECTION_LEFT:
          i16NewX--;
          break;

        case DIRECTION_RIGHT:
          i16NewX++;
          break;

        default:
          break;
      }

      if((i16NewX < 0) ||
         (i16NewY < 0) ||
         (i16NewX >= (I16)SNAKE_GRID_WIDTH) ||
         (i16NewY >= (I16)SNAKE_GRID_HEIGHT))
      {
        psGame->eState = GAME_STATE_GAME_OVER;
        Graphics_DrawOverlay("Game Over");
        return;
      }

      for(u16Index = 0u; u16Index < psGame->u16SnakeLength; u16Index++)
      {
        if((psGame->asSnake[u16Index].i16X == i16NewX) &&
           (psGame->asSnake[u16Index].i16Y == i16NewY))
        {
          psGame->eState = GAME_STATE_GAME_OVER;
          Graphics_DrawOverlay("Game Over");
          return;
        }
      }

      bGrow = Snake_IsFoodAt(psGame, i16NewX, i16NewY, &u8FoodIndex);

      if(!bGrow)
      {
        Graphics_DrawEmpty(psGame->asSnake[psGame->u16SnakeLength - 1u].i16X,
                           psGame->asSnake[psGame->u16SnakeLength - 1u].i16Y);
      }

      for(u16Index = psGame->u16SnakeLength; u16Index > 0u; u16Index--)
      {
        psGame->asSnake[u16Index] = psGame->asSnake[u16Index - 1u];
      }

      psGame->asSnake[0].i16X = i16NewX;
      psGame->asSnake[0].i16Y = i16NewY;

      Graphics_DrawSnakeBody(psGame->asSnake[1].i16X,
                             psGame->asSnake[1].i16Y);

      Graphics_DrawSnakeHead(i16NewX, i16NewY);

      if(bGrow)
      {
        u16OldScore = psGame->u16Score;

        psGame->u16SnakeLength++;
        psGame->u16Score++;

        Snake_RemoveFood(psGame, u8FoodIndex);

        if(psGame->u16SnakeLength >= SNAKE_MAX_LENGTH)
        {
          psGame->eState = GAME_STATE_WIN;
          Graphics_DrawScore(u16OldScore, psGame->u16Score);
          Graphics_DrawOverlay("Victory");
          return;
        }

        while(psGame->u8FoodCount < SNAKE_MAX_FOOD)
        {
          Snake_SpawnFood(psGame);
        }

        Graphics_DrawScore(u16OldScore, psGame->u16Score);
      }

      break;
    }

    case GAME_STATE_PAUSED:
    {
      if(BUTTON_PRESSED == Buttons_GetEvent(BUTTON_SW4_PUSH))
      {
        psGame->eState = GAME_STATE_IN_GAME;
        Graphics_DrawFullGame(psGame);
      }

      if(BUTTON_PRESSED == Buttons_GetEvent(BUTTON_SW3))
      {
        Tasks_EndTask();
      }

      break;
    }

    case GAME_STATE_GAME_OVER:
    case GAME_STATE_WIN:
    {
      if(BUTTON_PRESSED == Buttons_GetEvent(BUTTON_SW4_PUSH))
      {
        Game_Reset(psGame);
      }

      if(BUTTON_PRESSED == Buttons_GetEvent(BUTTON_SW3))
      {
        Tasks_EndTask();
      }

      break;
    }

    default:
      break;
  }
}

static void Graphics_DrawCell(I16 i16GridX, I16 i16GridY, U16 u16Color)
{
  U16 u16X0;
  U16 u16Y0;
  U16 u16X1;
  U16 u16Y1;

  u16X0 = (U16)(GRID_OFFSET_X + (i16GridX * SNAKE_CELL_SIZE));
  u16Y0 = (U16)(GRID_OFFSET_Y + (i16GridY * SNAKE_CELL_SIZE));

  u16X1 = (U16)(u16X0 + SNAKE_CELL_SIZE - 1u);
  u16Y1 = (U16)(u16Y0 + SNAKE_CELL_SIZE - 1u);

  LCD_DrawFilledRectangle(u16X0,
                          u16Y0,
                          u16X1,
                          u16Y1,
                          u16Color);
}

static void Graphics_DrawSnakeHead(I16 i16GridX, I16 i16GridY)
{
  Graphics_DrawCell(i16GridX, i16GridY, SNAKE_HEAD_COLOR);
}

static void Graphics_DrawSnakeBody(I16 i16GridX, I16 i16GridY)
{
  Graphics_DrawCell(i16GridX, i16GridY, SNAKE_BODY_COLOR);
}

static void Graphics_DrawFood(I16 i16GridX, I16 i16GridY)
{
  Graphics_DrawCell(i16GridX, i16GridY, SNAKE_FOOD_COLOR);
}

static void Graphics_DrawEmpty(I16 i16GridX, I16 i16GridY)
{
  Graphics_DrawCell(i16GridX, i16GridY, SNAKE_BACKGROUND_COLOR);
}

static void Graphics_DrawFullGame(const S_GAME_CONTEXT* psGame)
{
  U16 u16X;
  U16 u16Y;
  U16 u16Index;

  for(u16Y = 0u; u16Y < SNAKE_GRID_HEIGHT; u16Y++)
  {
    for(u16X = 0u; u16X < SNAKE_GRID_WIDTH; u16X++)
    {
      Graphics_DrawEmpty((I16)u16X, (I16)u16Y);
    }
  }

  for(u16Index = 0u; u16Index < psGame->u8FoodCount; u16Index++)
  {
    Graphics_DrawFood(psGame->asFood[u16Index].i16X,
                      psGame->asFood[u16Index].i16Y);
  }

  for(u16Index = 1u; u16Index < psGame->u16SnakeLength; u16Index++)
  {
    Graphics_DrawSnakeBody(psGame->asSnake[u16Index].i16X,
                           psGame->asSnake[u16Index].i16Y);
  }

  Graphics_DrawSnakeHead(psGame->asSnake[0].i16X,
                         psGame->asSnake[0].i16Y);

  Graphics_DrawScore(0u, psGame->u16Score);
}

static void Graphics_DrawScore(U16 u16OldScore, U16 u16NewScore)
{
  char acBuffer[12];
  U16 u16X;
  U16 u16Y;

  (void)u16OldScore;

  for(u16Y = SCORE_TEXT_Y; u16Y < (SCORE_TEXT_Y + 10u); u16Y++)
  {
    for(u16X = SCORE_TEXT_X; u16X < LCD_WIDTH; u16X++)
    {
      LCD_Pixel((U8)u16X, (U8)u16Y, SNAKE_BACKGROUND_COLOR);
    }
  }

  UIntToString(u16NewScore, acBuffer);

  LCD_PrintString(SCORE_TEXT_X,
                  SCORE_TEXT_Y,
                  acBuffer,
                  LCD_FONT_7x10,
                  SNAKE_TEXT_COLOR,
                  SNAKE_BACKGROUND_COLOR);
}

static void Graphics_DrawMainMenu(void)
{
  LCD_PrintString(80u,
                  25u,
                  "Snake",
                  LCD_FONT_16x26,
                  GREEN,
                  SNAKE_BACKGROUND_COLOR);

  LCD_DrawFilledRectangle(70u,
                          70u,
                          170u,
                          100u,
                          GREEN);

  LCD_PrintString(98u,
                  78u,
                  "Play",
                  LCD_FONT_11x18,
                  SNAKE_BACKGROUND_COLOR,
                  GREEN);

  LCD_PrintString(20u,
                  118u,
                  "Middle = Quit",
                  LCD_FONT_7x10,
                  SNAKE_TEXT_COLOR,
                  SNAKE_BACKGROUND_COLOR);

  LCD_PrintString(120u,
                  118u,
                  "PUSH = Start",
                  LCD_FONT_7x10,
                  SNAKE_TEXT_COLOR,
                  SNAKE_BACKGROUND_COLOR);
}

static void Graphics_DrawOverlay(const char* pcTitle)
{
  LCD_DrawFilledRectangle(OVERLAY_BOX_X,
                          OVERLAY_BOX_Y,
                          OVERLAY_BOX_X + OVERLAY_BOX_W,
                          OVERLAY_BOX_Y + OVERLAY_BOX_H,
                          SNAKE_OVERLAY_COLOR);

  LCD_PrintString(OVERLAY_BOX_X + 20u,
                  OVERLAY_BOX_Y + 10u,
                  pcTitle,
                  LCD_FONT_11x18,
                  SNAKE_TEXT_COLOR,
                  SNAKE_OVERLAY_COLOR);

  LCD_PrintString(OVERLAY_BOX_X + 10u,
                  OVERLAY_BOX_Y + 38u,
                  "PUSH = Continue",
                  LCD_FONT_7x10,
                  SNAKE_TEXT_COLOR,
                  SNAKE_OVERLAY_COLOR);

  LCD_PrintString(OVERLAY_BOX_X + 10u,
                  OVERLAY_BOX_Y + 50u,
                  "Middle = Quit",
                  LCD_FONT_7x10,
                  SNAKE_TEXT_COLOR,
                  SNAKE_OVERLAY_COLOR);
}

static void Input_UpdateDirection(S_GAME_CONTEXT* psGame)
{
  if(BUTTON_PRESSED == Buttons_GetEvent(BUTTON_SW4_UP))
  {
    if(!Snake_IsOppositeDirection(psGame->eDirection, DIRECTION_UP))
    {
      psGame->eNextDirection = DIRECTION_UP;
    }
  }

  if(BUTTON_PRESSED == Buttons_GetEvent(BUTTON_SW4_DOWN))
  {
    if(!Snake_IsOppositeDirection(psGame->eDirection, DIRECTION_DOWN))
    {
      psGame->eNextDirection = DIRECTION_DOWN;
    }
  }

  if(BUTTON_PRESSED == Buttons_GetEvent(BUTTON_SW4_LEFT))
  {
    if(!Snake_IsOppositeDirection(psGame->eDirection, DIRECTION_LEFT))
    {
      psGame->eNextDirection = DIRECTION_LEFT;
    }
  }

  if(BUTTON_PRESSED == Buttons_GetEvent(BUTTON_SW4_RIGHT))
  {
    if(!Snake_IsOppositeDirection(psGame->eDirection, DIRECTION_RIGHT))
    {
      psGame->eNextDirection = DIRECTION_RIGHT;
    }
  }
}

static BOOL Snake_IsOppositeDirection(E_DIRECTION eA, E_DIRECTION eB)
{
  if(((eA == DIRECTION_UP) && (eB == DIRECTION_DOWN)) ||
     ((eA == DIRECTION_DOWN) && (eB == DIRECTION_UP)) ||
     ((eA == DIRECTION_LEFT) && (eB == DIRECTION_RIGHT)) ||
     ((eA == DIRECTION_RIGHT) && (eB == DIRECTION_LEFT)))
  {
    return TRUE;
  }

  return FALSE;
}

static BOOL Snake_IsOccupied(const S_GAME_CONTEXT* psGame, I16 i16X, I16 i16Y)
{
  U16 u16Index;

  for(u16Index = 0u; u16Index < psGame->u16SnakeLength; u16Index++)
  {
    if((psGame->asSnake[u16Index].i16X == i16X) &&
       (psGame->asSnake[u16Index].i16Y == i16Y))
    {
      return TRUE;
    }
  }

  return FALSE;
}

static BOOL Snake_IsFoodAt(const S_GAME_CONTEXT* psGame,
                           I16 i16X,
                           I16 i16Y,
                           U8* pu8Index)
{
  U8 u8Index;

  for(u8Index = 0u; u8Index < psGame->u8FoodCount; u8Index++)
  {
    if((psGame->asFood[u8Index].i16X == i16X) &&
       (psGame->asFood[u8Index].i16Y == i16Y))
    {
      *pu8Index = u8Index;
      return TRUE;
    }
  }

  return FALSE;
}

static void Snake_SpawnFood(S_GAME_CONTEXT* psGame)
{
  I16 i16X;
  I16 i16Y;
  BOOL bOccupied;
  U8 u8FoodIndex;

  if(psGame->u8FoodCount >= SNAKE_MAX_FOOD)
  {
    return;
  }

  do
  {
    i16X = (I16)(rand() % SNAKE_GRID_WIDTH);
    i16Y = (I16)(rand() % SNAKE_GRID_HEIGHT);

    bOccupied = Snake_IsOccupied(psGame, i16X, i16Y);

    if(!bOccupied)
    {
      bOccupied = Snake_IsFoodAt(psGame,
                                 i16X,
                                 i16Y,
                                 &u8FoodIndex);
    }
  }
  while(bOccupied);

  psGame->asFood[psGame->u8FoodCount].i16X = i16X;
  psGame->asFood[psGame->u8FoodCount].i16Y = i16Y;

  psGame->u8FoodCount++;

  Graphics_DrawFood(i16X, i16Y);
}

static void Snake_RemoveFood(S_GAME_CONTEXT* psGame, U8 u8Index)
{
  U8 u8MoveIndex;

  if(u8Index >= psGame->u8FoodCount)
  {
    return;
  }

  for(u8MoveIndex = u8Index;
      u8MoveIndex < (psGame->u8FoodCount - 1u);
      u8MoveIndex++)
  {
    psGame->asFood[u8MoveIndex] = psGame->asFood[u8MoveIndex + 1u];
  }

  psGame->u8FoodCount--;
}

static void UIntToString(U16 u16Value, char* pcBuffer)
{
  char acTemp[6];
  U8 u8Index;
  U8 u8Length;

  if(0u == u16Value)
  {
    pcBuffer[0] = '0';
    pcBuffer[1] = '\0';
    return;
  }

  u8Index = 0u;

  while(u16Value > 0u)
  {
    acTemp[u8Index] = (char)('0' + (u16Value % 10u));
    u16Value /= 10u;
    u8Index++;
  }

  u8Length = u8Index;

  for(u8Index = 0u; u8Index < u8Length; u8Index++)
  {
    pcBuffer[u8Index] = acTemp[u8Length - u8Index - 1u];
  }

  pcBuffer[u8Length] = '\0';
}
