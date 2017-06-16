#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/adc.h"
#include "driverlib/can.h"
#include "driverlib/comp.h"
#include "driverlib/cpu.h"
#include "driverlib/debug.h"
#include "driverlib/eeprom.h"
#include "driverlib/epi.h"
#include "driverlib/ethernet.h"
#include "driverlib/fan.h"
#include "driverlib/flash.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/hibernate.h"
#include "driverlib/i2c.h"
#include "driverlib/i2s.h"
#include "driverlib/interrupt.h"
#include "driverlib/lpc.h"
#include "driverlib/mpu.h"
#include "driverlib/peci.h"
#include "driverlib/pin_map.h"
#include "driverlib/pwm.h"
#include "driverlib/qei.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/rtos_bindings.h"
#include "driverlib/ssi.h"
#include "driverlib/sysctl.h"
#include "driverlib/sysexc.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"
#include "driverlib/uart.h"
#include "driverlib/udma.h"
#include "driverlib/usb.h"
#include "driverlib/watchdog.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"

#define LED_OFF   0x00
#define RED_LED   GPIO_PIN_1
#define BLUE_LED  GPIO_PIN_2
#define GREEN_LED GPIO_PIN_3
#define RGB_LED (RED_LED|BLUE_LED|GREEN_LED)

// Defined in driverlib/pin_map.h but not working right?
#define GPIO_PA0_U0RX           0x00000001
#define GPIO_PA1_U0TX           0x00000401

typedef struct PIXEL_ {
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
} PIXEL, *PPIXEL;

#ifdef DEBUG
void__error__(char *pcFilename, unsigned long ulLine)
{
}
#endif

static inline void trap();
void initClock();
void initLEDs();
void initADC();
void initUART();
void printf(unsigned char* msg);
void setupTemperatureSensor();
void initHibernation();
void hibernateUntilWake();
void sleep(unsigned long ms);
void flash(unsigned char led, unsigned long ms);
void short_flash(unsigned char led);
void long_flash(unsigned char led);
void short_beep_flash(unsigned char led, unsigned char flashes);
void long_beep_flash(unsigned char led, unsigned char flashes);
void delay(unsigned long ulSeconds);
void initTimer(unsigned long ms);
void enableTimer();

// Interrupt callbacks
static void ResetISR(void);
static void NmiSR(void);
static void FaultISR(void);
static void IntDefaultHandler(void);
static void Timer0IntHandler(void);
static void UARTIntHandler(void);
void UARTSend(const unsigned char *pucBuffer, unsigned long ulCount);

extern void _c_int00(void);
// Linker variable that marks the top of the stack.
extern unsigned long __STACK_TOP;

