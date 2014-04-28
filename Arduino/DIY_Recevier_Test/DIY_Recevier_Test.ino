//
// Test XBee and Adafruit GFX Display...
#define XBeeSerial Serial1
#define DBGSerial Serial
#define XBEE_BAUD 115200
#define DEBUG_PINS_FIRST 2
//#define OPT_DEBUGPINS

#ifdef OPT_DEBUGPINS
#define DebugToggle(pin)  {digitalWrite(pin, !digitalRead(pin));}
#define DEBUGTOGGLE(pin) digitalWrite(pin, !digitalRead(pin))
#define DebugWrite(pin, state) {digitalWrite(pin, state);}
#else
#define DebugToggle(pin)  {;}
#define DEBUGTOGGLE(pin) 
#define DebugWrite(pin, state) {;}
#endif

//Use these pins for the shield!
#define sclk 13
#define mosi 11
#define cs   10
#define dc   8
#define rst  0  // you can also connect this to the Arduino reset
#define sdcs 4   // CS for SD card, can use any pin

//
#define FIELD_X   45
#define FIELD_X2  100
#define FIELD_Y1   10
#define FIELD_YINC 8

#define BUTTON_NONE 0
#define BUTTON_DOWN 1
#define BUTTON_RIGHT 2
#define BUTTON_SELECT 3
#define BUTTON_UP 4
#define BUTTON_LEFT 5

extern void DisplayField(int x, int y, word wVal, int fmt=DEC);


//========================================================================

#include "_diyxbee.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>


//=======================================================================
DIYPACKET  g_diyp;
DIYPACKET  g_diypPrev;
word g_wAPIDLPrev;
word g_wCompositButtons;
uint8_t g_bBtnPrev = 0; 

unsigned long ulStart;
word   wCnt = 0;
word wDelay = 25;
Adafruit_ST7735 tft = Adafruit_ST7735(cs, dc, rst);
boolean g_fDebugOutput = true;

boolean g_fFirstPacket = true; // On first packet should show all data.
//=======================================================================
void setup() {

  // Init the TFT display
  delay(2000);  // give me a little time to start terminal
  pinMode(sdcs, INPUT_PULLUP);  // don't touch the SD card
  Serial.begin(38400);
  Serial.println("DIY Receiver test");
  tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab
  tft.fillScreen(ST7735_BLACK);
  tft.setRotation(1);
  tft.setTextWrap(false);
  tft.setCursor(0, 10);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.println("Btns:");
  tft.println("RLR:");
  tft.println("RUD:");
  tft.println("LLR:");
  tft.println("LUD:");
  tft.println("R SL:");
  tft.println("L SL:");
  tft.println("R Pot:");
  tft.println("L Pot:");
  tft.println("M SL:");
  tft.println("Btns2");
  tft.println("DL:");
  tft.println("Rate:");
  tft.println("Delay:");


  InitXBee();  // Init the XBee
  Serial.print("XBee My: ");
  Serial.println(GetXBeeMY(), HEX);
  Serial.print("DL ");
  Serial.println(GetXBeeDL(), HEX);


  delay(20);
  ClearXBeeInputBuffer();
  DisplayField(FIELD_X, FIELD_Y1 + 13 * FIELD_YINC, wDelay);

  ulStart = millis();
}

void DisplayField(int x, int y, word wVal, int fmt) {
  tft.setCursor(x, y);
  tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
  tft.setTextSize(1);
  tft.print(wVal, fmt);
  tft.print("   ");  // Hack, if number is shorter this will get rid of other values...
}

void ClearFields()
{
  tft.fillRect(FIELD_X, FIELD_Y1, 160-FIELD_X, FIELD_YINC*11, ST7735_BLACK);
}

