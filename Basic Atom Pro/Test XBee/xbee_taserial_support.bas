;====================================================================
;Kurts Timer Serial and XBee support functions
;
;Description: This support file will be included by several projects 
;in this directory for building a consistent set of XBEE based function
;that can use the modified DIY Remote control.
;
;Hardware setup: DIY XBee
;
;NEW IN V1.0
;	- First Release
;====================================================================

;[DEBUG]
'DEBUG_OUT con 1
'DEBUG_VERBOSE con 1
'DEBUG_ENTERLEAVE con 1
'DEBUG_DUMP con 1
'DEBUG_WITH_LEDS con 1


#ifdef USEXBEE
#ifndef USETIMER
USETIMER con 1
#endif


#endif

#ifndef cSound
cSound con 9
#endif

;====================================================================
; [Private] - Things that will be used only inside this file
;====================================================================
;[Timer]
#ifdef USETIMER
lTimerCnt				var	LONG	; used now also in timing of how long since we received a message
#endif

;--------------------------------------------------------------------
;[DIY XBee Controller Variables]
#ifdef USEXBEE
_bPacketNum			var	byte		; A packet number...
_bTemp				var	byte(10)	; for reading in different strings...
_bChkSumIn			var	byte
_lCurrentTimeT		var	long		; 
_lTimerLastPacket	var	long		; the timer value when we received the last packet
_lTimerLastRequest	var	long		; what was the timer when we last requested data?
_lTimeDiffMS		var long		; calculated time difference

_fPacketValidPrev	var	bit			; The previous valid...
_fReqDataPacketSent	var	bit			; has a request been sent since we last received one?
_fReqDataForced		var	bit			; Was our request forced by a timeout?
_tas_i				var	byte
_tas_b				var	byte		; 
_cbRead				var	byte		; how many bytes did we read?

' XBee API Defines
_bAPISeqNum			var	byte		; We use as a sequence number to verify which packet we are receiving...
_wAPIDL				var	word		; this is the current DL we pass to apis...
_b					var	byte
_bAPI_i				var	byte		; 

;----------------- Other variables --------------------------------------------


; These pins are defined if they were not previously defined.  This allows projects
; to redefine them.  Be careful with RTS if using sparkfun regulated explroer as
; the RTS line is not voltage shifted on these adapters.

#ifndef cXBEE_IN
cXBEE_IN				con	p14		; The input pin
#endif

#ifndef cXBEE_OUT
cXBEE_OUT				con	p15		; the output pin
#endif

#ifndef CXBEE_BAUD
CXBEE_BAUD				con H38400				; Non-standard baud rate for xbee but...
#endif	; CXBEE_BAUD

; Also define some timeouts.  Allow users to override

#ifndef CXBEEPACKETTIMEOUTMS
CXBEEPACKETTIMEOUTMS con 500					; how long to wait for packet after we send request
#endif

#ifndef CXBEEFORCEREQMS	
CXBEEFORCEREQMS		con	1000					; if nothing in 1 second force a request...
#endif

#ifndef CXBEETIMEOUTRECVMS
CXBEETIMEOUTRECVMS	con	2000					; 2 seconds if we receive nothing
#endif
;==============================================================================
; If XBEE is on HSERIAL, then define some more stuff...
; Build 31 of studio and beyond will allow you to define HSERIAL2 functions on
; the bap 28 and it will be the same as HSERIAL.
;==============================================================================
HSERSTAT_CLEAR_INPUT	con 0 			;Clear input buffer
HSERSTAT_CLEAR_OUTPUT	con	1 			;Clear output buffer
HSERSTAT_CLEAR_BOTH		con	2 			;Clear both buffers
HSERSTAT_INPUT_DATA		con	3 			;If input data is available go to label
HSERSTAT_INPUT_EMPTY	con	4 			;If input data is not available go to label
HSERSTAT_OUTPUT_DATA	con	5 			;If output data is being sent go to label
HSERSTAT_OUTPUT_EMPTY	con	6 			;If output data is not being sent go to label
bHSerinHasData			var	byte		; 

#endif ; USE_XBEE


; TASerial definitions
_TASI_cFO	var	byte			; how many bytes have we cached away in our serial buffer

;==============================================================================
; Timer - Support
;==============================================================================
;==============================================================================
; [InitTimer] - Initialize the timer for our timer tics and the like
;==============================================================================
#ifdef USETIMER

InitTimer:
#ifdef DEBUG_OUT
#ifdef BASICATOMPRO28
	serout s_out, i9600, ["Init Timer", 13]
#else
	hserout["Init Timer", 13]
#endif	
#endif	
	; 
	; Timer A init, used for timing of messages and some times for timing code...
#ifdef BASICATOMPRO28
	TMA = 0	; clock / 8192					; Low resolution clock - used for timeouts...
	lTimerCnt = 0
	ONASMINTERRUPT TIMERAINT, HANDLE_TIMERA_ASM 
	ENABLE TIMERAINT
#else
	TMB1 = 0	; clock / 8192					; Low resolution clock - used for timeouts...
	lTimerCnt = 0
	ONASMINTERRUPT TIMERB1INT, HANDLE_TIMERB1_ASM 
	ENABLE TIMERB1INT

#endif
	return
