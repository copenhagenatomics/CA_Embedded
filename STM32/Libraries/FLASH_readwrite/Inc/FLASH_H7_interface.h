/*
 * FLASH_H7_interface.h
 *
 *  Created on: 22 Dec 2022
 *      Author: matias
 */

#ifndef INC_FLASH_H7_INTERFACE_H_
#define INC_FLASH_H7_INTERFACE_H_

#include <stdint.h>
#ifdef HAL_CRC_MODULE_ENABLED
  #include "stm32h7xx_hal_crc.h"
#endif

uint32_t eraseSectors(uint32_t startAddress, uint16_t size);
uint32_t writeToFlashH7(uint32_t startAddress, uint32_t *data, uint16_t size);
void readFromFlashH7(uint32_t startAddress, uint32_t *data, uint16_t size);

#ifdef HAL_CRC_MODULE_ENABLED
	uint32_t writeToFlashCRC(CRC_HandleTypeDef *hcrc, uint32_t startAddress, uint32_t *data, uint16_t size);
	void readFromFlashCRC(CRC_HandleTypeDef *hcrc, uint32_t startAddress, uint32_t *data, uint16_t size);
#endif

#endif /* INC_FLASH_H7_INTERFACE_H_ */
