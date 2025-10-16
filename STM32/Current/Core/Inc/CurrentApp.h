/*!
 * @file    CurrentApp.h
 * @brief   Header file of CurrentApp.c
 * @date    15/10/2021
 * @author  agp
*/

#ifndef INC_CURRENT_APP_H_
#define INC_CURRENT_APP_H_

#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define ADC_CHANNELS            5       // Channels: PhaseA, PhaseB, PhaseC, Fault Channel, AUX FB
#define ADC_CHANNEL_BUF_SIZE    400
#define ADC_RESOLUTION          4096
#define ADC_V                   3.3     // ADC reference voltage
#define ADC_F_S                 4000.0

#define NUM_CURRENT_CHANNELS    3

#define MOVING_AVERAGE_LENGTH   3

#define VSUPPLY_RANGE           28.05 
#define VSUPPLY_EXPECTED        24.0
#define VSUPPLY_UNDERVOLTAGE    22.00 

// Extern value defined in .ld linker script
extern uint32_t _FlashAddrCal;  // Starting address of calibration values in FLASH
#define FLASH_ADDR_CAL ((uintptr_t) &_FlashAddrCal)

/***************************************************************************************************
** PUBLIC FUNCTION DECLARATIONS
***************************************************************************************************/

void currentAppInit(ADC_HandleTypeDef* hadc, TIM_HandleTypeDef* adcTimer, CRC_HandleTypeDef* hcrc);
void currentAppLoop(const char* bootMsg);

#endif /* CURRENT_APP_H_ */