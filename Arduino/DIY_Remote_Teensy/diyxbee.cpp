/****************************************************************************
 * - DIY remote control XBee support file
 * - Trying a real simple protocol.  We will simply output a message every N milliseconds
 * - either the other side will want or toss it... 
 *
 ****************************************************************************/
#include "diyxbee.h"


// Add support for running on non-mega Arduino boards as well.
#if not defined(__arm__)
#if not defined(UBRR1H)
#include <NewSoftSerial.h>
NewSoftSerial XBeeSerial(cXBEE_IN, cXBEE_OUT);
#endif
#endif

//#define DEBUG
//#define DEBUG_OUT
//#define DEBUG_VERBOSE


DIYSTATE g_diystate;
//boolean g_fDebugOutput = true;
#define XBEE_API_PH_SIZE    1            // Changed Packet Header to just 1 byte - Type - we don't use sequence number anyway...
//#define XBEE_NEW_DATA_ONLY 1
// Some forward definitions
#ifdef DEBUG
extern void DebugMemoryDump(const byte* , int, int);
static boolean s_fDisplayedTimeout = false;
#endif

DIYPACKET g_diypLastSent={
  0,0,0,0,0,0,0,0};        // keep a copy of the last packet we sent so we can calculate delta packets...
#define MAXPACKETBYTEDIFF    4
typedef struct {
  uint16_t     wMask;                            // Mask of which bytes have changed
  byte     ab[MAXPACKETBYTEDIFF];            // Values of bytes that changed...
} 
DIYCHPACKET;
DIYCHPACKET g_diychp;

//=============================================================================
// Xbee stuff
//=============================================================================
void InitXBee(void)
{
  uint8_t abT[10];  
#ifdef XBEE_POWERUP_HACK  
  XBeeSerial.begin(9600);   
  XBeeSerial.println("ATCN");
  XBeeSerial.flush();
  XBeeSerial.end();
  delay(100);
#endif
  XBeeSerial.begin(XBEE_BAUD);    // BUGBUG??? need to see if this baud will work here.
  // Ok lets set the XBEE into API mode...
  delay(1500);  // give it some time to settle...
  XBeeSerial.write("+++");
  XBeeSerial.flush();

  // Lets try to get an OK 
  XBeeSerial.setTimeout(100);  // give time to settle also allows OLED version stuff to display...
  if (XBeeSerial.readBytesUntil(13, abT, sizeof(abT)) < 2) {
    // probably failed, maybe we have not set everything up yet...
    delay(2000);
    ClearXBeeInputBuffer();      
    XBeeSerial.print("+++");
    XBeeSerial.flush();
    XBeeSerial.setTimeout(2000);

    XBeeSerial.readBytesUntil(13, abT, sizeof(abT));
    XBeeSerial.println("ATGT 3");
  }

  delay(20);
  XBeeSerial.write("ATAP 1\rATCN\r");
  delay(100);
  ClearXBeeInputBuffer();      

  // for Xbee with no flow control, maybe nothing to do here yet...
  g_diystate.bPacketNum = 0;
  g_diystate.wDataPacketDeltaMS = CXBEEDATAPACKETDELTAMS;
}



