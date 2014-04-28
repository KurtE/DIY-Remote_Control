
//=============================================================================
//Project DIY Remote
//Software version: V1.0
//Date: July 2011
//Programmer:   Kurt Eckhardt(KurtE) converted to C and Arduino
// This version is geared around my DIY Remote board using an ATMega644P
//=============================================================================
//
// Seeeduino Mega Pro...
//
//#define DEBUG_SAVED_LIST
//

//KNOWN BUGS:
//    - Lots ;)
//
//=============================================================================
// Header Files
//=============================================================================
#include <EEPROM.h>
//#include <Streaming.h>
#include "globals.h"
#include "diyxbee.h"
#include <pins_arduino.h>

//=============================================================================
// Tables
//=============================================================================
static const uint16_t c_KeypadMapping[] = {
  1, 4, 7, 0,
  2, 5, 8, 0xf,
  3, 6, 9, 0xe,
  0xa, 0xb, 0xc, 0xd};

//=============================================================================
// Defines
//=============================================================================
// Lets try to define which IO pins will be used for What
//
//A1-A6 - PKT_RJOYLR, PKT_RJOYUD, PKT_LJOYLR, PKT_LJOYUD, PKT_RSLIDER, PKT_LSLIDER,	
//A7-A9 - PKT_RPOT, PKT_LPOT, PKT_MSLIDER};
//X7 - Battery pin...
// Analog information
#define BATTERYANALOG 0
#define FIRSTANALOG  1
#define NUMANALOGS    9


// Define command buttons.
#define CMDB_NEXT    0x1    // Above
#define CMDB_PREV    0x8    // Below
#define CMDB_ENTR    0x4    // left
#define CMDB_CNCL    0x2    // right
// Some battery defines
#define BATTERY_PIN    13    // Analog pin
#define BATTERY_MIN    256    // Min Analog value to use to report 0 256 is about 5V
#define BATTERY_MAX    384    // Max analog value to report 100% about 7.5 v  = (V * 1024) / 20

// Kepad on 
#define FIRST_KEYPAD_ROWPIN    24  // 0-7 used for Keypad
#define FIRST_KEYPAD_COLPIN    29
// 8,9, 10, 11 - Used for 2 Usarts
// 12, 14, 15, 16 - Command buttons - Could combine with keypad like before...
// 
#define CMDB_BTN1    2
#define CMDB_BTN2    3
#define CMDB_BTN3    4
#define CMDB_BTN4    5
static const byte s_abCMDBtns[] PROGMEM = {
  CMDB_BTN1,CMDB_BTN2, CMDB_BTN3, CMDB_BTN4};  
static const byte s_abExtraBtns[] PROGMEM = {
  12, 11};      // Extra buttons...

#define SOUND_PIN      6    // Teensy board
//#define SOUND_PIN    21        // Mega shield pin numbers

//=============================================================================
// Global Variables
//=============================================================================
uint16_t    wKeypadPrev;

// Mode information
byte              g_bMode;                                       // What mode are we in...
boolean           g_fNewMode;                                    // Have we just entered this mode?

uint16_t              g_aawRawAnalog[8][NUMANALOGS];                 // Keep 8 raw values for each analog input
uint16_t              g_awRawBatAnalog[8];
uint16_t              g_awRawSums[NUMANALOGS];                       // sum of the raw values.
uint16_t              g_wRawBatterySum;
byte              g_bCmdButtons;                                // which command buttons are pressed
byte              g_bCmdButtonsPrev;                            // which were set before...
byte              g_bExtendedAnalogMask;                        // Our Extended AnalogMask

static const byte g_aiRawToDIYP[] = {
  PKT_RJOYLR, PKT_RJOYUD, PKT_LJOYLR, PKT_LJOYUD, PKT_RSLIDER, PKT_LSLIDER,	
  PKT_RPOT, PKT_LPOT, PKT_MSLIDER};

static const boolean  g_afAnalogUseMids[] = {
  1,1,1,1,0,0,1,1, 0};       // Do we use the mid points when we normalize
static const uint16_t g_awAnalogMins[] = {
  0*8,0*8,0*8,0*8,0*8,0*8, 0*8, 0*8, 0*8};  // Will have a calibrate later where we read these in (if necessary)
static const uint16_t g_awAnalogMaxs[] = {
  1023*8,1023*8,1023*8,1023*8,1023*8,1023*8, 1023*8, 1023*8, 1023*8}; // dito

uint16_t              g_awAnalogMids[NUMANALOGS];                // This is our observed center points at init time. 
int    g_iRaw;                   // which raw value are we going to read into

uint16_t    g_aw0Prev;
boolean g_fShowDebugPrompt;
boolean g_fDebugOutput;

byte    g_bDispLoopCntr;                // Use this to decide which fields we should display per loop

RemoteDisplay g_display;                // Define our display here.


// Globals used in some of the config code.
byte            g_cNDList;                    // count of items in the list is saved ND list 
byte            g_iNDList;                   // Index to current one
uint16_t            g_wNewVal;                    // The new value
unsigned long   g_ulTimeLastMsg;             // Time of last message;
// forward definitions.
extern int TermReadln(byte *psz, byte cbMax);

DIYPACKET diyp;        // Our data packet we will send out...
DIYPACKET diypSave;

//--------------------------------------------------------------------------
// SETUP: the main arduino setup function.
//--------------------------------------------------------------------------
void setup(){
  int error;

#ifdef DBGSerial
  delay(3000);
  DBGSerial.begin(38400);
  DBGSerial.write("Program Start\n\r");
#endif    
  MSound(4, 100, 440, 100, 494, 100, 523, 100, 588);
  g_fDebugOutput = false;			// start with it off!
  g_fShowDebugPrompt = true;

  InitKeypad();            // Lets initialize the keypad
  InitJoysticks();          // Initialize the joysticks.
  g_display.Init();

  // Init the XBee - this will also give a delay to show the OLED info...
  GetSavedXBeeInfo();
  InitXBee();            // Now lets initialize The XBee

    g_bMode = MODE_NORMAL;
  g_display.SwitchToDisplayMode(MODE_NORMAL);
  g_ulTimeLastMsg = (unsigned long)-1;

  // Lets see what is actually on the XBee
#ifdef DBGSerial
  DBGSerial.print("XBee My: ");
  DBGSerial.print(GetXBeeHVal('M', 'Y'), HEX);
  DBGSerial.print(" DL: ");
  DBGSerial.println(GetXBeeHVal('D', 'L'), HEX);
#endif

  APISetXBeeHexVal('M', 'Y', g_diystate.wXBeeMY);
  SetXBeeDL(g_diystate.wXBeeDL);

  // Lets try to output some data to the status line...
  char szMyT[16];
  char szDLT[16];
  sprintf(szMyT, "My: %x", g_diystate.wXBeeMY);
  sprintf(szDLT, "Paired: %x", g_diystate.wAPIDL);
  g_display.DisplayStatus(szMyT, szDLT);

#ifdef DBGSerial
  DBGSerial.print("My : ");
  DBGSerial.print(g_diystate.wXBeeMY, HEX);
  DBGSerial.print(" Paired with: ");
  DBGSerial.println(g_diystate.wAPIDL, HEX);
#endif    
  g_bDispLoopCntr = 0;
}

