//====================================================================
//Project Lynxmotion Phoenix
//Description: 
//    This is the hardware configuration for the Lynxmotion SQ3 Quad
//    using type Aof legs.
//  
//    This version of the Configuration file is set up to run on the
//    Teensy 3.1 board (could also try on Arduino Mega/Due///)
//
//    This version of configuration file assumes that the servos will be controlled
//    directly by the processor and uses Trossen Arbotix Commander for input.
//
//====================================================================
#ifndef QUAD_CFG_H
#define QUAD_CFG_H
#define USE_XBEE
#define USEXBEE
// Which type of control(s) do you want to compile in

//#define DEBUG
//#define OPT_DEBUGPINS
#define DEBUG_PINS_FIRST 29

// Figure out which Serial ports we can use for what.  This will depend on which processor I am compiling for. 
#ifdef __AVR__
#if defined(UBRR3H)
#define DBGSerial    Serial
#dfefine XBeeSerial   Serial1
#define SSCSerial     Serial3
#endif
#else if defined(__MK20DX256__)
// Teensy 3.1
#define DBGSerial      Serial
#define XBeeSerial     Serial2
//#define SSCSerial      Serial3
#endif

//==================================================================================================================================
// Define which input class we will use. 
//==================================================================================================================================
#define QUADMODE            // We are building for quad support...

#ifdef DBGSerial
#define OPT_TERMINAL_MONITOR  // Only allow this to be defined if we have a debug serial port
#endif

#ifdef OPT_TERMINAL_MONITOR
#define OPT_DUMP_EEPROM
#define OPT_FIND_SERVO_OFFSETS    // Only useful if terminal monitor is enabled
#endif

//#define OPT_GPPLAYER
#define OPT_DYNAMIC_ADJUST_LEGS
#define ADJUSTABLE_LEG_ANGLES

#define USE_SSC32
//#define	cSSC_BINARYMODE	1			// Define if your SSC-32 card supports binary mode.

#if defined(__MK20DX256__)
#define XBEE_BAUD        38400   // May increase...
#define DISPLAY_GAIT_NAMES
#else
#define cSSC_BAUD        38400   //SSC32 BAUD rate
#endif
#define NOT_SURE_WHY_NEEDED_SOMETIMES
// Debug options
//#define DEBUG_IOPINS    // used to control if we are going to use IO pins for debug support

//==================================================================================================================================
//==================================================================================================================================
//==================================================================================================================================
// SQ3
//==================================================================================================================================

#if defined(__MK20DX256__)
#define SOUND_PIN    6
// PS2 definitions
#define PS2_DAT      2        
#define PS2_CMD      3
#define PS2_SEL      4
#define PS2_CLK      5

// from my teensy board
#define CVADR1      402  // VD Resistor 1 - reduced as only need ratio... 40.2K and 10K
#define CVADR2      100    // VD Resistor 2
#define CVREF       330    // 3.3v


#else
#define SOUND_PIN    5        // Botboarduino JR pin number

#endif

// XBee was defined to use a hardware Serial port
#define XBEE_BAUD      38400
#define SERIAL_BAUD    38400

// Define Analog pin and minimum voltage that we will allow the servos to run
#define cVoltagePin  0      // Use our Analog pin jumper here...
#define cTurnOffVol  470     // 4.7v
#define cTurnOnVol   550     // 5.5V - optional part to say if voltage goes back up, turn it back on...

//====================================================================
// Warning I reversed my legs as I put left legs where right should be...
//[Servo pins on main processor board.
// 
#define cRRCoxaPin      2   //Rear Right leg Hip Horizontal
#define cRRFemurPin     3   //Rear Right leg Hip Vertical
#define cRRTibiaPin     4   //Rear Right leg Knee

#define cRFCoxaPin      8    //Front Right leg Hip Horizontal
#define cRFFemurPin     11    //Front Right leg Hip Vertical
#define cRFTibiaPin     12   //Front Right leg Knee

