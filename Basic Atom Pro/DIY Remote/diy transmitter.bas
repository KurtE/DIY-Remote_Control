;***************** XP eXtra Potmeter version, no sound *****************************
'DEBUG con 1
'DEBUG_OUT con 1
'DEBUG_SAVED_LIST con 1
'
;==============================================================================
;Description: Lynxmotion custom XBee based radio
;Software version: V2.0
;Programmer: Jim Frye (aka RobotDude), Kurt (aka Kurte), Jeroen Janssen (aka Xan) 
;
;KNOWN BUGS: 
;   - Probably Alot ;) 
; ###### WARNING ##### Work in progress use at your (and your robots) own risk!
; This is built using the standard Xbee, TASerial and Timer support functions.
;==============================================================================
; trying to build for both HSERIAL and non hserial
;USETASERIAL con	1		; regardless we use TASerial for LCD...
USE_XPOTS con 1
USE_OLD_TASEROUT con 1
XBEEONHSERIAL con 1			; removed non-hserial version from here, but support functions still have it!!!
DataPacketDeltaMS con 33	; how many MS to wait between sending messages.  This is about 30 per second
; 
;Hardware setup: ABB2 with ATOM 28 Pro, XBee, 2 joysticks, 2 sliders, HEX keypad, display 
; 
;NEW IN V1.0 
;   - As released 
; 
; New in V2.0
;	- Converted to use XBEE serial communications.

; 
;KNOWN BUGS: 
;   - Probably Alot ;) 
; 
; [Defines]
TRUE	con		1
FALSE	con		0


;==============================================================================
; [IO Definitions] Lets define what each IO line is used for.
;==============================================================================
;	p0, left vertical joystick 
;	p1, left horizontal joystick
;	p2, right vertical joystick
;	p3, right horizontal joystick
;	in4 					- Col1 on keypad
;	in5 					- Col2 "
;	in6 					- Col3 "
;	in7 					- Col4 "
#ifdef USE_XPOTS
Display 	con 8 				; LCD Display
'Speaker 	con 9 				; Speaker; Disable speaker when used extra potmeter
Row0   	 	con 10 				; Row 0 on keypad
Row1   	 	con 11 				; row 1 on keypad
Row2    	con 12 				; Row 2
Row3    	con 13    			; Row 3
; P14 RXD						; HSERIAL
; P15 TXD						; hserial
RowB		con	9				; Cmd buttons new row , XP: ex-speaker
;	p18						- Left slider 
;	p19 					- Right slider 
;	p16	(AX0)				- XP (eXtra potmeter), Left potmeter
;	p17	(AX1)				- XP (eXtra potmeter), Right potmeter
#else
Display 	con 8 				; LCD Display
Speaker 	con 9 				; Speaker
Row0   	 	con 10 				; Row 0 on keypad
Row1   	 	con 11 				; row 1 on keypad
Row2    	con 12 				; Row 2
Row3    	con 13    			; Row 3
; P14 RXD						; HSERIAL
; P15 TXD						; hserial
RowB		con	17				; Cmd buttons new row
;	p18						- Left slider 
;	p19 					- Right slider 

#endif
;---------------------------------------------------------------------------------
; Warning: I am using P16 for RTS line for my Transmitter.  This works fine for my 
; 		setup as my APPBEE-Sip does voltage shifting on RES.  If you are using a
;		Sparkfun addapter you will need to shift IO pins around to get P7 or P8 
;		available which have lower output voltages.
;==============================================================================
; [EEPROM] Define EEPROM memory locations
;==============================================================================
JoystickRangesDMStart	con	0x0		; 12 bytes - Stores Mins/Maxs of joysticks/sliders
XBeeDMStart				con	0x80	; 4 Bytes - Stores the XBee My and DL we want

XBeeNDDMCache			con	0x88	; 2 bytes - (count and checksum) How many items do we have cached out
									; followed by My array, SNL, SNH, and text strings... reserve 220 bytes for this!
XBNDDM_AMY				con	0x8A	; Start my at page.  
XBNDDM_ASL				con	0xA0	; Serial number low									
XBNDDM_ASH				con	0xC0	; Serial number high
XBNDDM_ANDI				con	0xE0	; Start node names here...

;==============================================================================
; [Display Modes]
; We are currently using the "D" key on the keypad to cycle through display 
; modes
;==============================================================================
TransMode				var	byte				; Our current mode
fTModeChanged			var	bit					; has our mode changed?
TMODE_NORMAL			con	0					; Normal Mode
TMODE_DATA				con	1					; Display Data from Robot
TMODE_SELECTDLNAME		con	2					; Select which robot Host to control by name
#ifdef USE_OLD_SELECT_DL
TMODE_SELECTDL			con	3					; Select which robot Host to control
TMODE_SELECTMY			con 4					; Change My address
#else
TMODE_SELECTMY			con 3					; Change My address
#endif
TMODE_CALIBRATE			con	(TMODE_SELECTMY+1)	; try to calibrate the joysticks...

#ifdef USE_XPOTS
TMODE_SETXPZONE			con (TMODE_CALIBRATE+1)
TMODE_MAX				con	TMODE_SETXPZONE					; What is our max mode
#else
TMODE_MAX				con	TMODE_CALIBRATE					; What is our max mode
#endif
TransMode = TMODE_NORMAL						; Initialize to normal display mode
fTmodeChanged = FALSE							; the mode has not changed.
bTDataLast				var	byte				; which is the last data we displayed

; Strings we use to display on the LCD
_KEYSTRING				bytetable "Key:   "						
_LJOYSTRING				bytetable "Left:  "
_RJOYSTRING				bytetable "Right: "
_SLIDESTRING			bytetable "Slide: "
#ifdef USE_XPOTS
_POTSTRING				bytetable "Pot:   " ;XP
#endif
_BLANKS					bytetable "                "	; setup for one whole line worth
_LCDLINE2				bytetable 254, 71, 1, 2	; Position to first character in line 2


bPacketPrev				var	byte(XBEEPACKETSIZE)		; This is the previous data
bPacketLastSent			var	byte(XBEEPACKETSIZE)		; 
fPacketChanged 			var word		; Did the packet change.  XP byte -> word	
bTemp					var	byte(32)	; Will assume max packet of data for now
cIdleCycles				var	word		; How many cycles through the code have we gone with out a request?
CIDLECYCLESMAX 			con 100			; If I go this many times without sending stuff, resync...
fSendNewPacketMsg		var	bit			; Do we need to send a New packet msg
fNewPacketMsgMode		var	bit			; Are we in New Packet Message Mode?
fNewPacketMsgSent		var	bit			; Have we already sent a new Packet Msg?
lTimerLastDataPacket	var long = 0		; Time we sent last data packet
lTimerCur				var long		; Current time

; Other defines for Xbee
MYADDR					con	$0
DESTADDR				con	$1			; Default DL - can now set by keypad

DIY_TRANSMITTER			con 1		; tell the support file we don't need some stuff

; XBee data we store in EEPROM
cbXbee					var	byte		; how many bytes are stored in EEPROM
bXBeeCS					var	byte		; checksum
XBeeEE					var	word(2)		; This is the actual data we read from EEPROM
XBeeMY					var	XBeeEE(0)	; First word is our MY
XBeeDL					var	XBeeEE(1)	; Second word is the DL we want to send 2

;==============================================================================
; Now define our in memory NDList data.  This data is stored in EEPROM and is
; built by doing an ATND command to the XBEE
;==============================================================================
CMAXNDLIST	con	8						; currently the max number of nodes we will cache
										; Currently set to 8 as to make it easy to align for 32 byte increments...
CBNIMAX		con	14						; currently we will only keep 14 bytes per name
cNDList		var	byte					; how many items are in our list?
csNDList	var	byte					; checksum of data.
awNDMY		var	word(CMAXNDLIST+1)		; Keep a list of all of the Mys
alNDSNL		var	long(CMAXNDLIST+1)		; Serial numbers Low
alNDSNH		var	long(CMAXNDLIST+1)		; Serial Numbers High
abNDNIS		var	byte(CMAXNDLIST*CBNIMAX) ; keep all of the strings...



;==============================================================================
; [Command Buttons]
;==============================================================================

CmdBtns					var	byte							; Cmd Buttons state
; Define which bits are used for which button - May change later as I have the buttons in from R-L where I may want L-R
CMD_UP_MASK				con 0x08							; Logical Up Key
CMD_DOWN_MASK			con	0x04							; Logical Down key
CMD_ENTER_MASK			con	0x02							; Logical Enter key
CMD_ESC_MASK			con	0x01							; logical Esc/Cancel key

CmdBtnsPrev				var	byte							; previous time through


;==============================================================================
; [Sliders and Joystick defines ane variables]
;==============================================================================
; Lets define constants for the different ranges of the sliders.
; 
;		Left			Right			Sliders
;		638				630				1023   1023
;  361      598      392   636			  |      |
;       426				416			      0      0

Calibrated 				var bit 		; Are we calibrated
keypress 				var byte		; Character to display on LCD for keypress

col1 					var bit 		; Column values on keypad used in check keypad
col2 					var bit 
col3 					var bit 
col4 					var bit 

; create variables for averaging code 
index 					var byte 
buffer0 				var sword(8) 	; Last 8 values from joystick to generate averages from
buffer1 				var sword(8) 
buffer2 				var sword(8) 
buffer3 				var sword(8) 
buffer4 				var sword(8) 
buffer5 				var sword(8) 
#ifdef USE_XPOTS
buffer6 				var sword(8) ;XP
buffer7 				var sword(8) ;XP
#endif
;Turn Zero dead zone on/off for the potmeters

RPZon					var bit
LPZon					var bit
; Zero zone values for potmeter (maybe setting these values in the calibration part?)
RPLowZ					con 106
RPHighZ					con 130
LPLowZ					con 112
LPHighZ					con 135 

; Our AtoD Sums are stored in the order: RH RV LH LV RS LS
AtoDOrder				bytetable	3, 2, 1, 0, 19, 18
; The next 12 words will be stored in EEPROM, so we can properly generate the values.
;AtoDMins				var word(6)
;AtoDRanges				var	word(6)

AtoDMinT8				var word(6)
AtoDMaxT8				var	word(6)

;AToDMins	swordtable 392, 416, 361, 426, 0, 0
;AToDRanges	swordtable 636-392+1, 630-416+1, 598-361+1, 638-426+1, 1023-0+1, 1023-0+1
sums					var	sword(CNT_SUMS)	;XP (6)->(8) The sum of the last 8 reads of each joystick/slider for averaging
;SumOffsets  			var sword(6) 	; Generated offsets to center joysticks
AtoDMidT8				var word(6)		; measured center point

VSAtoD					var	word


;==============================================================================
; [General defines and variables]
;==============================================================================
i						var	sbyte
fFirstDataPacket		var bit
wMBit					var word
cChanged				var byte
wMask					var word

; XP, tables of Xs and Ys for the different LCD updates
#ifdef USE_XPOTS
abLCDUpdX 				bytetable 13, 13, 1, 1, 9, 5, 9, 5		; RJoyH,RJoyV,LJoyH,LJoyV,Rslider,Lslider,Rpot,Lpot
abLCDUpdY				bytetable  2,  1, 2, 1, 2, 2, 1, 1		; 
#else
abLCDUpdX 				bytetable 13, 13, 1, 1, 6, 6		; did not include the ones for the one byte updates, would be 11, 11
abLCDUpdY				bytetable  2,  1, 2, 1, 2, 1		; " 1, 1
#endif

abDispValCols			bytetable 0, 6, 13
g_bTxStatusLast 		var byte = 99;

; Text tables

;==============================================================================
; [Init] - First make sure all of the IO pins are in the state that they should
; be, and make sure interrupts are enabled
;==============================================================================
;
input p4 
input p5 
input p6 
input p7 
input p17		; our cmd button.
output p10 
output p11 
output p12 
output p13 


;==============================================================================
; initialize each buffer element, each sum, and then index to 0 
gosub GetSavedJoystickRanges			; Ok lets get our saved away joystick offsets.

; Simple Inits, don't need anything special just zero out as the calibrate function
; will get actual values to summarize
buffer0 = rep 0\8
buffer1 = rep 0\8
buffer2= rep 0\8
buffer3 = rep 0\8
buffer4 = rep 0\8
buffer5 = rep 0\8
#ifdef USE_XPOTS
buffer6 = rep 0\8 ;XP
buffer7 = rep 0\8 ;XP
#endif
;SumOffsets = rep 0\6
sums = rep 0\CNT_SUMS
index = 0 
CmdBtns = 0			; Assumume no command button has been pressed

