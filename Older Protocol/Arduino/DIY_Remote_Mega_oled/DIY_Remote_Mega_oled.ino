
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
//#define DEBUG_BATVOLT 
//
#define DEBUG_TIMING


//KNOWN BUGS:
//    - Lots ;)
//
//=============================================================================
// Header Files
//=============================================================================
#include <EEPROM.h>
#include <Streaming.h>
//#include <digitalWriteFast.h>
#include "globals.h"
#include "diyxbee.h"
#include <pins_arduino.h>

//=============================================================================
// Tables
//=============================================================================
static const word c_KeypadMapping[] = {
  1, 4, 7, 0,
  2, 5, 8, 0xf,
  3, 6, 9, 0xe,
  0xa, 0xb, 0xc, 0xd};

//=============================================================================
// Defines
//=============================================================================
// Lets try to define which IO pins will be used for What
//
//A0-A5 - PKT_RJOYLR, PKT_RJOYUD, PKT_LJOYLR, PKT_LJOYUD, PKT_RSLIDER, PKT_LSLIDER,	
//X0-X5 - PKT_RPOT, PKT_LPOT, PKT_MSLIDER};
//X7 - Battery pin...
// Analog information

const uint8_t g_abAnalogPins[] = {
  0, 1, 2, 3, 4, 5, 8, 9, 10};
#define NUMANALOGS    (sizeof(g_abAnalogPins)/sizeof(g_abAnalogPins[0]))

enum {
  AP_RJOYLR=0, AP_RJOYUD, AP_LJOYLR, AP_LJOYUD, AP_RSLIDER, AP_LSLIDER, AP_RPOT, AP_LPOT,  AP_MSLIDER};

// Define command buttons.
#define CMDB_NEXT    0x8    // Above
#define CMDB_PREV    0x2    // Below
#define CMDB_ENTR    0x4    // left
#define CMDB_CNCL    0x1    // right
// Some battery defines
#define BATTERY_PIN    7    // Analog pin X7
#define BATTERY_MIN    256    // Min Analog value to use to report 0 256 is about 5V
#define BATTERY_MAX    384    // Max analog value to report 100% about 7.5 v  = (V * 1024) / 20

// Kepad on 
#define FIRST_KEYPAD_IOPIN    2  // 2-97 used for Keypad
// 8,9, 10, 11 - Used for 2 Usarts
// 12, 14, 15, 16 - Command buttons - Could combine with keypad like before...
// 
#define CMDB_BTN1    10
// Pin 13 has LED, not Good with PU
#define CMDB_BTN2    22
#define CMDB_BTN3    11
#define CMDB_BTN4    12
static const byte s_abCMDBtns[] PROGMEM = {
  CMDB_BTN1,CMDB_BTN2, CMDB_BTN3, CMDB_BTN4};  
static const byte s_abExtraBtns[] PROGMEM = {
  24, 25};      // Extra buttons...

#define SOUND_PIN      46    // Old Mega shield
//#define SOUND_PIN    21        // Mega shield pin numbers

//=============================================================================
// Global Variables
//=============================================================================
word    wKeypadPrev;

// Mode information
byte              g_bMode;                                       // What mode are we in...
boolean           g_fNewMode;                                    // Have we just entered this mode?

word              g_aawRawAnalog[8][NUMANALOGS];                 // Keep 8 raw values for each analog input
word              g_awRawBatAnalog[8];
word              g_awRawSums[NUMANALOGS];                       // sum of the raw values.
word              g_wRawBatterySum;
byte              g_bCmdButtons;                                // which command buttons are pressed
byte              g_bCmdButtonsPrev;                            // which were set before...
byte              g_bExtendedAnalogMask;                        // Our Extended AnalogMask

static const byte g_aiRawToDIYP[] = {
  PKT_RJOYLR, PKT_RJOYUD, PKT_LJOYLR, PKT_LJOYUD, PKT_RSLIDER, PKT_LSLIDER,	
  PKT_RPOT, PKT_LPOT, PKT_MSLIDER};

static const boolean  g_afAnalogUseMids[] = {
  1,1,1,1,0,0,1,1, 0};       // Do we use the mid points when we normalize
static const word g_awAnalogMins[] = {
  0*8,0*8,0*8,0*8,0*8,0*8, 0*8, 0*8, 0*8};  // Will have a calibrate later where we read these in (if necessary)
static const word g_awAnalogMaxs[] = {
  1023*8,1023*8,1023*8,1023*8,1023*8,1023*8, 1023*8, 1023*8, 1023*8}; // dito