//*****************************************************************************
//
// The vector table.  Note that the proper constructs must be placed on this to
// ensure that it ends up at physical address 0x0000.0000 or at the start of
// the program if located at a start address other than 0.
//
//*****************************************************************************
#pragma DATA_SECTION(g_pfnVectors, ".intvecs")
void (* const g_pfnVectors[])(void) =
{
	(void (*)(void))((unsigned long)&__STACK_TOP),
	// The initial stack pointer
		ResetISR,// The reset handler
		NmiSR,// The NMI handler
		FaultISR,// The hard fault handler
		IntDefaultHandler,// The MPU fault handler
		IntDefaultHandler,// The bus fault handler
		IntDefaultHandler,// The usage fault handler
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		IntDefaultHandler,// SVCall handler
		IntDefaultHandler,// Debug monitor handler
		0,// Reserved
		IntDefaultHandler,// The PendSV handler
		IntDefaultHandler,// The SysTick handler
		IntDefaultHandler,// GPIO Port A
		IntDefaultHandler,// GPIO Port B
		IntDefaultHandler,// GPIO Port C
		IntDefaultHandler,// GPIO Port D
		IntDefaultHandler,// GPIO Port E
		UARTIntHandler,// UART0 Rx and Tx
		IntDefaultHandler,// UART1 Rx and Tx
		IntDefaultHandler,// SSI0 Rx and Tx
		IntDefaultHandler,// I2C0 Master and Slave
		IntDefaultHandler,// PWM Fault
		IntDefaultHandler,// PWM Generator 0
		IntDefaultHandler,// PWM Generator 1
		IntDefaultHandler,// PWM Generator 2
		IntDefaultHandler,// Quadrature Encoder 0
		IntDefaultHandler,//ADC0_Seq0_ISR,// ADC0 Sequence 0
		IntDefaultHandler,//ADC0_Seq1_ISR,// ADC0 Sequence 1
		IntDefaultHandler,//ADC0_Seq2_ISR,// ADC0 Sequence 2
		IntDefaultHandler,//ADC0_Seq3_ISR,// ADC0 Sequence 3
		IntDefaultHandler,// Watchdog timer
		Timer0IntHandler,// Timer 0 subtimer A
		IntDefaultHandler,// Timer 0 subtimer B
		IntDefaultHandler,// Timer 1 subtimer A
		IntDefaultHandler,// Timer 1 subtimer B
		IntDefaultHandler,// Timer 2 subtimer A
		IntDefaultHandler,// Timer 2 subtimer B
		IntDefaultHandler,// Analog Comparator 0
		IntDefaultHandler,// Analog Comparator 1
		IntDefaultHandler,// Analog Comparator 2
		IntDefaultHandler,// System Control (PLL, OSC, BO)
		IntDefaultHandler,// FLASH Control
		IntDefaultHandler,// GPIO Port F
		IntDefaultHandler,// GPIO Port G
		IntDefaultHandler,// GPIO Port H
		IntDefaultHandler,// UART2 Rx and Tx
		IntDefaultHandler,// SSI1 Rx and Tx
		IntDefaultHandler,// Timer 3 subtimer A
		IntDefaultHandler,// Timer 3 subtimer B
		IntDefaultHandler,// I2C1 Master and Slave
		IntDefaultHandler,// Quadrature Encoder 1
		IntDefaultHandler,// CAN0
		IntDefaultHandler,// CAN1
		IntDefaultHandler,// CAN2
		IntDefaultHandler,// Ethernet
		IntDefaultHandler,// Hibernate
		IntDefaultHandler,// USB0
		IntDefaultHandler,// PWM Generator 3
		IntDefaultHandler,// uDMA Software Transfer
		IntDefaultHandler,// uDMA Error
		IntDefaultHandler,//ADC1_Seq0_ISR,// ADC1 Sequence 0
		IntDefaultHandler,//ADC1_Seq1_ISR,// ADC1 Sequence 1
		IntDefaultHandler,//ADC1_Seq2_ISR,// ADC1 Sequence 2
		IntDefaultHandler,//ADC1_Seq3_ISR,// ADC1 Sequence 3
		IntDefaultHandler,// I2S0
		IntDefaultHandler,// External Bus Interface 0
		IntDefaultHandler,// GPIO Port J
		IntDefaultHandler,// GPIO Port K
		IntDefaultHandler,// GPIO Port L
		IntDefaultHandler,// SSI2 Rx and Tx
		IntDefaultHandler,// SSI3 Rx and Tx
		IntDefaultHandler,// UART3 Rx and Tx
		IntDefaultHandler,// UART4 Rx and Tx
		IntDefaultHandler,// UART5 Rx and Tx
		IntDefaultHandler,// UART6 Rx and Tx
		IntDefaultHandler,// UART7 Rx and Tx
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		IntDefaultHandler,// I2C2 Master and Slave
		IntDefaultHandler,// I2C3 Master and Slave
		IntDefaultHandler,// Timer 4 subtimer A
		IntDefaultHandler,// Timer 4 subtimer B
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		0,// Reserved
		IntDefaultHandler,// Timer 5 subtimer A
		IntDefaultHandler,// Timer 5 subtimer B
		IntDefaultHandler,// Wide Timer 0 subtimer A
		IntDefaultHandler,// Wide Timer 0 subtimer B
		IntDefaultHandler,// Wide Timer 1 subtimer A
		IntDefaultHandler,// Wide Timer 1 subtimer B
		IntDefaultHandler,// Wide Timer 2 subtimer A
		IntDefaultHandler,// Wide Timer 2 subtimer B
		IntDefaultHandler,// Wide Timer 3 subtimer A
		IntDefaultHandler,// Wide Timer 3 subtimer B
		IntDefaultHandler,// Wide Timer 4 subtimer A
		IntDefaultHandler,// Wide Timer 4 subtimer B
		IntDefaultHandler,// Wide Timer 5 subtimer A
		IntDefaultHandler,// Wide Timer 5 subtimer B
		IntDefaultHandler,// FPU
		IntDefaultHandler,// PECI 0
		IntDefaultHandler,// LPC 0
		IntDefaultHandler,// I2C4 Master and Slave
		IntDefaultHandler,// I2C5 Master and Slave
		IntDefaultHandler,// GPIO Port M
		IntDefaultHandler,// GPIO Port N
		IntDefaultHandler,// Quadrature Encoder 2
		IntDefaultHandler,// Fan 0
		0,// Reserved
		IntDefaultHandler,// GPIO Port P (Summary or P0)
		IntDefaultHandler,// GPIO Port P1
		IntDefaultHandler,// GPIO Port P2
		IntDefaultHandler,// GPIO Port P3
		IntDefaultHandler,// GPIO Port P4
		IntDefaultHandler,// GPIO Port P5
		IntDefaultHandler,// GPIO Port P6
		IntDefaultHandler,// GPIO Port P7
		IntDefaultHandler,// GPIO Port Q (Summary or Q0)
		IntDefaultHandler,// GPIO Port Q1
		IntDefaultHandler,// GPIO Port Q2
		IntDefaultHandler,// GPIO Port Q3
		IntDefaultHandler,// GPIO Port Q4
		IntDefaultHandler,// GPIO Port Q5
		IntDefaultHandler,// GPIO Port Q6
		IntDefaultHandler,// GPIO Port Q7
		IntDefaultHandler,// GPIO Port R
		IntDefaultHandler,// GPIO Port S
		IntDefaultHandler,// PWM 1 Generator 0
		IntDefaultHandler,// PWM 1 Generator 1
		IntDefaultHandler,// PWM 1 Generator 2
		IntDefaultHandler,// PWM 1 Generator 3
		IntDefaultHandler// PWM 1 Fault
};

