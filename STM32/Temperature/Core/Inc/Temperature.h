#pragma once

#include "stm32f4xx_hal.h"
#include "stdbool.h"

// Industry standard values. Can be found in https://datasheets.maximintegrated.com/en/ds/MAX31855.pdf
#define TYPE_J_DELTA	0.000057953 // Sensitivity in V/C
#define TYPE_J_CJ_DELTA	0.000052136 // Sensitivity at cold-junction in V/C
#define TYPE_K_DELTA	0.000041276
#define TYPE_K_CJ_DELTA	0.00004073

/* Temperature board status register definitions */

// There are 5 ADS1120 chips on the temperature board. 
// The position and mask below is mapped to the lsb of the board
// specific area of the board status code. Hence, this maps to the
// first ADS1120 chip. 
// Hence, to indicate an error on the subsequent ADS1120s the Msk need 
// to be shifted by the index of the relevant chip. 
#define TEMP_ADS1120_Error_Pos      0U
#define TEMP_ADS1120_Error_Msk      (1UL << TEMP_ADS1120_Error_Pos)


void initSensorCalibration();
void setSensorCalibration(int pinNumber, char type);
bool isCalibrationInputValid(const char *inputBuffer);
void InitTemperature(SPI_HandleTypeDef* hspi_, WWDG_HandleTypeDef* hwwdg_, CRC_HandleTypeDef* hcrc_);
void LoopTemperature();