//=============================================================================
// void WriteToXBee - output a buffer to the XBEE.  This is used to abstract
//            away the actual function needed to talk to the XBEE
//=============================================================================
inline void WriteToXBee(byte *pb, byte cb) __attribute__((always_inline));
void WriteToXBee(byte *pb, byte cb)
{
  XBeeSerial.write(pb, cb);
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
  WriteToXBee((byte*)szTemp, ich);
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
  WriteToXBee(abPH,9);

  // Ok lets write the extra bytes if any to the xbee
  if (cbExtra)
    WriteToXBee(pbExtra, cbExtra);

  // Last write out the checksum
  bChkSum = 0xff - bChkSum;
  WriteToXBee(&bChkSum, 1);

#ifdef DEBUG_OUT
  // We moved dump before the serout as hserout will cause lots of interrupts which will screw up our serial output...
  // Moved after as we want the other side to get it as quick as possible...
  DBGSerial.print("SDP: ");
  DBGSerial.print(bPHType, HEX);
  DBGSerial.print(" ");
  DBGSerial.println(cbExtra, HEX);

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

  XBeeSerial.setTimeout(Timeout);

  // Now lets try to read in the data from the xbee...
  // first read in the delimter and packet length
  // We now do this in two steps.  The first to try to resync if the first character
  // is not the proper delimiter...

  do {    
    cbRead = XBeeSerial.readBytes(abT, 1);
    if (cbRead == 0)
      return 0;
  } 
  while (abT[0] != 0x7e);

  cbRead = XBeeSerial.readBytes(abT, 2);
  if (cbRead != 2)
    return 0;                // did not read in full header or the header was not correct.

  wPacketLen = (abT[0] << 8) + abT[1];
  if (wPacketLen >= sizeof(g_diystate.bAPIPacket)) {
#ifdef DBGSerial      
    DBGSerial.print("Packet Length error: ");
    DBGSerial.println(wPacketLen, DEC);
#endif        
    ClearXBeeInputBuffer();  // maybe clear everything else out.
    return 0;  // Packet bigger than expected maybe bytes lost...
  }

  // Now lets try to read in the packet
  cbRead = XBeeSerial.readBytes(g_diystate.bAPIPacket, wPacketLen+1);


  // Now lets verify the checksum.
  bChksum = 0;
  for (i = 0; i < wPacketLen; i++)
    bChksum = bChksum + g_diystate.bAPIPacket[i];             // Add that byte to the buffer...


  if (g_diystate.bAPIPacket[wPacketLen] != (0xff - bChksum)) {
#ifdef DBGSerial      
    DBGSerial.print("Packet Checksum error: ");
    DBGSerial.print(wPacketLen, DEC);
    DBGSerial.print(" ");
    DBGSerial.print(g_diystate.bAPIPacket[wPacketLen], HEX);
    DBGSerial.print("!=");
    DBGSerial.println(0xff - bChksum, HEX);
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

  WriteToXBee(abT, sizeof(abT));

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
  WriteToXBee(abT, sizeof(abT));
}



//==============================================================================
// [GetXBeeHVal] - Set the XBee DL or MY or??? Simply pass the two characters
//             that were passed in to the XBEE
//==============================================================================
uint16_t GetXBeeHVal (char c1, char c2)
{

  // Output the request command
  APISendXBeeGetCmd(c1, c2);

  // Now lets loop reading responses 
  for (;;)
  {

    if (!APIRecvPacket(100))
      break;

    // Only process the cmd return that is equal to our packet number we sent and has a valid return state
    if ((g_diystate.bAPIPacket[0] == 0x88) && (g_diystate.bAPIPacket[1] == g_diystate.bPacketNum) &&
      (g_diystate.bAPIPacket[4] == 0))
    {
      // BUGBUG: Why am I using the high 2 bytes if I am only processing words?
      return     (g_diystate.bAPIPacket[5] << 8) + g_diystate.bAPIPacket[6];
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
  while (XBeeSerial.read() != -1)
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
    DBGSerial.print(line16, HEX);
    DBGSerial.print("|");

    // put hex values
    for(x = 0; x < 16; x++) {
      if(x + line16 < len) {
        c = b[off + x + line16];
        DBGSerial.print(c, HEX);
        DBGSerial.print(" ");
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
  unsigned long ulCurTime = millis();

  // First see if it is time for us to send out the next data packet:
  if ((ulCurTime - g_diystate.ulLastPacket) > g_diystate.wDataPacketDeltaMS) {
    SendXBeePacket(XBEE_TRANS_DATA, sizeof(DIYPACKET), (byte*)pdiyp);		// Ok we dumped the data to the the output
    g_diystate.ulLastPacket = ulCurTime;
  }


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
        DBGSerial.print("CTDB TStat: ");
        DBGSerial.println(g_diystate.bAPIPacket[2], HEX);
#endif
      }
      continue;	// this is an A TX Status or API status  message - May check status and maybe update something?
    }
    else if (g_diystate.bAPIPacket[0] == 0x88)
      continue;	// API status  message - May check status and maybe update something?
    else {
#ifdef DEBUG
      DBGSerial.print("Unknown XBEE Packet: ");
      DBGSerial.print(g_diystate.bAPIPacket[0], HEX);
      DBGSerial.print(" L:");
      DBGSerial.println(g_diystate.bAPIPacket[1] >> 8 + g_diystate.bAPIPacket[2], HEX);
#endif
      break;    // lets just bail from this loop
    }
#ifdef DEBUG
    //		serout s_out, i9600, ["Recv:", hex CmdPacket(0)\2, hex CmdPacket(1)\2,hex CmdPacket(2)\2,hex CmdPacket(3)\2, 13]
#endif	
    cbPacket -= (bDataOffset + XBEE_API_PH_SIZE); // This is the extra data size
    bPacketType = g_diystate.bAPIPacket[bDataOffset + 0];

    if (bPacketType == XBEE_RECV_DISP_VAL )  {
      // We will handle word values here
      if (cbPacket == 2)
        g_display.DisplayRemoteValue(2, (g_diystate.bAPIPacket[bDataOffset+XBEE_API_PH_SIZE] << 8) + g_diystate.bAPIPacket[bDataOffset+XBEE_API_PH_SIZE+1]);
      g_diystate.ulLastPacket = millis();
    }

    else if (bPacketType == XBEE_RECV_DISP_STR )  {
#ifdef DBGSerial
      DBGSerial.print(" XBEE_RECV_DISP_STR:");
      DBGSerial.write((char*)&g_diystate.bAPIPacket[bDataOffset+XBEE_API_PH_SIZE], cbPacket);
      DBGSerial.print("==");
      DBGSerial.println(cbPacket, DEC);
#endif            
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
      PlayRemoteSounds(cbPacket, (char*)&g_diystate.bAPIPacket[bDataOffset+XBEE_API_PH_SIZE]);
      g_diystate.ulLastPacket = millis();
    }

    else {        // Unknown packet
#ifdef DEBUG
      DBGSerial.print("TP IN:");
      DBGSerial.print(g_diystate.bAPIPacket[bDataOffset + 0], HEX);
      DBGSerial.print( " ");
      DBGSerial.print(g_diystate.bAPIPacket[bDataOffset + 1], HEX);
      DBGSerial.print(" ");
      DBGSerial.print(g_diystate.bAPIPacket[bDataOffset + 2], HEX);
      DBGSerial.print(" ");
      DBGSerial.println(g_diystate.bAPIPacket[bDataOffset + 3], HEX);
#endif	
      delay(5);  // do simple way.
      ClearXBeeInputBuffer();	// try to clear everything else out.
      break;    // and get out of this loop;
    }
  }
}

//==============================================================================
// FLastXBeeWriteSucceeded
//==============================================================================
extern boolean FLastXBeeWriteSucceeded(void) {
  return (g_bTxStatusLast==0);
}