;==============================================================================
;[Handle_Timer_asm] - Handle timer A overlfow in assembly language.  Currently only
;used for timings for debuging the speed of the code
;Now used to time how long since we received a message from the remote.
;this is important when we are in the NEW message mode, as we could be hung
;out with the robot walking and no new commands coming in.
;==============================================================================

#ifdef BASICATOMPRO28
   BEGINASMSUB 
HANDLE_TIMERA_ASM 
	push.l 	er1                  ; first save away ER1 as we will mess with it. 
	bclr 	#6,@IRR1:8               ; clear the cooresponding bit in the interrupt pending mask 
	mov.l 	@LTIMERCNT:16,er1      ; Add 256 to our counter 
	add.l	#256,er1 
	mov.l 	er1, @LTIMERCNT:16 
	pop.l 	er1 
	rte 
	ENDASMSUB 
#else
   BEGINASMSUB 
HANDLE_TIMERB1_ASM 
	push.l 	er1                  ; first save away ER1 as we will mess with it. 
	bclr 	#5,@IRR2:8           ; clear the cooresponding bit in the interrupt pending mask 
	mov.l 	@LTIMERCNT:16,er1      ; Add 256 to our counter 
	add.l	#256,er1 
	mov.l 	er1, @LTIMERCNT:16 
	pop.l 	er1 
	rte 
	ENDASMSUB 
#endif

;--------------------------------------------------------------------
;[GetCurrentTime] - Gets the Timer value from our overflow counter as well as the TCA counter.  It
;                makes sure of consistancy. That is it is very posible that 
;                after we grabed the timers value it overflows, before we grab the other part
;                so we check to make sure it is correct and if necesary regrab things.
;==============================================================================
	return		; Put a basic statement before...
lCurrentTime			var	long
	
GetCurrentTime:
#ifdef BASICATOMPRO28
	lCurrentTime = lTimerCnt + TCA
	
	; handle wrap
	if lTimerCnt <> (lCurrentTime & 0xffffff00) then
		lCurrentTime = lTimerCnt + TCA
	endif

	return lCurrentTime
#else
	lCurrentTime = lTimerCnt + TCB1
	
	; handle wrap
	if lTimerCnt <> (lCurrentTime & 0xffffff00) then
		lCurrentTime = lTimerCnt + TCB1
	endif

	return lCurrentTime

#endif
	
#endif ; USETIMER

;==============================================================================
; XBEE - Support
;==============================================================================
#ifdef USEXBEE

;==============================================================================
; [WaitHSeroutComplete] - This simple helper function waits until the HSersial
; 							output buffer is empty before continuing...
; BUGBUG: Rolling our own as the hserstat reset the processor
;==============================================================================
WaitHseroutComplete:
#ifdef DEBUG_ENTERLEAVE
#ifdef BASICATOMPRO28
	serout s_out, i9600, ["Enter: WaitHseroutComplete", 13]
#else
	hserout ["Enter: WaitHseroutComplete", 13]
#endif	
#endif
	nop						; transisition to assembly language.
_WTC_LOOP:	
#ifdef BASICATOMPRO28
	mov.b	@_HSEROUTSTART,r0l
	mov.b	@_HSEROUTEND,r0h
#else	; BAP40 or Arc32...	
	mov.b	@_HSEROUT2START,r0l
	mov.b	@_HSEROUT2END,r0h
#endif
	cmp.b	r0l,r0h
	bne		_WTC_LOOP:8

#ifdef DEBUG_ENTERLEAVE
#ifdef BASICATOMPRO28
	serout s_out, i9600, ["Exit: WaitHseroutComplete", 13]
#else
	hserout ["Exit: WaitHseroutComplete", 13]
#endif	
#endif	

	;hserstat 5, WaitTransmitComplete				; wait for all output to go out...	
	return


;==============================================================================
; [InitXbee] - Initialize the XBEE for use with DIY support
;==============================================================================
InitXBee:
#ifdef DEBUG_ENTERLEAVE
#ifdef BASICATOMPRO28
	serout s_out, i9600, ["Enter: Init XBEE", 13]
#else	
	hserout ["Enter: Init XBEE", 13]
#endif	
#endif
	pause 20							; have to have a guard time to get into Command sequence
#ifdef BASICATOMPRO28
	sethserial1 cXBEE_BAUD
	HSP_XBEE	con 	1
#else
	sethserial2 cXBEE_BAUD
	HSP_XBEE	con 	2
#endif
	
#ifdef cXBEE_RTS			; RTS line optional for us with HSERIAL 
	hserout HSP_XBEE, [str _XBEE_PPP_STR\3]
	gosub WaitHSeroutComplete
	pause 20							; have to wait a bit again
	hserout HSP_XBEE, [	"ATD6 1",13,|			; turn on flow control
				str _XBEE_ATCN_STR\5]				; exit command mode
	low cXBEE_RTS						; make sure RTS is output and input starts off enabled...

#endif ; cXBEE_RTS
	pause 20							; have to have a guard time to get into Command sequence
	; will start off trying to make sure we are in api mode...
	hserout HSP_XBEE, [str _XBEE_PPP_STR\3]
	gosub WaitHSeroutComplete
	pause 20							; have to wait a bit again
	hserout HSP_XBEE, [	"ATAP 1",13,|			; turn on flow control
				str _XBEE_ATCN_STR\5]				; exit command mode

	pause 10	; need to wait to exit command mode...

	cPacketTimeouts = 0
	fTransReadyRecvd = 0
	_fPacketValidPrev = 0
	fPacketEnterSSCMode = 0   
	_fReqDataPacketSent = 0
	_fReqDataForced = 0			;
    gosub ClearInputBuffer		; make sure we toss everything out of our buffer and init the RTS pin

