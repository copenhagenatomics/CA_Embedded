/*
 * gpio.h
 *
 *  Created on: 10 Nov 2021
 *      Author: agp
 */

#ifndef INC_STMGPIO_H_
#define INC_STMGPIO_H_

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"

#define stmSetGpio(x, activate) { (x).set(&(x), activate); }
#define stmGetGpio(x)           (x).get(&(x))

typedef struct StmGpio
{
    GPIO_TypeDef* blk;   // GPIO Block
    uint16_t      pin;   // Pin in the block (in/out)

    void (*set)(struct StmGpio* gpio, bool activate);
    bool (*get)(struct StmGpio* gpio);
} StmGpio;

void stmGpioInit(StmGpio *ctx, GPIO_TypeDef* blk, uint16_t pin);

#endif /* INC_STMGPIO_H_ */
