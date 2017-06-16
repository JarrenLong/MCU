// mmc.c : MultiMediaCard functions: init, read, write ...
// Works also with SC cars. Modes: SPI mode.
//
// Rolf Freitag 5/2003
//
/* See also:
http://www.sandisk.com/pdf/oem/ProdManualSDCardv1.9.pdf Sandisk SD card product manual
http://www.sandisk.com/pdf/oem/ProdManualminiSDv1.1.pdf MiniSD card
http://www.sandisk.com/download/Product%20Manuals/Product%20ManualSDCardv1.7.pdf Older SD card
http://www.compile-it.com/support/crndatasheets/MMC%20ADAPTER.pdf
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 44):
 * Rolf Freitag (webmaster at true-random.com) wrote this file.
 * As long as you retain this notice you can do whatever
 * the LGPL (Lesser GNU public License) allows with this stuff.
 * If you think this stuff is worth it, you can send me money via
 * paypal or if we met some day you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 */

// MMC Lib
#ifndef _MMCLIB_C
#define _MMCLIB_C
//---------------------------------------------------------------------
#include "mmc.h"
#include "led.h"

#include  "MSP430x14x.H"
#include  "math.h"
#include  "string.h"

char mmcGetResponse(void);
char mmcGetXXResponse(const char resp);
char mmcCheckBusy(void);

void initSPI (void);
unsigned char spiSendByte(const unsigned char data);

char mmc_buffer[512] =                            // Buffer for mmc i/o for data and registers
{
  0
};

extern char card_state;                           // 0 for no card found, 1 for card found (init successfull)

//---------------------------------------------------------------------

// setup usart1 in spi mode
void initSPI (void)
{
  ME2 |= USPIE1;                                  // Enable USART1 SPI mode
  UTCTL1 = CKPH | SSEL1 | SSEL0 | STC;            // SMCLK, 3-pin mode, clock idle low, data valid on rising edge, UCLK delayed
  UBR01 = 0x02;                                   // 0x02: UCLK/2 (4 MHz), works also with 3 and 4
  UBR11 = 0x00;                                   // -"-
  UMCTL1 = 0x00;                                  // no modulation
  UCTL1 = CHAR | SYNC | MM;                       // 8-bit SPI Master **SWRST**
  P5SEL |= 0x0E;                                  // P5.1-3 SPI option select
  P5DIR |= 0x01;                                  // P5.0 output direction
  P5OUT = 0xff;
  while (!(IFG2 & UTXIFG1));                      // USART1 TX buffer ready (empty)?
  // debug_printf("init......SPI");
}


// Initialisieren
char initMMC (void)
{

  //raise SS and MOSI for 80 clock cycles
  //SendByte(0xff) 10 times with SS high
  //RAISE SS
  int i;
  char response=0x01;

  // debug_printf("Start iniMMC......");
  initSPI();
  //initialization sequence on PowerUp
  CS_HIGH();
  for(i=0;i<=9;i++)
    spiSendByte(0xff);
  CS_LOW();
  //Send Command 0 to put MMC in SPI mode
  mmcSendCmd(0x00,0,0x95);
  //Now wait for READY RESPONSE
  if(mmcGetResponse()!=0x01);
  //       debug_printf("no responce");

  while(response==0x01)
  {
    //  debug_printf("Sending Command 1");
    CS_HIGH();
    spiSendByte(0xff);
    CS_LOW();
    mmcSendCmd(0x01,0x00,0xff);
    response=mmcGetResponse();
  }
  CS_HIGH();
  spiSendByte(0xff);
  // debug_printf("MMC INITIALIZED AND SET TO SPI MODE PROPERLY.");
  return MMC_SUCCESS;
}


// Ti added mmc Get Responce
char mmcGetResponse(void)
{
  //Response comes 1-8bytes after command
  //the first bit will be a 0
  //followed by an error code
  //data will be 0xff until response
  int i=0;

  char response;

  while(i<=64)
  {
    response=spiSendByte(0xff);
    if(response==0x00)break;
    if(response==0x01)break;
    i++;
  }
  return response;
}


