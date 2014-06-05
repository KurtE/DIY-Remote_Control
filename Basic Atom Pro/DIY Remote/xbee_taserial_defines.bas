;==============================================================================
;Kurts Timer, Serial Functions and XBee support Definitions
;
;Description: This support file contains the definitions of all of the
; 			public variables and definitions before they are used by other  
;			You do not need to include this file for only TASerial support
;			however if you do include it, we will assume that you wish for
;			full functionality.
;==============================================================================
;==============================================================================
; [PUBLIC - Things that will be used outside of this support file]
;==============================================================================
USEXBEE		con	1
USETIMER	con	1
;USETASERIAL	con	1		; will be automatically defined in support function
;							unless XBEEONHSERIAL is defined
;==============================================================================
;[System Timer]
;==============================================================================
; The program needs to call to InitTimer to initialize the timer.  These functions
; use TimerA on Bap28.
; [gosub InitTimer]

; Use the function GetCurrentTime, which returns a long value which is the number
; of timer ticks/8192 since the timer was last reset.  A multiplier and diviser
; have been defined to convert the returned value into miliseconds. For example
; to time a sequence of code, one might do something like:
; gosub GetCurrentTime[], lStartTime
; ...
; gosub GetCurrentTime[], lEndTime
; lDeltaTime = ((lEndTime - lStartTime)*WTIMERTICSPERMSMUL)
#ifdef BASICATOMPRO28
WTIMERTICSPERMSMUL con 64	; BAP28 is 16mhz need a multiplyer and divider to make the conversion with /8192
WTIMERTICSPERMSDIV con 125  ; 
#else
WTIMERTICSPERMSMUL con 16	; Bap 40 arc32 20mhz need a multiplyer and divider to make the conversion with /8192
WTIMERTICSPERMSDIV con 25  ; 
#endif

;==============================================================================
; [XBee definitions]
;==============================================================================
;
; We are rolling our own communication protocol between multiple XBees, one in
; the receiver (this one) and one in each robot.  We may also setup a PC based
; program that we can use to monitor things...
; Packet format:
; 		Packet Header: <Packet Type><Checksum><SerialNumber><cbExtra>
;


; Packet Types:
;[Packets sent from Remote to Robot]
;XBEE_TRANS_READY		con	0x01	; Transmitter is ready for requests.
	; Optional Word to use in ATDL command
;XBEE_TRANS_NOTREADY		con 0x02	; Transmitter is exiting transmitting on the sent DL
	; No Extra bytes.
XBEE_TRANS_DATA			con	0x03	; Data Packet from Transmitter to Robot*
	; Packet data described below.  Will only be sent when the robot sends
	; the remote a XBEE_RECV_REQ_DATA packet and we must return sequence number
;XBEE_TRANS_NEW			con	0x04	; New Data Available
	; No extra data.  Will only be sent when we are in NEW only mode
XBEE_ENTER_SSC_MODE		con	0x05	; The controller is letting robot know to enter SSC echo mode
	; while in this mode the robot will try to be a pass through to the robot. This code assumes
	; cSSC_IN/cSSC_OUT.  The robot should probalby send a XBEE_SSC_MODE_EXITED when it decides
	; to leave this mode...	
	; When packet is received, fPacketEnterSSCMode will be set to TRUE.  Handlers should probalby
	; get robot to a good state and then call XBeeHandleSSCMode, which will return when some exit
	; condition is reached.  Start of with $$<CR> command as to signal exit
XBEE_REQ_SN_NI			con 0x06	; Request the serial number and NI string

XBEE_DEBUG_ATTACH		con 0x07	; Debug Attach - used to say send debug info to display
XBEE_DEBUG_DETACH		con 0x08	; End debug output messages...

;[Packets sent from Robot to remote]
;XBEE_RECV_REQ_DATA		con	0x80	; Request Data Packet*
	; No extra bytes, but we do pass a serial number that we expect back 
	; from Remote in the XBEE_TRANS_DATA_PACKET
;XBEE_RECV_REQ_NEW		con	0x81	; Asking us to send a XBEE_TRANS_NEW message when data changes
;XBEE_RECV_REQ_NEW_OFF	con	0x82	; Turn off that feature...

;XBEE_RECV_NEW_THRESH	con 0x83	; Set new Data thresholds
	; currently not implemented
XBEE_RECV_DISP_VAL		con	0x84	; Display a value on line 2
	; Will send 4 bytes extra data for value.
