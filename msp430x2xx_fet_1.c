#include "msp430g2231.h"

//Description; Toggle P1.0 by xor'ing P1.0 inside of a software loop.

void InitPort1(unsigned int outputs) {
	P1DIR = outputs;
}

void OutPort1(unsigned int outPort) {
	P1OUT = outPort;
}

int InPort1(unsigned int inPort) {
	return P1IN;
}

void StopWatchdog() {
	WDTCTL = WDTPW + WDTHOLD;
}

void FlashLEDS(unsigned int greenFirst) {
  InitPort1(0xFF);
  OutPort1(0);
  
  volatile unsigned int toggle=greenFirst;            // volatile to prevent optimization
  
  for (;;)
  {
    volatile unsigned int i;            // volatile to prevent optimization

    i = 10000;                          // SW Delay
    do i--;
    while (i != 0);
    
    //P1OUT ^= (toggle ? 0x40 : 0x01); //Turn each port On/Off once per loop
    //toggle = !toggle;
    OutPort1(toggle ? 0x40 : 0x01);
    toggle = !toggle;
    //checkButton();
  }
}

void ButtonOnLED() {
	InitPort1(BIT3|BIT6);
	OutPort1(BIT1);
	
	for (;;) {
		volatile unsigned int in = P1IN;
    	if(P1IN) {
    		OutPort1(BIT6);
    	} else {
    		OutPort1(BIT1);
    	}
    }
}

inline unsigned int getPortStatus(unsigned int port, unsigned int subport)
{
    return !(subport & port);
}

void ButtonLED() {
  P1DIR |= 0x41; //P1.0/6 = output
  
  do {
  	if(1 ==  getPortStatus(P1IN, BIT3)) {P1OUT |= 0x41;} //Turn on P1.0/6
  	else {P1OUT &= ~0x41;} // Reset P1.0/6
  } while(1);
}

void AssignInterrupt() {
  P1DIR = 0x01;                             // P1.0 output, else input
  P1OUT =  BIT3;                            // P1.3 set, else reset
  P1REN |= BIT3;                            // P1.3 pullup
  P1IE |= BIT3;                             // P1.3 interrupt enabled
  P1IES |= BIT3;                            // P1.3 Hi/lo edge
  P1IFG &= ~BIT3;                           // P1.3 IFG cleared

  _BIS_SR(LPM4_bits + GIE);                 // Enter LPM4 w/interrupt
}
// Port 1 ISR (interrupt service routine)
#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
  P1OUT ^= 0x01;                            // P1.0 = toggle
  P1IFG &= ~BIT3;                           // P1.4 IFG cleared
}


int main(void)
{
	StopWatchdog();
	
  //FlashLEDS(0); //Flash green/red lights
  //ButtonLED(); //Turn LED on when button is pushed
  AssignInterrupt(); //Turn LED on when button is pushed, toggle
}
