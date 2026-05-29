/*! *******************************************************************************************************
* Copyright (c) 2023-2026 K. Sz. Horvath
*
* All rights reserved
*
* \file task_menu.c
*
* \brief Menu system
*
* \author K. Sz. Horvath
*
**********************************************************************************************************/

//--------------------------------------------------------------------------------------------------------/
// Include files
//--------------------------------------------------------------------------------------------------------/
#include <stdio.h>  // for snprintf
#include <string.h>
#include "main.h"
#include "types.h"
#include "lcd.h"
#include "buttons.h"
#include "housekeeping.h"
#include "tasks.h"
#include "task_clock.h"
#include "task_tetris.h"
#include "task_snake.h"

// Own include
#include "task_menu.h"


//--------------------------------------------------------------------------------------------------------/
// Definitions
//--------------------------------------------------------------------------------------------------------/
#define FONT       (LCD_FONT_11x18)  //!< Font used by this module
#define FONT_Y     (18u)             //!< Vertical size of the font in pixels
#define FONT_X     (11u)             //!< Horizontal size of the font in pixels
#define COLOR_INK  (WHITE)           //!< Color of the ink when writing to screen
#define COLOR_BG   (BLACK)           //!< Color of the background when writing to screen


//--------------------------------------------------------------------------------------------------------/
// Types
//--------------------------------------------------------------------------------------------------------/


//--------------------------------------------------------------------------------------------------------/
// Static function declarations
//--------------------------------------------------------------------------------------------------------/
static U32  CorrigateBCD( U32 u32BCD, BOOL bUp );
static void DrawMenu( void );
static void OnOffWidget( BOOL bDraw );
static void NumberSetWidget( BOOL bDraw );
static void FileSelectWidget( BOOL bRedraw );
static void LoadDirectory( void );


//--------------------------------------------------------------------------------------------------------/
// Global variables
//--------------------------------------------------------------------------------------------------------/
// Forward declarations -- extend if you want submenus
static const S_MENU gsMainMenu;

//NOTE: put submenus here

//! \brief Structure containing the clock calibration menu
static const S_MENU gsCalibrateClockMenu =
{
  3u,    // Number of items
  NULL,  // When entering
  NULL,  // When exiting
  {
    {
      .pu8ItemString = ">> Go back <<",
      .bSelectable = TRUE,
      .eAction = MENU_ACTION_SUBMENU,
      .pAction = &gsMainMenu
    },
    {
      .pu8ItemString = "The clock is late...",
      .bSelectable = TRUE,
      .eAction = MENU_ACTION_WIDGET_NUMSET,
      .pAction = &(S_MENU_WIDGET_NUMSET)
      {
        .pu8WidgetString = "Enter inaccuracy\nin ppm",
        .pu32NumberToChange = &gu32HousekeepingRTCCalibrationLateppm,
        .bDecimal = TRUE,   // BCD
        .u32FixPoint = 1u,  // 7.1 format
        .u32Min = 0x0000u,  // 0 ppm
        .u32Max = 0x4871u,  // 487.1 ppm
        .pfExitWidgetAction = Housekeeping_RTCCalibrateLate
      }
    },
    {
      .pu8ItemString = "The clock is fast...",
      .bSelectable = TRUE,
      .eAction = MENU_ACTION_WIDGET_NUMSET,
      .pAction = &(S_MENU_WIDGET_NUMSET)
      {
        .pu8WidgetString = "Enter inaccuracy\nin ppm",
        .pu32NumberToChange = &gu32HousekeepingRTCCalibrationFastppm,
        .bDecimal = TRUE,   // BCD
        .u32FixPoint = 1u,  // 7.1 format
        .u32Min = 0x0000u,  // 0 ppm
        .u32Max = 0x4885u,  // 488.5 ppm
        .pfExitWidgetAction = Housekeeping_RTCCalibrateFast
      }
    },
  }
};

//! \brief Structure containing the main (root) menu
static const S_MENU gsMainMenu =
{
  4u,    // Number of items
  NULL,  // When entering
  NULL,  // When exiting
  {
    {
      .pu8ItemString = "Clock",
      .bSelectable = TRUE,
      .eAction = MENU_ACTION_WIDGET_TASK,
      .pAction = &(S_MENU_WIDGET_TASK)
      {
        .pfnInitFunction = Task_Clock_Init,
        .pfnMainCycleFunction = Task_Clock_Cycle
      }
    },
    {
      .pu8ItemString = "Tetris",
      .bSelectable = TRUE,
      .eAction = MENU_ACTION_WIDGET_TASK,
      .pAction = &(S_MENU_WIDGET_TASK)
      {
        .pfnInitFunction = Task_Tetris_Init,
        .pfnMainCycleFunction = Task_Tetris_Cycle
      }
    },
    {
      .pu8ItemString = "Snake",
      .bSelectable = TRUE,
      .eAction = MENU_ACTION_WIDGET_TASK,
      .pAction = &(S_MENU_WIDGET_TASK)
      {
        .pfnInitFunction = Task_Snake_Init,
        .pfnMainCycleFunction = Task_Snake_Cycle
      }
    },
    {
      .pu8ItemString = "Calibrate clock",
      .bSelectable = TRUE,
      .eAction = MENU_ACTION_SUBMENU,
      .pAction = &gsCalibrateClockMenu
    },
    /*
    {
      .pu8ItemString = "Connect as USB drive",
      .bSelectable = TRUE,
      .eAction = MENU_ACTION_WIDGET_FUNCTION,
      .pAction = &(S_MENU_WIDGET_FUNCTION)
      {
        .bOpenNewPage = TRUE,
        .pu8WidgetString = "USB mass storage\ndrive connected.\n\nPress middle button\nto exit.",
        .pfnAction = NULL,
        .pfnActionOnEntry = NULL,//TUSB_IF_EnableMSD,
        .pfnActionOnExit = NULL//TUSB_IF_DisableMSD
      }
    },
    */
  }
};

