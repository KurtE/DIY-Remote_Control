//=============================================================================
// Display.h XBee Support for the DIY Remote control
// This class is used to hopefully allow multiple implentations, depending on
// which type of display we are using.  First version will be for the OLED...
//=============================================================================
#ifndef _DISPLAY_H_
#define _DISPLAY_H_

// Also lets define some colors here to make it easier to read some of the code.
#define DCLR_WHITE        0xFFFF
#define DCLR_BLACK        0x0000
#define DCLR_RED          0xF800
#define DCLR_BLUE         0x001F
#define DCLR_GREEN        0x07E0

class RemoteDisplay {
public:
    void     Init(void);               // do whatever is needed for initial init
    
    void    SwitchToDisplayMode(byte bMode);    // Init to specified mode 0=default
   
    void    StartDispalyUpdate(void);  // Lets display know we are about to change things...
    void    DisplayKeypad(char ch);    // display the current keypad data.
    void    DisplayJoystick(byte iJoy, byte x, byte y, byte z);
    void    DisplaySliders(byte bLeft, byte bMid, byte bRight);
    void    DisplayKey(char ch);            // Display a character from the display...
    void    DisplayConnectStatus(boolean fConnected);
    
    void    DisplayRemoteValue(byte iField,  word wVal);
    void    DisplayStatus(char *pbTitle, char *pb);
    void    DisplayBatStatus(byte iBat, byte iPercent);    
    void    SetBatStatusChar(byte iBat, byte bChar);    // Set a display char to label the battery symbol (only 1 char)...
    
    // Functions for Change DL mode
    void    DMDisplayCurDL(word wDL);
    void    DMDisplayNewDL(word wDL);
    void    DMDisplayNI(char *psz, boolean fCurDL);
    void    DMDisplayStatus(char *pb);

    // Functions for Change MY mode
    void    MMDisplayCurMY(word wMy);
    void    MMDisplayNewMY(word wMy);
    void    MMDisplayStatus(char *pb);

    
private:
    word _awJoyX[2];                //  Joystick values
    word _awJoyY[2];                // Could be bytes b
    word _awJoyZ[2];                
    word _wRSlider;                // Likewise for sliders
    word _wMSlider;                
    word _wLSlider;
    byte _abBatPercent[3];        // We support up to 3 batteries (0=Ours, 1 and 2 is for robot VS and VL possible...)
    boolean _fConnected;

};

extern RemoteDisplay g_display;
#endif

