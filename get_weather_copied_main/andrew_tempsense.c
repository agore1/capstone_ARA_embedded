/*
 * andrew_tempsense.c
 *
 *  Created on: Dec 1, 2014
 *      Author: austin
 */

#include <msp430.h>
#include "andrew_tempsense.h"

//void WDTinit(void);		// sets the watchdog timer (turns it off)
void ADCinit(void);		// Initializes the ADC to take samples
//void interruptInit(void);	// enables needed interrupts
//void clockInit(void);	// initializes the clocks to correct source and frequency

void sample(void);  // requests a sample from the ADC
int convert(void);  // Converts sample to farenhiet

// global variables
float andrew_temp = 0;
int andrew_data = 0;

/*
int main(void) {
  // setup
  WDTinit();		// turn off the watchdog timer
  clockInit();	// initialize clocks to run at 1MHZ
  ADCinit();		// set up the ADC for sampling
  interruptInit();	// set up interrupts last, so setup isn't messed up


  while (1)
  {
    sample();
    convert();
    _nop();  // breakpt
    __delay_cycles(10000);
  }
}
*/

// Convert ADC andrew_data to andrew_temp in farenheit
int convert(void) {
	float millivolts = andrew_data * (3300.0/4096.0);  // 3.3 v is reference voltage, 12^2 = 4096 possible bins
	float celcius = (millivolts - 500.0) / 10.0;  // from data sheet of TMP36G
	andrew_temp = (celcius  * 1.8 + 32.0);  // basic c to f conversion
	return (int) andrew_temp;
}

// Start conversion and read result
void sample(void) {
	ADC12CTL0 |= ADC12SC;         // Start conversion
	while (!(ADC12IFG & BIT0));
	andrew_data = ADC12MEM0;			  // get data from ADC reading
}


// Enable ADC to sample on demand
void ADCinit(void){
	  P6SEL |= 0x01;         // Enable A/D channel A0 - p6.0
	  REFCTL0 &= ~REFMSTR;    // Reset REFMSTR to hand over control to ADC12_A ref control registers
	  ADC12CTL0 = ADC12ON + ADC12SHT0_15 + ADC12REFON;  // turn it on, set sampling time for 16 cycles

	  ADC12CTL1 = ADC12SHP;   // Use sampling timer
	  ADC12MCTL0 = ADC12SREF_0;   // Vr+=VCC (3.3v) and Vr-=AVss (0V), input from A0 (p6.0)

	  __delay_cycles(1000000);   // let this thing settle

	  ADC12CTL0 |= ADC12ENC;  // Enable conversions
}


/*
void WDTinit(void) {
	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
}

void interruptInit(void) {
	_BIS_SR(GIE);		// enable global interrupts
}


// set clock to 16MHZ to match IR demo
void clockInit(void) {
	UCSCTL0 = 0x0000;                         // Set lowest possible DCOx, MODx
	UCSCTL1 = DCORSEL_5;                      // Select DCO range 24MHz operation
	UCSCTL2 = FLLD_1 + 486;                   // Set DCO Multiplier for 12MHz
	                                          // (N + 1) * FLLRef = Fdco
	                                          // (486+1) * 32768 = 16MHz
	                                          // Set FLL Div = fDCOCLK/2
	__bic_SR_register(SCG0);                  // Enable the FLL control loop
}

*/