Calibrated = 0 ; Mark for calibration
bTDataLast = 0xff	; none of the above...

; chirpy squeak kinda thing 
#ifndef USE_XPOTS
sound Speaker, [100\880, 100\988, 100\1046, 100\1175] ;musical notes, A,B,C,D. XP: NO SPEAKER!
#endif

; wake up the Matrix Orbital display module 
#ifdef USE_OLD_TASEROUT
; Part of the TASerout processing
ONASMINTERRUPT TIMERVINT, TASOIHANDLER
#endif

serout Display ,i19200, [254, 66, 0] ;Backlight on, no timeout. 
serout Display ,i19200, [254, 64, "The D-I-Y XBee  Robot Radio Set!"] ;startup screen. Do only once... 


;==============================================================================
; Initualize the  XBee.
; We will do the init in multiple stages.  We would like to not have very much
; communication going on until both sides are ready.  we will initialize everything
; and then send out a message saying we are ready and then we will wait for the other
; side to start requesting things.  If we don't get any messages for awhile, we will then
; go back to send out a ping saying we are ready...
;
enable		; make sure interrupts are enabled
gosub InitTimer				; Initialize our timer (Timer A)
gosub GetSavedXBeeInfo			; read in the XBee information
gosub InitXBee

gosub SetXBeeMy[XBeeMy]				; will set our My value from the saved location...

gosub SetXBeeDL[XBeeDL]				; will set the destination address after the initial init 


; try displaying the Robot Name we are currently paired with
; Make sure we have our list of items...
gosub ReadSavedXBeeNDList		; this reads in from the EEPROM.

if cNDList > 0 then
	for i = 0 to cndList-1
		if awNDMy(i) = XBeeDL then
			; found an item, so lets try outputing the pairing and the like...
			serout Display, i19200, [254, 71, 0, 1, "Paired With:    *", str abNDNIS(i*CBNIMAX)\CBNIMAX, " "]
			pause 1000
		endif
	next
endif




;==============================================================================
; --- top of main loop --- 
;==============================================================================
start: 
	; This is where we read everything and setup our packet of data
	; The data is very similar to a PS2 packet of data, with one byte of data per field
	; the data is arranged in teh following byte order:
	;	0 - Buttons High
	;	1 - Buttons Low
	; 	2 - Right Joystick L/R
	;	3 - Right Joystick U/D
	;	4 - Left Joystick L/R
	;	5 - Left Joystick U/D
	; 	6 - Right Slider
	;	7 - Left Slider
	
	gosub CheckKeypad[TRUE]						; This will fill in the two button values in the packet and return an ascii
												; value to display
	; The checkkeypad function may have changed the TransMode
	
	if TransMode = TMODE_CALIBRATE then	
		gosub CalibrateJoySticks
		goto start								; Will wait for new mode...
#ifdef USE_OLD_SELECT_DL
	elseif TransMode = TMODE_SELECTDL
		gosub UserSelectDL
		goto start								; Processing TDL selected new display mode so go back to start...
#endif
	elseif TransMode = TMODE_SELECTDLNAME
		gosub UserSelectDLName
		goto start								; Processing TDL selected new display mode so go back to start...
	elseif TransMode = TMODE_SELECTMY
		gosub UserSelectMy
		goto start								; Processing TDL selected new display mode so go back to start...
#ifdef USE_XPOTS
	elseif TransMode = TMODE_SETXPZONE
		gosub UserSelectXPDeadZone
		goto start
#endif		
	else
		; If we did a mode change back to a standard input mode tell the other side we are ready again for inputs...
	
		; One of our standard outputs	
		cIdleCycles = cIdleCycles + 1			; How many times have I gone through this loop without sending anything?
	
		; averaging expects that the a/d values are < 4096 
		; for each channel 
		;   read the a/d 
		;   subtract the previous value from 8 samples ago from the sum 
		;   store the new value in the circular buffer 
		;   add the new value to the sum 
		;   divide the sum by 8 to get the average value 
		;	Convert to value of 0-255 to fit a byte and emulate PS2 data.
		
		; Right Horizontal	
		sums(0) = sums(0) - buffer0(index) 		; subtract off old value
		adin 3, buffer0(index)				; 
		sums(0) = sums(0) + buffer0(index) 		; Add on new value
		if sums(0) >= AtoDMidT8(0) then
			;(v-510)*128/126 so 636->128 
			bPacket(2) = (128 + ((sums(0)-AtoDMidT8(0))*128)/(AtoDMaxT8(0)-AtoDMidT8(0))) max 255
		elseif sums(0) <= AtodMinT8(0)
			bPacket(2) = 0
		else  ; (v-510)*128/118 so 392-> -128 Note converted 0-127
			bPacket(2) = ((sums(0)-AtoDMinT8(0))*128)/(AtoDMidT8(0) - AtoDMinT8(0))
		endif
		;bPacket(2) = ((((((sums(0) + SumOffsets(0)) min (AToDMins(0)*8)) / 8) - AToDMins(0)) * 256) / AToDRanges(0)) max 255
	
		; Right Vertical
		sums(1) = sums(1) - buffer1(index) 		; subtract off old value
		adin 2, buffer1(index)				;  
		sums(1) = sums(1) + buffer1(index) 		; Add on new value
		if sums(1) >= AtoDMidT8(1) then
			;(v-510)*128/126 so 636->128 
			bPacket(3) = (128 + ((sums(1)-AtoDMidT8(1))*128)/(AtoDMaxT8(1)-AtoDMidT8(1))) max 255
		elseif sums(1) <= AtodMinT8(1)
			bPacket(3) = 0
		else  ; (v-510)*128/118 so 392-> -128 Note converted 0-127
			bPacket(3) = ((sums(1)-AtoDMinT8(1))*128)/(AtoDMidT8(1) - AtoDMinT8(1))
		endif

;		bPacket(3) = ((((((sums(1) + SumOffsets(1)) min (AToDMins(1)*8)) / 8) - AToDMins(1)) * 256) / AToDRanges(1)) max 255	; 
	
		; Left Horizontal	
		sums(2) = sums(2) - buffer2(index) 		; subtract off old value
		adin 1, buffer2(index)				;  
		sums(2) = sums(2) + buffer2(index) 		; Add on new value
		if sums(2) >= AtoDMidT8(2) then
			;(v-510)*128/126 so 636->128 
			bPacket(4) = (128 + ((sums(2)-AtoDMidT8(2))*128)/(AtoDMaxT8(2)-AtoDMidT8(2))) max 255
		elseif sums(2) <= AtodMinT8(2)
			bPacket(4) = 0
		else  ; (v-510)*128/118 so 392-> -128 Note converted 0-127
			bPacket(4) = ((sums(2)-AtoDMinT8(2))*128)/(AtoDMidT8(2) - AtoDMinT8(2))
		endif
;		bPacket(4) = ((((((sums(2) + SumOffsets(2)) min (AToDMins(2)*8)) / 8) - AToDMins(2)) * 256) / AToDRanges(2)) max 255	; 
	
		; Left Vertical 
		sums(3) = sums(3) - buffer3(index) 		; subtract off old value
		adin 0, buffer3(index)				;  
		sums(3) = sums(3) + buffer3(index) 		; Add on new value
		;XP use zero stuff:
#ifdef USE_XPOTS		
		if sums(3) >= AtoDMidT8(3) then
			;(v-510)*128/126 so 636->128 
			bPacket(5) = (128 + ((sums(3)-AtoDMidT8(3))*128)/(AtoDMaxT8(3)-AtoDMidT8(3))) max 255
		elseif sums(3) <= AtodMinT8(3)
			bPacket(5) = 0
		else  ; (v-510)*128/118 so 392-> -128 Note converted 0-127
			bPacket(5) = ((sums(3)-AtoDMinT8(3))*128)/(AtoDMidT8(3) - AtoDMinT8(3))
		endif
#endif		
; for now try without zero stuff...
;		bPacket(5) = ((((sums(3) min AToDMinT8(3)) - AToDMinT8(3)) * 256) / (AtoDMaxT8(3)-AToDMinT8(3))) max 255	;
		
;		bPacket(5) = ((((((sums(3) + SumOffsets(3)) min (AToDMins(3)*8)) / 8) - AToDMins(3)) * 256) / AToDRanges(3)) max 255	;
		
		; Right Slider	
		sums(4) = sums(4) - buffer4(index) 
		adin 19, buffer4(index) 
		sums(4) = sums(4) + buffer4(index)  
		bPacket(6) = (sums(4) / 8) / 4		; should hopefully get us in the range 0-255
		
		; Left Slider
		sums(5) = sums(5) - buffer5(index) 
		adin 18, buffer5(index) 
		sums(5) = sums(5) + buffer5(index)  
		bPacket(7) = (sums(5) / 8) / 4		; should hopefully get us in the range 0-255
#ifdef USE_XPOTS		
		; XP, Right potmeter
		sums(6) = sums(6) - buffer6(index) 
		adin 17, buffer6(index) 
		sums(6) = sums(6) + buffer6(index)
		if RPZon then  
			; Set Zero (dead)zone for pot:
			if (((sums(6) / 8) / 2)-128) <= RPLowZ then
				bPacket(8) = (((sums(6) / 8) / 2)-RPLowZ) MIN 0 ;Not using the hole range for potmeter, easier to use. Therefore /2
			elseif (((sums(6) / 8) / 2)-128) >= RPHighZ
				bPacket(8) = (((sums(6) / 8) / 2)-RPHighZ)MAX 255
			else
				bPacket(8) = 128	;Potmeter are in the zero range
			endif
		else
			bPacket(8) = ((((sums(6) / 8) / 2)-128) MIN 0)MAX 255
		endif
		;bPacket(8) = (sums(6) / 8) / 4		; should hopefully get us in the range 0-255
		
		; Left potmeter
		sums(7) = sums(7) - buffer7(index) 
		adin 16, buffer7(index) 			
		sums(7) = sums(7) + buffer7(index)
		if LPZon then  
			; Set Zero (dead)zone for pot:
			if (((sums(7) / 8) / 2)-128) <= LPLowZ then
				bPacket(9) = (((sums(7) / 8) / 2)-LPLowZ) MIN 0 ;Not using the hole range for potmeter, easier to use. Therefore /2
			elseif (((sums(7) / 8) / 2)-128) >= LPHighZ
				bPacket(9) = (((sums(7) / 8) / 2)-LPHighZ)MAX 255
			else
				bPacket(9) = 128	;Potmeter are in the zero range
			endif
		else
			bPacket(9) = ((((sums(7) / 8) / 2)-128) MIN 0)MAX 255
		endif
		;bPacket(9) = (sums(7) / 8) / 4		; should hopefully get us in the range 0-255
		; finally increment the index and limit its range to 0 to 7. 
#endif
		index = (index + 1) & 7 
	
		fPacketChanged = 0
		fSendNewPacketMsg = FALSE					
		; Note: we now send messages if anything changed.  The > 1 caused jerkyness...
		for i = 0 to XBEEPACKETSIZE-1
			if bPacket(i) <> bPacketPrev(i) then
;				if i < 2 then
;					fSendNewPacketMsg = TRUE
;				elseif (ABS(bPacket(i)-bPacketPrev(i)) > 1)	; BUGBUG:: will be able to set the threholds from robot...
				fSendNewPacketMsg = TRUE
;				endif
 				
				bPacketPrev(i)  = bPacket(i)		; save the new value
				fPacketChanged = fPacketChanged | (1 <<i)		; Say that we changed
			endif
		next
		; 
		if Calibrated=1 then 
			; update the display module 
			gosub CheckAndTransmitDataPacket ; check before and after we output to the LCD

			; Now depending on which mode we are displaying we will either simply display the state of the Joysticks...
			if TransMode = TMODE_NORMAL then
				;XP:
				if index = 7 then
#ifdef USE_XPOTS
					gosub DoLCDDisplay[9, 1, keypress, 0]	; Changed from 11 -> 9 
#else				
					gosub DoLCDDisplay[11, 1, keypress, 0]	; Changed from 11 -> 9 
#endif					
				endif
				if index < CNT_SUMS then
					gosub DoLCDDisplay[abLCDUpdX(index), abLCDUpdY(index), 0, bPacket(index+2)]