XBEE_RECV_DISP_STR		con	0x85	; Display a String on line 2
	; If <cbExtra> is  0 then we will display the number contained in <SerialNumber> 
	; If not zero, then it is a count of bytes in a string to display.
XBEE_PLAY_SOUND			con	0x86	; Will make sounds on the remote...
	;	<cbExtra> - 2 bytes per sound: Duration <0-255>, Sound: <Freq/25> to make fit in byte...
XBEE_SSC_MODE_EXITED	con	0x87	; a message sent back to the controller when
	; it has left SSC-mode.
XBEE_SEND_SN_NI_DATA	con 0x88	; Response for REQ_SN_NI - will return
	; 4 bytes - SNH
	; 4 bytes - SNL
	; up to 20 bytes(probably 14) for NI
XBEE_RECV_DISP_VAL0		con	0x89	; Display a 2nd value on line 2 - Col 0 on mine
XBEE_RECV_DISP_VAL1		con	0x8A	; Display a 2nd value on line 2
XBEE_RECV_DISP_VAL2		con	0x8B	; Display a value on line 2  - Cal 

XBEE_RECV_REQ_DATA2		con 0x90    ; New format... 


;[XBEE_TRANS_DATA] - has XBEEPACKETSIZE extra bytes
;	0 - Buttons High
;	1 - Buttons Low
; 	2 - Right Joystick L/R
;	3 - Right Joystick U/D
;	4 - Left Joystick L/R
;	5 - Left Joystick U/D
; 	6 - Right Slider
;	7 - Left Slider

PKT_BTNLOW		con 0				; Low Buttons 0-7
PKT_BTNHI		con	1				; High buttons 8-F
PKT_RJOYLR		con	2				; Right Joystick Up/Down
PKT_RJOYUD		con	3				; Right joystick left/Right
PKT_LJOYLR		con	4				; Left joystick Left/Right
PKT_LJOYUD		con	5				; Left joystick Up/Down
PKT_RSLIDER		con	6				; right slider
PKT_LSLIDER		con	7				; Left slider

;==============================================================================
; [Public Variables] that may be used outside of the helper file 
; Will also describe helper functions that are provided that use
; or return these values.
;==============================================================================

;------------------------------------------------------------------------------
; [InitXbee] - Intializes the XBee - This function assumes that
; 	gosub InitXBee
; 	cXBEE_OUT - is the output pin for the Xbee
; 	cXBEE_IN  - Is the input pin
;	cXBEE_RTS - is the flow control pin.  If using sparkfun regulated explorer
;			be careful that this is P6 or P7 as for lower voltage.
;------------------------------------------------------------------------------

;------------------------------------------------------------------------------
; [ReceiveXbeePacket] - Main workhorse, it does all of the work to ask for new
;				data if appropriate or returns last packet if in new mode.  it
;				will also handle several other packets.  If in new only packet mode
;				and we have not received a packet in awhile, we may deside to force
;				asking for a packet.  If so fPacketForced will be true.  If forced
;				and not valid then maybe something is wrong on the other side so probably
;				go into a safe mode.  If Forced we may want to check to see if our new
;				data matches our old data. If not we may want to retell the Remote that we
;				want new packet only mode.
;	
;				Note: we wont ask for data from the remote until we have received a Transmit ready
;				packet.  This is what fTransReadyRecvd variable is for.
;	gosub ReceiveXbeePacket
;
; On return if fPacketValid is true then the array bPacket Contains valid data.  Other
; flags are described below
;------------------------------------------------------------------------------
USE_XPOTS con 	1		; use Zentas extra pots...

#ifdef USE_XPOTS
CNT_SUMS	con 8				; there are 8 in this mode
#else
CNT_SUMS	con 6				; there are 6 in this mode
#endif
XBEEPACKETSIZE		con CNT_SUMS+2			; define the size of my standard pacekt.
bPacket				var	byte(XBEEPACKETSIZE)		; This is the packet of data we will send to the robot.

fPacketValid		var	bit			; Is the data valid
fPacketForced		var	bit			; did we force a packet
fPacketTimeout		var	bit			; Did a timeout happen
fSendOnlyNewMode	var	bit			; Are we in the mode that we will only receive data when it is new?
fPacketEnterSSCMode	var	bit			; Did we receive packet to enter SSC mode?
fTransReadyRecvd	var	bit			; Are we waiting for transmit ready?

