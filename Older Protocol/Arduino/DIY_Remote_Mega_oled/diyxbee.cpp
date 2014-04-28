/****************************************************************************
 * - DIY remote control XBee support file
 *
 ****************************************************************************/
//#define DEBUG
//#define DEBUG_OUT
//#define DEBUG_VERBOSE

#include "diyxbee.h"
#include <Streaming.h>


// Add support for running on non-mega Arduino boards as well.
#if not defined(UBRR1H)
#include <NewSoftSerial.h>
    NewSoftSerial XBeeSerial(cXBEE_IN, cXBEE_OUT);
#endif

DIYSTATE g_diystate;
//boolean g_fDebugOutput = true;
#define XBEE_API_PH_SIZE    1            // Changed Packet Header to just 1 byte - Type - we don't use sequence number anyway...
//#define XBEE_NEW_DATA_ONLY 1
// Some forward definitions
#ifdef DEBUG
extern void DebugMemoryDump(const byte* , int, int);
static boolean s_fDisplayedTimeout = false;
#endif

DIYPACKET g_diypLastSent={0,0,0,0,0,0,0,0};        // keep a copy of the last packet we sent so we can calculate delta packets...
#define MAXPACKETBYTEDIFF    4
typedef struct {
    word     wMask;                            // Mask of which bytes have changed
    byte     ab[MAXPACKETBYTEDIFF];            // Values of bytes that changed...
} DIYCHPACKET;
DIYCHPACKET g_diychp;

//=============================================================================
// Xbee stuff
//=============================================================================
void InitXBee(void)
{
    XBeeSerial.begin(38400);    // BUGBUG??? need to see if this baud will work here.
    // Ok lets set the XBEE into API mode...

    delay(20);
    XBeeSerial.print("+++");
    XBeeSerial.flush();
    delay(20);
    XBeeSerial.println("ATAP 1");        // Make sure we are in API mode
    XBeeSerial.println("ATCN");
    delay(10);
    ClearXBeeInputBuffer();    //     XBeeSerial.flush();                    // get rid of anything received from the XBEE

    // for Xbee with no flow control, maybe nothing to do here yet...
    g_diystate.bPacketNum = 0;
}


//=============================================================================
// byte ReadFromXBee - Read in a buffer of bytes.  We will pass in a timeout
//            value that if we dont receive a character in that amount of time 
//            something is wrong.  
//=============================================================================
// Quick and dirty helper function to read so many bytes in from the XBee with a timeout and an end of character marker...
byte ReadFromXBee(byte *pb, byte cb, ulong wTimeout, word wEOL)
{
    int ich;
    byte* pbIn = pb;
    unsigned long ulTimeLastChar = millis();

    while (cb) {
        while ((ich = XBeeSerial.read()) == -1) {
            // check for timeout
            if ((millis()-ulTimeLastChar) > wTimeout)
                return (byte)(pb-pbIn);
        }
        *pb++ = (byte)ich;
        cb--;
//        DBGSerial.print(ich, HEX);
//        DBGSerial.print(" ");

        if ((word)ich == wEOL)
            break;    // we matched so get out of here.
        ulTimeLastChar = millis();    // update to say we received something
    }
    return (byte)(pb-pbIn);
}

//=============================================================================
// void XBeePrintf - output a buffer to the XBEE.  This is used to abstract
//            away the actual function needed to talk to the XBEE
//=============================================================================
int XBeePrintf(const char *format, ...)
{
    char szTemp[80];
    int ich;
    va_list ap;
    va_start(ap, format);
    ich = vsprintf(szTemp, format, ap);
   XBeeSerial.write((byte*)szTemp, ich);
    va_end(ap);
}


