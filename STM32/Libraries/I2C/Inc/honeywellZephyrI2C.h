#pragma once

/*
 * Description:
 * Provides an interface to communicate with the honeywell zephyr air flow sensor.
 */
#if defined(STM32F401xC)
#include "stm32f4xx_hal.h"
#elif defined(STM32H753xx)
#include "stm32h7xx_hal.h"
#endif

// Get sensor serial number
HAL_StatusTypeDef honeywellZephyrSerial(I2C_HandleTypeDef* hi2c, uint32_t *serialNB);

// read measured value.
HAL_StatusTypeDef honeywellZephyrRead(I2C_HandleTypeDef *hi2c, float *flowData);