;					gosub DoLCDDisplay[abLCDUpdX(index+4), abLCDUpdY(index+4), 0, bPacket(index+6)]
				endif
			else
				; We are in the Combined display with Line one showing The last thing that changed...
				if (fPacketChanged & %0000000000000011) then ; last 2 bits are for keys
					if bTDataLast <> 0 then
						bTDataLast = 0
						gosub ShowLCDString[1, 1, @_KEYSTRING, 7]
						gosub CheckAndTransmitDataPacket ; check after each string we send...
						gosub TASerout[Display, i19200, @_blanks, 9]
						gosub CheckAndTransmitDataPacket ; check after each string we send...
					endif		

					; now display the actual Character for the keypress
					gosub DoLCDDisplay[6, 1, keypress, 0]	

				elseif (fPacketChanged & %0000000000001100)	; R Joystick X, Y
					if bTDataLast <> 1 then
						bTDataLast = 1
						gosub ShowLCDString[1, 1, @_RJOYSTRING, 7]
						gosub CheckAndTransmitDataPacket ; check after each string we send...
						gosub TASerout[Display, i19200, @_blanks, 9]
						gosub CheckAndTransmitDataPacket ; check after each string we send...
					endif
					
					; now display the joystick values
					gosub DoLCDDisplay[9, 1, 0, bPacket(2)]
					gosub DoLCDDisplay[13, 1, 0, bPacket(3)]
							
				elseif (fPacketChanged & %0000000000110000)	; L Joystick X, Y
					if bTDataLast <> 2 then
						bTDataLast = 2
						gosub ShowLCDString[1, 1, @_LJOYSTRING, 7]
						gosub CheckAndTransmitDataPacket ; check after each string we send...
						gosub TASerout[Display, i19200, @_blanks, 9]
						gosub CheckAndTransmitDataPacket ; check after each string we send...
					endif		
					
					; now display the joystick values
					gosub DoLCDDisplay[9, 1, 0, bPacket(4)]
					gosub DoLCDDisplay[13, 1, 0, bPacket(5)]

				elseif (fPacketChanged & %0000000011000000)	; Sliders R, L
					if bTDataLast <> 3 then
						bTDataLast = 3
						gosub ShowLCDString[1, 1, @_SLIDESTRING, 7]
						gosub CheckAndTransmitDataPacket ; check after each string we send...
						gosub TASerout[Display, i19200, @_blanks, 9]
						gosub CheckAndTransmitDataPacket ; check after each string we send...
					endif		
					
					; now display the joystick values
					gosub DoLCDDisplay[13, 1, 0, bPacket(6)]
					gosub DoLCDDisplay[9, 1, 0, bPacket(7)]
				;XP:
#ifdef USE_XPOTS
				elseif (fPacketChanged & %0000001100000000)	; Potmeter R, L
					if bTDataLast <> 4 then
						bTDataLast = 4
						gosub ShowLCDString[1, 1, @_POTSTRING, 7]
						gosub CheckAndTransmitDataPacket ; check after each string we send...
						gosub TASerout[Display, i19200, @_blanks, 9]
						gosub CheckAndTransmitDataPacket ; check after each string we send...
					endif	
					; now display the potmeter values
					gosub DoLCDDisplay[13, 1, 0, bPacket(8)]
					gosub DoLCDDisplay[9, 1, 0, bPacket(9)]
#endif
				endif
			endif
			gosub CheckAndTransmitDataPacket
		endif 
		
		if Calibrated=0 & index=0 then 
		  gosub Calibrate 
		endif 
	endif
		
		; do it again ad infinitum 
goto start 



;==============================================================================
; [CheckKeypad] function
;
; Input Parameters:
; 		FCheckModeChange: Should we do checks for mode change? Currently always true.
; 
; Variables updated: 
;		keypress - the ascii value associated with that key.
;		Ver 2.0 - Add 4 buttons hang off of keyboard as an extra row...
; 		change Highs to inputs to keep possibility of dead shorts from happening
;		when pressing multiple switches.
; BUGBUG:: Need to clean this up, maybe table drive?
;==============================================================================
fCheckModeChange var	byte		; should we check for a mode change ?
CheckKeypad[fCheckModeChange]:
	low  Row0 
	input Row1 
	input Row2 
	input Row3 
	input RowB	; buttons we hacked on
	
	col1 = in4 
	col2 = in5 
	col3 = in6 
	col4 = in7 

	bPacket(0) = 0		; assume no buttons pressed
	bPacket(1) = 0
    keypress = " "
    CmdBtnsPrev = CmdBtns
    CmdBtns = 0 
	if fCheckModeChange then
		fTModeChanged = FALSE
	endif

	;Read buttons - New version reads multiple keys as well are down.
	; Process Row 0
	if col1 = 0 then 
		bPacket(0).bit1 = 1
		keypress = "1"
	endif 
	if col2 = 0  then
		bPacket(0).bit2 = 1 
		keypress = "2" 
	endif 
	if col3 = 0 then
	    bPacket(0).bit3 = 1
	    keypress = "3" 
	endif 
	if col4 = 0 then
	    bPacket(1).bit2 = 1 
	    keypress = "A" 
	endif 

	; Process Row 1
	input Row0 
	low  Row1 
	col1 = in4 
	col2 = in5 
	col3 = in6 
	col4 = in7 
	if col1 = 0 then 
	    bPacket(0).bit4 = 1 
	    keypress = "4" 
	endif
	if col2 = 0  then
	    bPacket(0).bit5 = 1 
	    keypress = "5" 
	endif 
	if col3 = 0  then
		bPacket(0).bit6 = 1 
		keypress = "6" 
	endif 
	if col4 = 0 then
		bPacket(1).bit3 = 1 
		keypress = "B" 
	endif 

	; Process Row2
	input Row1 
	low  Row2 
	col1 = in4 
	col2 = in5 
	col3 = in6 
	col4 = in7 
	if col1 = 0 then 
	    bPacket(0).bit7 = 1 
	    keypress = "7" 
	endif 
	if col2 = 0 then
	    bPacket(1).bit0 = 1 
	    keypress = "8" 
	endif 
	if col3 = 0 then
	    bPacket(1).bit1 = 1 
	    keypress = "9" 
	endif 
	if col4 = 0 then
	    bPacket(1).bit4 = 1 
	    keypress = "C" 
	endif 

	; Process Row 3
	input Row2 
	low  Row3 
	col1 = in4 
	col2 = in5 
	col3 = in6 
	col4 = in7 
	if col1 = 0 then 
	    bPacket(0).bit0 = 1 
	    keypress = "0" 
	endif 
	if col2 = 0 then
	    bPacket(1).bit7 = 1 
	    keypress = "F" 
	endif 
	if col3 = 0 then
	    bPacket(1).bit6 = 1 
	    keypress = "E" 
	endif 
	if col4 = 0 then
	    bPacket(1).bit5 = 1 
	    keypress = "D" 
	endif 

	; Process Row B - Our 4 added on buttons.
	input Row3
	low	RowB
	col1 = in4 
	col2 = in5 
	col3 = in6 
	col4 = in7 
	if col1 = 0 then 
	    CmdBtns.bit0 = 1 
	    keypress = "W" 
	endif 
	if col2 = 0 then
	    CmdBtns.bit1 = 1 
	    keypress = "X" 
	endif 
	if col3 = 0 then
	    CmdBtns.bit2 = 1 
	    keypress = "Y" 
	endif 
	if col4 = 0 then
	    CmdBtns.bit3 = 1 
	    keypress = "Z" 
	endif 
	
	; Check to see if the state of the command buttons has changed.
	if fCheckModeChange and (CmdBtns <> CmdBtnsPrev) then			;
		if (CmdBtns & CMD_UP_MASK) and ((CmdBtnsPrev & CMD_UP_MASK) = 0) then
			; CmD Up button has been pressed
			if TransMode = TMODE_MAX then
				TransMode = 0
			else
				TransMode = TransMode + 1
			endif
			fTModeChanged = TRUE
		elseif (CmdBtns & CMD_DOWN_MASK) and ((CmdBtnsPrev & CMD_DOWN_MASK) = 0)
			; CmD Up button has been pressed
			if TransMode = 0 then
				TransMode = TMODE_MAX
			else
				TransMode = TransMode - 1
			endif
			fTModeChanged = TRUE
		endif
		
		if fTModeChanged then
			gosub ClearLCDDisplay
			bTDataLast = 0xff		; make sure we will display something when it changes.
		endif
	endif

	return
	


;==============================================================================
;[Calibrate] Calibrates the  middle positions of the sticks to 1500 
;Calibrates left vertical stick to be total down 
;==============================================================================
_CB_PROMPT_STRING	bytetable 254, 0, "Calibrating..."	; 16

Calibrate: 
	gosub ClearLCDDisplay
	gosub TASerout[Display, i19200, @_CB_PROMPT_STRING, 16]
	pause 1000     

	; For Right Joystick LR and Left joystick LR and UD we want the center values to be 128
	; using this example calculation:
	; bPacket(6) = ((((sums(3) min 3136) / 8) - 392) * 256) / 300	; Make sure bottom > 392 so no negative
	; we see we want the sum to be about 4340 which calculates back out between 128 and 129
	;bPacket(3) = (((((sums(0) + SumOffsets(0)) min (AToDMins(0)*8)) / 8) - AToDMins(0)) * 256) / (AtoDMaxs(0) - AToDMins(0) + 1)
	; Will add a little fudge which is 1/2 the way to the next unit
;	SumOffsets(0) = (((128 * AToDRanges(0))/256) + AtoDMins(0)) * 8 + AToDRanges(0)/64 - sums(0)
;	SumOffsets(1) = (((128 * AToDRanges(1))/256) + AtoDMins(1)) * 8 + AToDRanges(1)/64 - sums(1)
;	SumOffsets(2) = (((128 * AToDRanges(2))/256) + AtoDMins(2)) * 8 + AToDRanges(2)/64 - sums(2)
	for i = 0 to 3		; simply remember our measured center location
		AtoDMidT8(i) = sums(i)
	next
	
	; For Right Joystick U/D assume at bottom - but do some validations just in case...
;	if sums(3) < 3350 then
;		SumOffsets(3) =  AtoDMins(3) * 8 + AToDRanges(3)/64 - sums(3)
;	else
;		SumOffsets(3) = 0
;	endif

	gosub ClearLCDDisplay
    
   ; Mark as calibrated 
     Calibrated=1 
return

	
	
;==============================================================================
;[GetSavedJoystickRanges] - This reads in the Saved XBee information. - Currently
; 		we are only readining in the My and Dest(DL) but may also read in other data
;		Start block with Count of bytes as well as a checksum to make sure
;		we are not just reading in Junk...
;		We will start at EEPROM location: 0x0	
;==============================================================================
GetSavedJoystickRanges:

	ReadDm	JoystickRangesDMStart, [cbXBee, bXBeeCS]
#ifdef DEBUG
	serout s_out, i9600, ["Get Saved Joystick ", dec cbXBee, " ", dec bXBeeCS, 13]
#endif		

	if cbXBee = 24 then
		ReadDm JoystickRangesDMStart+2, [str AtoDMinT8\12, str AtoDMaxT8\12]
		
		for i = 0 to 5
			bXBeeCS = bXBeeCS - (AtoDMinT8(i).lowbyte + AtoDMinT8(i).highbyte + AtoDMaxT8(i).lowbyte + AtoDMaxT8(i).highbyte)
		next
		
		; If I did the right if valid the cs should be zero
		if bXBeeCS = 1 then		; changed checksum to bias by 1 to not match old data...
			return;
		endif
	endif
	
	; if we fail for some reason just default to our defaults...
	AtoDMinT8 = 392*8, 416*8, 361*8, 426*8, 0, 0
	AtoDMaxT8 = 636*8, 630*8, 598*8, 638*8, 1023*8, 1023*8
#ifndef USE_XPOTS	
   sound p9, [50\4000,50\3500]; XP: NO SPEAKER
#endif   
#ifdef DEBUG
	serout s_out, i9600, ["GSJ ", dec bXBeeCS, 13]
#endif		

	return
	
;==============================================================================
;[SaveJoystickRanges] - This Saves XBee information. - Currently
; 		we are only saving My and Dest(DL) 
;		We will start at EEPROM location: 0x0	
;==============================================================================
SaveJoystickRanges:
	bXBeeCS = 1		; BUGBUG:: changed the checksum so wont match the old stuff...
	for i = 0 to 5
		bXBeeCS = bXBeeCS + AtoDMinT8(i).lowbyte + AtoDMinT8(i).highbyte + AtoDMaxT8(i).lowbyte + AtoDMaxT8(i).highbyte
	next
	
	WriteDM JoystickRangesDMStart, [24, bXBeeCS, str AtoDMinT8\12, str AtoDMaxT8\12]
	
	return
	
