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
#include  <msp430x14x.h>

void initLED(void)
{
  P4DIR |= 0x03;
  P4SEL &= ~0x03;
  P4OUT = 0x03;
}
