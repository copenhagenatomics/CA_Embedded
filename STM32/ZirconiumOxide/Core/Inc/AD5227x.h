/*
 * AD5227x.h
 *
 *  Created on: Oct 24, 2022
 *      Author: matias
 */

#ifndef INC_AD5227X_H_
#define INC_AD5227X_H_

#include <stdbool.h>
#include "main.h" // needed for GPIO pins.

#define MAX_GAIN 64

typedef struct PortCfg {
    int    gain;       // Gain on the amplifier.
    float  bias;
    float  scalar;
    float  quadrant;
} PortCfg;

typedef struct PinGainCtrl
{
	GPIO_TypeDef *const blk;	// GPIO block
    const uint16_t clk;     	// GPIO for clock.
    const uint16_t dir;     	// Direction GPIO pin for direction
	GPIO_TypeDef *const cs_blk; // GPIO block for chip select
	const uint16_t cs_pin;     	// GPIO for chip select pin.
    PortCfg* cfg;            	// pointer to the configuration saved in flash
} PinGainCtrl;

void changeGain(GPIO_PinState increaseGain, const PinGainCtrl *pin);
int setGain(const PinGainCtrl* pin, int currentPoint);
void initGain(const PinGainCtrl * pin);

#endif /* INC_AD5227X_H_ */