//! \brief Currently selected menu element structure
static struct
{
  const S_MENU* psMenu;         //!< Pointer to the current menu
  U32           u32Index;       //!< Index of current element in the menu
  U32           u32ListScroll;  //!< Start displaying elements from this index
  BOOL          bIsActive;      //!< Element actively selected
} gsMenu;


//--------------------------------------------------------------------------------------------------------/
// Static functions
//--------------------------------------------------------------------------------------------------------/
/*! *******************************************************************
 * \brief  Corrigate BCD upwards or downwards
 * \param  u32BCD: the number that is supposed to be in BCD format
 * \param  bUp: set to TRUE when the number is increased
 * \return Correct BCD number
 *********************************************************************/
static U32 CorrigateBCD( U32 u32BCD, BOOL bUp )
{
  U32 u32Index;
  U32 u32Result = 0u;
  U8  u8Nibble;

  // Going through the nibbles from least to most significant
  for( u32Index = 0u; u32Index < 8u; u32Index++ )
  {
    u8Nibble = ( u32BCD & (0x0Fu<<(u32Index*4u)) )>>(u32Index*4u);
    if( u8Nibble >= 10u )
    {
      if( TRUE == bUp )
      {
        u8Nibble -= 10u;
        u32BCD += (0x01u<<((u32Index+1u)*4u));
      }
      else
      {
        u8Nibble -= 6u;
        //u32BCD -= (0x01u<<((u32Index+1u)*4u));
      }
    }
    u32Result |= (U32)u8Nibble<<(u32Index*4u);
  }

  return u32Result;
}

/*! *******************************************************************
 * \brief  Draws the menu on screen
 * \param  -
 * \return -
 *********************************************************************/
static void DrawMenu( void )
{
  U32 u32Index;
  
  // Start by clearing the contents of the display
  LCD_Clear( COLOR_BG );
  
  // Display the currently visible part of the menu
  for( u32Index = 0u; u32Index + gsMenu.u32ListScroll < gsMenu.psMenu->u32NumItems; u32Index++ )
  {
    // Check if the menu item would be able to fit the screen
    if( (u32Index+1u)*FONT_Y > LCD_HEIGHT )
    {
      break;
    }
    
    // Offset the item horizontally by 1 pixel
    LCD_PrintString( 1u, u32Index*(FONT_Y+1u), gsMenu.psMenu->asMenuItems[ u32Index + gsMenu.u32ListScroll ].pu8ItemString, FONT, COLOR_INK, COLOR_BG );
    
    // If the cursor is on the current item...
    if( ( u32Index + gsMenu.u32ListScroll ) == gsMenu.u32Index )
    {
      //...then highlight the menu item
      LCD_SwitchColorsInRectangle( 0u, u32Index*(FONT_Y+1u), LCD_WIDTH-1u,u32Index*(FONT_Y+1u)+FONT_Y, COLOR_INK, COLOR_BG );
    }
  }
}

/*! *******************************************************************
 * \brief  Implements an on/off widget
 * \param  bDraw: draw the widget now or only when needed
 * \return -
 *********************************************************************/
static void OnOffWidget( BOOL bDraw )
{
  const S_MENU_WIDGET_ONOFF* psParameters = gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction;
  
  // Check buttons and act accordingly
  if( ( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_SW4_DOWN ) )
   || ( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_SW4_UP ) ) )
  {
    // Set BOOL to opposite value
    *(psParameters->pbBoolToChange) = ( TRUE == *(psParameters->pbBoolToChange) ) ? FALSE : TRUE;
    bDraw = TRUE;
  }
  if( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_SW4_PUSH ) )
  {
    // Exit widget
    gsMenu.bIsActive = FALSE;
    bDraw = FALSE;
    if( NULL != psParameters->pfExitWidgetAction )
    {
      (psParameters->pfExitWidgetAction)();
    }
  }
  
  // Draw only if it's needed
  if( TRUE == bDraw )
  {
    LCD_Clear( BLACK );
    // Offset the widget description string horizontally by 1 pixel
    LCD_PrintString( 1u, 0u, psParameters->pu8WidgetString, LCD_FONT_11x18, COLOR_INK, COLOR_BG );
    
    // Draw on/off according to the current value of the BOOL
    if( TRUE == *(psParameters->pbBoolToChange) )
    {
      LCD_PrintString( (LCD_WIDTH/2u-1u*FONT_X), LCD_HEIGHT-2u*FONT_Y, "On", LCD_FONT_11x18, COLOR_INK, COLOR_BG );
      LCD_SwitchColorsInRectangle( (LCD_WIDTH/2u-2u*FONT_X), LCD_HEIGHT-2u*FONT_Y, (LCD_WIDTH/2u+2u*FONT_X), LCD_HEIGHT-1u*FONT_Y, COLOR_INK, COLOR_BG );
    }
    else
    {
      LCD_PrintString( (LCD_WIDTH/2u-1.5*FONT_X), LCD_HEIGHT-2u*FONT_Y, "Off", LCD_FONT_11x18, COLOR_INK, COLOR_BG );
      LCD_SwitchColorsInRectangle( (LCD_WIDTH/2u-2.5*FONT_X), LCD_HEIGHT-2u*FONT_Y, (LCD_WIDTH/2u+2.5*FONT_X), LCD_HEIGHT-1u*FONT_Y, COLOR_INK, COLOR_BG );
    }
  }
}