//==============================================================================
// [SendXBeePacket] - Simple helper function to send the 4 byte packet header
//     plus the extra data if any
//      gosub SendXBeePacket[bPacketType, cbExtra, pExtra]
//==============================================================================
void SendXBeePacket(byte bPHType, byte cbExtra, byte *pbExtra)
{
    // Tell system to now output to the xbee
    byte abPH[9];
    byte *pbT;
    byte bChkSum;
    int i;

    // We need to setup the xbee Packet
    abPH[0]=0x7e;                        // Command prefix
    abPH[1]=0;                            // msb of size
    abPH[2]=cbExtra+XBEE_API_PH_SIZE + 5;    // size LSB
    abPH[3]=1;                             // Send to 16 bit address.

    g_diystate.bPacketNum = g_diystate.bPacketNum + 1;
    if (g_diystate.bPacketNum == 0)
        g_diystate.bPacketNum = 1;        // Don't pass 1 as this says no ack
    abPH[4]=g_diystate.bPacketNum;        // frame number
    abPH[5]=g_diystate.wAPIDL >> 8;        // Our current destination MSB/LSB
    abPH[6]=g_diystate.wAPIDL & 0xff;
    abPH[7]=0;                            // No Options

    abPH[8]=bPHType;

    // Now compute the initial part of the checksum
    bChkSum = 0;
    for (i=3;i <= 8; i++)
        bChkSum += abPH[i];

    // loop through the extra bytes in the exta to build the checksum;
    pbT = pbExtra;
    for (i=0; i < cbExtra; i++)
        bChkSum += *pbT++;                // add each byte to the checksum

    // Ok lets output the fixed part
    XBeeSerial.write(abPH,9);

    // Ok lets write the extra bytes if any to the xbee
    if (cbExtra)
        XBeeSerial.write(pbExtra, cbExtra);

    // Last write out the checksum
    bChkSum = 0xff - bChkSum;
    XBeeSerial.write(&bChkSum, 1);

#ifdef DEBUG_OUT
    // We moved dump before the serout as hserout will cause lots of interrupts which will screw up our serial output...
    // Moved after as we want the other side to get it as quick as possible...
    DBGSerial << "SDP: " << _HEX(bPHType) << " " << _HEX(cbExtra) << endl;

#ifdef DEBUG_VERBOSE        // Only ouput whole thing if verbose...
    if (cbExtra) {
        byte i;
        for (i = 0; i < cbExtra; i++) {
            DBGSerial.print(*pbExtra++, HEX);
        }
    }
#endif    
    DBGSerial.println("\r");

#endif

}

//////////////////////////////////////////////////////////////////////////////
//==============================================================================
// [APIRecvPacket - try to receive a packet from the XBee. 
//        - Will return true if it receives something, else false
//        - Pass in buffer to receive packet.  Assumed it is big enough...
//        - pass in timeout if zero will return if no data...
//        
//==============================================================================
byte APIRecvPacket(ulong Timeout)
{
    byte cbRead;
    byte abT[3];
    byte bChksum;
    int i;

    short wPacketLen;
    //  First see if the user wants us to wait for input or not
    //    hserstat HSERSTAT_INPUT_EMPTY, _TP_Timeout            // if no input available quickly jump out.
    if (Timeout == 0) 
    {
        if (!XBeeSerial.available())
            return 0;        // nothing waiting for us...
        Timeout = 100;    // .1 second?

    }
    
    
    // Now lets try to read in the data from the xbee...
    // first read in the delimter and packet length
    // We now do this in two steps.  The first to try to resync if the first character
    // is not the proper delimiter...

    do {    
        cbRead = ReadFromXBee(abT, 1, Timeout, 0xffff);
        if (cbRead == 0)
            return 0;
    } 
    while (abT[0] != 0x7e);

    cbRead = ReadFromXBee(abT, 2, Timeout, 0xffff);
    if (cbRead != 2)
        return 0;                // did not read in full header or the header was not correct.

    wPacketLen = (abT[0] << 8) + abT[1];

    // Now lets try to read in the packet
    cbRead = ReadFromXBee(g_diystate.bAPIPacket, wPacketLen+1, Timeout, 0xffff);


    // Now lets verify the checksum.
    bChksum = 0;
    for (i = 0; i < wPacketLen; i++)
        bChksum = bChksum + g_diystate.bAPIPacket[i];             // Add that byte to the buffer...


    if (g_diystate.bAPIPacket[wPacketLen] != (0xff - bChksum)) {
#ifdef DBGSerial      
        DBGSerial << "Packet Checksum error: " << _DEC(wPacketLen) << endl;
#endif        
        return 0;                // checksum was off
    }
    return wPacketLen;    // return the packet length as the caller may need to know this...
}



//==============================================================================
// [APISetXbeeHexVal] - Set one of the XBee Hex value registers.
//==============================================================================

