#pragma once

#include "stm32f4xx_hal.h"
//error codes
#define FAULT_OPEN      10000.0
#define FAULT_SHORT_GND 10001.0
#define FAULT_SHORT_VCC 10002.0

void Max31855_Read_Temp(SPI_HandleTypeDef *hspi, float *temp_probe, float *temp_junction);
