/*!
** @file    flashHandler.c
** @brief   Flash memory handling for the DC board (e-fuse current limits)
** @date:   01/06/2026
** @author: Luke W
*/

#include <stdint.h>

#include "FLASH_readwrite.h"
#include "flashHandler.h"
#include "DCBoard.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#ifndef FLASH_ADDR_CAL
    extern uint32_t _FlashAddrCal;
    #define FLASH_ADDR_CAL ((uint32_t) &_FlashAddrCal)
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

#ifdef __cplusplus
    depositUnit_t dpu;
#else
    depositUnit_t dpu = {0};
#endif

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

float* fhGetCurrentLimits() { return dpu.currentLimits; }

void fhLoadDeposit() { readFromFlash(FLASH_ADDR_CAL, (uint8_t*)&dpu, sizeof(dpu)); }
void fhSaveDeposit() { writeToFlash (FLASH_ADDR_CAL, (uint8_t*)&dpu, sizeof(dpu)); }