char mmcGetXXResponse(const char resp)
{
  //Response comes 1-8bytes after command
  //the first bit will be a 0
  //followed by an error code
  //data will be 0xff until response
  int i=0;

  char response;

  while(i<=500)
  {
    response=spiSendByte(0xff);
    if(response==resp)break;
    i++;
  }
  return response;
}


char mmcCheckBusy(void)
{
  //Response comes 1-8bytes after command
  //the first bit will be a 0
  //followed by an error code
  //data will be 0xff until response
  int i=0;

  char response;
  char rvalue;
  while(i<=64)
  {
    response=spiSendByte(0xff);
    response &= 0x1f;
    switch(response)
    {
      case 0x05: rvalue=MMC_SUCCESS;break;
      case 0x0b: return(MMC_CRC_ERROR);
      case 0x0d: return(MMC_WRITE_ERROR);
      default:
        rvalue = MMC_OTHER_ERROR;
        break;
    }
    if(rvalue==MMC_SUCCESS)break;
    i++;
  }
  i=0;
  do
  {
    response=spiSendByte(0xff);
    i++;
  }while(response==0);
  return response;
}


// The card will respond with a standard response token followed by a data
// block suffixed with a 16 bit CRC.

// Ti Modification: long int -> long ; int -> long
char mmcReadBlock(const unsigned long address, const unsigned long count)
{
  unsigned long i = 0;
  char rvalue = MMC_RESPONSE_ERROR;

  // Set the block length to read
  if (mmcSetBlockLength (count) == MMC_SUCCESS)   // block length could be set
  {
    // SS = LOW (on)
    CS_LOW ();
    // send read command MMC_READ_SINGLE_BLOCK=CMD17
    mmcSendCmd (17,address, 0xFF);
    // Send 8 Clock pulses of delay, check if the MMC acknowledged the read block command
    // it will do this by sending an affirmative response
    // in the R1 format (0x00 is no errors)
    if (mmcGetResponse() == 0x00)
    {
      // now look for the data token to signify the start of
      // the data
      if (mmcGetXXResponse(MMC_START_DATA_BLOCK_TOKEN) == MMC_START_DATA_BLOCK_TOKEN)
      {
        // clock the actual data transfer and receive the bytes; spi_read automatically finds the Data Block
        for (i = 0; i < 512; i++)
          mmc_buffer[i] = spiSendByte(0xff);      // is executed with card inserted

        // get CRC bytes (not really needed by us, but required by MMC)
        spiSendByte(0xff);
        spiSendByte(0xff);
        rvalue = MMC_SUCCESS;
      }
      else
      {
        // the data token was never received
        rvalue = MMC_DATA_TOKEN_ERROR;            // 3
      }
    }
    else
    {
      // the MMC never acknowledge the read command
      rvalue = MMC_RESPONSE_ERROR;                // 2
    }
  }
  else
  {
    rvalue = MMC_BLOCK_SET_ERROR;                 // 1
  }
  CS_HIGH ();
  spiSendByte(0xff);
  return rvalue;
}                                                 // mmc_read_block