;==============================================================================
;[UserSelectXPDeadZone] - This function allow the user to turn on/off the zero
;dead zone for the extra potmeters
;==============================================================================	
;local variables
#ifdef USE_XPOTS
XPcnt		var byte
_XPzone_Prompt0	bytetable	254, 71, 0, 1, "XP Dead Zone " ;17 char

_XPzone_Prompt_AllON	bytetable	254, 71, 0, 2, "Both ON      "	;17 char
_XPzone_Prompt_AllOFF	bytetable	254, 71, 0, 2, "Both OFF     "	;17
_XPzone_Prompt_LPON		bytetable	254, 71, 0, 2, "LP ON, RP OFF"	;17
_XPzone_Prompt_RPON		bytetable	254, 71, 0, 2, "LP OFF, RP ON"	;17

UserSelectXPDeadZone:
	gosub XPZoneModeBranch		; update status
	gosub TASerout[Display, i19200, @_XPzone_Prompt0, 17]

XPzoneLoop:

	bPacketPrev(0) = bPacket(0)	; need to save old state...
	bPacketPrev(1) = bPacket(1)
	gosub CheckKeypad[TRUE]		; Will try go get something from keypad - tell it to do Transmode changes...
							
	; check to see if we changed modes...
	if fTModeChanged then
		
		; We have a new mode so return...
		bPacketPrev(0) = bPacket(0)	; Make sure the main loop has our updated state of keys!
		bPacketPrev(1) = bPacket(1)
		return
	endif
	; See if the Enter key is pressed
	if (CmdBtns & CMD_ENTER_MASK) and ((CmdBtnsPrev & CMD_ENTER_MASK) = 0) then
		;Toogle mode
		XPcnt = XPcnt + 1
		if XPcnt > 3 then
			XPcnt = 0
		endif
		gosub XPZoneModeBranch
	endif
	
	goto XPzoneLoop
	return ; Is the return here really needed? Just in case..
	
XPZoneModeBranch:
	branch XPcnt,[AllON,LPon,RPon,AllOFF]
	return ; to be safe
	ALLON:
		RPZon = TRUE
		LPZon = TRUE
		gosub TASerout[Display, i19200, @_XPzone_Prompt_AllON, 17]
	return
	LPon:
		RPZon = FALSE
		LPZon = TRUE
		gosub TASerout[Display, i19200, @_XPzone_Prompt_LPON, 17]
	return
	RPon:
		RPZon = TRUE
		LPZon = FALSE
		gosub TASerout[Display, i19200, @_XPzone_Prompt_RPON, 17]
	return
	ALLOFF:
		RPZon = FALSE
		LPZon = FALSE
		gosub TASerout[Display, i19200, @_XPzone_Prompt_AllOFF, 17]


return
#endif

;==============================================================================
;[CalibrateJoySticks] - This function asks the user to move all of the joystick
;	and sliders the entire range and then hit a key, which will save away the
; 	mins and maxs for each of the joysticks as well as center point  
;==============================================================================
wAtoDIn		var	word
wAtoDMins	var	word(6)
wAtoDMaxs	var	word(6)
fError		var	bit

_CPrompt1	bytetable 	254, 0, "Cal Joysticks"				; 15 chars
_cPrompt2	bytetable	254, 71, 0, 2, "Move Full Ranges"	; 20 chars
_cUPDMsg	bytetable	254, 71, 0, 2, "*** Updated ***"	; 19
_cErrMsg	bytetable	254, 71, 0, 2, "***  Error  ***"	; 19

CalibrateJoySticks:

	gosub TASerout[Display, i19200, @_CPrompt1, 15]
	gosub TASerout[Display, i19200, @_CPrompt2, 20]

	;First initialize the mins/maxs to current values
	for i = 0 to 5
		adin AtoDOrder(i), wAtoDMins(i)
		wAtoDMaxs(i) = wAtoDMins(i)
	next
	
	
_CJOY_LOOP:

	; We wish to loop through all 6 joystick/sliders and find the minimum and maximum values for their range.
	; 
	for i = 0 to 5
		adin AtoDOrder(i), wAtoDIn
		if wAtoDIn > wAtoDMaxs(i) then
			wAtoDMaxs(i) = wAtoDIn
		endif
		
		if wAtoDIn < wAtoDMins(i) then
			wAtoDMins(i) = wAtoDIn
		endif
	next

	bPacketPrev(0) = bPacket(0)	; need to save old state...
	bPacketPrev(1) = bPacket(1)
	gosub CheckKeypad[TRUE]		; Will try go get something from keypad - tell it to do Transmode changes...
							
	; check to see if we changed modes...
	if fTModeChanged then
		
		; We have a new mode so return...
		bPacketPrev(0) = bPacket(0)	; Make sure the main loop has our updated state of keys!
		bPacketPrev(1) = bPacket(1)
		return
	endif

	; See if the Enter key is pressed
	if (CmdBtns & CMD_ENTER_MASK) and ((CmdBtnsPrev & CMD_ENTER_MASK) = 0) then
		; So a key was pressed... should check for other consistency but for now good enough.
		
		; But first make sure the ranges look OK...
		fError = FALSE
		; First 4 are for joysticks so assume limited range
		; BUGBUG: could put this part in tables as well
		for i = 0 to 3
			if (wAtoDMins(i) > 450) or (wAtoDMaxs(i) < 575) then
				fError = TRUE
			endif
		next
		;last 2 are for sliders and should have full range...
		for i = 4 to 5
			if (wAtoDMins(i) > 50) or (wAtoDMaxs(i) < 975) then
				fError = TRUE
			endif
		next
		
		if not fError then
			; No errors, so lets update our memory ranges and call the save function
			for i = 0 to 5
				AtoDMinT8(i) = wAtoDMins(i) * 8		; bugbug we multiply by 8 as are sums are all 8 samples added to each other
				AtoDMaxT8(i) = wAtoDMaxs(i) * 8
			next
			gosub SaveJoystickRanges
			Calibrated=0				; We will need to recalibrate our center point offsets
			index=0 

			gosub TASerout[Display, i19200, @_cUPDMsg, 19]
		else
			gosub TASerout[Display, i19200, @_cErrMsg, 19]
		endif
		
	endif
	
	goto _CJOY_LOOP

return




;==============================================================================
; [CheckAndTransmitDataPacket] function
; 
; This function will output a packet of data over the XBee to the receiving robot.  This function
; will start off simple and simply dump the raw data.  Then I will build in additional smarts.  Things like:
; only transmit data if it has changed.  Maybe allow the remote robot be able to send us information like
; how much slop should we allow in the deadband range for each channel.  Also Maybe we need to generate
; a pulse to allow the other side know that we are still there...
;==============================================================================

bDataOffset var	byte
bPacketType var byte

CheckAndTransmitDataPacket:

	; First see if it is time for us to send a data packet out.
	gosub GetCurrentTime[], lTimerCur
	if (lTimerCur - lTimerLastDataPacket) > ((DataPacketDeltaMS * WTIMERTICSPERMSDIV) / WTIMERTICSPERMSMUL) then 
		gosub SendXbeePacket[XBEE_TRANS_DATA, XBEEPACKETSIZE, @bPacket]		; Ok we dumped the data to the the output
		gosub GetCurrentTime[], lTimerLastDataPacket
	endif

	gosub APIRecvPacket[0], _bAPIRecvRet
	

	if _bAPIRecvRet then
		' We received an XBee Packet, See what type it is.
		' first see if it is a RX 16 bit or 64 bit packet?
		If _bAPIPacket(0) = 0x81 Then
			' 16 bit address sent, so there is 5 bytes of packet header before our data
			bDataOffset = 5
		ElseIf _bAPIPacket(0) = 0x7E
			' 64 bit address so our data starts at offset 11...
			bDataOffset = 11
			
		ElseIf _bAPIPacket(0) = 0x89
			' this is an A TX Status message - May check status and maybe update something?
			' We can detect that someone is receiving data from us here.
	        g_bTxStatusLast = _bAPIPacket(2);
			goto CheckAndTransmitDataPacket

		ElseIf _bAPIPacket(0) = 0x88
			' Api Status
	        g_bTxStatusLast = _bAPIPacket(2);
			goto CheckAndTransmitDataPacket
		
		Else
#ifdef DEBUG
			serout s_out, i9600,["Unknown XBEE Packet: ", hex _bAPIPacket(0), " L:", hex (_bAPIPacket(1) >> 8 + _bAPIPacket(2)), 13]
#endif
			goto CheckAndTransmitDataPacket
		EndIf
#ifdef DEBUG
'		serout s_out, i9600, ["Recv:", hex CmdPacket(0)\2, hex CmdPacket(1)\2,hex CmdPacket(2)\2,hex CmdPacket(3)\2, 13]
#endif	
		_bAPIRecvRet = _bAPIRecvRet - (bDataOffset + XBEE_API_PH_SIZE) ; This is the extra data size
		bPacketType = _bAPIPacket(bDataOffset + 0)
		; Now look what type packet it is.  First check to see if the user is asking for data...
		; we removed some of the testing as the function we called already validated the checksum and sizes...
		; Next check for the packet may be a Req New which changes when we transmite data...	
		; Packet to display a value/string on LCD of the remote...
		; BUGBUG:::::::::::: split into two commands. One for a value and other for string...
		if (bPacketType = XBEE_RECV_DISP_VAL ) then
			; We will handle word values here
			if (_bAPIRecvRet = 2) then
				Gosub DisplayRemoteValue[13, (_bAPIPacket(bDataOffset+XBEE_API_PH_SIZE) << 8) + _bAPIPacket(bDataOffset+XBEE_API_PH_SIZE+1)]
			endif
			goto CheckAndTransmitDataPacket

		elseif (bPacketType >= XBEE_RECV_DISP_VAL0 ) and (bPacketType <= XBEE_RECV_DISP_VAL2 )
			; We will handle word values here
			if (_bAPIRecvRet = 2) then
				Gosub DisplayRemoteValue[abDispValCols(bPacketType-XBEE_RECV_DISP_VAL0), |
						(_bAPIPacket(bDataOffset+XBEE_API_PH_SIZE) << 8) + _bAPIPacket(bDataOffset+XBEE_API_PH_SIZE+1)]
			endif
			goto CheckAndTransmitDataPacket

		elseif (bPacketType = XBEE_RECV_DISP_STR ) 
			; Try to handle both text and a simple byte value passed to us...
			if (_bAPIRecvRet > 0) and (_bAPIRecvRet <= 16) then
				Gosub DisplayRemoteString[@_bAPIPacket + bDataOffset+XBEE_API_PH_SIZE, _bAPIRecvRet]
			endif
			goto CheckAndTransmitDataPacket
		;
		; Packet to play a sound - BUGBUG - not checking checksum...
		elseif (bPacketType = XBEE_PLAY_SOUND )
			if (_bAPIRecvRet > 0) and (_bAPIRecvRet <= 16) then
				Gosub PlayRemoteSounds[@_bAPIPacket + bDataOffset+XBEE_API_PH_SIZE,_bAPIRecvRet];
			endif
			goto CheckAndTransmitDataPacket
		else
#ifdef DEBUG
			serout s_out, i9600, ["TP IN:", hex _bAPIPacket(bDataOffset + 0)\2, hex _bAPIPacket(bDataOffset + 1)\2, hex _bAPIPacket(bDataOffset + 2)\2, hex _bAPIPacket(bDataOffset + 3)\2, 13]
#endif	
			;We got something we were not expecting so error out.	
			gosub ClearInputBuffer	; try to clear everything else out.
		endif
	endif

	return 	; 
	
;==============================================================================
;[GetSavedXBeeInfo] - This reads in the Saved XBee information. - Currently
; 		we are only readining in the My and Dest(DL) but may also read in other data
;		Start block with Count of bytes as well as a checksum to make sure
;		we are not just reading in Junk...
;		We will start at EEPROM location: 0x80	
;==============================================================================
GetSavedXBeeInfo:

	ReadDm	XBeeDMStart, [cbXBee, bXBeeCS]

	if cbXBee = 4 then
		ReadDm XBeeDMStart+2, [str XbeeEE\cbXBee]
#ifdef DEBUG_SAVED_LIST
		serout s_out, i9600, ["My: ", hex xBeeMy, " DL: ", hex XBeeDL, 13]
#endif		
		
		for i = 0 to cbXBee/2
			bXBeeCS = bXBeeCS - (XBeeEE(i).lowbyte + XBeeEE(i).highbyte)
		next
		
		; If I did the right if valid the cs should be zero
		if bXBeeCS = 0 then
			return;
		endif
	endif
	
	; if we fail for some reason just default to our defaults...
	XBeeMy = MYADDR
	XBeeDL = DESTADDR

	return
	