void APISetXBeeHexVal(char c1, char c2, unsigned long _lval)
{
    byte abT[12];

    // Build a command buffer to output
    abT[0] = 0x7e;                    // command start
    abT[1] = 0;                        // Msb of packet size
    abT[2] = 8;                        // Packet size
    abT[3] = 8;                        // CMD=8 which is AT command

    g_diystate.bPacketNum = g_diystate.bPacketNum + 1;
    if (g_diystate.bPacketNum == 0)
        g_diystate.bPacketNum = 1;        // Don't pass 1 as this says no ack

    abT[4] = g_diystate.bPacketNum;    // Frame id
    abT[5] = c1;                    // Command name
    abT[6] = c2;

    abT[7] = _lval >> 24;            // Now output the 4 bytes for the new value
    abT[8] = (_lval >> 16) & 0xFF;
    abT[9] = (_lval >> 8) & 0xFF;
    abT[10] = _lval & 0xFF;

    // last but not least output the checksum
    abT[11] = 0xff - 
        ( ( 8+g_diystate.bPacketNum + c1 + c2 + (_lval >> 24) + ((_lval >> 16) & 0xFF) +
        ((_lval >> 8) & 0xFF) + (_lval & 0xFF) ) & 0xff);

    XBeeSerial.write(abT, sizeof(abT));

}


//==============================================================================
// [SetXbeeDL] - Set the XBee DL to the specified word that is passed
//         simple wrapper call to hex val
//==============================================================================
void SetXBeeDL (unsigned short wNewDL)
{
    APISetXBeeHexVal('D','L', wNewDL);
    g_diystate.wAPIDL = wNewDL;        // remember what DL we are talking to.
}


//==============================================================================
// [APISendXBeeGetCmd] - Output the command packet to retrieve a hex or string value
//==============================================================================

void APISendXBeeGetCmd(char c1, char c2)
{
    byte abT[8];

    // just output the bytes that we need...
    abT[0] = 0x7e;                    // command start
    abT[1] = 0;                        // Msb of packet size
    abT[2] = 4;                        // Packet size
    abT[3] = 8;                        // CMD=8 which is AT command

    g_diystate.bPacketNum = g_diystate.bPacketNum + 1;
    if (g_diystate.bPacketNum == 0)
        g_diystate.bPacketNum = 1;        // Don't pass 1 as this says no ack

    abT[4] = g_diystate.bPacketNum;    // Frame id
    abT[5] = c1;                    // Command name
    abT[6] = c2;

    // last but not least output the checksum
    abT[7] = 0xff - ((8 + g_diystate.bPacketNum + c1 + c2) & 0xff);
    XBeeSerial.write(abT, sizeof(abT));
}



//==============================================================================
// [GetXBeeHVal] - Set the XBee DL or MY or??? Simply pass the two characters
//             that were passed in to the XBEE
//==============================================================================
word GetXBeeHVal (char c1, char c2)
{

    // Output the request command
    APISendXBeeGetCmd(c1, c2);
    XBeeSerial.flush();  // make sure the command goes out. 

    // Now lets loop reading responses 
#ifdef DBGSerial
      DBGSerial.print("GXBHV: ");
      DBGSerial.write(c1);
      DBGSerial.write(c2);
      DBGSerial.print(" : ");
      DBGSerial.println(g_diystate.bPacketNum, HEX);
#endif
    for (;;)
    {
        byte cbPacket;
        if (!(cbPacket = APIRecvPacket(200)))
            break;
#ifdef DBGSerial
      DBGSerial.print(": ");
      for (int i = 0; i < cbPacket; i++) {
         DBGSerial.print(g_diystate.bAPIPacket[i]);
         DBGSerial.print(" ");
      }
      DBGSerial.println("");
      
#endif
        // Only process the cmd return that is equal to our packet number we sent and has a valid return state
        if ((g_diystate.bAPIPacket[0] == 0x88) && (g_diystate.bAPIPacket[1] == g_diystate.bPacketNum) &&
            (g_diystate.bAPIPacket[4] == 0))
        {
            // Sometimes it only returns 2 data bytes, other times it return 4....
            if (cbPacket == 7)
              return     (g_diystate.bAPIPacket[5] << 8) + g_diystate.bAPIPacket[6];
            else  
              return     (g_diystate.bAPIPacket[7] << 8) + g_diystate.bAPIPacket[8];
        }
    }
    return 0xffff;                // Did not receive the data properly.
}





/////////////////////////////////////////////////////////////////////////////


