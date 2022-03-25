/*
 * airconCtrl.c
 *
 *  Created on: Mar 24, 2022
 *      Author: agp
 */
#include "airconCtrl.h"
#include "time32.h"
#include "USBprint.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "usb_cdc_fops.h"
#include "stm32f4xx_hal_tim.h"

static CAProtocolCtx caProto =
{
        .undefined = NULL,
        .printHeader = CAPrintHeader,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL, // TODO: change method for calibration?
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL
};

uint32_t tempCode;
uint8_t bitIndex;
uint8_t cmd;
uint8_t cmdli;
TIM_HandleTypeDef  timerHandel;

void airconCtrlInit(TIM_HandleTypeDef htim1)
{
	timerHandel = htim1;
	initCAProtocol(&caProto, usb_cdc_rx);
    HAL_TIM_Base_Start(&timerHandel);
    __HAL_TIM_SET_COUNTER(&timerHandel, 0);

}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == GPIO_PIN_11)
  {
    if (__HAL_TIM_GET_COUNTER(&timerHandel) > 8000)
    {
      tempCode = 0;
      bitIndex = 0;
    }
    else if (__HAL_TIM_GET_COUNTER(&timerHandel) > 1700)
    {
      tempCode |= (1UL << (31-bitIndex));   // write 1
      bitIndex++;
    }
    else if (__HAL_TIM_GET_COUNTER(&timerHandel) > 1000)
    {
      tempCode &= ~(1UL << (31-bitIndex));  // write 0
      bitIndex++;
    }
    if(bitIndex == 32)
    {
      cmdli = ~tempCode; // Logical inverted last 8 bits
      cmd = tempCode >> 8; // Second last 8 bits
      if(cmdli == cmd) // Check for errors
      {
	    USBnprintf("%d", tempCode);
      }
      bitIndex = 0;
    }
    __HAL_TIM_SET_COUNTER(&timerHandel, 0);
  }
}

void airconCtrlLoop()
{
	static uint32_t tStamp = 0;

	inputCAProtocol(&caProto);

	if (tdiff_u32(HAL_GetTick(), tStamp) > 1000)
	{
		tStamp = HAL_GetTick();
		USBnprintf("Hello world\r\n");
	}
}

