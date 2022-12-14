#pragma once

#if defined(STM32F401xC)
#include "stm32f4xx_hal.h"
#elif defined(STM32H753xx)
#include "stm32h7xx_hal.h"
#endif
#include <StmGpio.h>

// Specific structure for the current setup of the ADS1120.
typedef enum
{
    INPUT_TEMP,      // config for temperature reading.
    INPUT_CHA,       // config for first input
    INPUT_CHB,       // config for second input
    INPUT_CALIBRATE // calibration, measure (AVDD-AVSS)/2.
} ADS1120_input;

typedef struct ADS1120Data {
    double chA;
    double chB;
    double internalTemp;
    int16_t calibration;

    // Calibration value (AVDD-AVSS)/2
    ADS1120_input currentInput;  // Input to read.
    uint32_t      readStart;     // time stamp for last ADC acquire.
} ADS1120Data;

typedef struct ADS1120Device {
    SPI_HandleTypeDef* hspi; // Pointer to SPI interface.
    StmGpio cs;              // Chip select for the specific device.
    StmGpio drdy;            // Data ready pin. is not effected by CS

    // Data acquired from device and state information.
    ADS1120Data data;
} ADS1120Device;

/* Description: Configure a single ADS1120 device. Configuration is fixed
 * @param dev pointer to ADS1120 device.
 * @param cfg setup ADS1120 to read the specific input.
 * @return     0 on success, else negative value (See code for values)
 */
int ADS1120Init(ADS1120Device *dev);

// Loop to update the next input. Note, ADS1120Init must be called
// called before this function. It will not break bu result is undefined.
// Device will run in a loop { calibrate, temperature, CHA, CHB }
void ADS1120Loop(ADS1120Device *dev, float *type_calibration);