//=============================================================================
// Loop: the main arduino main Loop function
//=============================================================================
void loop(void)
{
  char ch;
  byte bMask;
  uint16_t w;
  byte i;
  // We also have a simple debug monitor that allows us to 
  // check things. call it here..
  if (TerminalMonitor())
    return;           
  diyp.s.wButtons = ReadKeypad(&ch);
  byte bJoyVal;

  ReadJoysticks();


  // Extra buttons on new DIY...
  diyp.s.bButtons2 = 0;
  for (i=0; i < sizeof(s_abExtraBtns)/sizeof(s_abExtraBtns[0]); i++) {
    if (!digitalRead(pgm_read_byte(&s_abExtraBtns[i])))
      diyp.s.bButtons2 |= (1 << i);
  }

  // And check to see if any of the CommandButtons are pressed...
  // Could split this off... 
  g_bCmdButtons = 0;
  for (i=0; i < sizeof(s_abCMDBtns)/sizeof(s_abCMDBtns[0]); i++) {
    if (digitalRead(pgm_read_byte(&s_abCMDBtns[i])))
      g_bCmdButtons |= (1 << i);
  }
  if (g_bCmdButtons != g_bCmdButtonsPrev) {
    if (g_fDebugOutput) {
      Serial.print("Cmd Button: ");
      Serial.println(g_bCmdButtons, HEX);
    }

    if ((g_bCmdButtons & CMDB_NEXT) && !(g_bCmdButtonsPrev & CMDB_NEXT)) {
      // Next button pressed.
      g_bMode++;
      if (g_bMode >= MODE_MAX)        // Handle wrap around...
        g_bMode = MODE_NORMAL;
      g_display.SwitchToDisplayMode(g_bMode);
      g_ulTimeLastMsg = (unsigned long)-1;
      g_fNewMode = true;
    } 
    else if ((g_bCmdButtons & CMDB_PREV) && !(g_bCmdButtonsPrev & CMDB_PREV)) {
      if (g_bMode)
        g_bMode--;        // decrement
      else
        g_bMode = MODE_MAX-1;    // wrap back    
      g_display.SwitchToDisplayMode(g_bMode);
      g_fNewMode = true;
      g_ulTimeLastMsg = (unsigned long)-1;
    }
  }

  switch (g_bMode) {
  case MODE_NORMAL:
    // Now lets call off to maybe output packet...
    CheckAndTransmitDataPacket(&diyp);

    g_display.DisplayConnectStatus(FLastXBeeWriteSucceeded());

    if (diyp.s.wButtons != wKeypadPrev) {
      g_display.DisplayKey(ch);
    }

    // Lets display some joystick data...
    g_display.StartDispalyUpdate();
    switch (g_bDispLoopCntr) {
    case 0:
      g_display.DisplayJoystick(0, diyp.s.bLJoyLR,  diyp.s.bLJoyUD, diyp.s.bLPot);
      break;
    case 1:
      g_display.DisplayJoystick(1, diyp.s.bRJoyLR,  diyp.s.bRJoyUD, diyp.s.bRPot);
      break;
    case 2:
      g_display.DisplaySliders(diyp.s.bLSlider, diyp.s.bMSlider, diyp.s.bRSlider); 
      break;
    case 3:
#ifdef DEBUG_BATVOLT        
      if (g_fDebugOutput) {
        DBGSerial <<"V: " << _DEC(g_wRawBatterySum/8) << " " << _DEC((g_wRawBatterySum/8)*25/128) << " " <<
          _DEC(((g_wRawBatterySum-(BATTERY_MIN*8))*100)/((BATTERY_MAX-BATTERY_MIN)*8)) << endl;
      }    
#endif            
      // Lets output Battery voltage value - We keep a running sum of the last 8 reads, which we will average for the display          
      g_display.DisplayBatStatus(0, (g_wRawBatterySum < (BATTERY_MIN*8))? 0 :( (g_wRawBatterySum > (BATTERY_MAX*8))? 100 
        : ((g_wRawBatterySum-(BATTERY_MIN*8))*100)/((BATTERY_MAX-BATTERY_MIN)*8)));

      // lets try to display our battery condition.

      break; 
    }
    g_bDispLoopCntr = (g_bDispLoopCntr + 1) & 0x3;    // update for what we will display next...

    CheckAndTransmitDataPacket(&diyp);    // see if request came in during display

#ifdef DEBUG
    if (g_fDebugOutput) {
      if (memcmp(&diyp, &diypSave, sizeof(diyp))) {
        memcpy(&diypSave, &diyp, sizeof(diyp)); 
        DBGSerial << _HEX(diyp.s.wButtons) << " : " << _DEC(diyp.s.bRJoyLR) << " " << _DEC(diyp.s.bRJoyUD) << 
          " " << _DEC(diyp.s.bLJoyLR) << " " <<_DEC(diyp.s.bLJoyUD) << " : " <<
          _DEC(diyp.s.bRSlider) << " " << _DEC(diyp.s.bLSlider)	<< " : " <<
          _DEC( diyp.s.bRPot) << " " << _DEC(diyp.s.bLPot) << " - " << _HEX(diyp.s.bButtons2) << endl ;
      }

    }
#endif        
    break;
  case MODE_CHANGE_DEST_LIST:
    ChangeDestMode();
    break;

  case MODE_CHANGE_MY:
    ChangeMyMode();
    break;

  case MODE_CALIBRATE:
    CalibrateMode();
    break;    
  } // End of our mode case statement...

  // Only at end of pass update some previous states as function may need to know if anything changed...
  wKeypadPrev = diyp.s.wButtons;
  g_bCmdButtonsPrev = g_bCmdButtons;
  memcpy(&diypSave, &diyp, sizeof(diyp));     // save away the last packet
}

