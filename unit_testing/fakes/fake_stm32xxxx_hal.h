/*!
** @file    fake_stm32xxxx_hal.h
** @author  Luke W
** @date    12/10/2023
**/

/* Prevent inclusion of real HALs, as well as re-inclusion of this one */
#ifndef __STM32xxxx_HAL_H
#define __STM32xxxx_HAL_H
#define __STM32F4xx_HAL_H
#define __STM32H7xx_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define GPIOA (uint32_t*)(0)
#define GPIOB (uint32_t*)(0)
#define GPIOC (uint32_t*)(0)
#define GPIOD (uint32_t*)(0)
#define GPIOE (uint32_t*)(0)
#define GPIOH (uint32_t*)(0)

#define GPIO_PIN_0   0x0001
#define GPIO_PIN_1   0x0002
#define GPIO_PIN_2   0x0004
#define GPIO_PIN_3   0x0008
#define GPIO_PIN_4   0x0010
#define GPIO_PIN_5   0x0020
#define GPIO_PIN_6   0x0040
#define GPIO_PIN_7   0x0080
#define GPIO_PIN_8   0x0100
#define GPIO_PIN_9   0x0200
#define GPIO_PIN_10  0x0400
#define GPIO_PIN_11  0x0800
#define GPIO_PIN_12  0x1000
#define GPIO_PIN_13  0x2000
#define GPIO_PIN_14  0x4000
#define GPIO_PIN_15  0x8000
#define GPIO_PIN_All 0xFFFF

/***************************************************************************************************
** PUBLIC TYPEDEFS
***************************************************************************************************/

typedef struct ADC_HandleTypeDef
{
    struct {
        uint32_t NbrOfConversion;
    } Init;
} ADC_HandleTypeDef;

typedef uint32_t WWDG_HandleTypeDef;
typedef uint32_t GPIO_TypeDef;
typedef uint32_t WWDG_HandleTypeDef;
typedef uint32_t HAL_StatusTypeDef;

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

HAL_StatusTypeDef HAL_ADC_Start_DMA(ADC_HandleTypeDef* hadc, uint32_t* pData, uint32_t Length);

HAL_StatusTypeDef HAL_WWDG_Refresh(WWDG_HandleTypeDef *hwwdg);

void forceTick(uint32_t next_val);
void autoIncTick(uint32_t next_val, bool disable=false);
uint32_t HAL_GetTick(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32xxxx_HAL_H */