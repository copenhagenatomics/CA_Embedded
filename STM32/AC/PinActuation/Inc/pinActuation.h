/*
 * pinActuation.h
 *
 *  Created on: Sep 2, 2021
 *      Author: matias
 */

#ifndef INC_PINACTUATION_H_
#define INC_PINACTUATION_H_

#define PORTS	4

int actuationDuration[PORTS];
int actuationStart[PORTS];
int actuationPWMStart[PORTS];
bool port_state[PORTS];

/*PWM variables*/
float duty_frequency;
float onPeriods[PORTS];
float offPeriods[PORTS];
bool isPinPWMOn[PORTS];


void pinWrite(int pinNumber, bool val);

void allOff();

void allOn();

void turnOnPin(int pinNumber);

void turnOffPin(int pinNumber);

void turnOnPWMPin(int pinNumber);

void turnOffPWMPin(int pinNumber);

void turnOnPinDuration(int pinNumber, int duration);

void setPWMPin(int pinNumber, float pwmPct, float duration);

// Shut off pins if they have been on for the specified duration
void autoOff();

void actuatePWM();

// Turn on and off from button press
void handleButtonPress();
void checkButtonPress();

#endif /* INC_PINACTUATION_H_ */
