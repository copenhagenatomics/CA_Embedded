/*
 * AD5227x.c
 *
 *  Created on: Oct 24, 2022
 *      Author: matias
 */

#include "AD5227x.h"

/* Change gain up or down on AD5227BUJZ50-RL7 potentiometer
** Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/AD5227.pdf */
void changeGain(GPIO_PinState increaseGain, const PinGainCtrl *pin)
{
    HAL_GPIO_WritePin(pin->cs_blk, pin->cs_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(pin->blk, pin->dir, increaseGain);

    /* The potentiometer changes gain on falling edges of the clock pin.
    ** The clock pin frequency allows for max 50MHz of switching. Hence, 
    ** with a system clock of 16MHz it is possible to simply call the reset 
    ** and set instructions in sequence with no delay. */
    HAL_GPIO_WritePin(pin->blk, pin->clk, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(pin->blk, pin->clk, GPIO_PIN_SET);

    HAL_GPIO_WritePin(pin->cs_blk, pin->cs_pin, GPIO_PIN_SET);
}

int setGain(const PinGainCtrl* pin, int currentPoint)
{
    while (pin->cfg->gain != currentPoint)
    {
        GPIO_PinState increaseGain = (currentPoint < pin->cfg->gain) ? GPIO_PIN_SET: GPIO_PIN_RESET;
        changeGain(increaseGain, pin);
        (increaseGain) ? currentPoint++ : currentPoint--;
    }
    return currentPoint;
}

void initGain(const PinGainCtrl * pin)
{
    HAL_GPIO_WritePin(pin->cs_blk, pin->cs_pin, GPIO_PIN_SET);

    // Idle mode for AD5227BUJZ50-RL7 is to have clock pins set high
    HAL_GPIO_WritePin(pin->blk, pin->clk, GPIO_PIN_SET);

    // Force all AD5227BUJZ50-RL7 to lowest gain, assume that gain is at MAX.
    // It is not possible to read current gain from device during startup, and an
    // old gain will survive a CPU reset. To fix the set point go to zero from MAX.
    int	targetGain = pin->cfg->gain;
    pin->cfg->gain = 0; // Initial gain must be set to zero.
    setGain(pin, MAX_GAIN); // Assume that gain max, (yes, paranoia).

    // Now, Set the real gain. Current gain is set to zero above.
    pin->cfg->gain = targetGain;
    setGain(pin, 0);
}