#define cLRCoxaPin      23   //Rear Left leg Hip Horizontal
#define cLRFemurPin     22   //Rear Left leg Hip Vertical
#define cLRTibiaPin     21   //Rear Left leg Knee

#define cLFCoxaPin      17   //Front Left leg Hip Horizontal
#define cLFFemurPin     16   //Front Left leg Hip Vertical
#define cLFTibiaPin     15   //Front Left leg Knee

//--------------------------------------------------------------------
//[SERVO PULSE INVERSE]
#define cRRCoxaInv      1
#define cRRFemurInv     0
#define cRRTibiaInv     0
#define cRRTarsInv      0

#define cRFCoxaInv      1
#define cRFFemurInv     1
#define cRFTibiaInv     1
#define cRFTarsInv      1

#define cLRCoxaInv      0
#define cLRFemurInv     1
#define cLRTibiaInv     1
#define cLRTarsInv      1

#define cLFCoxaInv      0
#define cLFFemurInv     0
#define cLFTibiaInv     0
#define cLFTarsInv      0

//--------------------------------------------------------------------
//[MIN/MAX ANGLES]
#define cRRCoxaMin1     -650      //Mechanical limits of the Right Rear Leg
#define cRRCoxaMax1     650
#define cRRFemurMin1    -1050
#define cRRFemurMax1    750
#define cRRTibiaMin1    -420
#define cRRTibiaMax1    900
#define cRRTarsMin1     -1300	//4DOF ONLY - In theory the kinematics can reach about -160 deg
#define cRRTarsMax1	500	//4DOF ONLY - The kinematics will never exceed 23 deg though..

#define cRFCoxaMin1     -650      //Mechanical limits of the Right Front Leg
#define cRFCoxaMax1     650
#define cRFFemurMin1    -1050
#define cRFFemurMax1    750
#define cRFTibiaMin1    -420
#define cRFTibiaMax1    900
#define cRFTarsMin1     -1300	//4DOF ONLY - In theory the kinematics can reach about -160 deg
#define cRFTarsMax1	500	//4DOF ONLY - The kinematics will never exceed 23 deg though..

#define cLRCoxaMin1     -650      //Mechanical limits of the Left Rear Leg
#define cLRCoxaMax1     650
#define cLRFemurMin1    -1050
#define cLRFemurMax1    750
#define cLRTibiaMin1    -420
#define cLRTibiaMax1    900
#define cLRTarsMin1     -1300	//4DOF ONLY - In theory the kinematics can reach about -160 deg
#define cLRTarsMax1	500	//4DOF ONLY - The kinematics will never exceed 23 deg though..

#define cLFCoxaMin1     -650      //Mechanical limits of the Left Front Leg
#define cLFCoxaMax1     650
#define cLFFemurMin1    -1050
#define cLFFemurMax1    750
#define cLFTibiaMin1    -420
#define cLFTibiaMax1    900
#define cLFTarsMin1     -1300	//4DOF ONLY - In theory the kinematics can reach about -160 deg
#define cLFTarsMax1	500	//4DOF ONLY - The kinematics will never exceed 23 deg though..

//--------------------------------------------------------------------
//[LEG DIMENSIONS]
//Universal dimensions for each leg in mm
#define cXXCoxaLength     29    // This is for CH3-R with Type 3 legs
#define cXXFemurLength    57
#define cXXTibiaLength    141
#define cXXTarsLength     85    // 4DOF only...

#define cRRCoxaLength     cXXCoxaLength	    //Right Rear leg
#define cRRFemurLength    cXXFemurLength
#define cRRTibiaLength    cXXTibiaLength
#define cRRTarsLength	  cXXTarsLength	    //4DOF ONLY

#define cRFCoxaLength     cXXCoxaLength	    //Rigth front leg
#define cRFFemurLength    cXXFemurLength
#define cRFTibiaLength    cXXTibiaLength
#define cRFTarsLength	  cXXTarsLength    //4DOF ONLY