word              g_awAnalogMids[NUMANALOGS];                // This is our observed center points at init time. 
byte    g_iRaw;                   // which raw value are we going to read into

word    g_aw0Prev;
boolean g_fShowDebugPrompt;
boolean g_fDebugOutput;

byte    g_bDispLoopCntr;                // Use this to decide which fields we should display per loop

RemoteDisplay g_display;                // Define our display here.


// Globals used in some of the config code.
byte            g_cNDList;                    // count of items in the list is saved ND list 
byte            g_iNDList;                   // Index to current one
word            g_wNewVal;                    // The new value
unsigned long   g_ulTimeLastMsg;             // Time of last message;
// forward definitions.
extern int TermReadln(byte *psz, byte cbMax);

DIYPACKET diyp;        // Our data packet we will send out...
DIYPACKET diypSave;

// Lets put some timing code in place...
word wLoopCnt;
unsigned long ulTimeLoopStart;
#ifdef DEBUG_TIMING
#define CNT_TIMINGS 8
unsigned long ulLastTiming;
unsigned long aulTimings[CNT_TIMINGS];
word awCntTimings[CNT_TIMINGS];

void INITTIMINGS() {
  for (byte i=0; i< CNT_TIMINGS; i++) {
    aulTimings[i] = 0;
    awCntTimings[i] = 0;
  }
}


inline void TIMINGS(byte iT) {
  unsigned long ul = micros();
  aulTimings[iT] += ul-ulLastTiming;
  ulLastTiming = ul;
  awCntTimings[iT]++;
}  
#else
#define INITTIMINGS()
#define Timings(iT)
#endif

//--------------------------------------------------------------------------
// SETUP: the main arduino setup function.
//--------------------------------------------------------------------------
void setup(){
  int error;

#ifdef DBGSerial
  DBGSerial.begin(38400);
  DBGSerial.write("Program Start\n\r");
#endif    
  MSound(4, 100, 440, 100, 494, 100, 523, 100, 588);
  g_fDebugOutput = false;			// start with it off!
  g_fShowDebugPrompt = true;

  InitKeypad();            // Lets initialize the keypad
  InitJoysticks();          // Initialize the joysticks.

  g_display.Init();
  g_bMode = MODE_NORMAL;
  g_display.SwitchToDisplayMode(MODE_NORMAL);
  g_ulTimeLastMsg = (unsigned long)-1;
  GetSavedXBeeInfo();
  InitXBee();            // Now lets initialize The XBee
  APISetXBeeHexVal('M', 'Y', g_diystate.wXBeeMY);
  SetXBeeDL(g_diystate.wXBeeDL);

  // Lets try to output some data to the status line...
  char szMyT[16];
  char szDLT[16];
  word wDLT;
  sprintf(szMyT, "My: %x", GetXBeeHVal('M','Y'));
  wDLT = GetXBeeHVal('D','L');
  if (!GetNIForDL(wDLT, szDLT))
    sprintf(szDLT, "Paired: %x", wDLT);
  g_display.DisplayStatus(szMyT, szDLT);


#ifdef DBGSerial
  DBGSerial << "My : " << _HEX(g_diystate.wXBeeMY) << " Paired with: " << _HEX(g_diystate.wXBeeDL) << endl;
#endif    
  g_bDispLoopCntr = 0;
  INITTIMINGS();
  wLoopCnt = 0;
}

