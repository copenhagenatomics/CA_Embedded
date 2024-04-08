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

/* GPIO module */
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

/* Watchdog module */
#define RCC_FLAG_HSIRDY                  ((uint8_t)0x21)
#define RCC_FLAG_HSERDY                  ((uint8_t)0x31)
#define RCC_FLAG_PLLRDY                  ((uint8_t)0x39)
#define RCC_FLAG_PLLI2SRDY               ((uint8_t)0x3B)
#define RCC_FLAG_LSERDY                  ((uint8_t)0x41)
#define RCC_FLAG_LSIRDY                  ((uint8_t)0x61)
#define RCC_FLAG_BORRST                  ((uint8_t)0x79)
#define RCC_FLAG_PINRST                  ((uint8_t)0x7A)
#define RCC_FLAG_PORRST                  ((uint8_t)0x7B)
#define RCC_FLAG_SFTRST                  ((uint8_t)0x7C)
#define RCC_FLAG_IWDGRST                 ((uint8_t)0x7D)
#define RCC_FLAG_WWDGRST                 ((uint8_t)0x7E)
#define RCC_FLAG_LPWRRST                 ((uint8_t)0x7F)

#define __HAL_RCC_WWDG_CLK_DISABLE()  0
#define __HAL_RCC_CLEAR_RESET_FLAGS() 0
#define __HAL_RCC_GET_FLAG(x)         0

#define RCC (&RCC_obj)

/***************************************************************************************************
** PUBLIC TYPEDEFS
***************************************************************************************************/

typedef struct ADC_HandleTypeDef
{
    struct {
        uint32_t NbrOfConversion;
    } Init;

    /* For imitating DMA samples */
    uint32_t* dma_address;
    uint32_t  dma_length;
} ADC_HandleTypeDef;

typedef struct
{
    uint32_t CSR;            /*!< RCC clock control register,                                  Address offset: 0x00 */
} RCC_TypeDef;

typedef uint32_t WWDG_HandleTypeDef;
typedef uint32_t TIM_HandleTypeDef;
typedef uint32_t GPIO_TypeDef;
typedef uint32_t HAL_StatusTypeDef;

/***************************************************************************************************
** PUBLIC OBJECTS
***************************************************************************************************/

extern RCC_TypeDef RCC_obj;

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

/* ADC */
HAL_StatusTypeDef HAL_ADC_Start_DMA(ADC_HandleTypeDef* hadc, uint32_t* pData, uint32_t Length);

/* Watchdog */
HAL_StatusTypeDef HAL_WWDG_Refresh(WWDG_HandleTypeDef *hwwdg);

/* TIM */
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *htim);

/* HAL */
void forceTick(uint32_t next_val);
void autoIncTick(uint32_t next_val, bool disable=false);
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t Delay);

#ifdef __cplusplus
}
#endif

#endif /* __STM32xxxx_HAL_H */