//==============================================================================
// ChangeMyMode - Handles display and UI when we are in the change My
//==============================================================================
void ChangeMyMode(void) {
  uint16_t w;
  byte i;
  if (g_fNewMode) {
    g_fNewMode = false;

    // Display our initial informatation for this mode.
    g_display.MMDisplayCurMY(g_diystate.wXBeeMY);
    g_wNewVal = g_diystate.wXBeeMY;
    g_display.MMDisplayNewMY(g_wNewVal);
  }

  // See if one of the keypad buttons is pressed.     
  if (diyp.s.wButtons && (diyp.s.wButtons != wKeypadPrev)) {  // should probably handle cases of multiple buttons...
    // need to convert the bit number to hex value...
    w = diyp.s.wButtons;
    for (i=0; !(w & 1) && (i<16); i++) {
      w = w >> 1;
    }

    g_wNewVal = (g_wNewVal << 4) | i;
    g_display.MMDisplayNewMY(g_wNewVal);
  }        

  // See if the user clicks on the OK button, to apply the new value.
  if ((g_bCmdButtons & CMDB_ENTR) && (g_bCmdButtons != g_bCmdButtonsPrev)) {

    g_diystate.wXBeeMY = g_wNewVal;
    APISetXBeeHexVal('M', 'Y', g_diystate.wXBeeMY);

    // and update saved XBee info
    UpdateSavedXBeeInfo();

    g_display.MMDisplayCurMY(g_diystate.wXBeeMY);
  }    

}

//==============================================================================
// CDM_DisplayNIItem - Display an NDList Item
//==============================================================================
void CDM_DisplayNIItem() {
  // Lets read in the text for this node
  char ab[CBNIMAX+1];
  uint16_t w;
  byte j;

  // First lets get the My(DL) for this item...
  g_wNewVal = (EEPROM.read(XBNDDM_AMY + g_iNDList*2) << 8) + EEPROM.read(XBNDDM_AMY+g_iNDList*2+1);
  g_display.DMDisplayNewDL(g_wNewVal);

  // Then lets get the Node Identifier text for this item...
  w =  g_iNDList * CBNIMAX;
  for (j=0; j < CBNIMAX; j++) {
    ab[j] = EEPROM.read(XBNDDM_ANDI+w+j);
  }
  ab[CBNIMAX]=0;
  g_display.DMDisplayNI(ab, g_diystate.wXBeeDL == g_wNewVal);

}

//==============================================================================
// ChangeDestMode - Handles display and UI when we are in the change destination
//         mode, 
//==============================================================================
void ChangeDestMode(void) {
  uint16_t w;
  byte i;
  boolean fCurDL;
  if (g_fNewMode) {
    // Display our initial informatation for this mode.

    g_display.DMDisplayCurDL(g_diystate.wXBeeDL);
    g_wNewVal = g_diystate.wXBeeDL;
    g_display.DMDisplayNewDL(g_wNewVal);
    g_fNewMode = false;

    // Lets see if our current DL is in our saved list, if so lets get the index...
    g_cNDList = EEPROM.read(XBeeNDDMCache);
    if (g_cNDList> CMAXNDLIST)
      g_cNDList = 0;

    for (g_iNDList = 0; g_iNDList < g_cNDList; g_iNDList++) {
      w = (EEPROM.read(XBNDDM_AMY + g_iNDList*2) << 8) + EEPROM.read(XBNDDM_AMY+g_iNDList*2+1);
      if (fCurDL = (w == g_wNewVal))
        break;
    }

    if ( g_iNDList == g_cNDList)
      g_iNDList = 0;    // did not find... display first one (if any)

    // Display information about this item...    
    if (g_cNDList) {
      CDM_DisplayNIItem();
    }            
  }

  // Lets see if the user wants us to do a scan for new XBee Devices...
  if ((diyp.s.bButtons2 & 1) && !(diypSave.s.bButtons2 & 1)) {
    g_display.DMDisplayStatus("Start Scan");
    UpdateNDListFromXBee();        // Ok lets see if we can find any new XBees out there...
    g_display.DMDisplayStatus("Completed");
    g_fNewMode = true;    // setup to reinitialize things.
  }


  // Handle scrolling through list with right joystick up/down
  if (g_cNDList) {
    // Only process if the last time was > some time threshold...
    // Hack only do this once per movement, could/should use timer...
#ifdef DEBUG        
    if (g_fDebugOutput) {
      if (((diyp.s.bRJoyUD < 64) && (diypSave.s.bRJoyUD >= 64)) ||
        ((diyp.s.bRJoyUD > 196) && (diypSave.s.bRJoyUD <= 196))) {

        Serial << _DEC(diyp.s.bRJoyUD) << ":" << _DEC(diypSave.s.bRJoyUD) << ":" << _DEC(g_iNDList) << ":" << _DEC(g_cNDList) << endl;
      }
    }        
#endif

    if ((diyp.s.bRJoyUD < 64) && (diypSave.s.bRJoyUD >= 64) && (g_iNDList > 0)) {
      g_iNDList--;
      CDM_DisplayNIItem();

    } 
    else if ((diyp.s.bRJoyUD > 196) && (diypSave.s.bRJoyUD <= 196) && (g_iNDList < (g_cNDList-1))) {
      g_iNDList++;
      CDM_DisplayNIItem();
    }
  }


  // See if one of the keypad buttons is pressed.     
  if (diyp.s.wButtons && (diyp.s.wButtons != wKeypadPrev)) {  // should probably handle cases of multiple buttons...
    // need to convert the bit number to hex value...
    w = diyp.s.wButtons;
    for (i=0; !(w & 1) && (i<16); i++) {
      w = w >> 1;
    }

    g_wNewVal = (g_wNewVal << 4) | i;
    g_display.DMDisplayNewDL(g_wNewVal);
  }        

  // See if the user clicks on the OK button, to apply the new value.
  if ((g_bCmdButtons & CMDB_ENTR) && (g_bCmdButtons != g_bCmdButtonsPrev)) {

    g_diystate.wXBeeDL = g_wNewVal;
    g_diystate.wAPIDL = g_wNewVal;
    SetXBeeDL(g_diystate.wXBeeDL);

    // and update saved XBee info
    UpdateSavedXBeeInfo();

    g_display.DMDisplayCurDL(g_diystate.wXBeeDL);
  }    

}


//==============================================================================
// CalibrateMode: Go through and Calibrate all of the joysticks from their minimum
//     to maximum values to allow us to better output a standardized range...
//==============================================================================
void CalibrateMode(void) {
}


