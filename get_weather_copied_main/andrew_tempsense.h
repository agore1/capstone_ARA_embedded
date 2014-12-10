/*
 * andrew_tempsense.h
 *
 *  Created on: Dec 1, 2014
 *      Author: austin
 */

#ifndef ANDREW_TEMPSENSE_H_
#define ANDREW_TEMPSENSE_H_

void ADCinit(void);		// Initializes the ADC to take samples
void sample(void);  // requests a sample from the ADC
int convert(void);  // Converts sample to farenhiet


#endif /* ANDREW_TEMPSENSE_H_ */
