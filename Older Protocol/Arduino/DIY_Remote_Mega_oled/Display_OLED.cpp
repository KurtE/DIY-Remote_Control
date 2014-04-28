//=============================================================================
// Display_OLED.cpp - implementation for the OLED version of display for the
//            remote.
//=============================================================================
#include "globals.h"
#include <Streaming.h>



//=============================================================================
// OLEDWaitForAck: Wait for a simple ack...
//=============================================================================
boolean OLEDWaitForAck(unsigned long ulDelay) {               // lets initialize our display
    unsigned long ulTimeStart = millis();
    int ch;
    do {
        while (!OLEDSerial.available()) {
            if ((millis() - ulTimeStart) > ulDelay) {
                return false;
            }
        } 
        ch = OLEDSerial.read();
    } while ((ch !=0x6) && (ch != 0x15));
    return ch == 0x6;
}

//=============================================================================
// OLEDWaitForPreviousCommandToComplete: If there was a previous command output, 
//        wait for it to complete. before we allow the next command to start...
//=============================================================================
boolean g_fOLEDCommandActive = false;

boolean OLEDWaitForPreviousCommandToComplete(unsigned long ulDelay) {               // lets initialize our display
    if (g_fOLEDCommandActive) {
        g_fOLEDCommandActive = false;
        unsigned long ulTimeStart = millis();
    
    
        int ch;
        do {
            while (!OLEDSerial.available()) {
                if ((millis() - ulTimeStart) > ulDelay) {
                    return false;
                }
            } 
            ch = OLEDSerial.read();
        } while ((ch !=0x6) && (ch != 0x15));
        return ch == 0x6;
    }
    return true;
}
//=============================================================================
// OLEDSetCommandActive
//=============================================================================
inline void OLEDSetCommandActive(void) {
    g_fOLEDCommandActive = true;
}    


//=============================================================================
// OLEDWaitForResp: Some commands return a multi byte response
//=============================================================================
boolean OLEDWaitforResp(byte *pb, byte cb, unsigned long ulDelay) {               // lets initialize our display
    unsigned long ulTimeStart = millis();

    while (cb > 0) {
        do {
            if ((millis() - ulTimeStart) > ulDelay)
                return false;
        } 
        while (OLEDSerial.available() < 1);
        *pb++ = OLEDSerial.read();
        cb--;
    }
}

//=============================================================================
// OLEDDisplayString
//=============================================================================
void OLEDDisplayString(byte x, byte y, byte bFont, word wColor, const char *pb) {

    OLEDWaitForPreviousCommandToComplete(100);
    OLEDSerial << "s" << _BYTE(x) << _BYTE(y) << _BYTE(bFont)
        << _BYTE((wColor >> 8) & 0xff) <<_BYTE(wColor & 0xff);
    OLEDSerial.write(pb);
    OLEDSerial.write((uint8_t)0x0);
    OLEDSetCommandActive();
}

//=============================================================================
// OLEDDisplayNum
//=============================================================================
void OLEDDisplayNum(byte x, byte y, byte bFont, word wColor, word w, byte bCnt) {
    char ab[6];        // can hold the max string a word can 

    // convert number to string, that is right justified in our string.
    ab[bCnt] = 0;        // last character
    while (bCnt > 0) {
        bCnt--;
        ab[bCnt] = '0' + w % 10;  // get the last digit probably faster ways
        w /= 10;            // Setup for the next digit
    }

    OLEDWaitForPreviousCommandToComplete(100);
    OLEDSerial << "s" << _BYTE(x) << _BYTE(y) << _BYTE(bFont)
        << _BYTE((wColor >> 8) & 0xff) <<_BYTE(wColor & 0xff);

    OLEDSerial.write(ab);
    OLEDSerial.write((uint8_t)0);
    OLEDSetCommandActive();
}