//==============================================================================
// TerminalMonitor - Simple background task checks to see if the user is asking
//    us to do anything, like update debug levels ore the like.
//==============================================================================
boolean TerminalMonitor(void)
{
#ifdef DBGSerial
  byte szCmdLine[20];
  int ich;
  int ch;
  // See if we need to output a prompt.
  if (g_fShowDebugPrompt) {
    Serial.println("Arduino DIY Remote Monitor");
    Serial.println("D - Toggle debug on or off");
    Serial.println("R - Choose Remote Dest");
    Serial.println("Mxxxx - Set our address in hex");
    Serial.println("A - Test Analog");
    Serial.println("E - Dump Eprom");
    g_fShowDebugPrompt = false;
  }

  // First check to see if there is any characters to process.
  while (Serial.peek() == 10)    
    Serial.read();        // strip off any leading LF characters...

  if (Serial.available()) {
    ich = TermReadln(szCmdLine, sizeof(szCmdLine));
    Serial.print("Serial Cmd Line:");        
    Serial.write(szCmdLine, ich);
    Serial.println("!!!");

    // So see what are command is.
    if (ich == 0) {
      g_fShowDebugPrompt = true;
    } 
    else if ((ich == 1) && ((szCmdLine[0] == 'd') || (szCmdLine[0] == 'D'))) {
      g_fDebugOutput = !g_fDebugOutput;
      if (g_fDebugOutput) 
        Serial.println("Debug is on");
      else
        Serial.println("Debug is off");
    } 
    else if ((ich == 1) && ((szCmdLine[0] == 'r') || (szCmdLine[0] == 'R'))) {
      TermChooseXBeeDL();

    } 
    else if ((szCmdLine[0] == 'm') || (szCmdLine[0] == 'M')) {
      if (ich > 1) {
        uint16_t wMy = ParseHexString(&szCmdLine[1]); 

        if (wMy && wMy != g_diystate.wXBeeMY) {
          g_diystate.wXBeeMY = wMy;
          UpdateSavedXBeeInfo();
          APISetXBeeHexVal('M', 'Y', g_diystate.wXBeeMY);
        }
      }
#ifdef DBGSerial            
      DBGSerial.print("Current My: ");
      DBGSerial.println(g_diystate.wXBeeMY, HEX);
#endif            
    }
    else if ((szCmdLine[0] == 'a') || (szCmdLine[0] == 'A')) {
      // Start off just pringting out all of the Analog values
      DBGSerial.print("Analog Values: ");
      for (int i = 0; i < 10; i++) {
        DBGSerial.print(analogRead(i), DEC);
        DBGSerial.print(" ");
      }
      DBGSerial.println();
    }
    else if ((szCmdLine[0] == 'e') || (szCmdLine[0] == 'E')) {
      // Dump out the contents of the EEPROM. 
      for (int i = 0; i < XBeeDMStart+0x80; i++) {
        if ((i&0xf)== 0x0) {
          DBGSerial.println();
          DBGSerial.print(i, HEX);
          DBGSerial.print(": ");
        }
        DBGSerial.print(EEPROM.read(i), HEX);
        DBGSerial.print(" ");
      }
      DBGSerial.println();
    }

    return true;
  }
#endif
  return false;
} 


//==============================================================================
// TermReadln - Simple helper function to read in a command line from the termainal
//==============================================================================
#ifdef DBGSerial
int TermReadln(byte *psz, byte cbMax) 
{
  byte ich;
  int ch;
  // For now assume we receive a packet of data from serial monitor, as the user has
  // to click the send button...
  ich = 0;
  while (((ch = Serial.read()) != 13) && (ich < cbMax)) {
    if ((ch != -1) && (ch != 10)) {
      *psz++ = ch;
      ich++;
    }
  }
  *psz = '\0';    // go ahead and null terminate it...
  return ich;
}    
#endif


//=============================================================================
// InitKeypad: Lets setup the IO pins associated with the keypad
//=============================================================================

void InitKeypad(void) {
  // Assume first four pins are the rows and the next four pins are the cols.
  byte bPin;

  // Init Rows
  for (bPin = FIRST_KEYPAD_ROWPIN; bPin < FIRST_KEYPAD_ROWPIN+4; bPin++) {
    pinMode(bPin, INPUT);        // set all of the pins to input
  }

  // Init Cols
  for (bPin = FIRST_KEYPAD_COLPIN; bPin < FIRST_KEYPAD_COLPIN+4; bPin++) {
    pinMode(bPin, INPUT_PULLUP);        // set all of the pins to input
  }
}        

//=============================================================================
// ReadKeypad: Now lets read the keypad and see if any buttons are pressed
//=============================================================================
uint16_t ReadKeypad(char *pch) {
  uint16_t wRet = 0;
  byte bRow;
  byte bCol;
  byte bBit;

  *pch = ' ';    // assume a blank
  for (bRow = FIRST_KEYPAD_ROWPIN; bRow < FIRST_KEYPAD_ROWPIN+4; bRow++) {
    // Set the row to low
    pinMode(bRow, OUTPUT);
    digitalWrite(bRow, LOW);

    for (bCol = FIRST_KEYPAD_COLPIN; bCol <  FIRST_KEYPAD_COLPIN+4; bCol++) {
      if (digitalRead(bCol) == LOW) {
        bBit =  c_KeypadMapping[(bRow-FIRST_KEYPAD_ROWPIN)*4 + bCol-(FIRST_KEYPAD_COLPIN)];
        wRet |= 1 << bBit;
        if (bBit <= 9)
          *pch = '0'+ bBit;
        else
          *pch = 'A'+ bBit-10;
      }
    }
    // restore the IO pin to be an input...
    pinMode(bRow, INPUT_PULLUP);    // 
  }

  return wRet;
}

//=============================================================================
// InitJoysticks: Initialize our Joysticks and other analog devices
//=============================================================================
void InitJoysticks(void) {          // Initialize the joysticks.
  byte i;
  uint16_t w;
  // Later may need to read in Min/Max values for each of the Anlog inputs from EEPROM.

  // We need to prefill our array of raw items and sums.  The first read for each analog
  // may be trash, but that will get removed on our first real read.
  // First do our Real Analogs...
  for (i=0; i<NUMANALOGS; i++) {
    g_awRawSums[i] = 0;        // initialize to zero
    w = analogRead(FIRSTANALOG+i);    // first read is not valid
    for (g_iRaw=0; g_iRaw < 8; g_iRaw++) {
      g_aawRawAnalog[g_iRaw][i] = analogRead(FIRSTANALOG+i);
      g_awRawSums[i] += g_aawRawAnalog[g_iRaw][i];
      delay(1);
    }
    // Save away these sums as our mid points...
    g_awAnalogMids[i] = g_awRawSums[i];
  }

  // Init the two buttons on the joysticks to be input with PU enabled.
  // Right now assume Digital pins 2-9 are used for our extra buttons state
  // likewise for our command buttons on pins 10-13
  for (i=0; i < sizeof(s_abCMDBtns)/sizeof(s_abCMDBtns[0]); i++) {
    pinMode(pgm_read_byte(&s_abCMDBtns[i]), INPUT_PULLUP);
  }

  // likewise for our extra buttons...
  for (i=0; i < sizeof(s_abExtraBtns)/sizeof(s_abExtraBtns[0]); i++) {
    pinMode(pgm_read_byte(&s_abExtraBtns[i]), INPUT_PULLUP);
  }


}


