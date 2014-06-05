XBEEONHSERIAL con 1
CXBEE_RTS		con	p34
DEBUG con 1
'DEBUG_OUT con 1
'DEBUG_VERBOSE con 1

;==============================================================================
;Description: Simple Test program that tries to test some of the XBEE and HSerial
;			stuff...
;
;KNOWN BUGS: 
;   - Probably Alot ;) 
;==============================================================================
DEBUG_WITH_LEDS con 1
fPacketChanged		var bit			; Something change in the packet
bPacketPrev			var	byte(XBEEPACKETSIZE)
fPrevPacketValid	var	bit
i					var	byte

atmy				var	word
atdh				var	long
atdl				var	long
atvr				var	long
atsh				var	long
atsl				var	long
atni				var byte(21)
cbRead				var	byte


wXbeeDL	var	word

	sethserial1 H38400
	enable
	gosub InitTimer				; Initialize our timer (Timer A)
	gosub InitXBee

;	hservo [p0\0, p1\0]
	pause 1000
	Sound P22,[100\4400]

	; Now try seeing what information we can retrieve.
	; now lets get the data to send back
	gosub APIGetXBeeHexVal["S","H"], atsh		; get the serial high, enter command, don't exit
	gosub APIGetXBeeHexVal["S","L"], atsl		; get the serial low, don't enter or leave
	gosub APIGetXBeeHexVal["M","Y"], atmy		; get My value
	gosub APIGetXBeeHexVal["D","L"], atdl		; get destination address
	gosub APIGetXBeeHexVal["D","H"], atdh		; get
	gosub APIGetXBeeHexVal["V","R"], atvr		; get the version number
	gosub APIGetXBeeStringVal["N","I", @atni, 21], cbRead ; 
	
	
	; Now lets output this to the debug terminal
	gosub XbeeRTSAllowInput[0]
	disable TIMERINT 	;disable timer interrupt
	HSEROUT 1, [13,13, "******** XBee Test Program ********", 13, |
		"MY: ", hex atmy, 13, |
		"SH/L: ", hex atsh, " ", hex atsl, 13, |
		"Dest: ", hex atdh, " ", hex atdl, 13, |
		"Ver: ", hex atvr, 13]

	HSEROUT 1,["NI: "]
	for i = 0 to 20
		HSEROUT 1, [" ", hex2 atni(i)]
	next
	HSEROUT 1,[": ", str atni\21\0]
	HSEROUT 1, [13]
	


	enable TIMERINT 	;disable timer interrupt
	gosub XbeeRTSAllowInput[1]
	
'	gosub GetXBeeDL[],wXbeeDL
'	HSEROUT 1, ["Get XBee Dest ", hex wXbeeDL, 13]

	pause 50

main:
	gosub ReceiveXBeePacket
	; See if we have a valid packet to process
	if fPacketValid  then
		fPacketChanged = 0	; don't need to worry about slop 
		for i = 0 to XBEEPACKETSIZE-1
			if (bPacket(i) <> bPacketPrev(i)) then
				fPacketChanged = 1
				bPacketPrev(i) = bPacket(i)
			endif
		next
		
		if fPacketChanged  then
			gosub XbeeRTSAllowInput[0]
			disable TIMERINT 	;disable timer interrupt
			HSEROUT 1, [bin8 bPacket(PKT_BTNHI)\8, bin8 bPacket(PKT_BTNLOW)\8, " "]
			for i = 2 to cbPacket-1
				HSEROUT 1, [dec bPacket(i)," "]
			next
			HSEROUT 1,[13] 
			enable TIMERINT 	;disable timer interrupt
			gosub XbeeRTSAllowInput[0]

		endif
	else
		if fPrevPacketValid then
			gosub XbeeRTSAllowInput[0]
			disable TIMERINT 	;disable timer interrupt
			HSEROUT 1, ["Invalid Packet", 13]
			enable TIMERINT 	;disable timer interrupt
			gosub XbeeRTSAllowInput[0]
		endif
	endif
	fPrevPacketValid = fPacketValid
	goto main