//=============================================================================
// OLEDDrawRect: Draw a rectangle
//=============================================================================
void OLEDDrawRect(byte x1, byte y1, byte x2, byte y2, word wColor) {
    OLEDWaitForPreviousCommandToComplete(100);
    OLEDSerial << "r" << _BYTE(x1) << _BYTE(y1) << _BYTE(x2) << _BYTE(y2)
        << _BYTE((wColor >> 8) & 0xff) <<_BYTE(wColor & 0xff);
    OLEDSetCommandActive();
}

//=============================================================================
// OLEDSetPenSize: Set the pensize to 00 - fill or 01 for wire frame
//=============================================================================
void OLEDSetPenSize(byte bPenSize) {
    OLEDWaitForPreviousCommandToComplete(100);
    OLEDSerial << "p" << _BYTE(bPenSize);
    OLEDSetCommandActive();
}

//=============================================================================
// InitOLED: Initialize our OLED display
//=============================================================================
void RemoteDisplay::Init(void) {                // lets initialize our display
    byte bBuffer[10];                // Have a default buffer
    // first do the default stuff
    OLEDSerial.begin(125000);        // lets try as fast a speed as possible.
    do {
        delay(250);                // give some time for hardware to initialize
        OLEDSerial.write((uint8_t)0x55);
        OLEDSerial.write((uint8_t)0x0);
    } 
    while (!OLEDWaitForAck(100));

    // Try the same init as forum member...
    OLEDSerial << _BYTE(0x45) << _BYTE(0x00);       // Clear the screen
    OLEDWaitForAck(200);
    delay(200);
    
    // clear out anything that is hung up in the queue...
    while (OLEDSerial.read() != -1)
      ;
    //    OLEDSerial.flush();                             // get rid of any other data received
    OLEDSerial << _BYTE(0x56) << _BYTE(0x01) << _BYTE(0);       // Show version
    OLEDWaitforResp(bBuffer, 5, 1000);              // Expect a 5 byte response
    delay(1500);            // give some time to see version
}  

//=============================================================================
//=============================================================================
void RemoteDisplay::SwitchToDisplayMode(byte bMode) {
    OLEDWaitForPreviousCommandToComplete(100);
    OLEDSerial << _BYTE(0x45) << _BYTE(0x00);       // Clear the screen
    OLEDSetCommandActive();
    delay(200);
    // Lets try some simple text output for now.
    switch (bMode) {
        case MODE_NORMAL:
        default:
            OLEDDisplayString(0, 1, 2, DCLR_WHITE, "Left");
            OLEDDisplayString(8, 1, 2, DCLR_WHITE, "Right");
            OLEDDisplayString(0, 5, 2,DCLR_WHITE, "Sliders");

            OLEDWaitForPreviousCommandToComplete(100);
            OLEDSerial << _BYTE(0x4f) << _BYTE(1);             // set text as opaque
            OLEDSetCommandActive();

            // make sure we display each of the values.
            _awJoyX[0] = 0xffff;
            _awJoyX[1] = 0xffff;
            _awJoyY[0] = 0xffff;
            _awJoyY[1] = 0xffff;
            _awJoyZ[0] = 0xffff;
            _awJoyZ[1] = 0xffff;
            _wRSlider = 0xffff;                // Likewise for sliders
            _wLSlider = 0xffff;
            _wMSlider = 0xffff;
            _abBatPercent[0] = 0xff;
            _abBatPercent[1] = 0xff;
            _abBatPercent[2] = 0xff;
            _fConnected = 0;
            break;
            
        case MODE_CHANGE_DEST_LIST:
            OLEDDisplayString(0, 0, 2, DCLR_WHITE, "Choose Dest");
            OLEDDisplayString(0, 1, 2, DCLR_GREEN, "RJoy Select Item");
            OLEDDisplayString(0, 2, 2, DCLR_GREEN, "LJoy Btn - Scan");
            OLEDDisplayString(0, 3, 2, DCLR_GREEN, "Keypad Direct");
            OLEDDisplayString(0, 7, 2, DCLR_WHITE, "Current:");
            OLEDDisplayString(0, 8, 2,DCLR_WHITE, "New:");  
            
        
            break;
            
        case MODE_CHANGE_MY:
            OLEDDisplayString(0, 0, 2, DCLR_WHITE, "Choose My");
            OLEDDisplayString(0, 7, 2, DCLR_WHITE, "Current:");
            OLEDDisplayString(0, 8, 2,DCLR_WHITE, "New:");  
            break;
            
        case MODE_CALIBRATE:
            OLEDDisplayString(0, 0, 2, DCLR_WHITE, "Calibrate");
            OLEDDisplayString(0, 1, 2, DCLR_GREEN, "Move All Joysticks");
            OLEDDisplayString(0, 2, 2, DCLR_GREEN, "and sliders to");
            OLEDDisplayString(0, 3, 2, DCLR_GREEN, "to full extent");
            break;    
    }
}