#ifdef DEBUG_ENTERLEAVE
#ifdef BASICATOMPRO28
	serout s_out, i9600, ["Exit: Init Controller", 13]
#else
	hserout["Exit: Init Controller", 13]
#endif
#endif	
 
return

;==============================================================================
; [XBeeOutputVal[bXbeeVal]] - Output a value back to the device.  Works for XBee
; 		by sending Byte value back to the DIY remote to display.
; Parmameters:
;		bXbeeVal - Value to send back
;==============================================================================
wXbeeVal	var	word
XBeeOutputVal[wXbeeVal]:
	gosub SendXBeePacket[XBEE_RECV_DISP_VAL, 2, @wXbeeVal]			
return

;==============================================================================
; [XBeeOutputString] - Output a string back to the device.  Works for XBee
; 		by outputing the string back to the DIY remote to display.
; Parmameters:
;		pString - Pointer to string.  First byte contains the length
;==============================================================================
pString	var	pointer
XBeeOutputString[pString]:
	; BUGBUG:: Will not work for zero count, but what the heck.
	; First we need to get the count a compute the checksum - for the heck of it will use assembly...
	mov.l	@PSTRING:16, er0		; get the pointer value into ero
	mov.b	@er0+, r1l				; Get the count into R1l - start of the checksum
	mov.l	er0, @PSTRING:16		; Update pointer value to be after the count byte.
	mov.b	r1l, @_TAS_B:16			; save away count to pass to the serial output...

	gosub SendXBeePacket[XBEE_RECV_DISP_STR, _tas_b, pString]		; Send Data to remote (CmdType, ChkSum, Packet Number, CB extra data)


#ifdef DEBUG_OUT
#ifdef BASICATOMPRO28
	serout s_out, i9600, ["XOS: ", dec _tas_b, ":", str @pString\_tas_b, 13]
#else
	hserout["XOS: ", dec _tas_b, ":", str @pString\_tas_b, 13]
#endif	
#endif

return

;==============================================================================
; [XBeePlaySounds] - Sends the buffer of souncs back to the remote to play
; Parmameters:
;		pSnds - Pointer to the sounds
;		cbSnds - Size of the buffer
;==============================================================================
; Too bad I don't have real macros to save space...
pSnds 	var pointer
cbSnds	var	byte
XBeePlaySounds[pSnds, cbSnds]:
	gosub SendXBeePacket[XBEE_PLAY_SOUND, cbSnds, pSnds]		; Send Data to remote (CmdType, ChkSum, Packet Number, CB extra data)
	return
	
;==============================================================================
; [XbeeRTSAllowInput] - This function enables or disables the RTS line for the XBEE
; This is only used for the XBEE on HSERIAL mode as the TASerial has the RTS stuff
; built in and we don't want to throw away characters...
;==============================================================================
fRTSEnable	var	byte
XbeeRTSAllowInput[fRTSEnable]
#ifdef cXBEE_RTS
	if fRTSEnable then
		low cXBEE_RTS
	else
		high cXBEE_RTS
	endif
#endif
return

;==============================================================================
;==============================================================================
;==============================================================================

;==============================================================================
; [APISetXbeeHexVal] - Set one of the XBee Hex value registers
;==============================================================================
_c1		var		byte
_c2		var		byte
_lval	var		long
APISetXBeeHexVal[_c1, _c2, _lval]:
	' We are going to reuse _bAPIPacket to generate the packet...
	_bAPIPacket(0) = 0x7E
	_bAPIPacket(1) = 0
	_bAPIPacket(2) = 8      			' this is the length LSB
	_bAPIPacket(3) = 8      			' CMD=8 which is AT command
	_bAPISEQnum = _bAPISEQnum + 1 		' keep a running sequence number so we can see which ack we are processing		
	if _bAPISEQnum = 0 then				' Don't have sequence number = 0 as this causes no ACK...
		_bAPISEQnum = 1
	endif
	_bAPIPacket(4) = _bAPISeqnum     	' could be anything here
	_bAPIPacket(5) = _c1
	_bAPIPacket(6) = _c2
	_bAPIPacket(7) = _lval >> 24
	_bAPIPacket(8) = (_lval >> 16) & 0xFF
	_bAPIPacket(9) = (_lval >> 8) & 0xFF
	_bAPIPacket(10) = _lval & 0xFF
	_b = 0
    
    For _bAPI_i = 3 To 10  ' don't include the frame delimter or length in count - likewise not the checksum itself...
        _b = _b + _bAPIPacket(_bAPI_i)
    Next
    _bAPIPacket(11) = 0xFF - _b
        
    ' now write out the data to the hardware serial port
    hserout HSP_XBEE,[str _bAPIPacket\12]

	' BUGBUG:: Should we wait for and process the response?
	return

;==============================================================================
; Simple wrapper functions for setting My and DL
;==============================================================================
SetXBeeMy[_lval]				
	gosub APISetXBeeHexVal["M","Y", _lval]
	return
	
SetXBeeDL[_lval]			
	gosub APISetXBeeHexVal["D","L", _lval]
	_wAPIDL = _lval
	return
