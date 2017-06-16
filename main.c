/*********************************************************

Program for Communikation of an MSP430F149 and an MMC via SPI in unprotected Mode.
Sytem quartz: 8 MHz, layout: see mmc.h.

Version 0.02 from 11. May 2003

Status: Everything works, but approx. 2 % of all MMC/SDCs do need some longer waiting cycles,
because some times are not limitated in the standards and every card is not strictly standard
conforming; they all do need more waiting cycles as specified in the standards.
*********************************************************/
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 43):
 * Dr. Rolf Freitag (webmaster at true-random.com) wrote this file.
 * As long as you retain this notice you can do whatever
 * the LGPL (Lesser GNU public License) allows with this stuff.
 * If you think this stuff is worth it, you can send me money via
 * paypal or if we met some day you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 */
#include  <msp430x14x.h>
#include  <string.h>
#include "mmc.h"
#include "led.h"

extern char card_state;

extern char mmc_buffer[512];

char card_state = 0;                              // card state: 0: not found, 1 found (init successfull)
unsigned long loop;

void main (void)
{
  volatile unsigned char dco_cnt = 255;

  BCSCTL1 &= ~XT2OFF;                             // XT2on
  do                                              // wait for MCLK from quartz
  {
    IFG1 &= ~OFIFG;                               // Clear OSCFault flag
    for (dco_cnt = 0xff; dco_cnt > 0; dco_cnt--); // Time for flag to set
  }
  while ((IFG1 & OFIFG) != 0);                    // OSCFault flag still set?

  WDTCTL = WDTPW + WDTHOLD;                       // Stop WDT

  BCSCTL1 = 0x07;                                 // LFXT1: XT2on: LF quartz for MCLK

  BCSCTL2 = SELM1 + SELS;                         // LFXT2: use HF quartz (XT2)

  DCOCTL = 0xe0;

  P3SEL = 0x00;
  P3DIR = 0xcf;
  P3OUT = 0x0f;

  // Port 4 Function           Dir     On/Off
  //         4.0-Led red       Out       0 - off   1 - On
  //         4.1-Led green     Out       0 - off   1 - On
  //         4.5-CardDetected   ?        0 - ?     1 - ?
  //         4.6-WriteProtected ?        0 - ?     1 - ?
  // D 7 6 5 4 3 2 1 0
  P4SEL = 0x00;                                   //   0 0 0 0 0 0 0 0
  P4DIR = 0x03;                                   //   0 0 0 0 0 0 1 1
  P4OUT = 0x00;

  // Port 5 Function           Dir       On/Off
  //         5.1-Dout          Out       0 - off    1 - On
  //         5.2-Din           Inp       0 - off    1 - On
  //         5.3-Clk           Out       -
  //         5.4-mmcCS         Out       0 - Active 1 - none Active

  P5SEL = 0x00;
  P5DIR = 0xff;
  P5OUT = 0xfe;

  P6SEL = 0x00;
  P6OUT = 0x00;
  P6DIR = 0xff;

  ADC12IE = 0;
  ADC12IFG = 0;

  initLED();
  for (;;)
  {
    // switch on red led to indicate -> insert card

    // #ifdef DeBuG0
    RED_LED_ON();
    GREEN_LED_OFF();
    // Card insert?
    while(P4IN&0x20);
    //switch off both led's
    GREEN_LED_OFF();
    RED_LED_OFF();
    //init mmc card
    if (initMMC() == MMC_SUCCESS)                 // card found
    {
      card_state |= 1;
      GREEN_LED_ON();
      // Read Out Card Type and print it or trace memory
      memset(&mmc_buffer,0,512);
      mmcReadRegister (10, 16);
      mmc_buffer[7]=0;
      // PLease mofify based on your Compiler sim io function
      // debug_printf("Multi Media Card Name: %s",&mmc_buffer[3]);

      // Fill first Block (0) with 'A'
      memset(&mmc_buffer,'A',512);                //set breakpoint and trace mmc_buffer contents
      mmcWriteBlock(0x00);
      // Fill second Block (1)-AbsAddr 512 with 'B'
      memset(&mmc_buffer,'B',512);
      mmcWriteBlock(512);

      // Read first Block back to buffer
      memset(&mmc_buffer,0x00,512);
      mmcReadBlock(0x00,512);
      memset(&mmc_buffer,0x00,512);               //set breakpoint and trace mmc_buffer contents
      mmcReadBlock(512,512);
      memset(&mmc_buffer,0x00,512);               //set breakpoint and trace mmc_buffer contents
    }

    else
    {
      //Error card not detected or rejected during writing
      // switch red led on
      card_state = 0;                             // no card
      RED_LED_ON();
    }

    // #endif

  }
  //  return;
}
