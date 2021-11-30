#pragma once

#include "stm32f4xx_hal.h"

void InitTemperature(SPI_HandleTypeDef* hspi_, I2C_HandleTypeDef* hi2c_);
void LoopTemperature();
