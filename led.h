/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 43):
 * Rolf Freitag (webmaster at true-random.com) wrote this file.
 * As long as you retain this notice you can do whatever
 * the LGPL (Lesser GNU public License) allows with this stuff.
 * If you think this stuff is worth it, you can send me money via
 * paypal or if we met some day you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 */
#ifndef _LED_H
#define _LED_H
#include  <msp430x14x.h>

#define GREEN_LED_ON()  P4OUT |=  0x02
#define GREEN_LED_OFF() P4OUT &= ~0x02

#define RED_LED_ON()    P4OUT |=  0x01
#define RED_LED_OFF()   P4OUT &= ~0x01

void initLED(void);
#endif