//=============================================================================
//=============================================================================
void RemoteDisplay::StartDispalyUpdate(void) {
//    OLEDSerial.flush();    // simply flush anything that may have been stuck here.
}

//=============================================================================
// DisplayJoytick : display information about one joystick.
//=============================================================================
void RemoteDisplay::DisplayJoystick(byte iJoy, byte x, byte y, byte z) {
    byte bCol;
    byte bColP;
    byte bJoyVal;
    
    bCol = iJoy? 8 : 0;
    bColP = bCol * 8 + 28;        // convert to pixel offsets	
    
    // First lets output the Text values
    if (x != _awJoyX[iJoy]) {
        _awJoyX[iJoy] = x;
        OLEDDisplayNum(bCol, 2, 2, DCLR_RED, x, 3);        // X - Red
        
        //X StickUpdate
        bJoyVal = x/8;     // Range of 0-31...
        OLEDDrawRect(bColP, 25, bColP + bJoyVal, 34, DCLR_WHITE);          // White square
        OLEDDrawRect(bColP + bJoyVal, 25, bColP + 32, 34, DCLR_BLACK);     // black Square
    }
    
    
    if (y != _awJoyY[iJoy]) {
        _awJoyY[iJoy] = y;
        OLEDDisplayNum(bCol, 3, 2, DCLR_RED, y, 3);        // Y
   
        bJoyVal = y/8;     // Range of 0-31...
        OLEDDrawRect(bColP, 37, bColP + bJoyVal, 46, DCLR_GREEN);          // Green square
        OLEDDrawRect(bColP + bJoyVal, 37, bColP + 32, 46, DCLR_BLACK);     // black Square
    }
   
    if (z != _awJoyZ[iJoy]) {
        _awJoyZ[iJoy] = z;
        OLEDDisplayNum(bCol, 4, 2, DCLR_RED, z, 3);        // Z

        bJoyVal = z/8;     // Range of 0-31...
        OLEDDrawRect(bColP, 49, bColP + bJoyVal, 58, DCLR_BLUE);          // Blue square
        OLEDDrawRect(bColP + bJoyVal, 49, bColP + 32, 58, DCLR_BLACK);     // black Square
    }
}    



//=============================================================================
// DisplaySliders : display information about the sliders - only output if changed
//=============================================================================
void RemoteDisplay::DisplaySliders(byte bLeft, byte bMid, byte bRight) {
    byte bCol;
    byte bJoyVal;

    // First lets output the left slider value and 
    if (_wLSlider != bLeft) {
        _wLSlider = bLeft;
        OLEDDisplayNum(0, 6, 2, DCLR_RED, bLeft, 3);        // 
        // Left Slider
        bCol = 24;        // convert to pixel offsets	
    
        bJoyVal = bLeft/16;     // Range of 0-15...
        OLEDDrawRect(bCol, 73, bCol + bJoyVal, 82, DCLR_GREEN);          // Green square
        OLEDDrawRect(bCol + bJoyVal, 73, bCol + 16, 82, DCLR_BLACK);     // black Square
    }

    // Now lets process MSlider
    if (_wMSlider != bMid) {
        _wMSlider = bMid;
        OLEDDisplayNum(5, 6, 2, DCLR_RED, bMid, 3);      // Y
        bCol = 5 * 8 + 24;        // convert to pixel offsets	
        bJoyVal = bMid/16;     // Range of 0-15...
        OLEDDrawRect(bCol, 73, bCol + bJoyVal, 82, DCLR_GREEN);          // Green square
        OLEDDrawRect(bCol + bJoyVal, 73, bCol + 16, 82, DCLR_BLACK);     // black Square
    }
    // Now lets process RSlider
    if (_wRSlider != bRight) {
        _wRSlider = bRight;
        OLEDDisplayNum(10, 6, 2, DCLR_RED, bRight, 3);      // Y
        bCol = 10 * 8 + 24;        // convert to pixel offsets	
        bJoyVal = bRight/16;     // Range of 0-15...
        OLEDDrawRect(bCol, 73, bCol + bJoyVal, 82, DCLR_GREEN);          // Green square
        OLEDDrawRect(bCol + bJoyVal, 73, bCol + 16, 82, DCLR_BLACK);     // black Square
    }
}    