static unsigned long ulADC0Value[4];
static unsigned long ulTempAvg;
static unsigned long ulTempValueC;
static unsigned long ulTempValueF;

static void ResetISR(void) {
	//
	// Jump to the CCS C initialization routine.  This will enable the
	// floating-point unit as well, so that does not need to be done here.
	//
	__asm(
			"    .global _c_int00\n"
			"    b.w     _c_int00"
	);
}
static inline void trap() {
	while (1) {
	}
}
static void NmiSR(void) {
	trap();
}
static void FaultISR(void) {
	trap();
}
static void IntDefaultHandler(void) {
	trap();
}
static void Timer0IntHandler(void) {
	// Clear the timer interrupt
	ROM_TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

	/*
	 // Read the current state of the GPIO pin and
	 // write back the opposite state
	 if (ROM_GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_2)) {
	 ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3,
	 0);
	 } else {
	 ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 4);
	 }
	 */
}

static void UARTIntHandler(void) {
	unsigned long ulStatus;

	// Get the interrupt status.
	ulStatus = ROM_UARTIntStatus(UART0_BASE, true);
	// Clear the asserted interrupts.
	ROM_UARTIntClear(UART0_BASE, ulStatus);

	// Loop while there are characters in the receive FIFO.
	while (ROM_UARTCharsAvail(UART0_BASE)) {
		// Read the next character from the UART and write it back to the UART.
		ROM_UARTCharPutNonBlocking(UART0_BASE,
				ROM_UARTCharGetNonBlocking(UART0_BASE));
	}
}

