#ifndef STM32F4XX_HAL_H
#define STM32F4XX_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Shadow types used by STM HW.
typedef struct ADC_HandleTypeDef
{
	struct {
		uint32_t NbrOfConversion;
	} Init;
} ADC_HandleTypeDef;

// HW depended functions
void HAL_ADC_Start_DMA(ADC_HandleTypeDef* hadc, uint32_t* pData, uint32_t Length);
void HAL_Delay(uint32_t);
int USBnprintf(const char * format, ... );
void JumpToBootloader();

#ifdef __cplusplus
}
#endif

#endif