/*! *******************************************************************
 * \brief  Implements a number setting widget
 * \param  bDraw: draw the widget now or only when needed
 * \return -
 *********************************************************************/
static void NumberSetWidget( BOOL bDraw )
{
  const S_MENU_WIDGET_NUMSET* psParameters = gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction;
  static U8   u8Position = 8u;    // default: exit widget
  static BOOL bSelected = FALSE;  // default: digit not selected yet
  U32         u32Length;
  char        au8ValueStr[ 11u ];
  
  // Check buttons and act accordingly
  if( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_SW4_DOWN ) )
  {
    if( TRUE == psParameters->bDecimal )  // if decimal values
    {
      // Get the length of the number displayed
      if( 0u != psParameters->u32FixPoint )
      {
        // With fractional point
        u32Length = (U32)snprintf( (char*)au8ValueStr, sizeof( au8ValueStr ), "%lX.%lX", *(psParameters->pu32NumberToChange)>>(4u*psParameters->u32FixPoint), *(psParameters->pu32NumberToChange) & (0xFFFFFFFFu>>(32u - 4u*psParameters->u32FixPoint)) );
        u32Length--;  // the fractional point is not part of the value
      }
      else
      {
        // No fractional point
        u32Length = (U32)snprintf( (char*)au8ValueStr, sizeof( au8ValueStr ), "%lX", *(psParameters->pu32NumberToChange) );
      }
      if( TRUE != bSelected )
      {
        if( u8Position > 8u-u32Length )
        {
          u8Position--;
        }
      }
      else  // decrease value
      {
        if( *(psParameters->pu32NumberToChange) != psParameters->u32Min )  // only when it's not the minimal number
        {
          *(psParameters->pu32NumberToChange) -= 1u<<(4u*(u32Length-(8u-u8Position)));
          *(psParameters->pu32NumberToChange) = CorrigateBCD( *(psParameters->pu32NumberToChange), FALSE );
          if( ( *(psParameters->pu32NumberToChange) < psParameters->u32Min )
           || ( *(psParameters->pu32NumberToChange) > psParameters->u32Max ) )
          {
            *(psParameters->pu32NumberToChange) = psParameters->u32Min;
          }
        }
      }
    }
    else  // hex
    {
      if( TRUE != bSelected )
      {
        if( u8Position > 0u )
        {
          u8Position--;
        }
      }
      else  // decrease value
      {
        if( *(psParameters->pu32NumberToChange) != psParameters->u32Min )  // only when it's not the minimal number
        {
          *(psParameters->pu32NumberToChange) -= 1u<<(4u*u8Position);
          if( ( *(psParameters->pu32NumberToChange) < psParameters->u32Min )
           || ( *(psParameters->pu32NumberToChange) > psParameters->u32Max ) )
          {
            *(psParameters->pu32NumberToChange) = psParameters->u32Min;
          }
        }
      }
    }
    bDraw = TRUE;
  }
  if( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_SW4_UP ) )
  {
    if( TRUE == psParameters->bDecimal )  // if decimal values
    {
      // Get the length of the number displayed
      if( 0u != psParameters->u32FixPoint )
      {
        // With fractional point
        u32Length = (U32)snprintf( (char*)au8ValueStr, sizeof( au8ValueStr ), "%lX.%lX", *(psParameters->pu32NumberToChange)>>(4u*psParameters->u32FixPoint), *(psParameters->pu32NumberToChange) & (0xFFFFFFFFu>>(32u - 4u*psParameters->u32FixPoint)) );
        u32Length--;  // the fractional point is not part of the value
      }
      else
      {
        // No fractional point
        u32Length = (U32)snprintf( (char*)au8ValueStr, sizeof( au8ValueStr ), "%lX", *(psParameters->pu32NumberToChange) );
      }
      if( TRUE != bSelected )
      {
        if( u8Position < 8u )
        {
          u8Position++;
        }
      }
      else  // increase value
      {
        if( *(psParameters->pu32NumberToChange) != psParameters->u32Max )  // only when it's not the maximal number
        {
          *(psParameters->pu32NumberToChange) += 1u<<(4u*(u32Length-(8u-u8Position)));
          *(psParameters->pu32NumberToChange) = CorrigateBCD( *(psParameters->pu32NumberToChange), TRUE );
          if( ( *(psParameters->pu32NumberToChange) > psParameters->u32Max )
           || ( *(psParameters->pu32NumberToChange) < psParameters->u32Min ) )
          {
            *(psParameters->pu32NumberToChange) = psParameters->u32Max;
          }
        }
      }
    }
    else  // hex
    {
      if( TRUE != bSelected )
      {
        if( u8Position < 8u )
        {
          u8Position++;
        }
      }
      else  // increase value
      {
        if( *(psParameters->pu32NumberToChange) != psParameters->u32Max )  // only when it's not the maximal number
        {
          *(psParameters->pu32NumberToChange) += 1u<<(4u*u8Position);
          if( ( *(psParameters->pu32NumberToChange) > psParameters->u32Max )
           || ( *(psParameters->pu32NumberToChange) < psParameters->u32Min ) )
          {
            *(psParameters->pu32NumberToChange) = psParameters->u32Max;
          }
        }
      }
    }
    bDraw = TRUE;
  }
  if( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_SW4_PUSH ) )
  {
    if( 8u <= u8Position )
    {
      // Exit widget
      gsMenu.bIsActive = FALSE;
      bDraw = FALSE;
      bSelected = FALSE;
      u8Position = 8u;
      if( NULL != psParameters->pfExitWidgetAction )
      {
        (psParameters->pfExitWidgetAction)();
      }
    }
    else  // select digit
    {
      bSelected = (TRUE == bSelected) ? FALSE : TRUE;
      bDraw = TRUE;
    }
  }
  
  // Draw only if it's needed
  if( TRUE == bDraw )
  {
    LCD_Clear( COLOR_BG );
    // Offset the widget description string horizontally by 1 pixel
    LCD_PrintString( 1u, 0u, psParameters->pu8WidgetString, FONT, COLOR_INK, COLOR_BG );
    
    // Print value depending on the type
    if( TRUE == psParameters->bDecimal )
    {
      // Print BCD number
      if( 0u != psParameters->u32FixPoint )
      {
        // With fractional point
        u32Length = (U32)snprintf( (char*)au8ValueStr, sizeof( au8ValueStr ), "%lX.%lX", *(psParameters->pu32NumberToChange)>>(4u*psParameters->u32FixPoint), *(psParameters->pu32NumberToChange) & (0xFFFFFFFFu>>(32u - 4u*psParameters->u32FixPoint)) );
      }
      else
      {
        // No fractional point
        u32Length = (U32)snprintf( (char*)au8ValueStr, sizeof( au8ValueStr ), "%lX", *(psParameters->pu32NumberToChange) );
      }
      au8ValueStr[ sizeof(au8ValueStr)-1u ] = 0u;
      LCD_PrintString( LCD_WIDTH/2u-u32Length*FONT_X/2u, LCD_HEIGHT-2u*FONT_Y, au8ValueStr, FONT, COLOR_INK, COLOR_BG );
      
      // Print cursor
      if( 8u > u8Position )
      {
        // Select only one digit
        if( TRUE == bSelected )
        {
          // The value is selected for change
          if( u32Length-(7u-u8Position)-((0u != psParameters->u32FixPoint)?1u:0u) > psParameters->u32FixPoint )
          {
            // Digits left from the decimal/binary point
            LCD_SwitchColorsInRectangle( LCD_WIDTH/2u-u32Length*FONT_X/2u+(7u-u8Position)*FONT_X-1u, LCD_HEIGHT-2u*FONT_Y-(FONT_Y/3), LCD_WIDTH/2u-u32Length*FONT_X/2u+(7u-u8Position+1u)*FONT_X, LCD_HEIGHT-1u*FONT_Y+(FONT_Y/3), COLOR_INK, COLOR_BG );
          }
          else
          {
            // Digits right from the decimal/binary point
            LCD_SwitchColorsInRectangle( LCD_WIDTH/2u-u32Length*FONT_X/2u+(7u-u8Position+1u)*FONT_X-1u, LCD_HEIGHT-2u*FONT_Y-(FONT_Y/3), LCD_WIDTH/2u-u32Length*FONT_X/2u+(7u-u8Position+2u)*FONT_X, LCD_HEIGHT-1u*FONT_Y+(FONT_Y/3), COLOR_INK, COLOR_BG );
          }
        }
        else
        {
          // Just scrolling through the digits
          if( u32Length-(7u-u8Position)-((0u != psParameters->u32FixPoint)?1u:0u) > psParameters->u32FixPoint )
          {
            // Digits left from the decimal/binary point
            LCD_SwitchColorsInRectangle( LCD_WIDTH/2u-u32Length*FONT_X/2u+(7u-u8Position)*FONT_X, LCD_HEIGHT-2u*FONT_Y-(FONT_Y/3), LCD_WIDTH/2u-u32Length*FONT_X/2u+(7u-u8Position+1u)*FONT_X, LCD_HEIGHT-1u*FONT_Y+(FONT_Y/3), COLOR_INK, COLOR_BG );
          }
          else
          {
            // Digits right from the decimal/binary point
            LCD_SwitchColorsInRectangle( LCD_WIDTH/2u-u32Length*FONT_X/2u+(7u-u8Position+1u)*FONT_X, LCD_HEIGHT-2u*FONT_Y-(FONT_Y/3), LCD_WIDTH/2u-u32Length*FONT_X/2u+(7u-u8Position+2u)*FONT_X, LCD_HEIGHT-1u*FONT_Y+(FONT_Y/3), COLOR_INK, COLOR_BG );
          }
        }
      }
      else
      {
        // Exit widget selected, invert the whole value
        LCD_SwitchColorsInRectangle( LCD_WIDTH/2u-u32Length*FONT_X/2u-FONT_X/2u, LCD_HEIGHT-2u*FONT_Y-(FONT_Y/3), LCD_WIDTH/2u+u32Length*FONT_X/2u + FONT_X/2u, LCD_HEIGHT-1u*FONT_Y+(FONT_Y/3), COLOR_INK, COLOR_BG );
      }
    }
    else  // Hexadecimal parameter
    {
      // Print hex value
      (void)snprintf( (char*)au8ValueStr, sizeof( au8ValueStr ), "0x%08lX", *(psParameters->pu32NumberToChange) );
      au8ValueStr[ sizeof(au8ValueStr)-1u ] = 0u;
      LCD_PrintString( LCD_WIDTH/2u-8u*FONT_X/2u, LCD_HEIGHT-2u*FONT_Y, au8ValueStr, FONT, COLOR_INK, COLOR_BG );
      
      // Print cursor
      if( 8u > u8Position )
      {
        // Select only one digit
        if( TRUE == bSelected )
        {
          // The value is selected for change
          LCD_SwitchColorsInRectangle( LCD_WIDTH/2u-8u*FONT_X/2u+(7u-u8Position)*FONT_X-1u, LCD_HEIGHT-2u*FONT_Y-(FONT_Y/3), LCD_WIDTH/2u-8u*FONT_X/2u+(7u-u8Position+1u)*FONT_X, LCD_HEIGHT-1u*FONT_Y+(FONT_Y/3), COLOR_INK, COLOR_BG );
        }
        else
        {
          // Just scrolling through the digits
          LCD_SwitchColorsInRectangle( LCD_WIDTH/2u-8u*FONT_X/2u+(7u-u8Position)*FONT_X, LCD_HEIGHT-2u*FONT_Y, LCD_WIDTH/2u-8u*FONT_X/2u+(7u-u8Position+1u)*FONT_X, LCD_HEIGHT-1u*FONT_Y, COLOR_INK, COLOR_BG );
        }
      }
      else
      {
        // Exit widget selected, invert the whole value
        LCD_SwitchColorsInRectangle( LCD_WIDTH/2u-9u*FONT_X/2u, LCD_HEIGHT-2u*FONT_Y, LCD_WIDTH/2u+9u*FONT_X/2u, LCD_HEIGHT-1u*FONT_Y, COLOR_INK, COLOR_BG );
      }
    }
  }
}

