/*!
** @file    flashHandler.h
** @brief   This file contains headers for the flash memory handling of the AC board
** @date:   12/11/2024
** @author: Luke W
*/

#ifndef FLASH_HANDLER_H_
#define FLASH_HANDLER_H_

#include "faultHandlers.h"

/*
** Usage:
** * Load data from flash using "fhLoadDeposit"
** * Get a part of the deposit using fhGetXX
** * Use the returned pointer to manipulate the correct part of the deposit
**   * This allows multiple variables to be saved while only writing to flash once. Writing to flash
**     multiple times in a row causes LoopControl to lose connection to the board briefly.
** * Commit data to flash by using "fhSaveDeposit"
*/

/***************************************************************************************************
** PUBLIC FUNCTION DECLARATIONS
***************************************************************************************************/

faultInfo_t* fhGetFaultInfo();

void fhLoadDeposit();
void fhSaveDeposit();

#endif /* FLASH_HANDLER_H_ */
