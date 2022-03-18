/*
 * gpio.c
 *
 *  Created on: 10 Nov 2021
 *      Author: agp
 */
#include <StmGpio.h>

static void writePin(StmGpio* ctx, bool active)
{
    GPIO_PinState pinState = (active) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(ctx->blk, ctx->pin, pinState);
}

static bool readPin(StmGpio *ctx)
{
    GPIO_PinState state = HAL_GPIO_ReadPin(ctx->blk, ctx->pin);
    return (state == GPIO_PIN_SET);
}

static void togglePin(struct StmGpio* ctx)
{
    HAL_GPIO_TogglePin(ctx->blk, ctx->pin);
}

void stmGpioInit(StmGpio *ctx, GPIO_TypeDef* blk, uint16_t pin, StmGpioMode_t gpioMode)
{
    ctx->blk = blk;
    ctx->pin = pin;

    ctx->set = writePin;
    ctx->get = readPin;
    ctx->toggle = togglePin;

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    switch(gpioMode)
    {
    case STM_GPIO_OUTPUT:
        GPIO_InitStruct.Pin = pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case STM_GPIO_INPUT:
        GPIO_InitStruct.Pin = pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        break;
    }
    HAL_GPIO_Init(ctx->blk, &GPIO_InitStruct);
}