;==============================================================================
;[SaveXBeeInfo] - This Saves XBee information. - Currently
; 		we are only saving My and Dest(DL) 
;		We will start at EEPROM location: 0x80	
;==============================================================================
SaveXBeeInfo:
	bXBeeCS = 0
	for i = 0 to 1
		bXBeeCS = bXBeeCS + (XBeeEE(i).lowbyte + XBeeEE(i).highbyte)
	next
	
	WriteDM XBeeDMStart, [4, bXBeeCS, str XBeeEE\4]
	
	return
	
;==============================================================================
; LookupDLInMYList[dwLU, fAdd] - This will try to lookup a DL in the list
;		of saved values.  It will return the index if it is found.  If not found
;		it will optionally try to query the other side for the information.
;==============================================================================
dwLU			var	word			; the value to look up
fAdd			var	byte			; Should we do a query if it is not found?
_i				var	byte
_ai				var	byte			; array index
_k				var	byte			; 
_fListChanged	var	bit							; have we changed the list
dwDLPrev		var	word			; previous DL

_UNK_ROBOT		bytetable			"_unknown_" ; 9 characters

LookupDLInMYList[dwLU, fAdd]:
	if cNDList > 0 then
		for _i = 0 to cndList-1
			if awNDMy(_i) = dwLU then
				return _i;	 ; let caller know we found the item
			endif
		next
	endif

	; If we get here then we did not find the item.  See if the user wants us to
	; add it to the list.  If not or if our list is full return -1
	if (fAdd = 0) or (cndList >= CMAXNDLIST) then
		return (-1)							; bail if we are not adding
	endif
	
	; OK they asked us to add one
	

	_i = cNDList						; OK we will be adding this to the list at the end
	awNDMY(cNDList) = dwLU;				; save away our lookup value
	alNDSNH(cNDList) = 0				; default to 0 for SNL
	alNDSNL(cNDList) = 0				; 
	abNDNIS(cNDList*CBNIMAX) = rep " "\CBNIMAX	; blank the string out.
	
	cNDList = cNDList + 1				; increment the size of the list...
	_fListChanged = 1					; yes the list changed - have new node
	
	dwDLPrev = XBeeDL					; Save away - need at end as well.
	if dwLU <> XBeeDL then
		XBeeDL = dwLU					; set the new value...
		gosub SetXBeeDL[XBeeDL]	; Actually set the destination in the XBee
	endif

	; Probably need to pass our MY here so they can talk to us?
	gosub SendXbeePacket[XBEE_REQ_SN_NI, 2, @XBeeMY]		; Now do our request

_LUMYP_AGAIN:
	gosub APIRecvPacket[500000], _bAPIRecvRet
	if _bAPIRecvRet then
		' We received an XBee Packet, See what type it is.
		' first see if it is a RX 16 bit or 64 bit packet?
		If _bAPIPacket(0) = 0x81 Then
			' 16 bit address sent, so there is 5 bytes of packet header before our data
			bDataOffset = 5
		ElseIf _bAPIPacket(0) = 0x7E
			' 64 bit address so our data starts at offset 11...
			bDataOffset = 11
			
		ElseIf _bAPIPacket(0) = 0x89
			' this is an A TX Status message - May check status and maybe update something?
			goto _LUMYP_AGAIN
		Else
			goto _LUMYP_AGAIN
		endif
		; Ok now lets make the data returned looks valid.
		_bAPIRecvRet = _bAPIRecvRet - (bDataOffset + XBEE_API_PH_SIZE) + 1 ; This is the extra data size

		if (_bAPIPacket(bDataOffset+0) = XBEE_SEND_SN_NI_DATA) and (_bAPIRecvRet >= 8) then
			alNDSNH(cNDList) = _bAPIPacket(bDataOffset + 0) << 24 + _bAPIPacket(bDataOffset + 1) << 16 + |
					_bAPIPacket(bDataOffset + 4) << 2 + _bAPIPacket(bDataOffset + 3)		
			alNDSNL(cNDList) = _bAPIPacket(bDataOffset + 4) << 24 + _bAPIPacket(bDataOffset + 5) << 16 + |
					_bAPIPacket(bDataOffset + 6) << 8 + _bAPIPacket(bDataOffset + 7)		
			; Now copy the name in.
			_bAPIRecvRet = (_bAPIRecvRet - 8) max CBNIMAX
			for _k = 0 to _bAPIRecvRet-1		; bugbug: hard coded size... ick
				abNDNIS(_i*CBNIMAX + _k) = _bAPIPacket(bDataOffset + 8 + _k)
			next
			while _k < CBNIMAX - 1
				abNDNIS(_i*CBNIMAX + _k) = " "
			wend		
			fAdd = 0			; BUGBUG reuse to know if we did get a title or not
		else
			goto _LUMYP_AGAIN		; was not our message, try looking again...
		endif
	endif
	
	; See if we got a tile or not.  If not then setup a default one.
#ifdef DEBUG
	serout s_out, i9600, ["LUDL -T", 13]
#endif
	if fAdd then
		; need to set default 
		_ai = _i*CBNIMAX
		for _k = 0 to 8		; bugbug: hard coded size... ick
			abNDNIS(_ai+_k) = _UNK_ROBOT(_k)
		next
		abNDNIS(_ai+9) = hex dwLU
	endif

	if dwLU <> dwDLPrev then
		XBeeDL = dwDLPrev				; set the new value...
		gosub SetXBeeDL[XBeeDL]			; Restore the value
	endif

#ifdef DEBUG
	serout s_out, i9600, ["LUDL Add: ", dec _i, " ", hex awNDMY(_i), " ", hex alNDSNH(_i), " ", hex alNDSNL(_i), " ", |
			str abNDNIS(_i*CBNIMAX)\CBNIMAX, 13]
#endif	
	return _i							; return the item number we added

;==============================================================================
; ReadSavedXBeeNDList - Read in the ND List that we have samed in the EEPROM.
;==============================================================================

_csin			var	byte
_j				var	byte
_bNDLT			var	byte(20)					; temporary string to read thigns into...
_cNDListIn		var	byte						; count of items that were read in from EEPROM
_wNDSS			var	word						; signal strength, don't care...
_fItemDup		var	bit							; Have we seen this item before?
_pOut			var	pointer						; pointer to next byte to output

ReadSavedXBeeNDList:
	_fListChanged = 0							; assume the list has not changed.    

    ; Ok Lets first read in the existing ones from the EEPROM
   	ReadDm	XBeeNDDMCache, [cNDList, csNDList]	; get count of items cached and checksum for them...
#ifdef DEBUG_SAVED_LIST
	serout s_out, i9600, ["ReadSavedXBeeNDList cnt= ", dec cNDList, " CSIN: ", hex csNDList, 13]
#endif   	
   	if (cNDList > 0) and (cNDList < CMAXNDLIST) then
   		; need to read in the data...
   		ReadDm XBNDDM_AMY, [str awNDMY\2*cNDList]		; Read in the my array 2 bytes per element...
   		ReadDm XBNDDM_ASL, [str alNDSNL\4*cNDList]		; read in SNL 4 bytes per element
   		ReadDm XBNDDM_ASH, [str alNDSNH\4*cNDList]		; read in SNH 4 bytes per element
   		ReadDm XBNDDM_ANDI, [str abNDNIS\CBNIMAX*cNDList]		; read in Node Identifiers

    	; now lets compute the checksum
    	_csin = 0
    	for _i = 0 to cNDList-1 
    		 _csin = _csin + awNDMY(_i).lowbyte + awNDMY(_i).highbyte + alNDSNL(_i).byte0 + alNDSNL(_i).byte1 + alNDSNL(_i).byte2 + alNDSNL(_i).byte3
    		 _csin = _csin + alNDSNH(_i).byte0 + alNDSNH(_i).byte1 + alNDSNH(_i).byte2 + alNDSNH(_i).byte3
#ifdef DEBUG_SAVED_LIST
			serout s_out, i9600, ["    MY: ", hex awNDMy(_i), " SN: ", hex alNDSNH(_i), hex alNDSNL(_i), " (",str abNDNIS(_i*CBNIMAX)\CBNIMAX,")",13]
#endif
    	next
    	for _i = 0 to cndList*CBNIMAX - 1
    		_csin = _csin + abNDNIS(_i)
    	next
#ifdef DEBUG_SAVED_LIST
		serout s_out, i9600, ["   CS Computed: ", hex _csin, 13]
#endif   	
    	
    	; now validate the checksum...
    	if _csin <> csNDList then
    		cNDList = 0
    	endif
    else
    	cNDList = 0	; garbage
    endif
    return		; return how many we read in.
    
;==============================================================================
; SaveXBeeNDList - Save the XBeeNDList out to the EEPROM
;==============================================================================
SaveXBeeNDList:
	; First we need to compute the checksum
	; now lets compute the checksum
	_csin = 0
	if cNDList > 0 then
		for _i = 0 to cNDList-1 
			 _csin = _csin + awNDMY(_i).lowbyte + awNDMY(_i).highbyte + alNDSNL(_i).byte0 + alNDSNL(_i).byte1 + alNDSNL(_i).byte2 + alNDSNL(_i).byte3
			 _csin = _csin + alNDSNH(_i).byte0 + alNDSNH(_i).byte1 + alNDSNH(_i).byte2 + alNDSNH(_i).byte3
#ifdef DEBUG_SAVED_LIST
			serout s_out, i9600, ["MY: ", hex awNDMy(_i), " SN: ", hex alNDSNH(_i), hex alNDSNL(_i), " (",str abNDNIS(_i*CBNIMAX)\CBNIMAX,")",13]
#endif
		next
		for _i = 0 to cndList*CBNIMAX - 1
			_csin = _csin + abNDNIS(_i)
		next
	endif

#ifdef DEBUG_SAVED_LIST
	serout s_out, i9600, ["SXBL: ", dec CNDList, " ", hex _csin, 13]
#endif		

    ; Ok Lets first read in the existing ones from the EEPROM
   	WriteDm	XBeeNDDMCache, [cNDList, _csin]	; get count of items cached and checksum for them...
   	if (cNDList > 0) and (cNDList < CMAXNDLIST) then
   		; need to read in the data...
   		WriteDm XBNDDM_AMY, [str awNDMY\2*cNDList]		; Write out the my array 2 bytes per element...
   		WriteDm XBNDDM_ASL, [str alNDSNL\4*cNDList]		; Write out SNL 4 bytes per element
   		WriteDm XBNDDM_ASH, [str alNDSNH\4*cNDList]		; Write out SNH 4 bytes per element

		; The pages of the EEPROM are 32 bytes in size.  So lets not write more than 32 bytes at any time.
		; BUGBUG: I hate reusing variables, but...
		_wNDSS = XBNDDM_ANDI		; starting address to start write to
		_csin = CBNIMAX*cNDList		; How many bytes to output
		_pOut = @abNDNIS
		while (_csin >= 32)
	   		WriteDm _wNDSS, [str @_pOut\32]		; Write out Node Identifiers
	   		_wNDSS = _wNDSS + 32					; setup to start writing at the next page
	   		_csin = _csin - 32						; decrement how many bytes we have left to output
	   		_pOut = _pOut + 32						; setup to next byte to output
	   		
		wend	
		if _csin then
   			WriteDm _wNDSS, [str @_pOut\_csin]		; Write out the rest of the bytes that fit within the last page
   		endif
	endif
    return	
    
;==============================================================================
; [UpdateNDListFromXBee] - This function will update the ND list using information
; 		it receives back from issuing an ATND command.  It may add new nodes or
;		if it finds nodes with a serial number that matches, but data is different
;		it will update that information as well.
;==============================================================================
_cbRet	var	sword

UpdateNDListFromXBee:	
	_cNDListIn = cNDList	; remember how many we had from EEPROM (no need to check dups of items beyond this)

	gosub ClearInputBuffer

	; use same helper function to send command as our get functions
#ifdef DEBUG_SAVED_LIST
	serout s_out, i9600, ["Start ND Scan",13];
#ifdef BASICATOMPRO28
#else
#endif
#endif
	gosub APISendXBeeGetCmd["N","D"]
	
	; I think I can loop calling the APIRecvPacket - but as the data it returns is in a differet
	; file, I may have to have helper function over there.
	repeat
		gosub APIRecvPacket[3000000], _cbRet
		APIRECVDATAOFFSET con 5
#ifdef DEBUG_SAVED_LIST
#ifdef BASICATOMPRO28
				serout s_out, i9600, ["ND CB: ", dec _cbRet, ":"];
#else			
				hserout["ND CB: ", dec _cbRet, ":"];