//=============================================================================
// Calibrate and Normalize AnalogValue
//=============================================================================
void ReadCalibrateAndNormalizeAnalog(byte iRaw, byte i, uint16_t wAnalog) {
  g_awRawSums[i] -= g_aawRawAnalog[iRaw][i];        // remove the value we overwrite from sum
  g_aawRawAnalog[iRaw][i] = wAnalog;
  g_awRawSums[i] += wAnalog;        // Add the new value in
  // Lets calculate our calibrated values from 0-255 whith 128 as center point
  if (g_awRawSums[i] <= g_awAnalogMins[i])
    diyp.ab[g_aiRawToDIYP[i]] = 0;                // Take care of out of range low
  else if (g_awRawSums[i] >= g_awAnalogMaxs[i])
    diyp.ab[g_aiRawToDIYP[i]] = 255;              // out of range high
  else if (!g_afAnalogUseMids[i])
    diyp.ab[g_aiRawToDIYP[i]] = map(g_awRawSums[i], g_awAnalogMins[i], g_awAnalogMaxs[i], 0, 255);
  else if (g_awRawSums[i] <= g_awAnalogMids[i])
    diyp.ab[g_aiRawToDIYP[i]] = map(g_awRawSums[i], g_awAnalogMins[i], g_awAnalogMids[i], 0, 128);
  else
    diyp.ab[g_aiRawToDIYP[i]] = map(g_awRawSums[i], g_awAnalogMids[i], g_awAnalogMaxs[i], 128, 255);

  // If in normal mode make sure we transmit as often as possible...
  if (g_bMode==MODE_NORMAL)
    CheckAndTransmitDataPacket(&diyp);
}

//=============================================================================
// InitJoysticks: Initialize our Joysticks and other analog devices
//    Note: loop cntr will be  0xff if we are not in normal display mode...
//=============================================================================
uint16_t g_awAnalogsPrev[NUMANALOGS];

void ReadJoysticks() {          // Initialize the joysticks.
  byte i;
  boolean fChanged = false;

  // Calculate which index of our raw array we should now reuse.
  g_iRaw = (g_iRaw+1) & 0x7;

  // Now lets read in the next analog reading and smooth the values
  // Will separate out Normal analogs from MUX.  
  for (i=0; i<NUMANALOGS; i++) {
    uint16_t wAnalog = analogRead(FIRSTANALOG+i);
    ReadCalibrateAndNormalizeAnalog(g_iRaw, i, wAnalog);
    if (wAnalog != g_awAnalogsPrev[i]) {
      g_awAnalogsPrev[i] = wAnalog;
      fChanged = true;
    }
  }
#ifdef DEBUG_ANALOGS  
#ifdef DBGSerial  
  if (g_fDebugOutput && fChanged) {
    DBGSerial.print("A: ");
    for (i=0; i< NUMANALOGS; i++) {
      DBGSerial.print(g_awAnalogsPrev[i], DEC);
      DBGSerial.print(" ");
    }
    DBGSerial.println();
  }
#endif
#endif

  // Lets get our battery voltage...
  // Do the same for the Battery voltage
  g_wRawBatterySum -= g_awRawBatAnalog[g_iRaw];
  g_awRawBatAnalog[g_iRaw] = analogRead(BATTERYANALOG);
  g_wRawBatterySum += g_awRawBatAnalog[g_iRaw];
}

//=============================================================================
// GetSavedXBeeInfo - get our saved XBee information including what our My should
//        be and the default DL that we wish to talk to.  
//=============================================================================
void GetSavedXBeeInfo(void) {
  byte cbXBee = EEPROM.read(XBeeDMStart);
  byte bXBeeCS = EEPROM.read(XBeeDMStart+1);
  byte bCS;

  g_diystate.wXBeeMY = 0;
  g_diystate.wXBeeDL = 1;
  if (cbXBee == 4) {
    g_diystate.wXBeeMY = (EEPROM.read(XBeeDMStart+2) << 8) + EEPROM.read(XBeeDMStart+3);
    g_diystate.wXBeeDL = (EEPROM.read(XBeeDMStart+4) << 8) + EEPROM.read(XBeeDMStart+5);
    bCS = (g_diystate.wXBeeMY >> 8) + (g_diystate.wXBeeMY & 0xff) + 
      (g_diystate.wXBeeDL >> 8) + (g_diystate.wXBeeDL & 0xff);
    if (bCS != bXBeeCS) {
#ifdef DBGSerial
      DBGSerial.print("Saved Data checksum error ");
      DBGSerial.print(bCS, HEX);
      DBGSerial.print("!=");
      DBGSerial.print(bXBeeCS, HEX);
      DBGSerial.print(" : ");
      DBGSerial.print(g_diystate.wXBeeMY, HEX );
      DBGSerial.print(" ");
      DBGSerial.println(g_diystate.wXBeeDL, HEX);
#endif          
    }
    else {
#ifdef DBGSerial
      DBGSerial.print("Saved Data: ");
      DBGSerial.print(g_diystate.wXBeeMY, HEX );
      DBGSerial.print(" ");
      DBGSerial.println(g_diystate.wXBeeDL, HEX);
#endif          
      // lets see if the DL is in our saved list.  Will cheat and not check that we have
      // a valid checksum
      byte cNDList = EEPROM.read(XBeeNDDMCache);
#ifdef DBGSerial
      DBGSerial.println(cNDList);
#endif
      if (cNDList && (cNDList <= CMAXNDLIST)) {
        uint16_t wOffset = XBNDDM_AMY;
        uint16_t w;
        while (cNDList--) {
          uint16_t wDLt = (((uint16_t)EEPROM.read(wOffset) << 8)) + EEPROM.read(wOffset+1);
#ifdef DBGSerial
          DBGSerial.println(wDLt, HEX);
#endif
          if (wDLt == g_diystate.wXBeeDL) {
            // found a match read in the NI info for this item.
            char bNI[CBNIMAX+1];
            w = XBNDDM_ANDI + ((wOffset-XBNDDM_AMY)/2)*CBNIMAX;    // convert to index and then to the start of the text
            for(byte i=0; i< CBNIMAX; i++) 
              bNI[i] = EEPROM.read(w++);
            bNI[CBNIMAX] = 0;    // make sure to null terminate it.    
            g_display.DisplayStatus("Paired With", bNI);
#ifdef DBGSerial
            DBGSerial.print("DL->");
            DBGSerial.println(bNI);
#endif
            return;     // have a name
          }
          wOffset += 2;
        }
      }
    }        
  }
}
//=============================================================================
// GetSavedXBeeInfo - get our saved XBee information including what our My should
//        be and the default DL that we wish to talk to.  
//=============================================================================
void UpdateSavedXBeeInfo(void) {
  uint16_t w;    

  EEPROM.write(XBeeDMStart, 4);        // size of our saved data.
  EEPROM.write(XBeeDMStart+1,         // output checksum
  (g_diystate.wXBeeMY >> 8) + (g_diystate.wXBeeMY & 0xff) + (g_diystate.wAPIDL >> 8) + (g_diystate.wAPIDL & 0xff));
  EEPROM.write(XBeeDMStart+2, g_diystate.wXBeeMY  >> 8);    
  EEPROM.write(XBeeDMStart+3, g_diystate.wXBeeMY & 0xff);
  EEPROM.write(XBeeDMStart+4, g_diystate.wAPIDL >> 8);
  EEPROM.write(XBeeDMStart+5, g_diystate.wAPIDL & 0xff);
}   

