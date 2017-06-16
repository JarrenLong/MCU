/*
  mmc.h: Dekcarations for Communikation with the MMC (see mmc.c) in unprotected spi mode.

Pin configuration at MSP430F149:
--------------------------------
  MC                    MC Pin          MMC                      MMC Pin
  P5.4                  48              ChipSelect               1
  P5.1 / SlaveInMasterOut 45            DataIn                   2
  .                                     GND                      3 (0 V)
  .                                     VDD                      4 (3.3 V)
  P5.3 UCLK1 / SlaveCLocK 47            Clock                    5
  .                                     GND                      6 (0 V)
  P5.2 / SlaveOutMasterIn 46            DataOut                  7
  P5.4                                  CardDetect with pullup
---------------------------------------------------------------------

 Revisions
 Date           Author                                  Revision
 11. May 2003           Rolf Freitag                            0.02
(2004: corrected MC pin numbers (switched only 45, 46))
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
#ifndef _MMCLIB_H
#define _MMCLIB_H

#ifndef TXEPT                                     // transmitter-empty flag
#define TEXPT 0x01
#endif

// macro defines
#define HIGH(a) ((a>>8)&0xFF)                     // high byte from word
#define LOW(a) (a&0xFF)                           // low byte from word

#define CS_LOW()  P5OUT &= ~0x10                  // Card Select
#define CS_HIGH() P5OUT |= 0x10                   // Card Deselect
#define SPI_RXC (IFG2 & URXIFG1)
#define SPI_TXC (IFG2 & UTXIFG1)

#define SPI_RX_COMPLETE (IFG2 & URXIFG1)
#define SPI_TX_READY (IFG2 & UTXIFG1)

#define DUMMY 0xff

// Tokens (nessisary because at nop/idle (and CS active) only 0xff is on the data/command line)
#define MMC_START_DATA_BLOCK_TOKEN    0xfe        // Data token start byte, Start Single Block Read
#define MMC_START_DATA_MULTIPLE_BLOCK_READ  0xfe  // Data token start byte, Start Multiple Block Read
#define MMC_START_DATA_BLOCK_WRITE    0xfe        // Data token start byte, Start Single Block Write
#define MMC_START_DATA_MULTIPLE_BLOCK_WRITE 0xfc  // Data token start byte, Start Multiple Block Write
#define MMC_STOP_DATA_MULTIPLE_BLOCK_WRITE  0xfd  // Data toke stop byte, Stop Multiple Block Write

// an affirmative R1 response (no errors)
#define MMC_R1_RESPONSE       0x00

// this variable will be used to track the current block length
// this allows the block length to be set only when needed
// unsigned long _BlockLength = 0;

// error/success codes
#define MMC_SUCCESS           0x00
#define MMC_BLOCK_SET_ERROR   0x01
#define MMC_RESPONSE_ERROR    0x02
#define MMC_DATA_TOKEN_ERROR  0x03
#define MMC_INIT_ERROR        0x04
#define MMC_CRC_ERROR         0x10
#define MMC_WRITE_ERROR       0x11
#define MMC_OTHER_ERROR       0x12
#define MMC_TIMEOUT_ERROR     0xFF

// commands: first bit 0 (start bit), second 1 (transmission bit); CMD-number + 0ffsett 0x40
#define MMC_GO_IDLE_STATE   0x40                  //CMD0
#define MMC_SEND_OP_COND  0x41                    //CMD1
#define MMC_READ_CSD    0x49                      //CMD9
#define MMC_SEND_CID    0x4a                      //CMD10
#define MMC_STOP_TRANSMISSION   0x4c              //CMD12
#define MMC_SEND_STATUS   0x4d                    //CMD13
#define MMC_SET_BLOCKLEN  0x50                    //CMD16 Set block length for next read/write
#define MMC_READ_SINGLE_BLOCK   0x51              //CMD17 Read block from memory
#define MMC_READ_MULTIPLE_BLOCK 0x52              //CMD18
#define MMC_CMD_WRITEBLOCK  0x54                  //CMD20 Write block to memory
#define MMC_WRITE_BLOCK   0x58                    //CMD25
#define MMC_WRITE_MULTIPLE_BLOCK 0x59             //CMD??
#define MMC_WRITE_CSD     0x5b                    //CMD27 PROGRAM_CSD
#define MMC_SET_WRITE_PROT  0x5c                  //CMD28
#define MMC_CLR_WRITE_PROT  0x5d                  //CMD29
#define MMC_SEND_WRITE_PROT   0x5e                //CMD30
#define MMC_TAG_SECTOR_START  0x60                //CMD32
#define MMC_TAG_SECTOR_END  0x61                  //CMD33
#define MMC_UNTAG_SECTOR  0x62                    //CMD34
#define MMC_TAG_EREASE_GROUP_START 0x63           //CMD35
#define MMC_TAG_EREASE_GROUP_END 0x64             //CMD36
#define MMC_UNTAG_EREASE_GROUP  0x65              //CMD37
#define MMC_EREASE    0x66                        //CMD38
#define MMC_READ_OCR    0x67                      //CMD39
#define MMC_CRC_ON_OFF    0x68                    //CMD40

//TI added sub function for top two spi_xxx

// mmc init
char initMMC (void);
// send command to MMC
void mmcSendCmd (const char cmd, unsigned long data, const char crc);
// set MMC block length of count=2^n Byte
char mmcSetBlockLength (const unsigned long);
// read a size Byte big block beginning at the address.
char mmcReadBlock(const unsigned long address, const unsigned long count);
// write a 512 Byte big block beginning at the (aligned) adress
char mmcWriteBlock (const unsigned long address);
// Register arg1 der Laenge arg2 auslesen (into the buffer)
char mmcReadRegister(const char, const unsigned char);
#endif                                            /* _MMCLIB_H */