//---------------------------------------------------------------------
// Ti Modification: long int -> long
char mmcWriteBlock (const unsigned long address)
{
  unsigned long i = 0;
  char rvalue = MMC_RESPONSE_ERROR;               // MMC_SUCCESS;
  char c = 0x00;

  // Set the block length to read
  if (mmcSetBlockLength (512) == MMC_SUCCESS)     // block length could be set
  {
    // SS = LOW (on)
    CS_LOW ();
    // send write command
    mmcSendCmd (24,address, 0xFF);

    // check if the MMC acknowledged the write block command
    // it will do this by sending an affirmative response
    // in the R1 format (0x00 is no errors)
    if (mmcGetXXResponse(MMC_R1_RESPONSE) == MMC_R1_RESPONSE)
    {
      spiSendByte(0xff);
      // send the data token to signify the start of the data
      spiSendByte(0xfe);
      // clock the actual data transfer and transmitt the bytes
      for (i = 0; i < 512; i++)
        spiSendByte(mmc_buffer[i]);               // mmc_buffer[i];       Test: i & 0xff
      // put CRC bytes (not really needed by us, but required by MMC)
      spiSendByte(0xff);
      spiSendByte(0xff);
      // read the data response xxx0<status>1 : status 010: Data accected, status 101: Data
      //   rejected due to a crc error, status 110: Data rejected due to a Write error.
      mmcCheckBusy();
    }
    else
    {
      // the MMC never acknowledge the write command
      rvalue = MMC_RESPONSE_ERROR;                // 2
    }
  }
  else
  {
    rvalue = MMC_BLOCK_SET_ERROR;                 // 1
  }
  // give the MMC the required clocks to finish up what ever it needs to do
  //  for (i = 0; i < 9; ++i)
  //    spiSendByte(0xff);

  CS_HIGH ();
  // Send 8 Clock pulses of delay.
  spiSendByte(0xff);
  return rvalue;
}                                                 // mmc_write_block


//---------------------------------------------------------------------
void mmcSendCmd (const char cmd, unsigned long data, const char crc)
{
  char frame[6];
  char temp;
  int i;

  frame[0]=(cmd|0x40);
  for(i=3;i>=0;i--)
  {
    temp=(char)(data>>(8*i));
    frame[4-i]=(temp);
  }
  frame[5]=(crc);
  for(i=0;i<6;i++)
    spiSendByte(frame[i]);
}


//--------------- set blocklength 2^n ------------------------------------------------------
// Ti Modification: long int-> long
char mmcSetBlockLength (const unsigned long blocklength)
{
  char rValue = MMC_TIMEOUT_ERROR;
  char i = 0;

  // SS = LOW (on)
  CS_LOW ();

  // Set the block length to read
  //MMC_SET_BLOCKLEN =CMD16
  mmcSendCmd(16, blocklength, 0xFF);

  // get response from MMC - make sure that its 0x00 (R1 ok response format)
  if(mmcGetResponse()!=0x00);

  CS_HIGH ();

  // Send 8 Clock pulses of delay.
  spiSendByte(0xff);

  return MMC_SUCCESS;
}                                                 // block_length


//TI added substitution routine for spi_read and spi_write

unsigned char spiSendByte(const unsigned char data)
{
  while ((IFG2&UTXIFG1) ==0);                     // wait while not ready / for RX
  TXBUF1 = data;                                  // write
  while ((IFG2 & URXIFG1)==0);                    // wait for RX buffer (full)
  return (RXBUF1);
}


// Reading the contents of the CSD and CID registers in SPI mode is a simple
// read-block transaction.

char mmcReadRegister (const char cmd_register, const unsigned char length)
{
  char uc = 0;
  char rvalue = MMC_TIMEOUT_ERROR;
  //  char i = 0;

  if (mmcSetBlockLength (length) == MMC_SUCCESS)
  {
    CS_LOW ();
    // CRC not used: 0xff as last byte
    mmcSendCmd(cmd_register, 0x000000, 0xff);

    // wait for response
    // in the R1 format (0x00 is no errors)
    if (mmcGetResponse() == 0x00)
    {
      if (mmcGetXXResponse(0xfe)== 0xfe)
        for (uc = 0; uc < length; uc++)
          mmc_buffer[uc] = spiSendByte(0xff);
      // get CRC bytes (not really needed by us, but required by MMC)
      spiSendByte(0xff);
      spiSendByte(0xff);
    }
    else
      rvalue = MMC_RESPONSE_ERROR;
    // CS = HIGH (off)
    CS_HIGH ();

    // Send 8 Clock pulses of delay.
    spiSendByte(0xff);
  }
  CS_HIGH ();
  return rvalue;
}                                                 // mmc_read_register


//---------------------------------------------------------------------
#endif                                            /* _MMCLIB_C */