;==============================================================================
; [APISendXbeeGetCmd - Helper function that send the actual get command.
;		common code for get hex val, get string val and Get node list.
;==============================================================================
APISendXbeeGetCmd[_c1, _c2]:
	_bAPIPacket(0) = 0x7E
	_bAPIPacket(1) = 0
	_bAPIPacket(2) = 4      			' this is the length LSB
	_bAPIPacket(3) = 8      			' CMD=8 which is AT command
	_bAPISEQnum = _bAPISEQnum + 1 		' keep a running sequence number so we can see which ack we are processing		
	if _bAPISEQnum = 0 then				' Don't have sequence number = 0 as this causes no ACK...
		_bAPISEQnum = 1
	endif
	_bAPIPacket(4) = _bAPISeqnum     	' could be anything here
	_bAPIPacket(5) = _c1
	_bAPIPacket(6) = _c2
	_b = 0
    For _bAPI_i = 3 To 6  ' don't include the frame delimter or length in count - likewise not the checksum itself...
        _b = _b + _bAPIPacket(_bAPI_i)
    Next
    _bAPIPacket(7) = 0xFF - (_b & 0xFF)
    
    ' now write out the data to the hardware serial port
    hserout HSP_XBEE,[str _bAPIPacket\8]
	gosub WaitHSeroutComplete
	return

;==============================================================================
; [APIGetXbeeHexVal] - Set the XBee DL to the specified word that is passed
;==============================================================================
APIGetXBeeHexVal[_c1, _c2]:
	gosub APISendXbeeGetCmd[_c1, _c2]
	gosub WaitHseroutComplete
	do 
		gosub APIRecvPacket[100000], _bAPIRecvRet
		
		if _bAPIRecvRet then
#ifdef DEBUG_OUT
#ifdef BASICATOMPRO28
			serout s_out, i9600, ["AGHex: ", hex  _bAPIRecvRet, "-",hex _bAPIPacket(0), " ", hex _bAPIPacket(1), " ", hex _bAPIPacket(2), |
 				" ", hex _bAPIPacket(3), " ", hex _bAPIPacket(4), " ", hex _bAPIPacket(5), |
				" ", hex _bAPIPacket(6), " ", hex _bAPIPacket(7), " ", hex _bAPIPacket(8), 13]
#else
			hserout ["AGHex: ", hex  _bAPIRecvRet, "-",hex _bAPIPacket(0), " ", hex _bAPIPacket(1), " ", hex _bAPIPacket(2), |
 				" ", hex _bAPIPacket(3), " ", hex _bAPIPacket(4), " ", hex _bAPIPacket(5), |
				" ", hex _bAPIPacket(6), " ", hex _bAPIPacket(7), " ", hex _bAPIPacket(8), 13]
#endif				
#endif

			if (_bAPIPacket(0) = 0x88) and (_bAPIPacket(1) = _bAPISEQnum) and (_bAPIPacket(4) = 0) then
				if _bAPIRecvRet = 7 then ; only a word returned...
					return (_bAPIPacket(5) << 8) +  _bAPIPacket(6) 
				else
					return (_bAPIPacket(5) << 24) + (_bAPIPacket(6) << 16) + (_bAPIPacket(7) << 8) + _bAPIPacket(8) 
				endif
			endif
		endif	
	while (_bAPIRecvRet)
	
	return -1 ; some form of error


;==============================================================================
; [APIGetXBeeStringVal - Retrieve a string value back from the XBEE 
;		- Will return TRUE if it receives something, else false
;==============================================================================
_pbSVal			var	pointer
_cbSVal			var	byte
APIGetXBeeStringVal[_c1, _c2, _pbSVal, _cbSVal]:

	gosub APISendXbeeGetCmd[_c1, _c2]
	gosub WaitHseroutComplete
	do 
		gosub APIRecvPacket[100000], _bAPIRecvRet
		
#ifdef DEBUG_OUT
#ifdef BASICATOMPRO28
		serout s_out, i9600, ["AGStr: ", hex  _bAPIRecvRet, "-",hex _bAPIPacket(0), " ", hex _bAPIPacket(1), " ", hex _bAPIPacket(2), |
 				" ", hex _bAPIPacket(3), " ", hex _bAPIPacket(4), " ", hex _bAPIPacket(5), |
				" ", hex _bAPIPacket(6), " ", hex _bAPIPacket(7), " ", hex _bAPIPacket(8), 13]
#else
		hserout["AGStr: ", hex  _bAPIRecvRet, "-",hex _bAPIPacket(0), " ", hex _bAPIPacket(1), " ", hex _bAPIPacket(2), |
 				" ", hex _bAPIPacket(3), " ", hex _bAPIPacket(4), " ", hex _bAPIPacket(5), |
				" ", hex _bAPIPacket(6), " ", hex _bAPIPacket(7), " ", hex _bAPIPacket(8), 13]

#endif				
#endif
		if _bAPIRecvRet then
			if (_bAPIPacket(0) = 0x88) and (_bAPIPacket(1) = _bAPISEQnum) and (_bAPIPacket(4) = 0) then
				; The return value from the recv is the total packet length, we can use this to know how many
				; bytes were in the string.  (count - 5 )
				; Need to see if we read all of the value or not, ie did we get a CR
				; 
				_bAPIRecvRet = (_bAPIRecvRet - 5) max _cbSVal
				for _cbSVal = 0 to _bAPIRecvRet - 1
					@_pbSVal = _bAPIPacket(_cbSVal + 5)
					_pbSVal = _pbSVal + 1
				next
			endif
			return _bAPIRecvRet
		endif	
	while (_bAPIRecvRet)
	
	return -1 ; some form of error

;==============================================================================
; [APIRecvPacket - try to receive a packet from the XBee. 
;		- Will return TRUE if it receives something, else false
;		- Pass in buffer to receive packet.  Assumed it is big enough...
;		- pass in timeout if zero will return if no data...
;		
;==============================================================================
_fapiWaitTimeout	var	long
APIRecvPacket[_fapiWaitTimeout]:
'	toggle p4
	' First see if the user wants us to wait for input or not
	;	hserstat HSERSTAT_INPUT_EMPTY, _TP_Timeout			; if no input available quickly jump out.
	; Well Hserstat is failing, try rolling our own.
	if not _fapiWaitTimeout then
		
#ifdef BASICATOMPRO28
		mov.b	@_HSERINSTART, r0l
		mov.b	@_HSERINEND, r0h
#else
		mov.b	@_HSERIN2START, r0l
		mov.b	@_HSERIN2END, r0h
#endif
		sub.b	r0h, r0l
		mov.b	r0l, @BHSERINHASDATA
		if	(not bHSerinHasData) then
			return 0
		endif
		_fapiWaitTimeout = 20000		; default when 0 passed in...
	endif	


	' now lets try to read in the header part of the packet.  Note, we will provide some
	' timeout for this as we don't want to completely hang if something goes wrong!
	' Lets try to make sure we sync up on the right header character
	' will not get out of the loop until either we time out or we get an appropriate start
	' character for a packet
	repeat
		hserin HSP_XBEE,  _ARP_TO, _fapiWaitTimeout, [_bAPIStartDelim]
	until _bAPIStartDelim = 0x7E
	
	' Now lets get the packet length
	hserin HSP_XBEE, _ARP_TO, _fapiWaitTimeout, [_wAPIPacketLen.highbyte, _wAPIPacketLen.lowbyte]

	' Lets do a little verify that the packet looks somewhat valid.
	' Bail if the packet is bigger than we can receive...
	' With text messages may need to increase this size
	if (_wAPIPacketLen > APIPACKETMAXSIZE) then
'		toggle p6
'		toggle p4
		return 0
	endif

	' Then read in the packet including the checksum.
	hserin HSP_XBEE, _ARP_TO, _fapiWaitTimeout, [str _bAPIPacket\_wAPIPacketLen, _bChkSumIn]
	

	' Now lets verify the checksum.
	_b = 0
	for _bAPI_i = 0 to _wAPIPacketLen-1
		_b = _b + _bAPIPacket(_bAPI_i) 			; Add that byte to the buffer...
	next

	if _bChkSumIn <> (0xff - _b) then
'		toggle P6
'		toggle p4
		return 0		' checksum was off
	endif

'	toggle p4
	return _wAPIPacketLen 	' return the packet length as the caller may need to know this...
	
_ARP_TO:
'	toggle p5
'	toggle p4
	return 0  ; we had a timeout




;==============================================================================
; [SetXbeeDL] - Set the XBee DL to the specified word that is passed
; BUGBUG: sample function, need to clean up.
;==============================================================================

wNewDL	var	word

;==============================================================================
; [GetXbeeDL] - Retrieve the XBee DL and return it as a Word
;==============================================================================
GetXBeeDL:
	gosub APIGetXBeeHexVal["D","L",0x3], _wAPIDL
	
	return _wAPIDL


;==============================================================================
; [SendXBeePacket] - Simple helper function to send the 4 byte packet header
;	 plus the extra data if any
; 	 gosub SendXBeePacket[bPacketType, cbExtra, pExtra]
;==============================================================================

#ifdef DEBUG_OUT
_pbDump var pointer
#endif

_bPCBExtra	var	byte
_pbIN		var	pointer		; pointer to data to retrieve
_bPHType 	var _bAPIPacket(8)
SendXbeePacket[_bPHType, _bPCBExtra, _pbIN]:

	' We are going to reuse _bAPIPacket to generate the packet...
	' will be using 15 bit addressing to talk to destination.
	_bAPIPacket(0) = 0x7E
	_bAPIPacket(1) = 0
	_bAPIPacket(2) = 5 + XBEE_API_PH_SIZE+_bPCBExtra 		' this is the length LSB
	_bAPIPacket(3) = 1      			' CMD=1 which is TX with 16 bit addressing
	_bAPISEQnum = _bAPISEQnum + 1 		' keep a running sequence number so we can see which ack we are processing		
	if _bAPISEQnum = 0 then				' Don't have sequence number = 0 as this causes no ACK...
		_bAPISEQnum = 1
	endif
	_bAPIPacket(4) = _bAPISeqnum    
	_bAPIPacket(5) = _wAPIDL.highbyte	' set the destination DL in the message
	_bAPIPacket(6) = _wAPIDL.lowbyte
	_bAPIPacket(7) = 0					' normal type of message
	' So our data starts at offset 8 in this structure.  Use this in handling the parameters passed...
	' bytes 8-9 were set by the parameter handling in basic

	' We could have copied the extra data to our api buffer or we could simply
	' handle it on our write to the xbee, first pass do on write.
	' Need to first compute the XBee checksum
	
	_b = 0
    For _bAPI_i = 3 To 7 + XBEE_API_PH_SIZE  ' Compute the checksum for all of the standard parts of the packet header.
        _b = _b + _bAPIPacket(_bAPI_i)
    Next

	' Output the packet - easy if no extra data
	
	if not _bPCBExtra then
		hserout HSP_XBEE, [str _bAPIPacket\8 + XBEE_API_PH_SIZE, 0xff - _b]							; Real simple message 
	else
		' Now see if we need to add the extra bytes on 
		; need to finish checksum - could do in basic but need to verify pointer...
		mov.b	@_B:16, r1l				; get the checkum already calculated for header
		mov.b	@_BPCBEXTRA:16, r1h		; get the count of bytes 
		mov.l	@_PBIN, er0				; get the pointer to extra data
_SXBP_CHKSUM_LOOP:
		mov.b	@er0+, r2l				; get the next character
		add.b	r2l,  r1l				; add on to r0l for checksum
		dec.b	r1h						; decrement	our counter
		bne		_SXBP_CHKSUM_LOOP:8		; not done yet.  
		mov.b	r1l, @_B:16				; save away checksum for basic to use
    
		hserout HSP_XBEE, [str _bAPIPacket\8 + XBEE_API_PH_SIZE, str @_pbIN\_bPCBExtra, 0xff - _b]		; Real simple message 
	endif

#ifdef DEBUG_OUT
	; We moved dump before the serout as hserout will cause lots of interrupts which will screw up our serial output...
	; Moved after as we want the other side to get it as quick as possible...
#ifdef BASICATOMPRO28	
	serout s_out, i9600, [ "SDP:", hex _bAPIPacket(0)\2, hex _bAPIPacket(1)\2, hex _bAPIPacket(2)\2, hex _bAPIPacket(3)\2, hex _bAPIPacket(4)\2, hex _bAPIPacket(5)\2, hex _bAPIPacket(6)\2, hex _bAPIPacket(7)\2, |
		" ", hex _bAPIPacket(8)\2, " : "]
#else
	hserout [ "SDP:", hex _bAPIPacket(0)\2, hex _bAPIPacket(1)\2, hex _bAPIPacket(2)\2, hex _bAPIPacket(3)\2, hex _bAPIPacket(4)\2, hex _bAPIPacket(5)\2, hex _bAPIPacket(6)\2, hex _bAPIPacket(7)\2, |
		" ", hex _bAPIPacket(8)\2, " : "]
#endif		
#ifdef DEBUG_VERBOSE		; Only ouput whole thing if verbose...
	_pbDump = _pbIN
	_pbDump.highword = 0x2	; BUGBUG: force for pointer to bytes
	if _bPCBExtra then
		for _tas_i = 0 to _bPCBExtra -1
#ifdef BASICATOMPRO28	
			serout s_out, i9600,  [hex @_pbDump\2]
#else
			hserout [hex @_pbDump\2]
#endif			
			_pbDump = _pbDump + 1
		next
	endif
#endif	
#ifdef BASICATOMPRO28	
		serout s_out, i9600,  [" - ", hex 0xff-_b\2, 13]
#else
		hserout [" - ", hex 0xff-_b\2, 13]
#endif			
#endif

	
	return


;==============================================================================
; [ClearInputBuffer] - This simple helper function will clear out the input
;						buffer from the XBEE
;==============================================================================
ClearInputBuffer:

; 	warning this function does not handle RTS for HSERIAL
; 	assumes that it has been setup properly before it got here.
_CIB_LOOP:	
	hserin HSP_XBEE, _CIB_TO, 1000, [_tas_b]
	goto _CIB_LOOP
	
_CIB_TO:
	return

#ifndef DIY_TRANSMITTER
;--------------------------------------------------------------------
;[XBeeResetPacketTimeout] - This function resets the save timer value that is used to 
;				 decide if our XBEE has timed out It could/should be simplay
;				 a call to GetCurrentTime, but try to save a little time of
;				 not nesting the calls...
;==============================================================================
	
XBeeResetPacketTimeout:
#ifdef BASICATOMPRO28
	_lTimerLastPacket = lTimerCnt + TCA
	
	; handle wrap
	if lTimerCnt <> (_lTimerLastPacket & 0xffffff00) then
		_lTimerLastPacket = lTimerCnt + TCA
	endif
#else
	_lTimerLastPacket = lTimerCnt + TCB1
	
	; handle wrap
	if lTimerCnt <> (_lTimerLastPacket & 0xffffff00) then
		_lTimerLastPacket = lTimerCnt + TCB1
	endif
#endif
	return



;==============================================================================
; [ReceiveXBeePacket] - This function will try to receive a packet of information
; 		from the remote control over XBee.
;
; the data in a standard packet is arranged in the following byte order:
;	0 - Buttons High
;	1 - Buttons Low
; 	2 - Right Joystick L/R
;	3 - Right Joystick U/D
;	4 - Left Joystick L/R
;	5 - Left Joystick U/D
; 	6 - Right Slider
;	7 - Left Slider
;==============================================================================
bDataOffset	var	byte
_alNIPD		var long(7)		; 2 for SL+H + 20 bytes for NI data...
_lSNH		var _alNIPD(0)	; Serial Number High
_lSNL		var _alNIPD(1)	; Serial number low.
_bNI		var	_alNIPD(2)	; String to read NI into...
_pbNI		var	pointer		; need something to point as a byte pointer

ReceiveXBeePacket:
#ifdef DEBUG_ENTERLEAVE
#ifdef BASICATOMPRO28
	serout s_out, i9600, ["E: ReceiveXbeePacket", 13]
#else
	hserout ["E: ReceiveXbeePacket", 13]
#endif
#endif	
	_fPacketValidPrev = fPacketValid		; Save away the previous state as this is the state if no new data...
	fPacketValid = 0
	fPacketTimeOut = 0
	fPacketEnterSSCMode = 0   
	;	We will first see if we have a packet header waiting for us.
#ifdef CXBEE_RTS
	low cXBEE_RTS		; Ok enable input from the XBEE - wont turn off by default
	; bugbug should maybe check to see if it was high first and if
	; so maybe bypass the next check...
#endif	
_RXP_TRY_RECV_AGAIN:
	gosub APIRecvPacket[0], _bAPIRecvRet
	
	if  not _bAPIRecvRet then _RXP_CHECKFORHEADER_TO	; I hate gotos...

	' We received an XBee Packet, See what type it is.
	' first see if it is a RX 16 bit or 64 bit packet?
	If _bAPIPacket(0) = 0x81 Then
		' 16 bit address sent, so there is 5 bytes of packet header before our data
		bDataOffset = 5
	ElseIf _bAPIPacket(0) = 0x80
		' 64 bit address so our data starts at offset 11...
		bDataOffset = 11
		
	ElseIf _bAPIPacket(0) = 0x89
		' this is an A TX Status message - May check status and maybe update something?
		goto _RXP_TRY_RECV_AGAIN
		
	Else
#ifdef DEBUG_OUT
#ifdef BASICATOMPRO28
		serout s_out, i9600, ["Unknown XBEE Packet: ", hex _bAPIPacket(0), " L:", hex _bAPIPacket(1) >> 8 + _bAPIPacket(2), 13]
#else
		hserout ["Unknown XBEE Packet: ", hex _bAPIPacket(0), " L:", hex _bAPIPacket(1) >> 8 + _bAPIPacket(2), 13]
#endif
#endif
		goto _RXP_CHECKFORHEADER_TO
	EndIf

#ifdef DEBUG_VERBOSE
#ifdef BASICATOMPRO28
	serout s_out, i9600, ["RCV XBEE Packet: ", hex _bAPIPacket(0), " From:", hex _bAPIPacket(1), hex _bAPIPacket(2), |
				": ", hex _bAPIPacket(bDataOffset)," ",  hex _bAPIPacket(bDataOffset+1)," ",hex _bAPIPacket(bDataOffset+2)," ",hex _bAPIPacket(bDataOffset+3)," ",|
				hex _bAPIPacket(bDataOffset+4)," ",hex _bAPIPacket(bDataOffset+5)," ",hex _bAPIPacket(bDataOffset+6)," ",hex _bAPIPacket(bDataOffset+7),13]
#else
	hserout ["RCV XBEE Packet: ", hex _bAPIPacket(0), " From:", hex _bAPIPacket(1), hex _bAPIPacket(2), |
				": ", hex _bAPIPacket(bDataOffset)," ",  hex _bAPIPacket(bDataOffset+1)," ",hex _bAPIPacket(bDataOffset+2)," ",hex _bAPIPacket(bDataOffset+3)," ",|
				hex _bAPIPacket(bDataOffset+4)," ",hex _bAPIPacket(bDataOffset+5)," ",hex _bAPIPacket(bDataOffset+6)," ",hex _bAPIPacket(bDataOffset+7),13]
#endif
#endif	

	;-----------------------------------------------------------------------------
	; [XBEE_TRANS_DATA]
	;-----------------------------------------------------------------------------
	; process first as higher number of these come in...
	if (_bAPIPacket(bDataOffset + 0) = XBEE_TRANS_DATA) then

		
		if (_bAPIRecvRet-bDataOffset >= (XBEE_API_PH_SIZE + XBEEPACKETSIZE_MIN) ) then
			cbPacket = _bAPIRecvRet-bDataOffset - XBEE_API_PH_SIZE	; remember how many bytes are in packet 
			if cbPacket > XBEEPACKETSIZE then
				cbPacket = XBEEPACKETSIZE
			endif
			for _b = 0 to cbPacket-1
				bPacket(_b) = _bAPIPacket(_b + XBEE_API_PH_SIZE + bDataOffset)
			next

#ifdef DEBUG_VERBOSE
#ifdef BASICATOMPRO28
			serout s_out, i9600, ["P: ", hex bPacket(PKT_BTNLOW)\2, hex bPacket(PKT_BTNHI)\2, hex bPacket(PKT_RJOYLR)\2, hex bPacket(PKT_RJOYUD)\2, |
								     hex bPacket(PKT_LJOYLR)\2, hex bPacket(PKT_LJOYUD)\2, hex bPacket(PKT_RSLIDER)\2, hex bPacket(PKT_LSLIDER)\2, |
								     " CS: ", hex _bChkSumIn, " PN: ", hex _bPacketNum, 13]
#else
			hserout ["P: ", hex bPacket(PKT_BTNLOW)\2, hex bPacket(PKT_BTNHI)\2, hex bPacket(PKT_RJOYLR)\2, hex bPacket(PKT_RJOYUD)\2, |
								     hex bPacket(PKT_LJOYLR)\2, hex bPacket(PKT_LJOYUD)\2, hex bPacket(PKT_RSLIDER)\2, hex bPacket(PKT_LSLIDER)\2, |
								     " CS: ", hex _bChkSumIn, " PN: ", hex _bPacketNum, 13]
#endif
#endif

			fPacketValid = 1	; data is valid
			cPacketTimeouts = 0	; reset when we have a valid packet
			GOSUB XBeeResetPacketTimeout ; sets _lTimerLastPacket
			return	;  		; get out quick!
		else
#ifdef DEBUG_OUT
			;bugbug;;; debug
#ifdef BASICATOMPRO28
			serout s_out, i9600,  ["E Pacekt Size: ", hex _bAPIPacket(bDataOffset + 0)\2,  ":", hex _bChkSumIn, 13]
#else
			hserout ["E Pacekt Size: ", hex _bAPIPacket(bDataOffset + 0)\2,  ":", hex _bChkSumIn, 13]
#endif
#endif
			; the checksum and data not right lets clear our input buffer out...	
;			toggle p6			; BUGBUG - Debug
'			gosub ClearInputBuffer
		endif
	;-----------------------------------------------------------------------------
	; [XBEE_REQ_SN_NI]
	;-----------------------------------------------------------------------------
	elseif (_bAPIPacket(bDataOffset + 0) = XBEE_REQ_SN_NI)
		; The caller must pass through the DL to talk back to, or how else would we???
		; our DL to point to it...
		if (_bAPIRecvRet-bDataOffset = (XBEE_API_PH_SIZE + 2) ) then  ; 4 bytes for the packet header + 2 bytes for the extra data!.
			; we need to read in a new DL for the transmitter...
			_wAPIDL.highbyte = _bAPIPacket(bDataOffset)
			_wAPIDL.lowbyte = _bAPIPacket(bDataOffset+1)
#ifdef DEBUG_OUT
#ifdef BASICATOMPRO28
			serout s_out, i9600,  ["XBEE_REQ_SN_NI:  ", hex _wAPIDL, 13]
#else
			hserout ["XBEE_REQ_SN_NI:  ", hex _wAPIDL, 13]
#endif
#endif
			gosub APISetXBeeHexVal["D","L", _wAPIDL]
			; now lets get the data to send back
			gosub APIGetXBeeStringVal["N","I", @_bNI, 20], _cbRead ; 
			gosub APIGetXBeeHexVal["S","L",0x0], _lSNL		; get the serial low, don't enter or leave
			gosub APIGetXBeeHexVal["S","H",0x2], _lSNH		; get the serial high, 

#ifdef DEBUG_OUT
#ifdef BASICATOMPRO28
			serout s_out, i9600,  ["X._NI:  ", hex _lSNH, " ", hex _lSNL, "(", str _bNI\21\13,")", dec _cbRead, 13]
#else
			hserout ["X._NI:  ", hex _lSNH, " ", hex _lSNL, "(", str _bNI\21\13,")", dec _cbRead, 13]
#endif
#endif
			_pbNI	= @_bNI				; get address
			_pbni.highword = 0x2		; make it a byte pointer
			_pbni = _pbni + _cbRead
			; lets blank fill the name...
			while (_cbRead < 14)
				@_pbNI = " "
				_cbRead = _cbRead + 1
				_pbNI = _pbNI + 1
			wend

			; last but not least try to send the data as a packet.
			gosub SendXBeePacket[XBEE_SEND_SN_NI_DATA, 22, @_lSNH]		; Send Data to remote (CmdType, ChkSum, Packet Number, CB extra data)
		endif
	;-----------------------------------------------------------------------------
	; [UNKNOWN PACKET]
	;-----------------------------------------------------------------------------
	else
#ifdef DEBUG_OUT
#ifdef BASICATOMPRO28
		serout s_out, i9600,  ["Unk Packet", 13]
#else
		hserout  ["Unk Packet", 13]
#endif		
#endif
		'gosub ClearInputBuffer
	endif

;-----------------------------------------------------------------------------
; [See if we need to request data from the other side]
;-----------------------------------------------------------------------------
_RXP_CHECKFORHEADER_TO:

	; Only send when we know the transmitter is ready.  Also if we are in the New data only mode don't ask for data unless we have been told there
	; is new data. We relax this a little and be sure to ask for data every so often as to make sure the remote is still working...
	; 
	GOSUB GetCurrentTime[], _lCurrentTimeT

	; Time in MS since last packet
	_lTimeDiffMS = ((_lCurrentTimeT-_lTimerLastPacket) * WTIMERTICSPERMSMUL) / WTIMERTICSPERMSDIV

	fPacketValid = _fPacketValidPrev	; Say the data is in the same state as the previous call...
#ifdef DEBUG_ENTERLEAVE
#ifdef BASICATOMPRO28
	serout s_out, i9600, ["Exit: ReceiveXbeePacket", 13]
#else
	hserout ["Exit: ReceiveXbeePacket", 13]
#endif		
#endif	

	return


#endif	' DIY_TRANSMITTER
#endif	;' USEXBEE
