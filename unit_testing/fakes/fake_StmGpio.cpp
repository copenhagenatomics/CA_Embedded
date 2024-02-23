/*!
** @file    fake_StmGpio.c
** @author  Luke W
** @date    12/10/2023
**/

#include "fake_StmGpio.h"

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

static void writePin(StmGpio* ctx, bool active)
{
    ctx->state = active ? PIN_SET : PIN_RESET;
}

static bool readPin(StmGpio *ctx)
{
    return (ctx->state == PIN_SET);
}

static void togglePin(struct StmGpio* ctx)
{
    /* Implemented like this so that other states (e.g. 2)*/
    switch (ctx->state)
    {
        case PIN_RESET: ctx->state = PIN_SET;
                        break;
        case PIN_SET:   ctx->state = PIN_RESET;
                        break;
    }
}

void stmGpioInit(StmGpio *ctx, GPIO_TypeDef* blk, uint16_t pin, StmGpioMode_t gpioMode)
{
    ctx->set = writePin;
    ctx->get = readPin;
    ctx->toggle = togglePin;
    ctx->mode = gpioMode;

    if(gpioMode == STM_GPIO_OUTPUT) 
    {
        ctx->state = PIN_RESET;
    }
}

