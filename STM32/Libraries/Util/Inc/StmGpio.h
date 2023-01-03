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
#if defined(STM32F401xC)
#include "stm32f4xx_hal.h"
#elif defined(STM32H753xx)
#include "stm32h7xx_hal.h"
#endif


#define stmSetGpio(x, activate) { (x).set(&(x), activate); }
#define stmGetGpio(x)           (x).get(&(x))
#define stmToggleGpio(x)        (x).toggle(&(x))

typedef struct StmGpio
{
    GPIO_TypeDef* blk;   // GPIO Block
    uint16_t      pin;   // Pin in the block (in/out)

    void (*set)(struct StmGpio* gpio, bool activate);
    bool (*get)(struct StmGpio* gpio);
    void (*toggle)(struct StmGpio* gpio);
} StmGpio;


typedef enum
{
    STM_GPIO_OUTPUT,
    STM_GPIO_INPUT
    // More to come, speed etc.
} StmGpioMode_t;
void stmGpioInit(StmGpio *ctx, GPIO_TypeDef* blk, uint16_t pin, StmGpioMode_t type);

#endif /* INC_STMGPIO_H_ */