//==============================================================================
// [ClearXBeeInputBuffer] - This simple helper function will clear out the input
//                        buffer from the XBEE
//==============================================================================
extern boolean g_fDebugOutput;
void ClearXBeeInputBuffer(void)
{
    byte b[1];

#ifdef DEBUG
    boolean fBefore = g_fDebugOutput;
    g_fDebugOutput = false;
#endif    
//    XBeeSerial.flush();    // clear out anything that was queued up...      - On 1.0+ wait for output to be output...
    while (ReadFromXBee(b, 1, 5, 0xffff))
        ;    // just loop as long as we receive something...
#ifdef DEBUG
    g_fDebugOutput = fBefore;
#endif    
}



//==============================================================================
// [DebugMemoryDump] - striped down version of rprintfMemoryDump
//==============================================================================
#ifdef DEBUG
void DebugMemoryDump(const byte* data, int off, int len)
{
    int x;
    int c;
    int line;
    const byte * b = data;

    for(line = 0; line < ((len % 16 != 0) ? (len / 16) + 1 : (len / 16)); line++)  {
        int line16 = line * 16;
        DBGSerial << _HEX(line16) << "|";

        // put hex values
        for(x = 0; x < 16; x++) {
            if(x + line16 < len) {
                c = b[off + x + line16];
                DBGSerial << _HEX(c) << " ";
            }
            else
                DBGSerial.write("   ");
        }
        DBGSerial.write("| ");

        // put ascii values
        for(x = 0; x < 16; x++) {
            if(x + line16 < len) {
                c = b[off + x + line16];
                DBGSerial.write( ((c > 0x1f) && (c < 0x7f))? c : '.');
            }
            else
                DBGSerial.write(" ");
        }
        DBGSerial.write("\n\r\r");
    }
}

#endif


//==============================================================================
// [CheckAndTransmitDataPacket] function
// 
// This function will output a packet of data over the XBee to the receiving robot. 
//==============================================================================

byte g_bTxStatusLast = 99;

