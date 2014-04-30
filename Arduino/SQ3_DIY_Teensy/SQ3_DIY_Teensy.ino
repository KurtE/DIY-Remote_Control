

// Some of my boards/shields have external EEPROMs as some processors don't have internal EEPROMS
#ifdef USE_I2CEEROM
#include <I2CEEProm.h>
#endif

//=============================================================================
//Project Lynxmotion Phoenix
//Description: Phoenix software
//Software version: V2.0
//Date: 29-10-2009
//Programmer: Jeroen Janssen [aka Xan]
//         Kurt Eckhardt(KurtE) converted to C and Arduino
//   Kåre Halvorsen aka Zenta - Makes everything work correctly!     
//
// This version of the Phoenix code was ported over to the Arduino Environement
// and is specifically configured for the Lynxmotion BotBoarduino 
//
//=============================================================================
//
//KNOWN BUGS:
//    - Lots ;)
//NOTES:
// - Requires the Borboarduino and the SSC-32.
// - See Hex_CFG.h to onfigure options and see what pins are used.
// - Install all the provided libraires (those specific versions are required)
// - Update the SSC-32 firmware to its latest version. You can find it at: 
//     http://www.lynxmotion.com/p-395-ssc-32-servo-controller.aspx
//=============================================================================
// Header Files
//=============================================================================

#define DEFINE_HEX_GLOBALS
#if ARDUINO>99
#include <Arduino.h>
#else
#endif
#include <Wire.h>
#include <EEPROM.h>
#include "Quad_CFG.h"
#include <Phoenix.h>
#ifdef QUADMODE
#define ADD_GAITS
#define PYPOSE_GAIT_SPEED 30

#ifdef DISPLAY_GAIT_NAMES
extern "C" {
  // Move the Gait Names to program space...
  const char s_szAGN1[] PROGMEM = "Ripple 8";
  const char s_szAGN2[] PROGMEM = "Cross Leg";
  const char s_szAGN3[] PROGMEM = "Amble";
};  
#endif

PHOENIXGAIT APG_EXTRA[] = { 
  {PYPOSE_GAIT_SPEED, 8, 2, 1, 2, 6, 1, 0, 0,0, true, {7, 1, 3, 5} GAITNAME(s_szAGN1)},   // ripple
  {PYPOSE_GAIT_SPEED, 12, 2, 1, 2, 10, 1, 0, 0,0, true, {7, 1, 4, 10} GAITNAME(s_szAGN2)},   // ripple
  {PYPOSE_GAIT_SPEED, 4, 2, 1, 2, 2, 1, 0, 0, 0, true,{3, 1, 1, 3} GAITNAME(s_szAGN3)},  // Amble
//  {PYPOSE_GAIT_SPEED, 6, 3, 2, 2, 3, 2, 0, 0,0, true, {1, 4, 4, 1}}  // Smooth Amble 
};
#endif
#include <diyxbee.h>
#include <Phoenix_Input_DIYXbee.h>
#include <ServoEx.h>
#include <phoenix_driver_ServoEx.h>
#include <Phoenix_Code.h>

