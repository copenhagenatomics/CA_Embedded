/*
 * pinActuation.c
 *
 *  Created on: Sep 2, 2021
 *      Author: matias
 */


#include "main.h" // unfortunately necessary to access GPIO functions.
#include <stdbool.h>
#include "pinActuation.h"

#define PORTS	4


const uint16_t port1 = 0x0002; // addresses for the pin outs
const uint16_t port2 =  0x0008;
const uint16_t port3 =  0x0020;
const uint16_t port4 =  0x0080;

const uint16_t buttonport1 = 0x0020;
const uint16_t buttonport2 = 0x0010;
const uint16_t buttonport3 = 0x0008;
const uint16_t buttonport4 = 0x8000;


int actuationDuration[PORTS] = {0};
int actuationStart[PORTS] = {0};
int actuationPWMStart[PORTS] = {0};
bool port_state[PORTS] = { 0 };

/*PWM variables*/
float duty_frequency = 2000; // 2000ms cycles
float onPeriods[PORTS] = {0,0,0,0};
float offPeriods[PORTS] = {0,0,0,0};
bool isPinPWMOn[PORTS] = { 0 };


/* Actuation pin outs */
static const GPIO_TypeDef *const gpio_ports[] = { ctrl1_GPIO_Port,
ctrl2_GPIO_Port, ctrl3_GPIO_Port, ctrl4_GPIO_Port };
static const uint16_t *pins[] = { port1, port2, port3, port4 };


/* Button pin outs */
unsigned long lastCheckButtonTime = 0;
static const GPIO_TypeDef *const button_ports[] = { GPIOB, GPIOB, GPIOB,
btn4_GPIO_Port };
static const uint16_t *buttonPins[] = { buttonport1, buttonport2, buttonport3, buttonport4 };


void pinWrite(int pinNumber, bool val) {
	HAL_GPIO_WritePin(gpio_ports[pinNumber], pins[pinNumber], val);
}

// Shuts off all pins.
void allOff() {
	for (int pinNumber = 0; pinNumber < 4; pinNumber++) {
		pinWrite(pinNumber, 0);
		actuationDuration[pinNumber] = -1; // actuationDuration=-1 since it should be on indefinitely
		port_state[pinNumber] = 0;
		isPinPWMOn[pinNumber] = false;
	}
}

// Turn on all pins.
void allOn() {
	for (int pinNumber = 0; pinNumber < 4; pinNumber++) {
		pinWrite(pinNumber, 1);
		actuationDuration[pinNumber] = -1;
		port_state[pinNumber] = 1;
	}
}

// Turn on pin indefinitely
void turnOnPin(int pinNumber) {
	pinWrite(pinNumber, 1);
	actuationDuration[pinNumber] = -1;
	port_state[pinNumber] = 1;
}

// Turn off pin indefinitely
void turnOffPin(int pinNumber) {
	pinWrite(pinNumber, 0);
	actuationDuration[pinNumber] = -1;
	port_state[pinNumber] = 0;
}

// Turn on pin with added start timer for PWM functionality
void turnOnPWMPin(int pinNumber) {
	pinWrite(pinNumber, 1);
	port_state[pinNumber] = 1;
	actuationPWMStart[pinNumber] = HAL_GetTick();
}

// Turn on pin with added end timer for PWM functionality
void turnOffPWMPin(int pinNumber) {
	pinWrite(pinNumber, 0);
	port_state[pinNumber] = 0;
	actuationPWMStart[pinNumber] = HAL_GetTick();
}

// Turn on pin 100% for a set time
void turnOnPinDuration(int pinNumber, int duration) {
	pinWrite(pinNumber, 1);
	actuationStart[pinNumber] = HAL_GetTick();
	actuationDuration[pinNumber] = duration;
	port_state[pinNumber] = 1;
}

// Set up PWM for pin
void setPWMPin(int pinNumber, float pwmPct, float duration) {
	onPeriods[pinNumber] = pwmPct/100 * duty_frequency; // Convert from milliseconds to seconds
	offPeriods[pinNumber] = (1-pwmPct/100) * duty_frequency; // Convert from milliseconds to seconds
	isPinPWMOn[pinNumber] = true;
	actuationStart[pinNumber] = HAL_GetTick();
	actuationDuration[pinNumber] = duration;
	port_state[pinNumber] = 1;
}


// Shut off pins if they have been on for the specified duration
void autoOff() {
	unsigned long now = HAL_GetTick();
	for (int i = 0; i < 4; i++) {
		if ((now - actuationStart[i]) > actuationDuration[i]
				&& actuationDuration[i] != -1) {
			isPinPWMOn[i] = false;
			turnOffPin(i);
		}
	}
}

// Toggle pins with PWM functionality
void actuatePWM(){
	for (int i = 0; i < 4; i++) {
		if (!isPinPWMOn[i]){
			continue;
		}

		// If pin is turned on - check whether it should be turned off
		if (port_state[i]){
			if ((HAL_GetTick() - actuationPWMStart[i]) > onPeriods[i]) {
				turnOffPWMPin(i);
			}
		// If pin is turned off - check whether it should be turned on
		} else {
			if ((HAL_GetTick() - actuationPWMStart[i]) > offPeriods[i]) {
				turnOnPWMPin(i);
			}
		}
	}
}

// Turn on and off from button press
void handleButtonPress() {
	for (int i = 0; i < 4; i++) {
		if (HAL_GPIO_ReadPin(button_ports[i], buttonPins[i]) == 0) {
			HAL_GPIO_WritePin(gpio_ports[i], pins[i], 1);

		// Button press can not shut off pins if they are programmatically set
		} else if (HAL_GPIO_ReadPin(button_ports[i], buttonPins[i]) == 1
				&& port_state[i] != 1) {
			HAL_GPIO_WritePin(gpio_ports[i], pins[i], 0);
		}
	}
}

// Check for button press
void checkButtonPress() {
	int tsButton = 100;
	unsigned long now = HAL_GetTick();
	if (now - lastCheckButtonTime > tsButton) {
		handleButtonPress();
		lastCheckButtonTime = HAL_GetTick();
	}
}