CPACKETTIMEOUTMAX	con	100			; Need to define per program as our loops may take long or short...
cPacketTimeouts		var	word		; See how many timeouts we get...

XBEE_API_MODE con 1					; first put under ifdefs...
XBEE_API_PH_SIZE	con	1			; Changed Packet Header to just 1 byte - Type - we don't use sequence number anyway...

#ifdef XBEE_API_MODE
APIPACKETMAXSIZE	con	32
_bAPIPacket			var	byte(APIPACKETMAXSIZE+1)	; Used to sending and receiving packets using API mode
_bAPIStartDelim		var	byte		; USed to read in the start delimiter...
_wAPIPacketLen		var	word		; Api Packet length...
_bAPIRecvRet		var	byte		; return value from API recv packet
#endif


;------------------------------------------------------------------------------
; [XBeeOutputVal] - Send a byte value back to the Remote to display
; 	gosub XBeeOutputVal[bVal]
;------------------------------------------------------------------------------
;------------------------------------------------------------------------------
; [XBeeOutputString] - Output a string back to the display
;	gosub XbeeOutputString[@STR]
;
; Note: This function assumes the first byte in STR is the length of characters
; 	to display.  Example: _WALKSTR	bytetable	4,"Walk"
;	gosub XBeeOutputString[@_WALKSTR]
;------------------------------------------------------------------------------
;------------------------------------------------------------------------------
; [SetXbeeDL] - Set the XBee DL to the specified word that is passed in Destination
;		low.  Note this is used when we receive a packet saying the remote is ready
;		and use this address to talk to me
;	gosub SetXBeeDL[wNewDL]
;------------------------------------------------------------------------------
;------------------------------------------------------------------------------
; [SendXBeeNewDataOnlyPacket[fNewOnly]] - This function tells the remote if
;		we wish to know when the data has changed at the remote. If non-zero value
;		passed in, this will cause the remote to send us a packet whenever the data
;		changes.  Note: It will only send one notification packet, until you ask
;		for the next packet.
;	gosub SndXbeeNewDataOnlyPacket[1]
;------------------------------------------------------------------------------

;------------------------------------------------------------------------------
; [XBeeHandleSSCMode] - some simple code to try to allow the host to transparently 
;		talk to SSC-32.  We will put a simple check to see when we should exit.
;		This function will return when an exit condition is received.
;	gosub XBeeHandleSSCMode
;
; The define SSCBUFLEN defines how large a buffer to use.  If it is undefined
; it will default to 80.  If defined as 0 will disable the code
;------------------------------------------------------------------------------

;==============================================================================
; XBEE standard strings to save memory - by Sharin
;==============================================================================
_XBEE_PPP_STR	bytetable	"+++"		; 3 bytes long
_XBEE_ATCN_STR	bytetable	"ATCN",13	; 5 byte

;==============================================================================
;==============================================================================
;==============================================================================
; Timer Based Serial Input and Output Functions.
;==============================================================================
;------------------------------------------------------------------------------
; [TASerout(pin, baudmode, buffer, bufferlen]
;		pin - IO Pin Number - will be set to Output mode
;		Baudmode -	only support bit rate and Normal/Inversion... ie I38400
;		buffer - Pointer variable 
;		bufferlen - Number of bytes to output
;	mystar var byte(4)
;	gosub TASerout(p15, i9600, @MYSTR, 4]
;------------------------------------------------------------------------------

;------------------------------------------------------------------------------
; [TASerin(pin, fPin, baudmode, buffer, bufferlen, timeout, bEOL]
;		pin - IO Pin to read from.  Will be set to input mode
;		fPin - Optional (Not 0) - will turn on flow control.  Will also try to
;			handle cases like XBEE that send extra bytes after the flow pin
;			goes high.  BUGBUG: since 0 is valid should change to maybe -1
;			Pin will be left in output mode
;		BaudMode - Like TaSerout
;		Buffer - pointer to buffer to receive bytes
;		Bufferlen - Max number of bytes to receive
;		Timeout - Max time to wait without timing out (Need to specify value..)
;		wEol - If high byte is zero, we will use the low byte to match EOL character.
;			example usage is <CR> for reading in command lines.
;	returns: count of bytes actually read (0 if timeout)
;
;	gosub TASerin[14, 8, @bBuf, 80, 20000, 13], cbRead
;------------------------------------------------------------------------------
