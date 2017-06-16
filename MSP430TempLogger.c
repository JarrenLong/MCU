//******************************************************************************
//  Simple Temperature Logger
//
//  hold button on power up to erase flash 0xfe00..0xffff
//  stores temp every 30 seconds
//  calibration depends on battery/chip
//  stops after 200 points and leaves LED on
//  timing is not accurate without a xtal
//  button starts and stops recording
//  copy flash data from IAR to Excel and use (x-52120)/69 to get °C
//
// Peter Jennings http://benlo.com/msp430
//******************************************************************************


#include  <msp430x20x3.h>

#define POLL_RATE 5 //   seconds between samples

#define ADCDeltaOn 2                       // ~0.5 Deg C delta with 31
#define END_FLASH  0xffc0   // depends on chip and interrupt vectors used
#define FLASH  ((char*)0xfe00)  // beginning of available flash
#define MAX_BINS (((char*)(END_FLASH))-FLASH)/2 // available number of bins
// #define MAX_BINS 100   // if you want to define custom number of samples

#define BUTTON !(P1IN & 0x04)     // button (P1.2) pressed?


static int tick;
static unsigned int min;
static unsigned int max;
static int record;
static int bin;            // bin to deposit data in
static unsigned int time;  // seconds since last save

void writeFlash(unsigned int data, short addr);
unsigned int readFlash( short addr );
void eraseFlash( );

void main(void)
{
  WDTCTL = WDTPW + WDTHOLD;             // Stop watchdog timer

  P1SEL |= 0x02;                        // P1.1 option select - read temp ADC?
  P1DIR |= 0x01;                        // Set P1.0 P1.1 to output direction
  P1OUT &= ~ 0x01;                      // P1.1 off
  P1OUT |= 0x01;                        // P1.0 on (LED)
  P1REN |= 0x04;            // P1.2 pull-up for push button
  P1OUT |= 0x04;            // P1.2 pull-up

  BCSCTL2 |= DIVS_3;                      // SMCLK/8
  WDTCTL = WDT_MDLY_32;                   // WDT Timer interval 32mS

  tick = 0;
  time = 0;
  max = 0;
  min = 0xffff;
  record = 0;
  if ( BUTTON )
     {
     eraseFlash();                        // hold button at power on to erase
     }

  for ( bin=0; bin < MAX_BINS; bin++)
      {
      if ( readFlash(bin) == 0xffff ) break;
      }


  P1OUT &= ~0x01;                         // LED off

  CCTL0 = OUTMOD_4;                         // CCR0 toggle mode
  CCR0 = 250;
  TACTL = TASSEL_2 + MC_3;                  // SMCLK, up-downmode


  SD16CTL = SD16REFON +SD16SSEL_1;          // 1.2V ref, SMCLK
  SD16INCTL0 = SD16INCH_6;                  // A6+/-
  SD16CCTL0 = SD16SNGL + SD16IE ;           // Single conv, interrupt

  IE1   |= WDTIE;                         // Enable WDT interrupt 256 mSec

  _BIS_SR(LPM0_bits + GIE);                 // Enter LPM0 with interrupt
}

// Watchdog Timer interrupt service routine
#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void)
{
SD16CCTL0 |= SD16SC;                      // Start SD16 conversion
}


#pragma vector=SD16_VECTOR               // temp reading ready
__interrupt void SD16ISR(void)
  {
  CCTL0 = OUTMOD_5;                         // CCR0 reset mode
  if ( tick & 0x03 )            // most of the time
     {
     if ( record )
        P1OUT &= ~0x01;             // LED off
     }
  else                            // about once very 1.024 seconds
     {
     if ( BUTTON )              // only read once a second
         {
         record = ~record;    // toggle record
         if ( record )        // start of recording
             {
             time = 0;
             }
         }
     if ( record )
         {
         time++;
         P1OUT  |= 0x01;           // LED on
         if ( time >= POLL_RATE )  // poll rate in seconds
             {
             time = 0;
             writeFlash( SD16MEM0, bin++);
             if ( bin >= MAX_BINS-2 )  // stop at n data points
                 {                     // leave 2 for min/max
                 writeFlash( min, bin++);
                 writeFlash( max, bin++);
                 record = 0;
                 }
             }
         }
     }
  if ( min > SD16MEM0 )    // save "real time" min and max
     min = SD16MEM0;
  if ( max < SD16MEM0 )
     max = SD16MEM0;
  tick++;                  // 256 mSec ticks
  }


void writeFlash(unsigned int data, short addr)
  {

  FCTL3 = FWKEY | LOCKA;              // clear LOCK but keep LOCKA
  FCTL2= FWKEY|FSSEL1|FN1;            //SMCLK/3
  FCTL1 = FWKEY | WRT;                // enable write

  addr = addr<<1;
  if(addr >= END_FLASH ) LPM4;  // stop

  while(FCTL3 & BUSY);
  FLASH[addr++] = data >> 8;

  while(FCTL3 & BUSY);
  FLASH[addr] = data & 0xff;

  FCTL1 = FWKEY;                      // Done, clear WRT
  FCTL3 = FWKEY | LOCK | LOCKA;       // set LOCK and LOCKA
  }


unsigned int readFlash( short addr )  // read int
  {
  unsigned int val;

  addr = addr << 1;
  val = FLASH[addr]<<8;
  val |= FLASH[++addr];
  return val;
  }


void eraseFlash( )    // note only one segment erased (modify if using 2)
  {                   // see slau144 sec 7.2
  short resetVect = *(short*)0xFFFE;  // save and replace vectors
  short sd16vect = *(short*)0xFFEA;   // as necessary...
  short timerVect = *(short*)0xFFF0;
  short WDTVect = *(short*)0xFFF4;

  FCTL3 = FWKEY | LOCKA;              // clear LOCK but keep LOCKA
  FCTL2= FWKEY|FSSEL1|FN1;            //SMCLK/3
  FCTL1 = FWKEY | ERASE;              // enable erase
  FLASH[0] = 0xff;                    // dummy write

  FCTL1 = FWKEY |   WRT;              // enable write

  *(short*)0xFFFE = resetVect;
  *(short*)0xFFEA = sd16vect;
  *(short*)0xFFF0 = timerVect;
  *(short*)0xFFF4 = WDTVect;

  FCTL1 = FWKEY;                      // Done, clear ERASE

  FCTL3 = FWKEY |
          LOCK | LOCKA;               // set LOCK and LOCKA
  }

