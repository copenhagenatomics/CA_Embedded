/*!
 * @file    Temperature.h
 * @brief   Header file of Temperature.c
 * @date    22/10/2025
 * @author  Timothé Dodin
 */

#ifndef INC_TEMPERATURE_H_
#define INC_TEMPERATURE_H_

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

// Extern value defined in .ld linker script
extern uint32_t _FlashAddrCal;  // Starting address of calibration values in FLASH
#define FLASH_ADDR_CAL ((uintptr_t) & _FlashAddrCal)

// Industry standard values. Can be found in
// https://datasheets.maximintegrated.com/en/ds/MAX31855.pdf
#define TYPE_J_DELTA    0.000057953  // Sensitivity in V/C
#define TYPE_J_CJ_DELTA 0.000052136  // Sensitivity at cold-junction in V/C
#define TYPE_K_DELTA    0.000041276
#define TYPE_K_CJ_DELTA 0.00004073

// ---- Temperature board status register definitions ----

/* There are 5 ADS1120 chips on the temperature board which measure
 * the temperature on the 10 ports. The ADS1120 make up the main potential
 * source of hardware problems.
 * Each ADS1120 has two bits of space allocated for status codes.
 * Hence, the mask points to two bits at the time, but the bit shifting
 * changes depending on which chip is of relevance.
 */
#define TEMP_ADS1120_x_Error_Pos(x) (2 * (x))
#define TEMP_ADS1120_x_Error_Msk(x) (0x03 << TEMP_ADS1120_x_Error_Pos(x))
#define TEMP_ADS1120_ERROR_RANGE    0x3FF

// Mask to check whether there are currently any errors on the board
#define TEMP_ERRORS_Msk (BS_SYSTEM_ERRORS_Msk | TEMP_ADS1120_ERROR_RANGE)

/***************************************************************************************************
** PUBLIC FUNCTION DECLARATIONS
***************************************************************************************************/

void InitTemperature(SPI_HandleTypeDef* hspi_, WWDG_HandleTypeDef* hwwdg_,
                     CRC_HandleTypeDef* hcrc_);
void LoopTemperature(const char* bootMsg);

#endif /* INC_TEMPERATURE_H_ */
