/*!
** @file    flashHandler.c
** @brief   This file contains the flash memory handling
** @date:   12/11/2024
** @author: Luke W
*/
#include <string.h>
#include <stdint.h>

#include "FLASH_readwrite.h"
#include "flashHandler.h"
#include "faultHandlers.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#ifndef FLASH_ADDR_FAULT
    extern uint32_t _FlashAddrFault;   // Variable defined in ld linker script.
    #define FLASH_ADDR_FAULT ((uint32_t) &_FlashAddrFault)
#endif

/***************************************************************************************************
** TYPEDEFS
***************************************************************************************************/

typedef struct depositUnit_t {
    /* Deposit content */
    faultInfo_t fault_info;
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

faultInfo_t* fhGetFaultInfo() {return &(dpu.fault_info);}

void fhLoadDeposit() {readFromFlash(FLASH_ADDR_FAULT, (uint8_t*) &dpu, sizeof(dpu));}
void fhSaveDeposit() {writeToFlash (FLASH_ADDR_FAULT, (uint8_t*) &dpu, sizeof(dpu));}