//=============================================================================
// DisplayKey : display what key on the keypad is pressed
//=============================================================================
void RemoteDisplay::DisplayKey(char ch) {            // Display a character from the display...
    char abT[2];
    OLEDWaitForPreviousCommandToComplete(100);
    OLEDSerial << _BYTE(0x4f) << _BYTE(1);           // set text as opaque
    OLEDSetCommandActive();

    abT[0] = ch;
    abT[1] = 0;    // could use the other output
    OLEDDisplayString(0, 0, 2, DCLR_WHITE, abT);
}
//=============================================================================
// DisplaySliders : display information about the sliders - only output if changed
//=============================================================================
void RemoteDisplay::DisplayConnectStatus(boolean fConnected) {
    if (_fConnected != fConnected) {
        _fConnected = fConnected;    // remember the new status
        if (fConnected)
           OLEDDisplayString(15, 0, 2, DCLR_GREEN, "*");
        else
           OLEDDisplayString(15, 0, 2, DCLR_BLACK, " ");
    }
}

//=============================================================================
// DisplayStatus : display status information.  We allow for titles and also
//                 we scroll data between the last 2 lines
//=============================================================================
void RemoteDisplay::DisplayStatus(char *pbTitle, char *pb) {
    byte cb;
    // For this round I am going to display 2 lines of Status.  If no title
    // is specified, I will scroll the previous status line up, else will display
    // Title...
    if (pbTitle) {
       // Draw the text and clear the rest of the line out
       OLEDDisplayString(0, 8, 2, DCLR_WHITE, pbTitle);         // draw the text
       cb = strlen(pbTitle);
       OLEDDrawRect(cb*8, 8*12, 127, 9*12-1, DCLR_BLACK);    // should clear the rest of the line out.
    } else {
        // Scroll up the stuff from the next line... use screen copy paste command.
            OLEDSerial << "c" << _BYTE(0) << _BYTE(9*12) << _BYTE(0) << _BYTE(8*12) << _BYTE(128) << _BYTE(12);
            OLEDWaitForAck(100);
    }
    
    cb = 0;
    if (pb) {
       OLEDDisplayString(0, 9, 2, DCLR_WHITE, pb);         // draw the text
       cb = strlen(pb);
    }   
    OLEDDrawRect(cb*8, 9*12, 127, 10*12-1, DCLR_BLACK);    // should clear the rest of the line out.
    
}

//=============================================================================
// DisplayRemoteValue : Display a number that was passed to us...
//=============================================================================
void RemoteDisplay::DisplayRemoteValue(byte iField,  word wVal) {
}