void CheckAndTransmitDataPacket(PDIYPACKET pdiyp) {
    byte bDataOffset;
    byte bPacketType;
    byte cbPacket;

    while (cbPacket = APIRecvPacket(0)) {
        // We received an XBee Packet, See what type it is.
	// first see if it is a RX 16 bit or 64 bit packet?
	if (g_diystate.bAPIPacket[0] == 0x81)
	    bDataOffset = 5;     // 16 bit address sent, so there is 5 bytes of packet header before our data
        else if (g_diystate.bAPIPacket[0] == 0x80) 
	    bDataOffset = 11;    // 64 bit address so our data starts at offset 11...
        else if (g_diystate.bAPIPacket[0] == 0x89) {
            if (g_diystate.bAPIPacket[2] != g_bTxStatusLast) {
                g_bTxStatusLast = g_diystate.bAPIPacket[2];
#ifdef DEBUG
                DBGSerial << "CTDB TStat: " << _HEX(g_diystate.bAPIPacket[2]) <<endl;
#endif
            }
	    continue;	// this is an A TX Status or API status  message - May check status and maybe update something?
        }
        else if (g_diystate.bAPIPacket[0] == 0x88)
	    continue;	// API status  message - May check status and maybe update something?
        else {
#ifdef DEBUG
            DBGSerial << "Unknown XBEE Packet: " << _HEX(g_diystate.bAPIPacket[0]) << " L:"<< _HEX(g_diystate.bAPIPacket[1] >> 8 + g_diystate.bAPIPacket[2]) <<endl;
#endif
            break;    // lets just bail from this loop
	}
#ifdef DEBUG
//		serout s_out, i9600, ["Recv:", hex CmdPacket(0)\2, hex CmdPacket(1)\2,hex CmdPacket(2)\2,hex CmdPacket(3)\2, 13]
#endif	
        cbPacket -= (bDataOffset + XBEE_API_PH_SIZE); // This is the extra data size
	bPacketType = g_diystate.bAPIPacket[bDataOffset + 0];

        // Now look what type packet it is.  First check to see if the user is asking for data...
	// we removed some of the testing as the function we called already validated the checksum and sizes...
        // We will also allow for compressed packets to be sent back to the user.  The newer clients signal this by
        // passing an extra parameter on this command to say they understand...  This allows for new and old clients to use the same
        // code and likewise it allows the new clients to work with old transmitters as they will still receive back the full data packet...
        
    	if (bPacketType == XBEE_RECV_REQ_DATA) {
            SendXBeePacket(XBEE_TRANS_DATA, sizeof(DIYPACKET), (byte*)pdiyp);		// Ok we dumped the data to the the output
    	    g_diystate.ulLastPacket = millis();
	    g_diystate.fSendNewPacketMsg = false;				// Clear out that we have new data.
	    g_diystate.fNewPacketMsgSent = false;				// We have not sent a new Msg since the last data
            break;    // lets only process this message in this loop
        }
    	else if (bPacketType == XBEE_RECV_REQ_DATA2) {                // Request data supports compressed data
            // new Client...
            word wMBit = 1;
            byte cChanged = 0;
            byte i;
            g_diychp.wMask = 0;    
            if (g_diystate.fFirstDataPacket) {
                g_diystate.fFirstDataPacket = false;
                cChanged = MAXPACKETBYTEDIFF + 1;    // make sure first packet after active is full
            }     
            for (i=0; i < sizeof(DIYPACKET); i++) {
                if (pdiyp->ab[i] != g_diypLastSent.ab[i]) {
                     g_diypLastSent.ab[i] = pdiyp->ab[i];
                     g_diychp.wMask |= wMBit; 
                     if (cChanged < MAXPACKETBYTEDIFF)
                         g_diychp.ab[cChanged] = g_diypLastSent.ab[i];
                     cChanged++;
                }
                wMBit <<= 1;    // set up the next bit...
            }
            // Now see what type of packet to send...
            if (!cChanged)
    	        SendXBeePacket(XBEE_TRANS_NOTHIN_CHANGED, 0, 0);		// Tell caller nothing changed since last time...
            else if (cChanged <= MAXPACKETBYTEDIFF)
    	        SendXBeePacket(XBEE_TRANS_CHANGED_DATA, cChanged+2, (byte*)&g_diychp);		// Tell caller nothing changed since last time...
            else
    	        SendXBeePacket(XBEE_TRANS_DATA, sizeof(DIYPACKET), (byte*)pdiyp);		// Ok we dumped the data to the the output
    	    g_diystate.ulLastPacket = millis();
	    g_diystate.fSendNewPacketMsg = false;				// Clear out that we have new data.
	    g_diystate.fNewPacketMsgSent = false;				// We have not sent a new Msg since the last data
            break;    // lets only process this message in this loop
	} 

        // Next check for the packet may be a Req New which changes when we transmite data...	
	else if (bPacketType == XBEE_RECV_REQ_NEW ) 
	    g_diystate.fNewPacketMsgMode = 1;
	else if (bPacketType == XBEE_RECV_REQ_NEW_OFF ) 
	    g_diystate.fNewPacketMsgMode = 0;
	
	else if (bPacketType == XBEE_RECV_DISP_VAL )  {
    	    // Packet to display a value/string on LCD of the remote...
	    // BUGBUG:::::::::::: split into two commands. One for a value and other for string...

	    // We will handle word values here
	    if (cbPacket == 2)
		g_display.DisplayRemoteValue(2, (g_diystate.bAPIPacket[bDataOffset+XBEE_API_PH_SIZE] << 8) + g_diystate.bAPIPacket[bDataOffset+XBEE_API_PH_SIZE+1]);
    	    g_diystate.ulLastPacket = millis();
        }

	else if (bPacketType == XBEE_RECV_DISP_STR )  {
#ifdef DBGSerial
            DBGSerial << " XBEE_RECV_DISP_STR:" << _DEC(cbPacket) << endl;
#endif            
            // Try to handle both text and a simple byte value passed to us...
	    if ((cbPacket > 0) && (cbPacket <= 16)) 
                g_diystate.bAPIPacket[bDataOffset+XBEE_API_PH_SIZE+cbPacket] = 0;    // zero terminate string.
		g_display.DisplayStatus(0, (char*)&g_diystate.bAPIPacket[bDataOffset+XBEE_API_PH_SIZE]);
    	    g_diystate.ulLastPacket = millis();
        }		//

        else if ((bPacketType >= XBEE_RECV_DISP_VAL0 ) && (bPacketType <= XBEE_RECV_DISP_VAL2 )) {
	    // We will handle word values here
	    if (cbPacket == 2) 
		g_display.DisplayRemoteValue(bPacketType-XBEE_RECV_DISP_VAL0,
                    (g_diystate.bAPIPacket[bDataOffset+XBEE_API_PH_SIZE] << 8) + g_diystate.bAPIPacket[bDataOffset+XBEE_API_PH_SIZE+1]);
    	    g_diystate.ulLastPacket = millis();
        }
	
	
	// Packet to play a sound - BUGBUG - not checking checksum...
	else if (bPacketType == XBEE_PLAY_SOUND ) {
#ifdef LATER    
#endif
    	    g_diystate.ulLastPacket = millis();
        }

        else {        // Unknown packet
#ifdef DEBUG
            DBGSerial << "TP IN:" << _HEX(g_diystate.bAPIPacket[bDataOffset + 0]) << " " << _HEX(g_diystate.bAPIPacket[bDataOffset + 1]) 
                    << " " << _HEX(g_diystate.bAPIPacket[bDataOffset + 2]) << " " <<  _HEX(g_diystate.bAPIPacket[bDataOffset + 3]) << endl;
#endif	
            ClearXBeeInputBuffer();	// try to clear everything else out.
            break;    // and get out of this loop;
        }
    }


    // Do we have new data to tell the client about?
    if ( g_diystate.fNewPacketMsgMode &&  g_diystate.fSendNewPacketMsg && ! g_diystate.fNewPacketMsgSent) {
#ifdef DEBUG
//        DBGSerial << "New: " << _HEX(fPacketChanged) << endl;
#endif		
        SendXBeePacket(XBEE_TRANS_NEW,  0, 0);		// no extra data no seq number so chksum = 0 also
	g_diystate.fNewPacketMsgSent = true;
    }
	
    // we did not get anything or we did not get a complet packet...
    if ((millis() - g_diystate.ulLastPacket) > 2000) {
//	    ClearInputBuffer();			// clear out our buffer.

        XBeeTransReady();			// send out another ready signal.
        XBeeTransDataVersion(XBEEDATAVERSION);    // also send out our data version...
    }
}

