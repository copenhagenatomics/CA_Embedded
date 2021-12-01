#pragma once

#include <stm32f4xx_hal.h>
#include <StmGpio.h>

// Specific structure for the current setup of the ADS1120.
// TODO: make this generic.
typedef struct ADS1120_data {
    int16_t chA;
    int16_t chB;
    int16_t internalTemp;
} ADS1120_data;

typedef struct ADS1120Device {
    SPI_HandleTypeDef* hspi; // Pointer to SPI interface.
    StmGpio cs;              // Chip select for the specific device.
    StmGpio drdy;            // Data ready pin. Used during reading os device.
} ADS1120Device;

/* Description: Configure a single ADS1120 device. Configuration is fixed
 * @param ads1120 pointer to ADS1120 device.
 * @return     0 on success, else negative value (See code for values)
 */
int ADS1120Configure(ADS1120Device *ads1120);

/*
 * Read data from device.
 */
int ADS1120Read(ADS1120Device *ads1120, ADS1120_data* data);