#endif				
			for i = 0 to _cbret
#ifdef BASICATOMPRO28
				serout s_out, i9600, [hex _bAPIPacket(i)\2," "];
#else			
				hserout [hex _bAPIPacket(i)\2," "];
#endif				
			next
#ifdef BASICATOMPRO28
			serout s_out, i9600, [13]
#else			
			hserout [13]		
#endif			
#endif				
		if _cbret > (11+APIRECVDATAOFFSET) then ; MY(2), SH(4), SL(4), DB(1), NI???
			; ok lets extract the information for this item
			; Note with the API receive packet we need to skip over the header part of the API receive.
			awNDMY(cNDList) = (_bAPIPacket(APIRECVDATAOFFSET+0) << 8) + _bAPIPacket(APIRECVDATAOFFSET+1)
			alNDSNH(cNDList) = (_bAPIPacket(APIRECVDATAOFFSET+2) << 24) + (_bAPIPacket(APIRECVDATAOFFSET+3) << 16) + (_bAPIPacket(APIRECVDATAOFFSET+4) << 8) + _bAPIPacket(APIRECVDATAOFFSET+5)		
			alNDSNL(cNDList) = (_bAPIPacket(APIRECVDATAOFFSET+6) << 24) + (_bAPIPacket(APIRECVDATAOFFSET+7) << 16) + (_bAPIPacket(APIRECVDATAOFFSET+8) << 8) + _bAPIPacket(APIRECVDATAOFFSET+9)		
			
#ifdef DEBUG_SAVED_LIST
#ifdef BASICATOMPRO28
			serout s_out, i9600, [hex awNDMy(cNDList), " ", hex alNDSNH(cNDList),hex8 alNDSNL(cNDList)\8, 13]
#else			
			hserout [hex awNDMy(cNDList), " ", hex alNDSNH(cNDList),hex8 alNDSNL(cNDList)\8, 13]
#endif
#endif
			; The Node identifier starts in bTemp(11)
			; We want to blank this field out to our default size we use
			_i = _cbRet - (12+APIRECVDATAOFFSET)	; number of actual characters transfered.
			while _i < CBNIMAX
				_bAPIPacket(_i+11+APIRECVDATAOFFSET) = " "
				_i = _i + 1
			wend  

			_fItemDup = 0
			if _cNDListIn then
				for _i = 0 to _cNDListIn-1
					if (alNDSNH(_i) = alNDSNH(cNDList)) and (alNDSNL(_i) = alNDSNL(cNDList)) then
						; We have seen this one before...
						_fItemDup = 1; 		; signal that this item is a duplicate...
			
						; but we will also make sure the MY or the NI has not changed.
						if (awNDMY(_i) <> awNDMY(cNDList)) then
							_fListChanged = 1; 	we know that we need to write the stuff back out...
							awNDMY(_i) = awNDMY(cNDList)
						endif
						for _j = 0 to CBNIMAX-1
							if abNDNIS(_i*CBNIMAX + _j) <> _bAPIPacket(_j+11+APIRECVDATAOFFSET) then
								abNDNIS(_i*CBNIMAX + _j) = _bAPIPacket(_j+11+APIRECVDATAOFFSET)
								_fListChanged = 1; 	we know that we need to write the stuff back out...
							endif
						next
					endif
				next
			endif
			; only save away the data if this is not a duplicate and we have room
			if (_fItemDup = 0) and  (cNDList < CMAXNDLIST) then
				for _j = 0 to CBNIMAX-1			; copy the NI string in.
					abNDNIS(cNDList*CBNIMAX + _j) = _bAPIPacket(_j+11+APIRECVDATAOFFSET)
				next
				cNDList = cNDList + 1
				_fListChanged = 1	; yes the list changed - have new node
			endif
		endif
	until _cbret <= (11+APIRECVDATAOFFSET) ; MY(2), SH(4), SL(4), DB(1), NI???
						
	return

;==============================================================================
; CHexString[@_bNDLT], _wNDMY
;==============================================================================
_pstr	var	pointer
_lHex	var	long
CHexString[_pStr]:
	_lHex = 0

_CHS_Loop:	
	if (@_pstr >="0") and (@_pstr <= "9") then
		_lHex = _lHex * 16 + (@_pstr - "0")
	elseif (@_pstr >="a") and (@_pstr <= "f")
		_lHex = _lHex * 16 + (@_pstr - "a")
	elseif (@_pstr >="A") and (@_pstr <= "F")
		_lHex = _lHex * 16 + (@_pstr - "A")
	else 
		return _lHex
	endif
	_pStr = _pstr + 1
	goto _CHS_Loop

;==============================================================================
;[UserSelectDL] - This function shows the current XBee DL in the display and
; 	then allows the user to enter new 16 bit hex address.
;	The Logical Enter key will use to save the new value and Esc/Cancel will
;	reset.
;==============================================================================
; Some are used by this mode as well as the lookkup by name...
_lCurTimer		var	long
_lTimerDisp		var	long
_lTimerScroll	var	long
_bCurTextLine	var	byte
_bScrollDir		var	byte
_iNDList 		var	byte	; index of which item we are currently displaying.
_p				var pointer

;local variables
wNewVal		var	word			; 
bMask		var	byte			; not really needed but
fChanged	var	bit

#ifdef USE_OLD_SELECT_DL
_SDL_Prompt0	bytetable	254, 71, 0, 1, "Current DL: "   ;  16
_SDL_Prompt1	bytetable	254, 71, 0, 2, "New:"			;  6
_SDL_MARK		bytetable	254, 71, 0, 1,"* "



UserSelectDL:
	gosub ClearInputBuffer		; clear everything else out of our queue

	; Make sure we have current list from EEPROM
	gosub ReadSavedXBeeNDList		; this reads in from the EEPROM.

	gosub LookupDLInMYList[XBeeDL, 1], _iNDList

_STDL_AGAIN:

	; display the second line of text...
	gosub TASerout[Display, i19200, @_SDL_Prompt1, 8]

	 _lTimerDisp = 0	; force it to display the 1st prompt at first pass through loop
	_bCurTextLine = 0;
	 

	wNewVal = 0			; Start off at zero
	fChanged = FALSE	;

_STDL_LOOP:
	; We will alternate from showing The name of the current Dl and the number...
	gosub GetCurrentTime[], _lCurTimer
	if (_lCurTimer - _lTimerDisp) > ((2000 * WTIMERTICSPERMSDIV) / WTIMERTICSPERMSMUL) then
		if _bCurTextLine = 0 then		;Wish there was a cleaner way!!
			gosub TASerout[Display, i19200, @_SDL_Prompt0, 16]
			bTemp = hex4 XBeeDL\4
			gosub TASerout[Display, i19200, @bTemp, 4]				; output the current DL
		else
			if _iNDList < CMAXNDLIST then
				_p = @abNDNIS + _iNDList*CBNIMAX						; get to the start of the string
				gosub TASerout[Display, i19200, @_SDL_MARK, 6]			; tell the user this is the selected item
				gosub TASerout[Display, i19200, _p, 14]					; display the node name to the user
			endif
		endif
		_bCurTextLine = (_bCurTextLine + 1) & 0x1	; take care of wrap around
		_lTimerDisp = _lCurTimer
	endif

	bPacketPrev(0) = bPacket(0)	; need to save old state...
	bPacketPrev(1) = bPacket(1)
	
	gosub CheckKeypad[TRUE]		; Will try go get something from keypad may change mode
		
	; See if we changed modes
	if fTModeChanged then
		bPacketPrev(0) = bPacket(0)	; Make sure the main loop has our updated state of keys!
		bPacketPrev(1) = bPacket(1)
		return
	endif
	
	; Check to see if the Enter or Cancel have been pressed.
	if (CmdBtns & CMD_ENTER_MASK) and ((CmdBtnsPrev & CMD_ENTER_MASK) = 0) then
		if fChanged then 
			; only save if something actually was entered.
			XBeeDL = wNewVal
			gosub SaveXBeeInfo			; Save it away in EEPROM
			gosub SetXBeeDL[XBeeDL]	; Actually set the destination in the XBee

			; Now look this up and maybe add to our save list...
			gosub LookupDLInMYList[XBeeDL, 1], _iNDList

			; If this updated our list then save it away...
			if _fListChanged then
				gosub SaveXBeeNDList;
				_fListChanged = 0		; only need to do once...
			endif

			goto _STDL_AGAIN				; go back and display the new data.
		endif
	elseif (CmdBtns & CMD_ESC_MASK) and ((CmdBtnsPrev & CMD_ESC_MASK) = 0)
		fChanged = FALSE
		wNewVal = 0			; reset
		serout Display, i19200,	[254, 71, 5, 2, hex4 wNewVal\4]
	endif

	; Now check to see if any of the other keys have been pressed. quick test to see if a button pressed
	; and likely that it was not the same as the last pass...
	if ((bPacket(0) <> bPacketPrev(0)) and bPacket(0)) or ((bPacket(1) <> bPacketPrev(1)) and bPacket(1)) then
		for i = 0 to 7
			bMask = 1 << i
			if (bPacket(0) & bMask) and ((bPacketPrev(0) & bMask) = 0) then
				wNewVal = wNewVal * 16 + i
				fChanged = TRUE
			elseif (bPacket(1) & bMask) and ((bPacketPrev(1) & bMask) = 0)
				wNewVal = wNewVal * 16 + i + 8	; Takes care of 8-F
				fChanged = TRUE
			endif
		next
		
		; Display new number.  May actuall not be new yet, but...
		serout Display, i19200,	[254, 71, 5, 2, hex4 wNewVal\4]
	endif

	; And try again...
	goto _STDL_LOOP
#endif

;==============================================================================
;[UserSelectDLName] - This function shows the current XBee DL in the display and
; 	then allows the user to enter new 16 bit hex address.
;	The Logical Enter key will use to save the new value and Esc/Cancel will
;	reset.
;==============================================================================

;local variables
_SDLN_Prompt0	bytetable	254, 71, 0, 1, "Select By Name "
_SDLN_Prompt1	bytetable	254, 71, 0, 1, "  Scroll RJoy  "
_SDLN_Prompt2	bytetable	254, 71, 0, 1, "A - XBee scan  "
_SDLN_Prompt3	bytetable	254, 71, 0, 1, "D - Delete Item"

_SDLN_Empty		bytetable	254, 71, 0, 2,"*** None ***   "
_SDLN_SCAN		bytetable	254, 71, 0, 2,"-- Scan XBees --"
_SDLN_MARK		bytetable	254, 71, 0, 2,"*"
_SDLN_NOMARK	bytetable	254, 71, 0, 2," "


UserSelectDLName:
	
	; Display the first Text line - actuall we will wait for first pass in loop below to display it.
	;gosub TASerout[Display, i19200, @_SDLN_Prompt0, 19]
	_bCurTextLine = 0
	_lTimerDisp = 0

	; Make sure we have our list of items...
	gosub ReadSavedXBeeNDList		; this reads in from the EEPROM.
	_bScrollDir	= 0					; no scrolling

	; Try to find out if what we consider to be the current item is in our list

_USDLN_FIND_DISPLY_CURDL:	
#ifdef DEBUG
	serout s_out, i9600, ["Sel DL NAME: c=", dec cNDList, 13]
#endif
	
	_iNDList = 0xff		; assume none
	if cNDList = 0 then
		gosub TASerout[Display, i19200, @_SDLN_Empty, 19]	; tell the user the saved list is empty
	else
		for _i = 0 to cndList-1
			if awNDMy(_i) = XBeeDL then
				_iNDList = _i			; should break out ...
			endif
		next
		if _iNDList = 0xff then
			_indList = 0	; display the first one
		endif
		; display the node...
_USDLN_DISPLY_DL:
		_p = @abNDNIS + _iNDList*CBNIMAX		; get to the start of the string
#ifdef DEBUG
		serout s_out, i9600, ["  DL: ", hex awNDMy(_indList), " I: ", dec _indLIST, " SN: ", hex alNDSNH(_indList), hex alNDSNL(_indList), |
			" (", hex abNDNIS(_iNDList*CBNIMAX),")", str abNDNIS(_iNDList*CBNIMAX)\14, 13]
#endif
		if awNDMy(_iNDList) = XBeeDL then
			gosub TASerout[Display, i19200, @_SDLN_MARK, 5]		; tell the user this is the selected item
		else	
			gosub TASerout[Display, i19200, @_SDLN_NOMARK, 5]	; tell the user this is not the selected item
		endif	
		gosub TASerout[Display, i19200, _p, 14]					; display the node name to the user
	endif
_STDLN_AGAIN:

_STDLN_LOOP:
	gosub GetCurrentTime[], _lCurTimer
	if (_lCurTimer - _lTimerDisp) > ((2000 * WTIMERTICSPERMSDIV) / WTIMERTICSPERMSMUL) then
		if _bCurTextLine = 0 then		;Wish there was a cleaner way!!
			gosub TASerout[Display, i19200, @_SDLN_Prompt0, 19]
		elseif _bCurTextLine = 1
			gosub TASerout[Display, i19200, @_SDLN_Prompt1, 19]
		elseif _bCurTextLine = 2
			gosub TASerout[Display, i19200, @_SDLN_Prompt2, 19]
		else
			gosub TASerout[Display, i19200, @_SDLN_Prompt3, 19]
		endif
		_lTimerDisp = _lCurTimer
		_bCurTextLine = (_bCurTextLine + 1) & 0x3	; take care of wrap around
	endif
		
	bPacketPrev(0) = bPacket(0)	; need to save old state...
	bPacketPrev(1) = bPacket(1)
	
	gosub CheckKeypad[TRUE]		; Will try go get something from keypad may change mode
		
	; See if we changed modes
	if fTModeChanged then
		bPacketPrev(0) = bPacket(0)	; Make sure the main loop has our updated state of keys!
		bPacketPrev(1) = bPacket(1)
		return
	endif
	; Check to see if the Enter or Cancel have been pressed.
	if (CmdBtns & CMD_ENTER_MASK) and ((CmdBtnsPrev & CMD_ENTER_MASK) = 0) then
		; first if our list of items has changed saved them out to the EEPROM;
		if _fListChanged then
			gosub SaveXBeeNDList;
			_fListChanged = 0		; only need to do once...
		endif
		; then if the selected DL has changed then update that...
		if awNDMy(_iNDList) <> XBeeDL then
			; only save if something actually was entered.
			XBeeDL = awNDMy(_iNDList)
			gosub SaveXBeeInfo			; Save it away in EEPROM
			gosub SetXBeeDL[XBeeDL]	; Actually set the destination in the XBee
			goto _USDLN_FIND_DISPLY_CURDL		
		endif
	elseif (CmdBtns & CMD_ESC_MASK) and ((CmdBtnsPrev & CMD_ESC_MASK) = 0)
		fChanged = FALSE
	; some key was pressed!
	elseif ((bPacket(0) <> bPacketPrev(0)) and bPacket(0)) or ((bPacket(1) <> bPacketPrev(1)) and bPacket(1)) 
		if bPacket(1).bit2 then 		; "A" key was pressed
#ifndef USE_XPOTS
			sound p9, [50\4000] ;XP: NO SPEAKER!
#endif			
			gosub TASerout[Display, i19200, @_SDLN_SCAN, 19]		; show the user we are doing something
			
			gosub UpdateNDListFromXBee			; merge in the list from active XBEES
			goto _USDLN_FIND_DISPLY_CURDL		; and make sure we are displaying the right item...
		elseif bPacket(1).bit5		;"D" key was pressed
			; Ok Delete the current one (assuming the list is not empty!
#ifndef USE_XPOTS
			sound p9, [50\4000] ;XP: NO SPEAKER!
#endif			
			if cndList then
				cndList = cndList -1 	; decrement the number of items
				if cndList = 0 then _USDLN_FIND_DISPLY_CURDL	; this will display the empty guy
				if _indList = cndList then
					_indList = _indList -1	; delete last one so show previous one...
				else
					; we need to copy the other ones up
					for _i = _indList to cndList -1
						alNDSNH(_i) = alNDSNH(_i+1) 
						alNDSNL(_i) = alNDSNL(_i+1)
						awNDMY(_i) = awNDMY(_i+1)

						for _j = 0 to CBNIMAX-1
							abNDNIS(_i*CBNIMAX + _j) = abNDNIS((_i+1)*CBNIMAX + _j)
						next;
					next
				endif
				_fListChanged = 1; 	we know that we need to write the stuff back out...
				goto _USDLN_DISPLY_DL:
			endif
		endif
	elseif cndList > 1   ; if list does not have more than 1 item, nothing to scroll
		; No button pressed see about the Right joysick vertical  to scroll our list
		; BUGBUG: should we extract the reading of these to some function?
		; Right Vertical (p2)
		sums(1) = sums(1) - buffer1(index) 		; subtract off old value
		adin 2, buffer1(index)				;  
		sums(1) = sums(1) + buffer1(index) 		; Add on new value
;		bPacket(3) = ((((((sums(1) + SumOffsets(1)) min (AToDMins(1)*8)) / 8) - AToDMins(1)) * 256) / AToDRanges(1)) max 255	; 
;		could generate what our packet value would be, but instead just say are we far enough from center to increment or
;		decrement
		; finally increment the index and limit its range to 0 to 7. 
		index = (index + 1) & 7 	; need to go through all of the value or we will never get very far from center...
		
		; we are only going for three values +1, 0 and -1.  Also need to handle if held to scroll in that direction but not
		; too fast 

		if sums(1) > (AtoDMidT8(1)+256) then		; BUGBUG: not 100% accurate but probably good enough 
			; only scroll if first time or after delta times.
			if (_bScrollDir = 0) or ((_lCurTimer - _lTimerScroll) > ((1000 * WTIMERTICSPERMSDIV) / WTIMERTICSPERMSMUL)) then
				; Scroll up
				if _indList = 0 then
					_indList = cndList-1
				else
					_indList = _indList - 1
				endif
				_bScrollDir = -1
				_lTimerScroll = _lCurTimer			; save the time when we did the last scroll
				goto _USDLN_DISPLY_DL				; This will display the node - I hate gotos but it saves code
			endif
		elseif sums(1) < (AtoDMidT8(1)-256) 
			if (_bScrollDir = 0) or ((_lCurTimer - _lTimerScroll) > ((1000 * WTIMERTICSPERMSDIV) / WTIMERTICSPERMSMUL)) then
				; scroll down
				_indList = _indList + 1
				if _indList >= cndList then
					_indList = 0
				endif

				_bScrollDir = 1
				_lTimerScroll = _lCurTimer			; save the time when we did the last scroll
				goto _USDLN_DISPLY_DL				; This will display the node - I hate gotos but it saves code

			endif
		else
			_bScrollDir = 0  		; no more scroll...	
		endif
	endif
	
	goto _STDLN_LOOP

;==============================================================================
;[UserSelectMy] - This function shows the current XBee MYL in the display and
; 	then allows the user to enter new 16 bit hex address.
;	The Logical Enter key will use to save the new value and Esc/Cancel will
;	reset.
;==============================================================================
_SMY_Prompt0	bytetable	254, 71, 0, 1, "Current My: "   ;  16
_SMY_Prompt1	bytetable	254, 71, 0, 2, "New:"			;  8
_SMY_POS_NEW	bytetable	254, 71, 5, 2
;local variables
UserSelectMy:

_USMY_AGAIN:
	; bugbug: May need to to use our TASerout ...
	gosub TASerout[Display, i19200, @_SMY_Prompt0, 16]
	bTemp = hex4 XBeeMy\4
	gosub TASerout[Display, i19200, @bTemp, 4]				; output the current MY

	; display the second line of text...
	gosub TASerout[Display, i19200, @_SMY_Prompt1, 8]

	wNewVal = 0			; Start off at zero
	fChanged = FALSE	;

_USMY_LOOP:
	bPacketPrev(0) = bPacket(0)	; need to save old state...
	bPacketPrev(1) = bPacket(1)
	
	gosub CheckKeypad[TRUE]		; Will try go get something from keypad may change mode
		
	; See if we changed modes
	if fTModeChanged then
		bPacketPrev(0) = bPacket(0)	; Make sure the main loop has our updated state of keys!
		bPacketPrev(1) = bPacket(1)
		return
	endif
	
	; Check to see if the Enter or Cancel have been pressed.
	if (CmdBtns & CMD_ENTER_MASK) and ((CmdBtnsPrev & CMD_ENTER_MASK) = 0) then
		if fChanged then 
			; only save if something actually was entered.
			XBeeMy = wNewVal
			gosub SaveXBeeInfo			; Save it away in EEPROM
			gosub SetXBeeMy[XBeeMY]	; Actually set the My in the XBee
			goto _USMY_AGAIN				; go back and display the new data.
		endif
	elseif (CmdBtns & CMD_ESC_MASK) and ((CmdBtnsPrev & CMD_ESC_MASK) = 0)
		fChanged = FALSE
		wNewVal = 0			; reset
		gosub TASerout[Display, i19200, @_SMY_POS_NEW, 4]
		bTemp = hex4 wNewVal\4
		gosub TASerout[Display, i19200, @bTemp, 4]				; output the current MY
	endif

	; Now check to see if any of the other keys have been pressed. quick test to see if a button pressed
	; and likely that it was not the same as the last pass...
	if ((bPacket(0) <> bPacketPrev(0)) and bPacket(0)) or ((bPacket(1) <> bPacketPrev(1)) and bPacket(1)) then
		for i = 0 to 7
			bMask = 1 << i
			if (bPacket(0) & bMask) and ((bPacketPrev(0) & bMask) = 0) then
				wNewVal = wNewVal * 16 + i
				fChanged = TRUE
			elseif (bPacket(1) & bMask) and ((bPacketPrev(1) & bMask) = 0)
				wNewVal = wNewVal * 16 + i + 8	; Takes care of 8-F
				fChanged = TRUE
			endif
		next
		
		; Display new number.  May actuall not be new yet, but...
		gosub TASerout[Display, i19200, @_SMY_POS_NEW, 4]
		bTemp = hex4 wNewVal\4
		gosub TASerout[Display, i19200, @bTemp, 4]				; output the current MY
	endif

	; And try again...
	goto _USMY_LOOP

;==============================================================================
; [DisplayRemoteString (pStr, cbStr)]
; 
; This function takes care of displaying string and or a number sent to us 
; rom the remote robot.  For now hard coded to a specific location on line 2...
;
; This function will also switch to the appropriate display mode if necessary.
;==============================================================================
pStr	var	pointer
cbStr	var	byte

DisplayRemoteString[pstr, cbStr]
	; Make sure we are in the right mode to display the data
	if TransMode <> TMODE_DATA then
		; BUGBUG: Should put this into a change mode function...
		gosub ClearLCDDisplay
		bTDataLast = 0xff		; make sure we will display something when it changes.
		TransMode = TMODE_DATA
	endif

	gosub TASerout[Display, i19200, @_LCDLINE2, 4]	; hopefully position our self at the start of line 2...

	if cbStr then
		gosub TASerout[Display, i19200, pStr, cbStr]
	endif

	if cbStr < 16 then
		gosub TASerout[Display, i19200, @_blanks, 16-cbStr]
	endif

	return

;==============================================================================
; [DisplayRemoteValue (col, num)]
; 
; This function takes care of displaying a number sent to us 
; rom the remote robot.  For now hard coded to a specific location on line 2...
;
; This function will also switch to the appropriate display mode if necessary.
;==============================================================================
bDRVCol var byte 
wVal	var	word
DisplayRemoteValue[bDRVCol, wVal]
	; Make sure we are in the right mode to display the data
	if TransMode <> TMODE_DATA then
		; BUGBUG: Should put this into a change mode function...
		gosub ClearLCDDisplay
		bTDataLast = 0xff		; make sure we will display something when it changes.
		TransMode = TMODE_DATA
	endif

	; now display the value - For now one or the other.
	gosub DoLCDDisplay[bDRVCol, 2, 0, wVal]

	return

;==============================================================================
; [PlayRemoteSounds (pStr, cbStr)]
; 
; This function will output sounds that were sent by the remote computer
; the sounds are: 2 byte values: first is the length and the second is the
; freq/25...
;==============================================================================
_BDUR	var byte
_WFREQ	var	word
PlayRemoteSounds[pstr, cbStr]
	while (cbStr > 1)
		; could do in basic, but 
		mov.l	@PSTR:16, er1		; get the pointer
		mov.b	@er1+, r0l			; get the duration into r0l
		mov.b	r0l, @_BDUR:16		; save away the duration into a variable that I can use
		mov.b	@er1+, r0l			; get the freq/25 into r0l
		mov.l	er1, @PSTR:16		; save away the updated pointer
		mov.b	#25,r2l				; setup to multiply by 25
		mulxu.b	r2l, r0				; do 8 bit multiply
		mov.w	r0, @_WFREQ:16
	
#ifdef USE_XPOTS
		;sound Speaker, [_BDUR\_WFREQ] ;Sorry no sound on the XP edition.. ;)