/*! *******************************************************************
 * \brief  Implements a file selection widget
 * \param  bDraw: draw the widget now or only when needed
 * \return
 *********************************************************************/
static void FileSelectWidget( BOOL bRedraw )
{
  //TODO: implement when necessary
  /*
  const S_MENU_WIDGET_FILESELECT* psParameters = gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction;
  BOOL bSuccess = TRUE;
  BOOL bRootDir = FALSE;
  U32  u32Line;
  U32  u32Index;
  
  // Check buttons and act accordingly
  if( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_DOWN ) )
  {
    if( 6u > gsFileSelect.u32Cursor )
    {
      gsFileSelect.u32Cursor++;
    }
    else
    {
      if( gsFileSelect.u32NumberOfFilesInDir > gsFileSelect.u32ListScroll + 7u )
      {
        gsFileSelect.u32ListScroll++;
      }
      else if( 7u > gsFileSelect.u32Cursor )
      {
        gsFileSelect.u32Cursor++;
      }
    }
    bRedraw = TRUE;
  }
  if( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_UP ) )
  {
    if( 1u < gsFileSelect.u32Cursor )
    {
      gsFileSelect.u32Cursor--;
    }
    else
    {
      if( 0u < gsFileSelect.u32ListScroll )
      {
        gsFileSelect.u32ListScroll--;
      }
      else if( 0u < gsFileSelect.u32Cursor )
      {
        gsFileSelect.u32Cursor--;
      }
    }
    bRedraw = TRUE;
  }
  // Exit if both directions are pressed
  if( ( BUTTON_ACTIVE == gaeButtonsState[ BUTTON_DOWN ] )
   && ( BUTTON_ACTIVE == gaeButtonsState[ BUTTON_UP ] ) )
  {
    // Exit widget without changing anything
    gsMenu.bIsActive = FALSE;
    bRedraw = FALSE;
    (void)f_closedir( &gsFileSelect.sDirectory );
  }
  // If the file is selected
  if( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_SEL ) )
  {
    // Count the index of the selected item
    (void)f_rewinddir( &gsFileSelect.sDirectory );
    for( u32Index = 0u; u32Index < (gsFileSelect.u32Cursor + gsFileSelect.u32ListScroll ); u32Index++ )
    {
      (void)f_readdir( &gsFileSelect.sDirectory, &gsFileSelect.sFileInfo );
    }
    // From now on, gsFileSelect.sFileInfo contains the information about the selected file
    // If ".." is selected
    if( 0u == gsFileSelect.u32Cursor + gsFileSelect.u32ListScroll )
    {
      // Go 1 directory closer to root
      for( u32Index = strnlen( (char*)gsFileSelect.au8Path, sizeof( gsFileSelect.au8Path ) ); u32Index > 0u; u32Index-- )  // going from the end of the string
      {
        if( ( '\\' == gsFileSelect.au8Path[ u32Index ] )
           || ( '/'  == gsFileSelect.au8Path[ u32Index ] ) )
        {
          gsFileSelect.au8Path[ u32Index ] = 0u;
          break;
        }
      }
      if( 0u == u32Index )
      {
        // Root directory
        gsFileSelect.au8Path[ 0u ] = 0u;
        bRootDir = TRUE;
      }
    }
    else
    {
      // Concatenate the file name to the end of the path
      u32Index =  strnlen( (char*)gsFileSelect.au8Path, sizeof( gsFileSelect.au8Path ) );
      if( 0u != u32Index )  // don't concatenate if the path is in the root
      {
        gsFileSelect.au8Path[ u32Index++ ] = '/';
      }
      gsFileSelect.au8Path[ u32Index ] = 0u;
      (void)strncpy( (char*)&gsFileSelect.au8Path[ u32Index ], gsFileSelect.sFileInfo.fname, sizeof( gsFileSelect.au8Path ) - u32Index );
    }
    
    // If we reached the root directory
    if( TRUE == bRootDir )
    {
      // Enter directory
      (void)f_closedir( &gsFileSelect.sDirectory );
      (void)f_opendir( &gsFileSelect.sDirectory, (char*)gsFileSelect.au8Path );
      gsFileSelect.u32Cursor = 0u;
      gsFileSelect.u32ListScroll = 0u;
      LoadDirectory();
      bRedraw = TRUE;
    }
    // Check file info
    else if( FR_OK == f_stat( (char*)gsFileSelect.au8Path, &gsFileSelect.sFileInfo ) )
    {
      // If it's a directory
      if( 0u != ( gsFileSelect.sFileInfo.fattrib & (BYTE)AM_DIR ) )
      {
        // Enter directory
        (void)f_closedir( &gsFileSelect.sDirectory );
        (void)f_opendir( &gsFileSelect.sDirectory, (char*)gsFileSelect.au8Path );
        gsFileSelect.u32Cursor = 0u;
        gsFileSelect.u32ListScroll = 0u;
        LoadDirectory();
        bRedraw = TRUE;
      }
      else  // it's a file
      {
        // Check if the file is valid
        if( NULL != psParameters->pfnFileCheckerFunction )
        {
          bSuccess = (psParameters->pfnFileCheckerFunction)( gsFileSelect.au8Path );
        }
        if( TRUE == bSuccess )
        {
          // Set file by storing its path
          (void)strncpy( (char*)psParameters->pu8FilePathBuffer, (char*)gsFileSelect.au8Path, psParameters->u32FilePathBufferSize );
          
          // Call exit widget function
          if( NULL != psParameters->pfExitWidgetAction )
          {
            (psParameters->pfExitWidgetAction)();
          }
          
          // Exit widget
          gsMenu.bIsActive = FALSE;
          bRedraw = FALSE;
          (void)f_closedir( &gsFileSelect.sDirectory );
        }
      }
    }
    else
    {
      ErrorHandler_Fatal( __FILE__, __LINE__, (U32)psParameters->pu8FilePathBuffer, 0u, 0u );
    }
  }
  
  // Draw only if it's needed
  if( TRUE == bRedraw )
  {
    Display_Clear();

    // Set first list element based on the current scroll value
    (void)f_rewinddir( &gsFileSelect.sDirectory );
    for( u32Line = 1u; u32Line < gsFileSelect.u32ListScroll; u32Line++ )
    {
      (void)f_readdir( &gsFileSelect.sDirectory, &gsFileSelect.sFileInfo );
    }
    
    // Display filenames in each line
    for( u32Line = 0u; u32Line < 8u; u32Line++ )
    {
      if( ( 0u == u32Line ) && ( 0u == gsFileSelect.u32ListScroll ) )
      {
        // Display ".."
        Display_Print( 0u, u32Line*8u, "\002.." );
        continue;
      }
      if( FR_OK == f_readdir( &gsFileSelect.sDirectory, &gsFileSelect.sFileInfo ) )
      {
        // If file/directory exists
        if( 0u != gsFileSelect.sFileInfo.fname[ 0u ] )
        {
          gsFileSelect.sFileInfo.fname[ 20u ] = 0u;  // limit file name display
          Display_Print( 7u, u32Line*8u, (U8*)gsFileSelect.sFileInfo.fname );
          if( 0u != ( gsFileSelect.sFileInfo.fattrib & (BYTE)AM_DIR ) )
          {
            // Directory pictogram
            Display_Print( 0u, u32Line*8u, "\002" );
          }
          else  // file
          {
            // File pictogram
            Display_Print( 0u, u32Line*8u, "\020" );
          }
        }
      }
    }
    
    // Display cursor
    Display_InvertRectangle( 0u, gsFileSelect.u32Cursor*(FONT_Y+1u), 127u, gsFileSelect.u32Cursor*(FONT_Y+1u)+8u );
  }
  */
}