//==============================================================================
// FLastXBeeWriteSucceeded
//==============================================================================
extern boolean FLastXBeeWriteSucceeded(void) {
    return (g_bTxStatusLast==0);
}

//==============================================================================
// [XBeeTransReady] - Simple message that we send out when we are ready
//		or if we have not received anything for a long time...
//
//		This packet currently our MY that the destination will use to
//		talk back to us, such that we can have multiple transmitters.
//==============================================================================

void XBeeTransReady(void) {
    	
    // We will now transmit our My address so the robot knows who the controller is.
    // CmdPacket(0) = XBEE_TRANS_READY
    // CmdPacket(1) = Checksum
    // CmdPacket(2) = Sequence Num = 0
    // CmdPacket(3) = Count of extra data in this case 2
    // 2 bytes of MY
    byte ab[2];
    ab[0] = g_diystate.wXBeeMY >> 8;    // first byte MSB
    ab[1] = g_diystate.wXBeeMY & 0xff;    // LSB
    SendXBeePacket(XBEE_TRANS_READY, 2, ab);		// Real simple message 
    //cIdleCycles = 0 						// Ok reset our count
    g_diystate.fFirstDataPacket = true;
    g_diystate.ulLastPacket = millis();
}

//==============================================================================
// [XBeeTransDataVersion] - Let client know what formats we support
//==============================================================================
void XBeeTransDataVersion(byte bVer) {
    SendXBeePacket(XBEE_TRANS_DATA_VERSION, 1, &bVer);		// Real simple message 
}

//==============================================================================
// [XBeeTransNotReady] - Simple message that we send out when are no longer ready
//		to receive anything.  Such as we are in some configuration menu...
//		especially if we may change our MY setting...
//
//		This packet currently has no extra data.
//==============================================================================

void XBeeTransNotReady(void) {
    	
    // We will now transmit our My address so the robot knows who the controller is.
    // CmdPacket(0) = XBEE_TRANS_READY
    // CmdPacket(1) = Checksum = 0
    // CmdPacket(2) = Sequence Num = 0
    // CmdPacket(3) = Count of extra data 0
    // 2 bytes of MY
     
    SendXBeePacket(XBEE_TRANS_NOTREADY, 0, 0);		// Real simple message 
}	
