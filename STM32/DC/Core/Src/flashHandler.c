/*!
** @file    flashHandler.c
** @brief   Flash memory handling for the DC board (e-fuse current limits)
** @date:   01/06/2026
** @author: Luke W
*/

#include <string.h>
#include <stdint.h>

#include "DCBoard.h"
#include "FLASH_readwrite.h"
#include "USBprint.h"
#include "flashHandler.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#ifndef FLASH_ADDR_CAL
extern uint32_t _FlashAddrCal;
#define FLASH_ADDR_CAL ((uint32_t)&_FlashAddrCal)
#endif

/***************************************************************************************************
** TYPEDEFS
***************************************************************************************************/

typedef struct {
    float currentLimits[DC_BOARD_NUM_PORTS];
} depositUnit_t;

/***************************************************************************************************
** PRIVATE VARIABLES
***************************************************************************************************/

depositUnit_t dpu = {0};

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

float* fhGetCurrentLimits() {
    return dpu.currentLimits;
}

void fhLoadDeposit(CRC_HandleTypeDef* hcrc) {
    if (readFromFlashCRC(hcrc, FLASH_ADDR_CAL, (uint8_t*)&dpu, sizeof(dpu)) != 0) {
        /* Will force DCBoard to use defaults */
        memset(&dpu, 0xFFFFFFFFU, sizeof(dpu));
        USBnprintf("Warning: calibration CRC error, resetting to defaults\r\n");
    }
}

void fhSaveDeposit(CRC_HandleTypeDef* hcrc) {
    writeToFlashCRC(hcrc, FLASH_ADDR_CAL, (uint8_t*)&dpu, sizeof(dpu));
}