//=============================================================================
// Loop: the main arduino main Loop function
//=============================================================================
void loop(void)
{
  char ch;
  byte bMask;
  word w;
  byte i;
  // We also have a simple debug monitor that allows us to 
  // check things. call it here..
  if (TerminalMonitor())
    return;           

  ulLastTiming = micros();    
  diyp.s.wButtons = ReadKeypad(&ch);
  byte bJoyVal;
  TIMINGS(0);
  ReadJoysticks(g_bMode==MODE_NORMAL? g_bDispLoopCntr : 0xff);
  TIMINGS(1);
  // simply define Digital inputs 2-9 to this byte... Trying the faster function out for this...
  diyp.s.bButtons2 = 0;

  // Extra buttons on new DIY...
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
      Serial << "Cmd Button: " << _HEX(g_bCmdButtons) << endl;
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
  TIMINGS(2);

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
      TIMINGS(3);
      break;
    case 1:
      g_display.DisplayJoystick(1, diyp.s.bRJoyLR,  diyp.s.bRJoyUD, diyp.s.bRPot);
      TIMINGS(4);
      break;
    case 2:
      g_display.DisplaySliders(diyp.s.bLSlider, diyp.s.bMSlider, diyp.s.bRSlider); 
      TIMINGS(5);
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
      TIMINGS(6);

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
    wLoopCnt++;
#ifdef DEBUG_TIMING
    if (wLoopCnt == 100) {
      DBGSerial.print("TM:");
      for (byte i=0; i< CNT_TIMINGS; i++) {
        Serial.print(" ");
        Serial.print(awCntTimings[i], DEC);
        Serial.print(":");
        Serial.print(aulTimings[i], DEC);
        if (awCntTimings[i]) {
          Serial.print(":");
          Serial.print(aulTimings[i]/awCntTimings[i], DEC);
        }
      }
      Serial.println();
      wLoopCnt = 0;
      INITTIMINGS();
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
  word w;
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
  word w;
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
// GetNIForDL
//==============================================================================
boolean GetNIForDL(word wDL, char *pb) {

  word w;
  byte i;
  byte j;
  g_cNDList = EEPROM.read(XBeeNDDMCache);
  if (g_cNDList> CMAXNDLIST) {
    g_cNDList = 0;
    return false;
  }

  for (i = 0; i < g_cNDList; i++) {
    w = (EEPROM.read(XBNDDM_AMY + i*2) << 8) + EEPROM.read(XBNDDM_AMY+i*2+1);
    if (w == wDL) {
      // We found our item
      // Then lets get the Node Identifier text for this item...
      w =  i * CBNIMAX;
      for (j=0; j < CBNIMAX; j++) {
        *pb++ = EEPROM.read(XBNDDM_ANDI+w+j);
      }
      *pb=0;
      return true;
    }
  }

  return false;  // did not find the item in the list...

}



//==============================================================================
// ChangeDestMode - Handles display and UI when we are in the change destination
//         mode, 
//==============================================================================
void ChangeDestMode(void) {
  word w;
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
        word wMy = ParseHexString(&szCmdLine[1]); 

        if (wMy && wMy != g_diystate.wXBeeMY) {
          g_diystate.wXBeeMY = wMy;
          UpdateSavedXBeeInfo();
          APISetXBeeHexVal('M', 'Y', g_diystate.wXBeeMY);
        }
      }
#ifdef DBGSerial            
      DBGSerial << "Current My: " << _HEX(g_diystate.wXBeeMY) << endl;
#endif            
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
  for (bPin = FIRST_KEYPAD_IOPIN; bPin < FIRST_KEYPAD_IOPIN+4; bPin++) {
    pinMode(bPin, INPUT);        // set all of the pins to inptu
  }

  // Init Cols
  for (; bPin < FIRST_KEYPAD_IOPIN+8; bPin++) {
    pinMode(bPin, INPUT_PULLUP);        // set all of the pins to input
  }
}        

//=============================================================================
// ReadKeypad: Now lets read the keypad and see if any buttons are pressed
//=============================================================================
word ReadKeypad(char *pch) {
  word wRet = 0;
  byte bRow;
  byte bCol;
  byte bBit;

  *pch = ' ';    // assume a blank
  for (bRow = FIRST_KEYPAD_IOPIN; bRow < FIRST_KEYPAD_IOPIN+4; bRow++) {
    // Set the row to low
    pinMode(bRow, OUTPUT);
    digitalWrite(bRow, LOW);

    for (bCol = FIRST_KEYPAD_IOPIN+4; bCol <  FIRST_KEYPAD_IOPIN+8; bCol++) {
      if (digitalRead(bCol) == LOW) {
        bBit =  c_KeypadMapping[(bRow-FIRST_KEYPAD_IOPIN)*4 + bCol-(FIRST_KEYPAD_IOPIN+4)];
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
  word w;
  // Later may need to read in Min/Max values for each of the Anlog inputs from EEPROM.

  // We need to prefill our array of raw items and sums.  The first read for each analog
  // may be trash, but that will get removed on our first real read.
  // First do our Real Analogs...
  for (i=0; i<NUMANALOGS; i++) {
    g_awRawSums[i] = 0;        // initialize to zero
    w = analogRead(g_abAnalogPins[i]);    // first read is not valid
    for (g_iRaw=0; g_iRaw < 8; g_iRaw++) {
      g_aawRawAnalog[g_iRaw][i] = analogRead(g_abAnalogPins[i]);
      g_awRawSums[i] += g_aawRawAnalog[g_iRaw][i];
      delay(1);
    }
    // Save away these sums as our mid points...
    g_awAnalogMids[i] = g_awRawSums[i];
  }

  g_bExtendedAnalogMask = 0;

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
void ReadCalibrateAndNormalizeAnalog(byte iRaw, byte i, word wAnalog) {
  g_awRawSums[i] -= g_aawRawAnalog[iRaw][i];        // remove the value we overwrite from sum
  g_aawRawAnalog[iRaw][i] = wAnalog;
  g_awRawSums[i] += g_aawRawAnalog[iRaw][i];        // Add the new value in
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
void ReadJoysticks(byte bDispLoopCntr) {          // Initialize the joysticks.
  byte i;

  // Calculate which index of our raw array we should now reuse.
  g_iRaw = (g_iRaw+1) & 0x7;

  // Now lets read in the next analog reading and smooth the values
  // Will separate out Normal analogs from MUX.  
  for (i=0; i<NUMANALOGS; i++) {
    ReadCalibrateAndNormalizeAnalog(g_iRaw, i, analogRead(g_abAnalogPins[i]));
  }

  g_wRawBatterySum -= g_awRawBatAnalog[g_iRaw];
  g_awRawBatAnalog[g_iRaw] = analogRead(BATTERY_PIN);
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

#ifdef DBGSerial
  DBGSerial << "Get Saved Xbee " << _DEC(cbXBee) << " " << _HEX(bXBeeCS)  << endl;
#endif          

  if (cbXBee == 4) {
    g_diystate.wXBeeMY = (EEPROM.read(XBeeDMStart+2) << 8) + EEPROM.read(XBeeDMStart+3);
    g_diystate.wXBeeDL = (EEPROM.read(XBeeDMStart+4) << 8) + EEPROM.read(XBeeDMStart+5);
    bCS = (g_diystate.wXBeeMY >> 8) + (g_diystate.wXBeeMY & 0xff) + 
      (g_diystate.wXBeeDL >> 8) + (g_diystate.wXBeeDL & 0xff);
    if (bCS != bXBeeCS) {
#ifdef DBGSerial
      DBGSerial << "Saved Data checksum error " << _HEX(bCS) << "!=" << _HEX(bXBeeCS) << " : " << _HEX(g_diystate.wXBeeMY ) << " " << _HEX(g_diystate.wXBeeDL) << endl;
#endif          
      g_diystate.wXBeeMY = 0;
      g_diystate.wXBeeDL = 1;
    }
    else {
#ifdef DBGSerial
      DBGSerial << "My " << _HEX(g_diystate.wXBeeMY) << " DL " << _HEX(g_diystate.wXBeeDL)  << endl;
#endif          
      // lets see if the DL is in our saved list.  Will cheat and not check that we have
      // a valid checksum
      byte cNDList = EEPROM.read(XBeeNDDMCache);
      if (cNDList && (cNDList <= CMAXNDLIST)) {
        word w;
        while (cNDList--) {
          word wDLt = (EEPROM.read(XBNDDM_AMY) >> 8) + EEPROM.read(XBNDDM_AMY+1);
          if (wDLt == g_diystate.wAPIDL) {
            // found a match read in the NI info for this item.
            char bNI[CBNIMAX+1];
            w = XBNDDM_ANDI + ((w-XBNDDM_AMY)/2)*CBNIMAX;    // convert to index and then to the start of the text
            for(byte i=0; i< CBNIMAX; i++) 
              bNI[i] = EEPROM.read(w++);
            bNI[CBNIMAX] = 0;    // make sure to null terminate it.    
            g_display.DisplayStatus("Paired With", bNI);
#ifdef DBGSerial
            DBGSerial.println(bNI);
#endif          
            return;     // have a name
          }
          w += 2;
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
  word w;    

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
  word w;
  word awNDMY[CMAXNDLIST+1];              // Keep a list of all of the Mys
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
    DBGSerial << _HEX(awNDMY[iItem]) << " " << _HEX(alNDSNH[iItem]) <<":"<< _HEX(alNDSNL[iItem]) << " - ";
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
    DBGSerial << "ND CB: " << _DEC(_cbRet) << endl;
    for (i = 0; i < _cbRet; i++ ) 
      DBGSerial << _HEX(g_diystate.bAPIPacket[i]) << " ";
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
  word w;
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
// ParseHexString
//==============================================================================
word ParseHexString(byte *psz) {
  word w = 0;
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
  word wDLt;
  word w;

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
        DBGSerial << "Choose Item: " << _DEC(iItem) << " DL: " << _HEX(wDLt) << endl;
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