#endif		
		cbStr = cbStr - 2;
	wend

	return
	

;==============================================================================
; [DoLCDDisplay (x, y, char, num)]
; 
; This function takes care of displaying the values to the screen.
; used to build buffers for our own version of serout, may combine later but
; for now leave it seperated as more logic may go in here later...
;==============================================================================
 
bLCDX 		var	byte
bLCDY		var byte
bLCDChar	var	byte
bLCDNum		var	word

abLCDBuff	var	byte(20)			; buffer to use to call TASeroutwith
cbLCDOut	var	word				; how many bytes to output

DoLCDDisplay[bLCDX, bLCDY, bLCDChar, bLCDNum]

	abLCDBuff = 254, 71, bLCDX, bLCDY		; filled in the start of the buffer with the LCD position stuff
	if bLCDChar <> 0 then
		abLCDBuff(4) = bLCDChar
		cbLCDOut = 5
	else
		; May later handle +- types of numbers
		abLCDBuff(4) = dec4 bLCDNum\4
		bLCDChar = 4
		while (bLCDChar < 7) and (abLCDBuff(bLCDChar) = "0")		; reuse the passed in char field as an index
			abLCDBuff(bLCDChar) = " "		; remove leading zeros
			bLCDChar = bLCDChar + 1
		wend
		cbLCDOut = 8
	endif	

	; now lets call our taserout function
	gosub TASerout[Display, i19200, @abLCDBuff, cbLCDOut]
	

	return				

;==============================================================================
; [ClearLCDDisplay]
; 
; This function clears the LCD Display
;==============================================================================
_LCD_CS	bytetable 254,88
ClearLCDDisplay:
	gosub TASerout[Display, i19200, @_LCD_CS, 2]	; Simply call off to our display function
	return
	
;==============================================================================
; [ShowLCDString (x, y, p, cnt)]
; 
; This function takes care of displaying a string on the screen.
;==============================================================================
pb	var	pointer
cb	var	word
ShowLCDString[bLCDX, bLCDY, pb, cb]

	abLCDBuff = 254, 71, bLCDX, bLCDY		; filled in the start of the buffer with the LCD position stuff
	gosub TASerout[Display, i19200, @abLCDBuff, 4]	; first simply output the position we want

	; Then simply output the string buffer
	gosub TASerout[Display, i19200, pb, cb]
	

	return				

#ifdef USE_OLD_TASEROUT
;==============================================================================
; [TASerout(pin, baudmode, buffer, bufferlen]
;
; Second attempt will be to use TimerV which is only 8 bits, this version will
; use the Compare Match A interrupt do do the acutual output bit processing.
; The main loop will still wait in for the work to be done.  Could later change
; this to queue the values and do it all in the background, but that could get
; complicated as maybe I would start doing multiple different IO ports at the same
; time...  But maybe later.
;
;==============================================================================

; define parameters to this function.
TASPin var	byte
TASBM	var	word
TASPBUF var	POINTER
TASCnt var	word

; Ok lets define the communication variables that will be used between
; the main function here and the interrupt handler.
TASOIH_R1		var	word			; status bits...
;	rll - The Byte to output
; 	r1h	- Other status
;		bit  7 - Do the start bit
;		bit  6 - Do the Stop bit
;		bits 4-5 - 0 not used
;		bits 0-3 - count of bits left to output...
;       ...
TASOIH_R2		var	word			; Some of the stuff to pass as follows:
;  r2 - the PDRX of the IO pin 
TASOIH_R3		var	word			; More stuff to and from the main function
; r3l - IO pin number on the PDRX for the BAP io pin
; r3h - The byte to output.


TASerout[TASPin, TASBM, TASPBUF, TASCNT]
								; BUGBUG: could do this in assembly as well

	; ok need to initialize TimerV
; transistion to assembly...
	mov.w	@TASBM:16, r1			; get the baudmode.
	bld.b	#6, r1h					; Get the normal/invert bit into C
	subx.b	r3h, r3h				; r3h = 0 if Normal or ff if inverted
	and.w	#0x1fff, r1				; extract the bit time
	mov.w	#3,e1					; start off assuming clock will be divided by 8 as the baud mode is set this way
_TASOMCL:	
	or.b	r1h, r1h				; see if our count is < 256
	beq		_TASOBMLD:8				; yep done with this loop
	shlr.w	r1						; nope divide by 2
	add.w	#1, e1					; change clock input for TimerV
	jmp		_TASOMCL:8				; Check again

_TASOBMLD:	
	; BUGBUG: should verify that we are in a valid range...
	mov.b	r1l, @TCORA:8			; save away the value that we should count up to for each bit
	mov.w	e1, r1
	mov.b	#0, r1h					; zero out the high byte
	rotr.w	r1						; so low order bit now in high bit of high word
	rotl.b	r1h						; now in low bit of r1h
	mov.b	r1h, @TCRV1:8			; OK so should have the low bit of timer calc stored properly away.
	or.b	#0x08, r1l				; Added in the bits: CCLR0 for the TCRV0
	mov.b	r1l, @TCRV0:8			; save away the generated configuration byte
		
; first lets use the Porttable that BAP has to convert logical pin to Port/pin
	xor.l	er0, er0				; zero out er0
	mov.b	@TASPIN:16, r0l			; move the logical pin number into r0l
	and.b	#0x3f, r0l				; make sure we are in the range 0-63
	shll.b	r0l						; double the offset as the port table is 2 bytes per entry.
	mov.w	@(_PORTTABLE:16, er0), r2	; OK R2 now has the two bytes high being the bit, and low being the port...
	mov.b	r2h, r3l				; Ok r3l now has the pin number
	mov.b	#0xff, r2h				; setup 32 bit address to port register PDRx
	
	; make sure it is setup for output
	mov.b	@(0x70-0xD0, er2), r0l	; get the current value for PCRx byte from the shadow location
	bset.b	r3l, r0l				; set the appropriate bit for our port
	mov.b	r0l, @(0x70-0xD0, er2)	; save away the updated value to the shadow location
	mov.b	r0l, @(0x10, er2)		; save away in the actual PCRx byte
		
	; Make sure the IO line is correct to start off with
	or.b	r3h, r3h
	beq		_TASOINORMAL:8			; OK will start up normal
	bset.b	r3l,@er2				; Inverted mode start high
	jmp		_TASOINITBUF:8			; continue to setup pointers to buffers
_TASOINORMAL:
	bclr.b	r3l, @er2				; this sets the IO line to low...

	; Now setup data pointer and counter.
_TASOINITBUF:	
	mov.l	@TASPBUF:16, er4			; OK now ER4 has pointer to the buffer to output.
	mov.w	@TASCNT:16, e3			; Now E3 has the count of bytes to output

	; put up to one bit delay at the start to get everything in sync...
_TASOWAB:	
	bld.b	#6, @TCSRV:8			; see if we had a timer cycle go through
	bcc		#_TASOWAB:8				; nope
	bclr.b	#6, @TCSRV:8			; clear out the status

	; save away some of the data that does not change for the interrupt.
	mov.w	r2, @TASOIH_R2:16		; save away our the PDrx value
	mov.w	r3, @TASOIH_R3:16		; save awty the port pin number and inversion state.
	
	
;------------------------------------------------
; Main output loop: helps to know what each register is doing:
; r2	- Word address for PDRx for the output pin
; r3l	- bit number of IO PIN
; r3h	- 0 if Normal communication, FF if inverted.
; e3	- Count of bytes left to output	
; er4 	- Pointing to the current byte to output
	
_TASOBYTELOOP:
	or.w	e3, e3					; see if more bytes left to go
	beq		_TASODONE:16			; NO BYtes left go to do the cleanup
	mov.b	@er4+, r1l				; Ok have the next byte to output
	xor.b	r3h,r1l					; Hack if inverted mode we will invert all of the bits here...
	
	mov.b	#0x88, r1h				; status of do start bit and 8 bits to loop through
	mov.w	r1, @TASOIH_R1:16		; save away byte, plus status byte
	
	; align our self with the bit clock to make sure we don't give to small of a start pulse
_TASOWSB0:	
	bld.b	#6, @TCSRV:8			; see if we had a timer cycle go through
	bcc		#_TASOWSB0:8			; nope
	bclr.b	#6, @TCSRV:8			; clear out the status
	

	; now lets enable the interrupt and wait until the interrupt handler tells us it's done
	bset.b	#6, @TCRV0:8			; OK turned on TCRV0
		
	; now lets wait until the interrupt handler says it is done with that byte.
_TASO_WAITFORBYTEDONE:
	mov.w	@TASOIH_R1, r1			; get the status byte
	bne		_TASO_WAITFORBYTEDONE:8	; not done yet.
				
	; now jump up to see if there are any more bytes left to output
	dec.w	#1,e3						; decrement the count and go back up to check to see if we are done
	bra		_TASOBYTELOOP:8
		

_TASODONE:	
; transistion back to basic	
return



;==============================================================================
; TASOIHandler
;
; This version will use the Compare Match Interrupt A to do all of the manipulation
; of the IO line.  This should keep the timing hopefully very consistent from
; bit to bit.  
;
; will need to use some memory locations to store the state information between
; interrupts as other interupts may be involved, which could cause any register
; using between the main function and the interrupt to be corrupted.
;
;==============================================================================

BEGINASMSUB 
TASOIHANDLER:
	push.w	r3					; save away...
	push.w	r2 					; first save away registers we will touch
	push.w	r1					;
	
	mov.w	@TASOIH_R1:16, r1	; now get all of our state information to use
	mov.w	@TASOIH_R2, r2		;
	mov.w	@TASOIH_R3, r3		;
	
	bclr.b	#6, @TCSRV:8		; clear out the interrupt bit so it can happen again
;	andc	#0x7f,ccr               ; allow other interrupts to happen 

	;	OK lets figure out what we need to do.
	; first check to see if we are doing a start bit
	;
	bld.b	#7, r1h				; the bit value goes into C bit
	bcc		_TASOIH_CHECKFORSTOPBIT:8
	bclr.b	#7, r1h				; clear it out
		
	; Ok lets do the start bit
	; See if we are inverted
	or.b	r3h,r3h				; see if we are in inverted mode
	bne		_TASOIH_STARTINVERT:8
	; normal inverted.
	bset.b	r3l,@er2				; Mormal mode Start goes high
	bra		_TASOIH_CLEANUP:8
_TASOIH_STARTINVERT:
	bclr.b	r3l, @er2				; this sets the IO line to low...
	bra		_TASOIH_CLEANUP:8		
		
_TASOIH_CHECKFORSTOPBIT:
	; Now test for Stop bit
	bld.b	#6, r1h
	bcc		_TASOIH_DODATABIT:8

	
	; Ok lets do the stop bit
	; See if we are inverted
	or.b	r3h, r3h
	bne		_TASOIH_STOPINVERT:8
	; normal inverted.
	bclr.b	r3l, @er2				; this sets the IO line to low...
	bra		_TASOIH_STOPCLEANUP:8

_TASOIH_STOPINVERT:
	bset.b	r3l,@er2				; Mormal mode Start goes high

_TASOIH_STOPCLEANUP:
	xor.w	r1,r1					; clear out all status - will trigger main function

	; Ok this byte is done, turn off the interrupt
	bclr.b	#6, @TCRV0:8			; OK turned off TCRV0
	bra		_TASOIH_CLEANUP:8		
	
_TASOIH_DODATABIT:
	; OK lets do the normal IO...

	shlr.b	r1l						; ok lets shift it down lowest bit goes into carry.
	bcc		_TASOIH_BIT0:8				; not set so was zero
	bclr.b	r3l,@er2				; The bit is a zero
	jmp		_TASOIH_BITCLEANUP:8			; 
_TASOIH_BIT0:
	bset.b	r3l, @er2				; Ok this bit goes high
	
_TASOIH_BITCLEANUP:
	; now see if we finished doing all of the bits
	dec.b	r1h						; decrement the number of bits left to output
	bne		_TASOIH_CLEANUP:8		; still more to go, get ready to return from interrupt

	; we have output all 8 bits, setup to do stop bit
	bset.b	#6, r1h					; set the do the invert bit to process

_TASOIH_CLEANUP:		
	; save away the values that have changed for the next interrupt...
	mov.w	r1,@TASOIH_R1:16
	;mov.w	r2,@TASOIH_R2:16	; does not change...
	;mov.w	r3,@TASOIH_R3:16	; does not change
	
	pop.w	r1					; restore back the registers we used to the caller
	pop.w	r2
	pop.w	r3 

	rte							; and return from the exception.

ENDASMSUB 
#endif ;USE_OLD_TASEROUT