char g_szButton[] = "  Pressed";
void loop() {
  if (ReceiveXBeePacket(&g_diyp)) {
    uint16_t wCompositButtons = g_diyp.s.wButtons;
    if(g_diystate.fNewPacket) {
      while (g_diystate.fNewPacket) {
        wCnt++;  // Count of messages
        wCompositButtons = (uint16_t)(wCompositButtons & (uint16_t)~g_diypPrev.s.wButtons) | (uint16_t)g_diyp.s.wButtons;
        if (!ReceiveXBeePacket(&g_diyp))
          break;
      }
      if (g_wCompositButtons!= wCompositButtons) {
        DisplayField(FIELD_X2, FIELD_Y1 + 0 * FIELD_YINC, wCompositButtons, HEX);
        g_wCompositButtons = wCompositButtons;
      }

      if (memcmp((void*)&g_diyp, (void*)&g_diypPrev, sizeof(g_diyp)) != 0) {
        if (g_fFirstPacket || g_diyp.s.wButtons != g_diypPrev.s.wButtons) {
          DisplayField(FIELD_X, FIELD_Y1 + 0 * FIELD_YINC, g_diyp.s.wButtons, HEX);
          if ((g_diyp.s.wButtons & 1) && !(g_diypPrev.s.wButtons & 1))
            XBeePlaySounds(1, 100, 3000);
          else if ((g_diyp.s.wButtons & 2) && !(g_diypPrev.s.wButtons & 2))
            XBeePlaySounds(2, 100, 3000, 100, 2000);
          else if (g_diyp.s.wButtons && !g_diypPrev.s.wButtons) {
            // not 100% good, but fine for test. 
            uint16_t w = g_diyp.s.wButtons;
            char ch = '0';
            while ((w & 1) == 0) {
              w>>= 1;
              ch = (ch == '9')? 'A' : (ch + 1);
            }
            g_szButton[0] = ch;
            XBeeOutputString(g_szButton);
          }   
        }  
        if (g_fFirstPacket || (g_diyp.s.bRJoyLR != g_diypPrev.s.bRJoyLR))
          DisplayField(FIELD_X, FIELD_Y1 + 1 * FIELD_YINC, g_diyp.s.bRJoyLR);
        if ((g_fFirstPacket || g_diyp.s.bRJoyUD != g_diypPrev.s.bRJoyUD))
          DisplayField(FIELD_X, FIELD_Y1 + 2 * FIELD_YINC, g_diyp.s.bRJoyUD);
        if (g_fFirstPacket || (g_diyp.s.bLJoyLR != g_diypPrev.s.bLJoyLR))
          DisplayField(FIELD_X, FIELD_Y1 + 3 * FIELD_YINC, g_diyp.s.bLJoyLR);
        if (g_fFirstPacket || (g_diyp.s.bLJoyUD != g_diypPrev.s.bLJoyUD))
          DisplayField(FIELD_X, FIELD_Y1 + 4 * FIELD_YINC, g_diyp.s.bLJoyUD);
        if (g_fFirstPacket || (g_diyp.s.bRSlider != g_diypPrev.s.bRSlider))
          DisplayField(FIELD_X, FIELD_Y1 + 5 * FIELD_YINC, g_diyp.s.bRSlider);
        if (g_fFirstPacket || (g_diyp.s.bLSlider != g_diypPrev.s.bLSlider))
          DisplayField(FIELD_X, FIELD_Y1 + 6 * FIELD_YINC, g_diyp.s.bLSlider);
        if (g_diystate.cbPacketSize > sizeof(DIYPACKETORIG)) {
          if (g_fFirstPacket || (g_diyp.s.bRPot != g_diypPrev.s.bRPot)) 

            // Note Not all remotes have these fields.
            DisplayField(FIELD_X, FIELD_Y1 + 7 * FIELD_YINC, g_diyp.s.bRPot);
          if (g_fFirstPacket || (g_diyp.s.bLPot != g_diypPrev.s.bLPot))
            DisplayField(FIELD_X, FIELD_Y1 + 8 * FIELD_YINC, g_diyp.s.bLPot);

          // New Arduino/Teensy has a few more fields.
          if (g_diystate.cbPacketSize > (sizeof(DIYPACKETORIG)+2)) {
            if (g_fFirstPacket || (g_diyp.s.bMSlider != g_diypPrev.s.bMSlider)) 
              DisplayField(FIELD_X, FIELD_Y1 + 9 * FIELD_YINC, g_diyp.s.bMSlider);
            if (g_fFirstPacket || (g_diyp.s.bButtons2 != g_diypPrev.s.bButtons2))
              DisplayField(FIELD_X, FIELD_Y1 + 10 * FIELD_YINC, g_diyp.s.bButtons2, HEX);
          }
        }
        if (g_diystate.wAPIDL != g_wAPIDLPrev){
          g_wAPIDLPrev = g_diystate.wAPIDL;
          DisplayField(FIELD_X, FIELD_Y1 + 11 * FIELD_YINC, g_wAPIDLPrev, HEX);
        }
        // remember current state
        g_diypPrev = g_diyp;
        g_fFirstPacket = false;
      }
    }
  }      

  unsigned long ulTime = millis();
  if ((ulTime - ulStart) >= 1000) {
    DisplayField(FIELD_X, FIELD_Y1 + 12 * FIELD_YINC, wCnt/*(wCnt*1000)/(ulTime - ulStart)*/);
    XBeeOutputVal((wCnt*1000)/(ulTime-ulStart));
    ulStart = ulTime;
    if (!wCnt && !g_fFirstPacket) {
      // We displayed stuff but nothing coming in, lets clear display...
      ClearFields();
      g_fFirstPacket = true;
    }
    wCnt = 0;
  }
  uint8_t bBtn = readButton();
  if (bBtn != g_bBtnPrev) {
    if ((bBtn == BUTTON_LEFT) && (wDelay > 0))
      wDelay -= 25;
    if ((bBtn == BUTTON_RIGHT) && (wDelay < 2000))
      wDelay += 25;
    g_bBtnPrev = bBtn; 
    DisplayField(FIELD_X, FIELD_Y1 + 13 * FIELD_YINC, wDelay);
  }      
  DisplayField(FIELD_X2, FIELD_Y1 + 13 * FIELD_YINC, analogRead(3));

  delay(wDelay);  // put some delay in to see about reading multiple packets.
}

uint8_t readButton(void) {
  float a = analogRead(3);

  a *= 3.3;
  a /= 1024.0;

  //  Serial.print("Button read analog = ");
  //  Serial.println(a);
  if (a < 0.2) return BUTTON_DOWN;
  if (a < 1.0) return BUTTON_RIGHT;
  if (a < 1.5) return BUTTON_SELECT;
  if (a < 2.0) return BUTTON_UP;
  if (a < 3.2) return BUTTON_LEFT;
  else return BUTTON_NONE;
}



void  DoBackgroundProcess() {
}


#include "_diyxbee_code.h"