//=============================================================================
// DisplayBatStatus : display information about one of the battery values.
//        if necessary we will draw the actual outline.  Otherwise we will
//        check to see if the value has changed and if so we will update the
//        display...
//=============================================================================
void RemoteDisplay::DisplayBatStatus(byte iBat, byte iPercent) {
    byte bx;
    byte bxFill;
    
    bx = iBat*38 + 14 + 8;    // Use 38 pixels per battery display save room to display 1 char before...
    
    if (_abBatPercent[iBat] != iPercent) {
        // This pass will redraw everything...
        
        // First outline...
        OLEDSetPenSize(1);                                // set to wire frame
        OLEDDrawRect(bx, 0, bx + 29, 9, DCLR_WHITE);          // White square
        OLEDSetPenSize(0);
        OLEDDrawRect(bx+30, 2, bx + 32, 7, DCLR_WHITE);       // White square
        
        // Noww lets display the fill the correct percentage of the indicator
        bxFill = bx + 1 + ((word)(7*iPercent))/25;        //(28*iPercent) / 100
        if (bxFill >= (bx + 2))
            OLEDDrawRect(bx+1, 1, bxFill, 8, DCLR_GREEN);          // Green square
        if (bxFill < (bx+28)) 
            OLEDDrawRect(bxFill+1, 1, bx+28, 8, 0x0);          // black square
    }        
        

}


//=============================================================================
// ConvertWordToHexString 
//=============================================================================
void ConvertWordToHexString(word w, char *pb) {
    char b;
    byte i;
    
    pb+=4;
    *pb = 0x0 ;     // make sure null terminated...
    for (i=0; i<4; i++) {
        b = w & 0xf;    // get low nibble
        if (b <= 9)
            *--pb = '0'+b;
        else
            *--pb = 'A'+b-10;
        w >>= 4;
    }    
}



//=============================================================================
// DisplayJoytick : display information about one joystick.
//=============================================================================
void RemoteDisplay::SetBatStatusChar(byte iBat, byte bChar) {    // Set a display char to label the battery symbol (only 1 char)...
    // First pass ignore...
}

//=============================================================================
// DMDisplayCurDL - Choose DL mode - Display current DL
//=============================================================================
void RemoteDisplay::DMDisplayCurDL(word wDL) {
    char ab[5];
    
    ConvertWordToHexString(wDL, ab);
    OLEDDisplayString(9, 7, 2, DCLR_RED, ab);
}

//=============================================================================
// DMDisplayNewDL - Choose DL Mode - Display New DL
//=============================================================================
void  RemoteDisplay::DMDisplayNewDL(word wDL) {
    char ab[5];
    
    ConvertWordToHexString(wDL, ab);
    OLEDDisplayString(5, 8, 2, DCLR_RED, ab);
}

//=============================================================================
// DMDisplayNI - display a NodeIdentifier.
//=============================================================================
void RemoteDisplay::DMDisplayNI(char *psz, boolean fCurDL) {
    OLEDDisplayString(0, 5, 2, DCLR_BLUE, fCurDL ? "*" : " ");
    OLEDDisplayString(1, 5, 2, DCLR_BLUE, psz);
}

//=============================================================================
// DMDisplayStatus - Display status about the operation...
//=============================================================================
void RemoteDisplay::DMDisplayStatus(char *pb) {
    byte cb = 0;
    if (pb) {
        cb = strlen(pb);
        OLEDDisplayString(0, 9, 2, DCLR_GREEN, pb);
    }
   
    OLEDDrawRect(cb*8, 9*12, 127, 10*12-1, DCLR_BLACK);    // should clear the rest of the line out.
}
//=============================================================================
// MMDisplayCurMY - Choose MY mode - Display Current My
//=============================================================================
void RemoteDisplay::MMDisplayCurMY(word wMy) {
    char ab[5];
    
    ConvertWordToHexString(wMy, ab);
    OLEDDisplayString(9, 7, 2, DCLR_RED, ab);
}

//=============================================================================
// MMDisplayNewMY - Choose My mode - Display New My
//=============================================================================
void RemoteDisplay::MMDisplayNewMY(word wMy) {
    char ab[5];
    
    ConvertWordToHexString(wMy, ab);
    OLEDDisplayString(5, 8, 2, DCLR_RED, ab);
}


//=============================================================================
// MMDisplayStatus - Display status about the operation...
//=============================================================================
void RemoteDisplay::MMDisplayStatus(char *pb) {
    OLEDDisplayString(0, 9, 2, DCLR_GREEN, pb);
}