// Send a string to the UART.
void UARTSend(const unsigned char *pucBuffer, unsigned long ulCount) {
	while (ulCount--) {
		// Write the next character to the UART.
		ROM_UARTCharPutNonBlocking(UART0_BASE, *pucBuffer++);
	}
}

static void getTemperature() {
	// Clear the interrupt for the ADC
	ROM_ADCIntClear(ADC0_BASE, ADC_INT_SS0);
	ROM_ADCProcessorTrigger(ADC0_BASE, ADC_INT_SS0);

	short_flash(RED_LED);

	// Wait for the ADC to cause an interrupt
	while (!ROM_ADCIntStatus(ADC0_BASE, ADC_INT_SS0, false)) {
	}

	// Collect samples from the ADC
	ROM_ADCSequenceDataGet(ADC0_BASE, ADC_INT_SS0, ulADC0Value);

	// Get an average of the samples from the ADC
	ulTempAvg = (ulADC0Value[0] + ulADC0Value[1] + ulADC0Value[2]
			+ ulADC0Value[3] + 2) / 4;
	// TempC = 147.5 - ((75 * (VREFP - VREFN) × ADCCODE) / 4096)
	ulTempValueC = (1475 - ((2475 * ulTempAvg)) / 4096) / 10;
	// TempF = ((TempC * 9) + 160) / 5;
	ulTempValueF = ((ulTempValueC * 9) + 160) / 5;

	short_flash(BLUE_LED);
}

void initLEDs() {
	// Configure GPIO to toggle red/green/blue LEDs, start with all LEDs off
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	ROM_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, RGB_LED);
	ROM_GPIOPinWrite(GPIO_PORTF_BASE, RGB_LED, LED_OFF);
}

void initClock() {
	// Set the clocking to run directly from the crystal (40MHz)
	ROM_SysCtlClockSet(
			SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN
					| SYSCTL_XTAL_16MHZ);

}

void initADC() {
	// Enable ADC0 for 250K samples/sec.
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
	ROM_SysCtlADCSpeedSet(SYSCTL_ADCSPEED_250KSPS);
}

void initUART() {
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

	// Set GPIO A0 and A1 as UART pins.
	ROM_GPIOPinConfigure(GPIO_PA0_U0RX);
	ROM_GPIOPinConfigure(GPIO_PA1_U0TX);
	ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

	// Configure the UART for 115,200, 8-N-1 operation.
	ROM_UARTConfigSetExpClk(UART0_BASE, ROM_SysCtlClockGet(), 115200,
			(UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

	ROM_IntEnable(INT_UART0);
	ROM_UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);
}

unsigned int strlen(unsigned char *src) {
	unsigned char *p = src;
	while (*p++)
		;
	return src - p;
}

void printf(unsigned char* msg) {
	UARTSend(msg, strlen(msg));
}

void initHibernation() {
	// Configure hibernation mode
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_HIBERNATE);
	ROM_HibernateEnableExpClk(ROM_SysCtlClockGet());
}

void hibernateUntilWake() {
	// Press SW2 to wake up Stellaris Launchpad and start sampling temperature sensor
	ROM_HibernateRTCSet(0);
	ROM_HibernateRTCEnable();
	ROM_HibernateRTCMatch0Set(5);
	ROM_HibernateWakeSet(HIBERNATE_WAKE_PIN | HIBERNATE_WAKE_RTC);
	ROM_HibernateRequest();
}

