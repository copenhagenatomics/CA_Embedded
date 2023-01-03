#pragma once

#if defined(STM32F401xC)
#include "stm32f4xx_hal.h"
#elif defined(STM32H753xx)
#include "stm32h7xx_hal.h"
#endif
#include "StmGpio.h"

// error codes
#define FAULT_OPEN      10000.0
#define FAULT_SHORT_GND 10001.0
#define FAULT_SHORT_VCC 10002.0

void Max31855_Read(SPI_HandleTypeDef *hspi, StmGpio *cs, float *temp_probe, float *temp_junction);