#define cLRCoxaLength     cXXCoxaLength	    //Left Rear leg
#define cLRFemurLength    cXXFemurLength
#define cLRTibiaLength    cXXTibiaLength
#define cLRTarsLength	  cXXTarsLength    //4DOF ONLY

#define cLFCoxaLength     cXXCoxaLength	    //Left front leg
#define cLFFemurLength    cXXFemurLength
#define cLFTibiaLength    cXXTibiaLength
#define cLFTarsLength	  cXXTarsLength	    //4DOF ONLY


//--------------------------------------------------------------------
//[BODY DIMENSIONS]
#define cRRCoxaAngle1   -450   //Default Coxa setup angle, decimals = 1
#define cRFCoxaAngle1    450   //Default Coxa setup angle, decimals = 1
#define cLRCoxaAngle1   -450   //Default Coxa setup angle, decimals = 1
#define cLFCoxaAngle1    450   //Default Coxa setup angle, decimals = 1

#define cRROffsetX      -54    //Distance X from center of the body to the Right Rear coxa
#define cRROffsetZ       54    //Distance Z from center of the body to the Right Rear coxa
#define cRFOffsetX      -54    //Distance X from center of the body to the Right Front coxa
#define cRFOffsetZ      -54    //Distance Z from center of the body to the Right Front coxa

#define cLROffsetX       54    //Distance X from center of the body to the Left Rear coxa
#define cLROffsetZ       54    //Distance Z from center of the body to the Left Rear coxa
#define cLFOffsetX       54     //Distance X from center of the body to the Left Front coxa
#define cLFOffsetZ      -54    //Distance Z from center of the body to the Left Front coxa

//--------------------------------------------------------------------
//[START POSITIONS FEET]
#define CRobotInitXZ	 80 
#define CRobotInitXZ45    57        // Sin and cos(45) .7071
#define CRobotInitY	80

// Lets try 37.5 for the fun of it.
#define cRobotInitXZCos     59         // 0.73727733681012404138429339498232
#define cRobotInitXZSin     54         // 0.67559020761566024434833935367435

#if 1
#define cRRInitPosX     cRobotInitXZCos      //Start positions of the Right Rear leg
#define cRRInitPosY     CRobotInitY
#define cRRInitPosZ     cRobotInitXZSin

#define cRFInitPosX     cRobotInitXZCos      //Start positions of the Right Front leg
#define cRFInitPosY     CRobotInitY
#define cRFInitPosZ     -cRobotInitXZSin

#define cLRInitPosX     cRobotInitXZCos      //Start positions of the Left Rear leg
#define cLRInitPosY     CRobotInitY
#define cLRInitPosZ     cRobotInitXZSin

#define cLFInitPosX     cRobotInitXZCos      //Start positions of the Left Front leg
#define cLFInitPosY     CRobotInitY
#define cLFInitPosZ     -cRobotInitXZSin


#else // 45 degrees
#define cRRInitPosX     CRobotInitXZ45      //Start positions of the Right Rear leg
#define cRRInitPosY     CRobotInitY
#define cRRInitPosZ     CRobotInitXZ45

#define cRFInitPosX     CRobotInitXZ45      //Start positions of the Right Front leg
#define cRFInitPosY     CRobotInitY
#define cRFInitPosZ     -CRobotInitXZ45

#define cLRInitPosX     CRobotInitXZ45      //Start positions of the Left Rear leg
#define cLRInitPosY     CRobotInitY
#define cLRInitPosZ     CRobotInitXZ45

#define cLFInitPosX     CRobotInitXZ45      //Start positions of the Left Front leg
#define cLFInitPosY     CRobotInitY
#define cLFInitPosZ     -CRobotInitXZ45
#endif

//--------------------------------------------------------------------
//[Tars factors used in formula to calc Tarsus angle relative to the ground]
#define cTarsConst	720	//4DOF ONLY
#define cTarsMulti	2	//4DOF ONLY
#define cTarsFactorA	70	//4DOF ONLY
#define cTarsFactorB	60	//4DOF ONLY
#define cTarsFactorC	50	//4DOF ONLY

#endif CFG_HEX_H