//==============================================================================
// [UpdateNDListFromXBee] - This function will update the ND list using information
// 		it receives back from issuing an ATND command.  It may add new nodes or
//		if it finds nodes with a serial number that matches, but data is different
//		it will update that information as well.
//==============================================================================
void UpdateNDListFromXBee(void) {
#define  APIRECVDATAOFFSET 5


  byte cNDList;                           // how many items are in our list?
  byte cNDListIn;                         // How many did we read in...
  byte csNDList;	                    //  checksum of data.
  byte iItem;
  uint16_t w;
  uint16_t awNDMY[CMAXNDLIST+1];              // Keep a list of all of the Mys
  unsigned long alNDSNL[CMAXNDLIST+1];    // Serial numbers Low
  unsigned long alNDSNH[CMAXNDLIST+1];    // Serial Numbers High
  byte  abNDNIS[CMAXNDLIST*CBNIMAX];      //keep all of the strings...
  boolean _fListChanged;

  byte i;
  byte j;
  byte _cbRet;

  // First we need to read in the current list of items.
  cNDList = EEPROM.read(XBeeNDDMCache);
  if (cNDList > CMAXNDLIST)
    cNDList = 0;
  for (iItem=0; iItem < cNDList; iItem++) {
    awNDMY[iItem] = (EEPROM.read(XBNDDM_AMY + iItem*2) << 8) + EEPROM.read(XBNDDM_AMY+iItem*2+1);
    alNDSNL[iItem] = (EEPROM.read(XBNDDM_ASL + iItem*4) << 24) |
      (EEPROM.read(XBNDDM_ASL + iItem*4+1) << 16) | (EEPROM.read(XBNDDM_ASL + iItem*4+2) << 8) | EEPROM.read(XBNDDM_ASL + iItem*4+3);
    alNDSNH[iItem] = (EEPROM.read(XBNDDM_ASH + iItem*4) << 24) +
      (EEPROM.read(XBNDDM_ASH + iItem*4+1) << 16) | (EEPROM.read(XBNDDM_ASH + iItem*4+2) << 8) | EEPROM.read(XBNDDM_ASH + iItem*4+3);

    // Need to get the node identifier.
    w =  iItem * CBNIMAX;
    for (j=0; j < CBNIMAX; j++)  {
      abNDNIS[w] = EEPROM.read(XBNDDM_ANDI+w);
      w++;
    }
#ifdef DBGSerial
    DBGSerial.print(awNDMY[iItem], HEX);
    DBGSerial.print( " ");
    DBGSerial.print(alNDSNH[iItem], HEX);
    DBGSerial.print(":");
    DBGSerial.print(alNDSNL[iItem], HEX);
    DBGSerial.print(" - ");
    DBGSerial.write(&abNDNIS[iItem * CBNIMAX], CBNIMAX);
    DBGSerial.println("");
#endif
  }

  cNDListIn = cNDList;	// remember how many we had from EEPROM (no need to check dups of items beyond this)
  _fListChanged = false;
  ClearXBeeInputBuffer();

  // use same helper function to send command as our get functions
#ifdef DBGSerial
  DBGSerial.println("Start ND Scan");
#endif
  APISendXBeeGetCmd('N','D');

  // I think I can loop calling the APIRecvPacket - but as the data it returns is in a differet
  // file, I may have to have helper function over there.
  do {
    _cbRet = APIRecvPacket(3000);
#ifdef DEBUG_SAVED_LIST
    DBGSerial.print("ND CB: ");
    DBGSerial.println(_cbRet, DEC);
    for (i = 0; i < _cbRet; i++ ) { 
      DBGSerial.print(g_diystate.bAPIPacket[i], HEX);
      DBGSerial.print( " ");
    }
    DBGSerial.println("");
#endif
    if (_cbRet > (11+APIRECVDATAOFFSET)) {  // MY(2), SH(4), SL(4), DB(1), NI???
      // ok lets extract the information for this item
      // Note with the API receive packet we need to skip over the header part of the API receive.
      awNDMY[cNDList] = (g_diystate.bAPIPacket[APIRECVDATAOFFSET+0] << 8) + g_diystate.bAPIPacket[APIRECVDATAOFFSET+1];
      alNDSNH[cNDList] = (g_diystate.bAPIPacket[APIRECVDATAOFFSET+2] << 24) + (g_diystate.bAPIPacket[APIRECVDATAOFFSET+3] << 16) + (g_diystate.bAPIPacket[APIRECVDATAOFFSET+4] << 8) + g_diystate.bAPIPacket[APIRECVDATAOFFSET+5];
      alNDSNL[cNDList] = (g_diystate.bAPIPacket[APIRECVDATAOFFSET+6] << 24) + (g_diystate.bAPIPacket[APIRECVDATAOFFSET+7] << 16) + (g_diystate.bAPIPacket[APIRECVDATAOFFSET+8] << 8) + g_diystate.bAPIPacket[APIRECVDATAOFFSET+9];		

      // The Node identifier starts in  g_diystate.bAPIPacket(11)
      // We want to blank this field out to our default size we use
      for (i = _cbRet - (12+APIRECVDATAOFFSET);  i < CBNIMAX; i++)	 // number of actual characters transfered.
        g_diystate.bAPIPacket[i+11+APIRECVDATAOFFSET] = ' ';
#ifdef DEBUG_SAVED_LIST
      DBGSerial << _HEX(awNDMY[cNDList]) << " " << _HEX(alNDSNH[cNDList]) << ":" << _HEX(alNDSNL[cNDList]) << " - ";
      DBGSerial.write(&g_diystate.bAPIPacket[11+APIRECVDATAOFFSET], APIRECVDATAOFFSET);
      DBGSerial.println("");
#endif

      boolean _fItemDup = false;
      _fListChanged = false;
      if (cNDListIn) {
        for (i = 0; i < cNDListIn; i++) {
          if ((alNDSNH[i] == alNDSNH[cNDList]) && (alNDSNL[i] == alNDSNL[cNDList])) {
            // We have seen this one before...
            _fItemDup = true; 		// signal that this item is a duplicate...
#ifdef DEBUG_SAVED_LIST
            DBGSerial.println("Dup");
#endif
            // but we will also make sure the MY or the NI has not changed.
            if (awNDMY[i] != awNDMY[cNDList]) {
              _fListChanged = 1;        // 	we know that we need to write the stuff back out...
#ifdef DEBUG_SAVED_LIST
              DBGSerial.println("Updated My");
#endif
              awNDMY[i] = awNDMY[cNDList];
            }

            for (j = 0; j <CBNIMAX; j++) {
              if (abNDNIS[i*CBNIMAX + j] != g_diystate.bAPIPacket[j+11+APIRECVDATAOFFSET]) {
                abNDNIS[i*CBNIMAX + j] = g_diystate.bAPIPacket[j+11+APIRECVDATAOFFSET];
                _fListChanged = true;     // 	we know that we need to write the stuff back out...
#ifdef DEBUG_SAVED_LIST
                DBGSerial.println("Updated NI");
#endif
              }
            }
          }
        }
      }

      // only save away the data if this is not a duplicate and we have room
      if (!_fItemDup && (cNDList < CMAXNDLIST)) {
        for (j = 0; j < CBNIMAX; j++ ) {			// copy the NI string in
          abNDNIS[cNDList*CBNIMAX + j] = g_diystate.bAPIPacket[j+11+APIRECVDATAOFFSET];
        }
        cNDList++;
        _fListChanged = true;	// yes the list changed - have new node
      }
    }
  } 
  while (_cbRet > (11+APIRECVDATAOFFSET)); // MY(2), SH(4), SL(4), DB(1), NI???

  if (_fListChanged) {
    // Save away the updated list.
    EEPROM.write(XBeeNDDMCache, cNDList);

    for (iItem=0; iItem < cNDList; iItem++) {
      EEPROM.write(XBNDDM_AMY + iItem*2, awNDMY[iItem] >> 8);
      EEPROM.write(XBNDDM_AMY + iItem*2+1, awNDMY[iItem] & 0xff);

      EEPROM.write(XBNDDM_ASL + iItem*4, (alNDSNL[iItem] >> 24) & 0xff);
      EEPROM.write(XBNDDM_ASL + iItem*4+1, (alNDSNL[iItem] >> 16) & 0xff);
      EEPROM.write(XBNDDM_ASL + iItem*4+2, (alNDSNL[iItem] >> 8) & 0xff);
      EEPROM.write(XBNDDM_ASL + iItem*4+3, alNDSNL[iItem] & 0xff);

      EEPROM.write(XBNDDM_ASH + iItem*4, (alNDSNH[iItem] >> 24) & 0xff);
      EEPROM.write(XBNDDM_ASH + iItem*4+1, (alNDSNH[iItem] >> 16) & 0xff);
      EEPROM.write(XBNDDM_ASH + iItem*4+2, (alNDSNH[iItem] >> 8) & 0xff);
      EEPROM.write(XBNDDM_ASH + iItem*4+3, alNDSNH[iItem] & 0xff);

      w =  iItem * CBNIMAX;
      for (j=0; j < CBNIMAX; j++) {
        EEPROM.write(XBNDDM_ANDI+w, abNDNIS[w]);
        w++;
      }
    }
  }        

}

