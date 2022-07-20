#pragma once

#include "stm32f4xx_hal.h"
#include "stdbool.h"

// Industry standard values. Can be found in https://datasheets.maximintegrated.com/en/ds/MAX31855.pdf
#define TYPE_J_DELTA	0.000057953 // Sensitivity in uV/C
#define TYPE_J_CJ_DELTA	0.000052136 // Sensitivity at cold-junction in uV/C
#define TYPE_K_DELTA	0.000041276
#define TYPE_K_CJ_DELTA	0.00004073

void initSensorCalibration();
void setSensorCalibration(int pinNumber, char type);
bool isCalibrationInputValid(const char *inputBuffer);
void InitTemperature(SPI_HandleTypeDef* hspi_);
void LoopTemperature();
