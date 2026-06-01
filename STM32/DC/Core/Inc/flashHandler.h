/*!
** @file    flashHandler.h
** @brief   Flash memory handling for the DC board (e-fuse current limits)
** @date:   01/06/2026
** @author: Luke W
*/

#ifndef DC_FLASH_HANDLER_H_
#define DC_FLASH_HANDLER_H_

#include <stdint.h>

/***************************************************************************************************
** PUBLIC FUNCTION DECLARATIONS
***************************************************************************************************/

float* fhGetCurrentLimits();

void fhLoadDeposit();
void fhSaveDeposit();

#endif /* DC_FLASH_HANDLER_H_ */
