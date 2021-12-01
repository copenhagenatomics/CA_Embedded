#pragma once

#include "stm32f4xx_hal.h"

void InitTemperature(SPI_HandleTypeDef* hspi_);
void LoopTemperature();
