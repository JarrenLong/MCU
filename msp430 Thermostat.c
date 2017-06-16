//******************************************************************************
//  Thermostat Control using internal temperature sensor
//
// Peter Jennings http://benlo.com/msp430
//******************************************************************************


#include  <msp430x20x3.h>

#define ADCDeltaOn 2                       // ~0.5 Deg C delta with 31
#define MAX_FLASH_STORE   64
#define FLASH_STORE_BASE  ((char*)0xFE00)

short dataIndex;
short len;

static int time;
static unsigned int min;
static unsigned int max;
static int button;
static int lastbutton;     // state last cycle
static int heat;
static unsigned int turnon;
static unsigned int turnoff;
void writeFlash(unsigned int data, short addr);
unsigned int readFlash( short addr );
void eraseFlash( );

void main(void)
{
  WDTCTL = WDTPW + WDTHOLD;             // Stop watchdog timer

  P1SEL |= 0x02;                            // P1.1 option select
  P1DIR |= 0x11;                        // Set P1.0 P1.4 to output direction
  P1OUT &= ~ 0x11;                      // P1.0 & P1.4 off
  P1OUT |= 0x11;                        // P1.0 & P1.4 on (LED)

  turnon = readFlash( 2 );
  if ( turnon == 0xffff ) turnon = 0;
  turnoff = readFlash( 1 );   // 0xffff is default erase anyway

  time = 0;    // length of time since last command or start
  max = 0;
  min = 0xffff;
  heat = 1;
  lastbutton = 0;

  BCSCTL2 |= DIVS_3;                        // SMCLK/8
  WDTCTL = WDT_MDLY_32;                     // WDT Timer interval
  IE1   |= WDTIE;                             // Enable WDT interrupt

  P1REN |= 0x04;            // P1.2 pull-up
  P1OUT |= 0x04;            // P1.2 pull-up

  CCTL0 = OUTMOD_4;                         // CCR0 toggle mode
  CCR0 = 250;
  TACTL = TASSEL_2 + MC_3;                  // SMCLK, up-downmode


  SD16CTL = SD16REFON +SD16SSEL_1;          // 1.2V ref, SMCLK
  SD16INCTL0 = SD16INCH_6;                  // A6+/-
  SD16CCTL0 = SD16SNGL + SD16IE ;           // Single conv, interrupt

  _BIS_SR(LPM0_bits + GIE);                 // Enter LPM0 with interrupt
}

// Watchdog Timer interrupt service routine
#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void)
{
SD16CCTL0 |= SD16SC;                      // Start SD16 conversion
}


#pragma vector=SD16_VECTOR
__interrupt void SD16ISR(void)           // about once very 2 seconds
  {
  CCTL0 = OUTMOD_5;                         // CCR0 reset mode
  if ( time & 0x07 )            // most of the time
     {
     P1OUT &= ~0x01;                         // LED off
     }
  else
     {
     P1OUT  |= 0x01;                         // LED on
     button = !(P1IN & 0x04);                // P1.2 low => button press
     if ( lastbutton && button )    // still presssed
        {
        if ( heat ) turnoff += 10;
        else        turnon  -= 10;   // increase hysteresis
        button = 0;                  // leave heat on (or off)
        }
     else
        lastbutton = 0;
     if ( button )
        {
        lastbutton = button;
        if ( heat )
           {
           P1OUT &= ~0x10;                      // P1.1 off
           turnoff = SD16MEM0-5;
           if ( turnon > turnoff - 10 ) turnon = turnoff - 10;
           }
        else
           {
           P1OUT  |= 0x10;                         // heat on
           turnon = SD16MEM0+5;
           if (turnoff < turnon + 10 ) turnoff = turnon + 10;
           }
        eraseFlash();
        writeFlash( turnoff,1);
        writeFlash( turnon, 2);
        }
     }
  if ( !heat && (SD16MEM0 <= turnon ))
     {
     P1OUT  |= 0x10;                         // heat on
     heat = 1;
     }
  if ( heat && (SD16MEM0 >= turnoff ))
     {
     heat = 0;
     P1OUT &= ~0x10;                      // P1.1 off
     }
  if ( min > SD16MEM0 )
     min = SD16MEM0;
  if ( max < SD16MEM0 )
     max = SD16MEM0;
  time++;
  }


void writeFlash(unsigned int data, short addr)  // write by char
  {

  FCTL3 = FWKEY | LOCKA;              // clear LOCK but keep LOCKA
  FCTL2= FWKEY|FSSEL1|FN1;            //SMCLK/3
  FCTL1 = FWKEY | WRT;                // enable write

  addr = addr<<1;
  if(addr >= MAX_FLASH_STORE ) LPM4;  // stop
  while(FCTL3 & BUSY);
  FLASH_STORE_BASE[addr++] = data >> 8;
  while(FCTL3 & BUSY);
  FLASH_STORE_BASE[addr] = data & 0xff;
  FCTL1 = FWKEY;                      // Done, clear WRT
  FCTL3 = FWKEY | LOCK | LOCKA;       // set LOCK and LOCKA
  }


unsigned int readFlash( short addr )  // read int
  {
  unsigned int val;

  addr = addr << 1;
  val = FLASH_STORE_BASE[addr]<<8;
  val |= FLASH_STORE_BASE[++addr];
  return val;
  }


void eraseFlash( )
  {
  short resetVect = *(short*)0xFFFE;  // save and replace vectors
  short sd16vect = *(short*)0xFFEA;   // as necessary...
  short timerVect = *(short*)0xFFF0;
  short WDTVect = *(short*)0xFFF4;

  FCTL3 = FWKEY | LOCKA;              // clear LOCK but keep LOCKA
  FCTL2= FWKEY|FSSEL1|FN1;            //SMCLK/3
  FCTL1 = FWKEY | ERASE;              // enable erase
  FLASH_STORE_BASE[0] = 0;            // dummy write

  FCTL1 = FWKEY |   WRT;              // enable write

  *(short*)0xFFFE = resetVect;
  *(short*)0xFFEA = sd16vect;
  *(short*)0xFFF0 = timerVect;
  *(short*)0xFFF4 = WDTVect;

  FCTL1 = FWKEY;                      // Done, clear ERASE

  FCTL3 = FWKEY |
          LOCK | LOCKA;               // set LOCK and LOCKA
  }