/*! *******************************************************************
 * \brief  Load information about the opened directory
 * \param  -
 * \return -
 *********************************************************************/
static void LoadDirectory( void )
{
  //TODO: implement when necessary
  /*
  gsFileSelect.u32NumberOfFilesInDir = 0u;
  (void)f_rewinddir( &gsFileSelect.sDirectory );
  // Load all elements from the directory
  while( 1u )
  {
    if( ( FR_OK != f_readdir( &gsFileSelect.sDirectory, &gsFileSelect.sFileInfo ) )
     || ( 0u == gsFileSelect.sFileInfo.fname[ 0u ] ) )
    {
      break;
    }
    gsFileSelect.u32NumberOfFilesInDir++;
  }
  (void)f_rewinddir( &gsFileSelect.sDirectory );
  */
}

/*! *******************************************************************
 * \brief  Implements a function call widget
 * \param  bDraw: draw the widget now or only when needed
 * \return -
 *********************************************************************/
static void FunctionCallWidget( BOOL bDraw )
{
  const S_MENU_WIDGET_FUNCTION* psParameters = gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction;
  
  // Exit if SW3 is pressed
  if( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_SW3 ) )
  {
    // Execute exit function if exists
    if( NULL != ((S_MENU_WIDGET_FUNCTION*)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction)->pfnActionOnExit )
    {
      ((S_MENU_WIDGET_FUNCTION*)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction)->pfnActionOnExit();
    }
    // Exit widget
    gsMenu.bIsActive = FALSE;
    bDraw = FALSE;
  }
  
  // Execute function if the middle button is pressed
  if( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_SW4_PUSH ) )
  {
    // Execute function only if it's specified
    if( NULL != ((S_MENU_WIDGET_FUNCTION*)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction)->pfnAction )
    {
      ((S_MENU_WIDGET_FUNCTION*)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction)->pfnAction();
    }
  }
  
  // Draw only if it's needed
  if( TRUE == bDraw )
  {
    LCD_Clear( COLOR_BG );
    // Offset the widget description string horizontally by 1 pixel
    LCD_PrintString( 1u, 0u, psParameters->pu8WidgetString, FONT, COLOR_INK, COLOR_BG );
  }
}

