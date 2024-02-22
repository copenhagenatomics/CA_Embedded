/*!
** @file    fake_StmGpio.h
** @author  Luke W
** @date    12/10/2023
**/

#ifndef INC_STMGPIO_H_
#define INC_STMGPIO_H_

#include <stdbool.h>
#include <stdint.h>

#include "fake_stm32xxxx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define stmSetGpio(x, activate) { (x).set(&(x), activate); }
#define stmGetGpio(x)           (x).get(&(x))
#define stmToggleGpio(x)        (x).toggle(&(x))

/***************************************************************************************************
** TYPEDEFS
***************************************************************************************************/

typedef enum {
    PIN_RESET = 0,
    PIN_SET
} stmGpioPinState_t;

typedef enum
{
    STM_GPIO_OUTPUT,
    STM_GPIO_INPUT
    // More to come, speed etc.
} StmGpioMode_t;

typedef struct StmGpio
{
    void (*set)(struct StmGpio* gpio, bool activate);
    bool (*get)(struct StmGpio* gpio);
    void (*toggle)(struct StmGpio* gpio);

    StmGpioMode_t   mode;
    uint8_t         state;
} StmGpio;

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void stmGpioInit(StmGpio *ctx, GPIO_TypeDef* blk, uint16_t pin, StmGpioMode_t type);

#ifdef __cplusplus
}
#endif

#endif /* INC_STMGPIO_H_ */