void setupTemperatureSensor() {
	// Disable ADC
	ROM_ADCSequenceDisable(ADC0_BASE, ADC_INT_SS0);

	// Enable processor event for input channel 0
	ROM_ADCSequenceConfigure(ADC0_BASE, ADC_INT_SS0, ADC_TRIGGER_PROCESSOR,
			ADC_CTL_CH0);
	// Enable processor event for temperature sensor
	ROM_ADCSequenceStepConfigure(ADC0_BASE, ADC_INT_SS0, ADC_TRIGGER_PROCESSOR,
			ADC_CTL_TS);
	// Enable operating in low-band region of signal from temp sensor
	ROM_ADCSequenceStepConfigure(ADC0_BASE, ADC_INT_SS0, ADC_TRIGGER_COMP0,
			ADC_CTL_TS);
	// Enable operating in mid-band region of signal from temp sensor
	ROM_ADCSequenceStepConfigure(ADC0_BASE, ADC_INT_SS0, ADC_TRIGGER_COMP1,
			ADC_CTL_TS);
	// Enable operating in high-band region of signal from temp sensor
	ROM_ADCSequenceStepConfigure(ADC0_BASE, ADC_INT_SS0, ADC_TRIGGER_COMP2,
			ADC_CTL_TS | ADC_CTL_IE | ADC_CTL_END);

	// Enable ADC
	ROM_ADCSequenceEnable(ADC0_BASE, ADC_INT_SS0);
}

void sleep(unsigned long ms) {
	unsigned long delay = ms;
	// ~1ms per cycle, rough implementation
	while (delay--)
		ROM_SysCtlDelay(((ROM_SysCtlClockGet() / 3) / 1000));
}

void flash(unsigned char led, unsigned long ms) {
	ROM_GPIOPinWrite(GPIO_PORTF_BASE, RGB_LED, led);
	sleep(ms);
	ROM_GPIOPinWrite(GPIO_PORTF_BASE, RGB_LED, LED_OFF);
}
void short_flash(unsigned char led) {
	flash(led, 250);
}
void long_flash(unsigned char led) {
	flash(led, 500);
}
void short_beep_flash(unsigned char led, unsigned char flashes) {
	do {
		short_flash(led);
		sleep(250);
	} while (--flashes);
}
void long_beep_flash(unsigned char led, unsigned char flashes) {
	do {
		long_flash(led);
		sleep(250);
	} while (--flashes);
}
// Delay for the specified number of seconds.  Depending upon the current
// SysTick value, the delay will be between N-1 and N seconds (i.e. N-1 full
// seconds are guaranteed, along with the remainder of the current second).
void delay(unsigned long ulSeconds) {
	while (ulSeconds--) {
		// Wait until the SysTick value is less than 1000.
		while (ROM_SysTickValueGet() > 1000) {
		}
		// Wait until the SysTick value is greater than 1000.
		while (ROM_SysTickValueGet() < 1000) {
		}
	}
}

void initTimer(unsigned long ms) {
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
	ROM_TimerConfigure(TIMER0_BASE, TIMER_CFG_32_BIT_PER);
	ROM_TimerLoadSet(TIMER0_BASE, TIMER_A, ms);
}

void enableTimer() {
	ROM_IntEnable(INT_TIMER0A);
	ROM_TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
	ROM_TimerEnable(TIMER0_BASE, TIMER_A);
}

int main(void) {
	// Enable lazy stacking for interrupt handlers.  This allows floating-point
	// instructions to be used within interrupt handlers, but at the expense of
	// extra stack usage.
	ROM_FPUEnable();
	ROM_FPULazyStackingEnable();

	initClock();
	initLEDs();
	initUART();

	initTimer(100);

	ROM_IntMasterEnable();

	enableTimer();

	printf((unsigned char*) "Hello world!\n");

	initADC();
	setupTemperatureSensor();

	initHibernation();
	hibernateUntilWake();

	long_beep_flash(GREEN_LED, 3);

	// Start sampling the temperature sensor using the ADC
	while (1) {
		getTemperature();
	}
}