/*! *******************************************************************
 * \brief
 * \param
 * \return
 *********************************************************************/

/*! *******************************************************************
 * \brief
 * \param
 * \return
 *********************************************************************/


//--------------------------------------------------------------------------------------------------------/
// Interface functions
//--------------------------------------------------------------------------------------------------------/
/*! *******************************************************************
 * \brief  Initialize layer
 * \param  -
 * \return -
 *********************************************************************/
void Task_Menu_Init( void )
{
  // Start from root menu first element
  gsMenu.bIsActive = FALSE;
  gsMenu.u32Index = 0u;
  gsMenu.u32ListScroll = 0u;
  gsMenu.psMenu = &gsMainMenu;
  // Set button repeat function
  Buttons_SetRepeatedPresses( BUTTON_SW1, FALSE );
  Buttons_SetRepeatedPresses( BUTTON_SW2, FALSE );
  Buttons_SetRepeatedPresses( BUTTON_SW3, FALSE );
  Buttons_SetRepeatedPresses( BUTTON_SW4_UP, TRUE );
  Buttons_SetRepeatedPresses( BUTTON_SW4_DOWN, TRUE );
  Buttons_SetRepeatedPresses( BUTTON_SW4_LEFT, TRUE );
  Buttons_SetRepeatedPresses( BUTTON_SW4_RIGHT, TRUE );
  Buttons_SetRepeatedPresses( BUTTON_SW4_PUSH, FALSE );
  // Draw the menu
  DrawMenu();
}