//==============================================================================
//==============================================================================
void DeleteItemFromNDList(byte iDel) {
  byte cNDList;
  byte iItem;
  byte j;
  uint16_t w;
  // First we need to read in the current list of items.

  cNDList = EEPROM.read(XBeeNDDMCache);
  if (iDel >= cNDList)
    return;    // Index out of range. 

  // First lets output a new counter
  EEPROM.write(XBeeNDDMCache, --cNDList);

  if (iDel == cNDList)
    return;     // deleted last one.


  for (iItem=0; iItem < cNDList; iItem++) {
    EEPROM.write(XBNDDM_AMY + iItem*2, EEPROM.read(XBNDDM_AMY + iItem*2+2));
    EEPROM.write(XBNDDM_AMY + iItem*2+1, EEPROM.read(XBNDDM_AMY + iItem*2+3));

    EEPROM.write(XBNDDM_ASL + iItem*4, EEPROM.read(XBNDDM_ASL + iItem*4+4));
    EEPROM.write(XBNDDM_ASL + iItem*4+1, EEPROM.read(XBNDDM_ASL + iItem*4+5));
    EEPROM.write(XBNDDM_ASL + iItem*4+2, EEPROM.read(XBNDDM_ASL + iItem*4+6));
    EEPROM.write(XBNDDM_ASL + iItem*4+3, EEPROM.read(XBNDDM_ASL + iItem*4+7));

    EEPROM.write(XBNDDM_ASH + iItem*4, EEPROM.read(XBNDDM_ASH + iItem*4+4));
    EEPROM.write(XBNDDM_ASH + iItem*4+1, EEPROM.read(XBNDDM_ASH + iItem*4+5));
    EEPROM.write(XBNDDM_ASH + iItem*4+2, EEPROM.read(XBNDDM_ASH + iItem*4+6));
    EEPROM.write(XBNDDM_ASH + iItem*4+3, EEPROM.read(XBNDDM_ASH + iItem*4+7));

    // Need to get the node identifier.
    w =  iItem * CBNIMAX;
    for (j=0; j < CBNIMAX; j++) {
      EEPROM.write(XBNDDM_ANDI+w, EEPROM.read(XBNDDM_ANDI+w+CBNIMAX));
      w++;
    }
  }

}

//==============================================================================
// ParseDecString
//==============================================================================
uint16_t ParseDecString(byte *psz) {
  uint16_t w = 0;
  while (*psz) {
    if (*psz >= '0' && *psz <= '9')
      w = w*10 + *psz-'0';
    psz++;
  }
  return w;
}

