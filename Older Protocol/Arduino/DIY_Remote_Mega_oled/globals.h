//=============================================================================
//Project DIY Remote
// Globals.h - Global definitions for the sketch
//Software version: V1.0
//Date: 29-10-2009
//Programmer:   Kurt Eckhardt(KurtE) converted to C and Arduino
//=============================================================================
#ifndef _GLOBALS_H_
#define _GLOBALS_H_
#if ARDUINO>99
#include <Arduino.h> // Arduino 1.0
#else
#include <Wprogram.h> // Arduino 0022 or 23
#endif

#include "Display.h"

#ifdef __AVR__
#if defined(UBRR2H)
#define DBGSerial        Serial
#define XBeeSerial       Serial2
#define OLEDSerial        Serial1
#else
  // Lets try for ATMega644 board
//#define DBGSerial        Serial
#define XBeeSerial       Serial
#define OLEDSerial        Serial1
#endif  
#endif

// Define our display modes
enum {MODE_NORMAL=0, MODE_CHANGE_DEST_LIST, MODE_CHANGE_MY, MODE_CALIBRATE, MODE_MAX};


//==============================================================================
// [EEPROM] Define EEPROM memory locations
//==============================================================================
#define CMAXNDLIST	        16              // currently the max number of nodes we will cache
						// Currently set to 8 as to make it easy to align for 32 byte increments...
#define CBNIMAX		        14		// currently we will only keep 14 bytes per name

#define JoystickRangesDMStart    0x0		// 12 bytes - Stores Mins/Maxs of joysticks/sliders
#define XBeeDMStart		0x80	        // 4 Bytes - Stores the XBee My and DL we want

#define XBeeNDDMCache		0x88	        // 2 bytes - (count and checksum) How many items do we have cached out
						// followed by My array, SNL, SNH, and text strings... reserve 220 bytes for this!
#define XBNDDM_AMY		0x90	        // Start my at page.  
#define XBNDDM_ASL		(XBNDDM_AMY+2*CMAXNDLIST)	        // Serial number low									
#define XBNDDM_ASH		(XBNDDM_ASL+4*CMAXNDLIST)	        // Serial number high
#define XBNDDM_ANDI		(XBNDDM_ASH+4*CMAXNDLIST)	        // Start node names here...



extern void MSound(byte cNotes, ...);

#endif