/*! *******************************************************************
 * \brief  Draw menu on screen and handle its inputs
 * \param  -
 * \return -
 *********************************************************************/
void Task_Menu_Cycle( void )
{
  BOOL bRedraw = FALSE;
  U32  u32Index, u32Scroll;
  
  // Normal menu
  if( TRUE != gsMenu.bIsActive )
  {
    // Check buttons and act accordingly
    if( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_SW4_DOWN ) )
    {
      if( gsMenu.u32Index < gsMenu.psMenu->u32NumItems-1u )
      {
        u32Index = gsMenu.u32Index;
        u32Scroll = gsMenu.u32ListScroll;
        // Select next item, and repeat when the next item is not selectable
        do
        {
          // Next item
          if( ( u32Scroll < gsMenu.psMenu->u32NumItems - 8u )
           && ( u32Index - u32Scroll > 3u ) )
          {
            u32Scroll++;
          }
          u32Index++;
          // Rollback when we are at the end, yet we didn't find any selectable item
          if( u32Index >= gsMenu.psMenu->u32NumItems )
          {
            u32Index = gsMenu.u32Index;
            u32Scroll = gsMenu.u32ListScroll;
            break;
          }
        }
        while( TRUE != gsMenu.psMenu->asMenuItems[ u32Index ].bSelectable );
        // Store new indexes
        gsMenu.u32Index = u32Index;
        gsMenu.u32ListScroll = u32Scroll;
        bRedraw = TRUE;
      }
    }
    if( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_SW4_UP ) )
    {
      if( gsMenu.u32Index > 0u )
      {
        u32Index = gsMenu.u32Index;
        u32Scroll = gsMenu.u32ListScroll;
        // Select next item, and repeat when the next item is not selectable
        do
        {
          // Previous one
          if( ( u32Scroll > 0u )
           && ( u32Index - gsMenu.u32ListScroll < 4u ) )
          {
            u32Scroll--;
          }
          u32Index--;
          // Rollback when we are at the first item, yet we didn't find any selectable item
          if( u32Index == -1 )
          {
            u32Index = gsMenu.u32Index;
            u32Scroll = gsMenu.u32ListScroll;
            break;
          }
        }
        while( TRUE != gsMenu.psMenu->asMenuItems[ u32Index ].bSelectable );
        // Store new indexes
        gsMenu.u32Index = u32Index;
        gsMenu.u32ListScroll = u32Scroll;
        bRedraw = TRUE;
      }
    }
    if( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_SW4_PUSH ) )
    {
      switch( gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].eAction )
      {
        case MENU_ACTION_SUBMENU:
          // Execute exit function if applicable
          if( NULL != gsMenu.psMenu->pfnActionOnExit ) gsMenu.psMenu->pfnActionOnExit();
          // Enter the new menu
          gsMenu.psMenu = gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction;
          gsMenu.u32Index = 0u;
          gsMenu.u32ListScroll = 0u;
          // Execute entry function if applicable
          if( NULL != gsMenu.psMenu->pfnActionOnEntry ) gsMenu.psMenu->pfnActionOnEntry();
          break;
          
        case MENU_ACTION_WIDGET_NUMSET:
        case MENU_ACTION_WIDGET_ONOFF:
          gsMenu.bIsActive = TRUE;
          break;

        case MENU_ACTION_WIDGET_FILE:
          //TODO: implement when necessary
          /*
          // Make a copy of the path to be selected
          (void)memcpy( gsFileSelect.au8Path, ((S_MENU_WIDGET_FILESELECT*)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction)->pu8FilePathBuffer, sizeof( gsFileSelect.au8Path ) );
          // Truncate file path so it will point to the directory
          gsFileSelect.au8Path[ sizeof( gsFileSelect.au8Path ) - 1u ] = 0u;  // make sure that the string is terminated
          for( u32Index = strnlen( (char*)gsFileSelect.au8Path, sizeof( gsFileSelect.au8Path ) )-1u; u32Index > 0u; u32Index-- )  // going from the end of the string
          {
            if( ( '\\' == gsFileSelect.au8Path[ u32Index ] )
             || ( '/'  == gsFileSelect.au8Path[ u32Index ] ) )
            {
              break;
            }
          }
          gsFileSelect.au8Path[ u32Index ] = 0u;
          // Open directory
          if( FR_OK != f_opendir( &gsFileSelect.sDirectory, (char*)gsFileSelect.au8Path ) )
          {
            // Invalid directory, start from the root
            gsFileSelect.au8Path[ 0u ] =  0u;
            (void)f_opendir( &gsFileSelect.sDirectory, (char*)gsFileSelect.au8Path );
          }
          LoadDirectory();
          gsFileSelect.u32Cursor = 0u;
          gsFileSelect.u32ListScroll = 0u;
          gsMenu.bIsActive = TRUE;
          */
          break;
          
        case MENU_ACTION_WIDGET_FUNCTION:
          gsMenu.bIsActive = ((S_MENU_WIDGET_FUNCTION*)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction)->bOpenNewPage;
          if( TRUE != gsMenu.bIsActive )  // if it does not open a new page
          {
            // Execute function
            if( NULL != ((S_MENU_WIDGET_FUNCTION*)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction)->pfnAction )
            {
              ((S_MENU_WIDGET_FUNCTION*)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction)->pfnAction();
            }
          }
          else  // It opens a new page
          {
            // Execute entry function
            if( NULL != ((S_MENU_WIDGET_FUNCTION*)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction)->pfnActionOnEntry )
            {
              ((S_MENU_WIDGET_FUNCTION*)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction)->pfnActionOnEntry();
            }
          }
          break;
          
        case MENU_ACTION_WIDGET_TASK:
          Tasks_Change( ((S_MENU_WIDGET_TASK*)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction)->pfnInitFunction, ((S_MENU_WIDGET_TASK*)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].pAction)->pfnMainCycleFunction );
          return;  // we end the menu task
          break;

        default:
          //TODO: error handling
          //ErrorHandler_Fatal( __FILE__, __LINE__, (U32)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].eAction, gsMenu.u32Index, 0u );
          break;
      }
      bRedraw = TRUE;
    }
    
    // Redraw menu only if its contents changed
    if( TRUE == bRedraw )
    {
      DrawMenu();
    }

    // Middle button push: exit
    if( BUTTON_PRESSED == Buttons_GetEvent( BUTTON_SW3 ) )
    {
      Tasks_EndTask();
    }
  }

  // If there is an active widget
  if( TRUE == gsMenu.bIsActive )
  {
    // Branch based on the widget
    switch( gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].eAction )
    {
      case MENU_ACTION_WIDGET_FUNCTION:  // Function call widget
        FunctionCallWidget( bRedraw );
        break;
        
      case MENU_ACTION_WIDGET_ONOFF:  // On/off widget
        OnOffWidget( bRedraw );
        break;
        
      case MENU_ACTION_WIDGET_NUMSET:  // Number set widget
        NumberSetWidget( bRedraw );
        break;
        
      case MENU_ACTION_WIDGET_FILE:  // File selection widget
        FileSelectWidget( bRedraw );
        break;
        
      default:  // Not implemented widget type
	//TODO: error handling
        //ErrorHandler_Fatal( __FILE__, __LINE__, (U32)gsMenu.psMenu->asMenuItems[ gsMenu.u32Index ].eAction, gsMenu.u32Index, 0u );
        break;
    }
    // Check if the widget exited -- if yes, redraw the menu
    if( TRUE != gsMenu.bIsActive )
    {
      DrawMenu();
    }
  }
}

/*! *******************************************************************
 * \brief
 * \param
 * \return
 *********************************************************************/



//-----------------------------------------------< EOF >--------------------------------------------------/