//==============================================================================
// ParseHexString
//==============================================================================
uint16_t ParseHexString(byte *psz) {
  uint16_t w = 0;
  while (*psz) {
    if (*psz >= '0' && *psz <= '9')
      w = w*16 + *psz-'0';
    else if (*psz >= 'a' && *psz <= 'f')
      w = w*16 + *psz-'a' + 10;
    else if (*psz >= 'A' && *psz <= 'F')
      w = w*16 + *psz-'A' + 0;
    psz++;
  }
  return w;
}

//==============================================================================
// [TermChooseXBeeDL] - This function allows us to display and choose which DL we
//         wish to communicate with, from the debug terminal.  Later we will do this
//         from the remote as well.
//==============================================================================
#ifdef DBGSerial
void TermChooseXBeeDL(void) {
  boolean fContinue;
  byte cNDList;
  byte    i;
  byte iItem;
  char bNI[CBNIMAX+1];
  byte szCmdLine[20];
  int ich;
  uint16_t wDLt;
  uint16_t w;

  for(;;) {
    // first lets print out the current list of nodes
    DBGSerial.print("Saved XBEE DL List - Curr: ");
    DBGSerial.println(g_diystate.wAPIDL, HEX);
    cNDList = EEPROM.read(XBeeNDDMCache);
    if (cNDList > CMAXNDLIST) {
      EEPROM.write(XBeeNDDMCache, 0);    // only do this once...
      cNDList = 0;
    }
    for (iItem=0; iItem < cNDList; iItem++) {
      DBGSerial.print(iItem, DEC);
      DBGSerial.print(" : ");
      wDLt = (EEPROM.read(XBNDDM_AMY+iItem*2) << 8) + EEPROM.read(XBNDDM_AMY + (iItem*2) + 1);
      if (wDLt == g_diystate.wAPIDL) 
        DBGSerial.print("*");            // Show this is our current one.
      DBGSerial.print(wDLt, HEX);
      DBGSerial.print(" - ");
      w = XBNDDM_ANDI + iItem*CBNIMAX;    // convert to index and then to the start of the text
      for(byte i=0; i< CBNIMAX; i++) 
        bNI[i] = EEPROM.read(w++);
      bNI[CBNIMAX] = 0;    // make sure to null terminate it.    
      DBGSerial.println(bNI);
    }
    DBGSerial.println("S - Scan for new XBEEs");
    DBGSerial.println("Dnn - Delete item");
    DBGSerial.println("#xxxx - Direct hex input of DL");
    DBGSerial.print("Enter new index or Cmd: ");

    // not get command line from user.
    ich = TermReadln(szCmdLine, sizeof(szCmdLine));
    if (!ich)
      break; // empty command line, lets bail out of here.
    else if ((ich == 1) && (szCmdLine[0]=='s' || szCmdLine[0] == 'S')) {
      UpdateNDListFromXBee();        // have the XBEE scan for new items
    } 
    else if ((ich > 1) && szCmdLine[0]=='#') {
      // Direct input of number
      wDLt = ParseHexString(&szCmdLine[1]); 

      if (wDLt && wDLt != g_diystate.wAPIDL) {
        SetXBeeDL(wDLt);
        UpdateSavedXBeeInfo();
      }
    } 
    else if ((ich > 1) && (szCmdLine[0]=='d' || (szCmdLine[0]=='D'))) {
      // User entered an index 
      iItem = 0; 
      for(i=0; i < ich; i++) {
        if (szCmdLine[i] >= '0' && szCmdLine[i] <= '9')
          iItem = iItem*10 + szCmdLine[i] -'0';
      }
      if (iItem < cNDList)
        DeleteItemFromNDList(iItem);
    }
    else if (szCmdLine[0]>='0' && szCmdLine[0] <='9')  {
      // User entered an index 
      iItem = 0; 
      for(i=0; i < ich; i++) {
        if (szCmdLine[i] >= '0' && szCmdLine[i] <= '9')
          iItem = iItem*10 + szCmdLine[i] -'0';
      }
      if (iItem < cNDList) {
        wDLt = (EEPROM.read(XBNDDM_AMY+iItem*2) << 8) + EEPROM.read(XBNDDM_AMY + (iItem*2) + 1);
        SetXBeeDL(wDLt);
        UpdateSavedXBeeInfo();
        DBGSerial.print("Choose Item: ");
        DBGSerial.print(iItem, DEC);
        DBGSerial.print(" DL: ");
        DBGSerial.println(wDLt, HEX);
      }
    }            

  }

} 
#endif

// BUGBUG:: Move to some library...
//==============================================================================
//    SoundNoTimer - Quick and dirty tone function to try to output a frequency
//            to a speaker for some simple sounds.
//==============================================================================
void SoundNoTimer(uint8_t _pin, unsigned long duration,  unsigned int frequency)
{
  volatile uint8_t *pin_port;
  volatile uint8_t pin_mask;
  long toggle_count = 0;
  long lusDelayPerHalfCycle;

  // Set the pinMode as OUTPUT
  pinMode(_pin, OUTPUT);

  pin_port = portOutputRegister(digitalPinToPort(_pin));
  pin_mask = digitalPinToBitMask(_pin);

  toggle_count = 2 * frequency * duration / 1000;
  lusDelayPerHalfCycle = 1000000L/(frequency * 2);

  // if we are using an 8 bit timer, scan through prescalars to find the best fit
  while (toggle_count--) {
    // toggle the pin
    *pin_port ^= pin_mask;

    // delay a half cycle
    delayMicroseconds(lusDelayPerHalfCycle);
  }    
  *pin_port &= ~(pin_mask);  // keep pin low after stop

}

void MSound(byte cNotes, ...)
{
  va_list ap;
  unsigned int uDur;
  unsigned int uFreq;
  va_start(ap, cNotes);

  while (cNotes > 0) {
    uDur = va_arg(ap, unsigned int);
    uFreq = va_arg(ap, unsigned int);
#ifdef SOUND_PIN
    SoundNoTimer(SOUND_PIN, uDur, uFreq);
#endif        
    cNotes--;
  }
  va_end(ap);
}

void PlayRemoteSounds(char cb, char *pb)
{
#ifdef SOUND_PIN
  unsigned int uDur;
  unsigned int uFreq;
  while (cb) {
    uDur = *pb++;
    uFreq = (*pb++) * 25;  // Passed in value is divded by 25 to fit in byte
    SoundNoTimer(SOUND_PIN, uDur, uFreq);
    cb -=2;
  }
#endif  
}











