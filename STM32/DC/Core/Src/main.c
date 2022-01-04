/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <math.h>
#include "string.h"
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <float.h>
#include "usb_cdc_fops.h"
#include "handleGenericMessages.h"
#include "si7051.h"
#include "inputValidation.h"
#include "ADCMonitor.h"
#include "systemInfo.h"
#include "USBprint.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_CHANNELS	6
#define ACTUATIONPORTS 4
#define ADC_CHANNEL_BUF_SIZE	400 // 400 samples each 100mSec

// Address offsets for the TIM5 timer
#define PWMPIN1 0x34
#define PWMPIN2 0x38
#define PWMPIN3 0x3C
#define PWMPIN4 0x40

#define BUTTONPIN2 0x8000
#define BUTTONPIN1 0x0008
#define BUTTONPIN4 0x0010
#define BUTTONPIN3 0x0020

#define TURNONPWM 999
#define TURNOFFPWM 0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim5;

/* USER CODE BEGIN PV */

/* ADC handling */
int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2]; // array for all ADC readings, filled by DMA.
uint16_t current_calibration[ADC_CHANNELS];

/* ADC to current calibration values */
float current_scalar = -0.011;
float current_bias = 23.43; // Offset calibrated to USB hubs.


/* General */
unsigned long lastCheckButtonTime = 0;
int tsButton = 100;


int actuationDuration[ACTUATIONPORTS] = { 0 };
int actuationStart[ACTUATIONPORTS] = { 0 };
bool port_state[ACTUATIONPORTS] = { 0 };
uint32_t ccr_states[ACTUATIONPORTS] = {0};

float si7051Val = 0;
bool isFirstWrite = true;

char inputBuffer[CIRCULAR_BUFFER_SIZE];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM5_Init(void);
/* USER CODE BEGIN PFP */
static void clearLineAndBuffer();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void printHeader()
{
    USBnprintf(systemInfo());
}

static double meanCurrent(const int16_t *pData, uint16_t channel)
{
    return current_scalar * ADCMean(pData, channel) + current_bias;
}

void printResult(int16_t *pBuffer, int noOfChannels, int noOfSamples)
{
    if (!isComPortOpen())
    {
        isFirstWrite=true;
        return;
    }
    if (isFirstWrite)
        clearLineAndBuffer();

    float temp;
    if (si7051Temp(&hi2c1, &temp) != HAL_OK)
        temp = 10000;

    USBnprintf("%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %d",
            meanCurrent(pBuffer, 0), meanCurrent(pBuffer, 1), meanCurrent(pBuffer, 2), meanCurrent(pBuffer, 3),
            temp, ADCrms(pBuffer, 5), ADCmax(pBuffer, 5));
}

void setPWMPin(int pinNumber, int pwmState, int duration) {

	if (pinNumber==0){
		TIM5->CCR1=pwmState;
	} else if (pinNumber==1){
		TIM5->CCR2=pwmState;
	} else if (pinNumber==2){
		TIM5->CCR3=pwmState;
	} else if (pinNumber==3){
		TIM5->CCR4=pwmState;
	}

	actuationStart[pinNumber] = HAL_GetTick();
	actuationDuration[pinNumber] = duration;
	ccr_states[pinNumber] = *(&TIM5->CCR1 + pinNumber);
	port_state[pinNumber] = 1;

}

void pinWrite(int pinNumber, bool turnOn){
	// Normal turn off is done by choosing min PWM value i.e. pin always low.
	if (pinNumber==0){
		TIM5->CCR1 = (turnOn) ? TURNONPWM : TURNOFFPWM;
	} else if (pinNumber==1){
		TIM5->CCR2 = (turnOn) ? TURNONPWM : TURNOFFPWM;
	} else if (pinNumber==2){
		TIM5->CCR3 = (turnOn) ? TURNONPWM : TURNOFFPWM;
	} else if (pinNumber==3){
		TIM5->CCR4 = (turnOn) ? TURNONPWM : TURNOFFPWM;
	}
}


// Turn on all pins.
void allOn() {
	for (int pinNumber = 0; pinNumber < 4; pinNumber++) {
		pinWrite(pinNumber, SET);
		actuationDuration[pinNumber] = 0; // actuationDuration=0 since it should be on indefinitely
		port_state[pinNumber] = 1;
		ccr_states[pinNumber] = *(&TIM5->CCR1 + pinNumber);
	}
}

void turnOnPin(int pinNumber) {
	pinWrite(pinNumber, SET);
	actuationDuration[pinNumber] = 0; // actuationDuration=0 since it should be on indefinitely
	port_state[pinNumber] = 1;
	ccr_states[pinNumber] = *(&TIM5->CCR1 + pinNumber);
}

void turnOnPinDuration(int pinNumber, int duration) {
	pinWrite(pinNumber, SET);
	actuationStart[pinNumber] = HAL_GetTick();
	actuationDuration[pinNumber] = duration;
	port_state[pinNumber] = 1;
	ccr_states[pinNumber] = *(&TIM5->CCR1 + pinNumber);
}


// Shuts off all pins.
void allOff() {
	for (int pinNumber = 0; pinNumber < 4; pinNumber++) {
		pinWrite(pinNumber, RESET);
		actuationDuration[pinNumber] = 0;
		port_state[pinNumber]=0;
		ccr_states[pinNumber] = *(&TIM5->CCR1 + pinNumber);
	}
}

void turnOffPin(int pinNumber) {
	pinWrite(pinNumber, RESET);
	actuationDuration[pinNumber] = 0;
	port_state[pinNumber]=0;
	ccr_states[pinNumber] = *(&TIM5->CCR1 + pinNumber);
}


void actuatePins(struct actuationInfo actuationInfo){

	// all off (pin == -1 means all pins)
	if (actuationInfo.pin == -1 && actuationInfo.pwmDutyCycle==0){
		allOff();
	// all on (pin == -1 means all pins)
	} else if (actuationInfo.pin == -1 && actuationInfo.pwmDutyCycle==100){
		allOn();
	// pX on or pX off (timeOn == -1 means indefinite)
	} else if (actuationInfo.timeOn == -1 && (actuationInfo.pwmDutyCycle==100 || actuationInfo.pwmDutyCycle==0)){
		if (actuationInfo.pwmDutyCycle == 0){
			turnOffPin(actuationInfo.pin);
		} else {
			turnOnPin(actuationInfo.pin);
		}
	// pX on YY
	} else if (actuationInfo.timeOn != -1 && actuationInfo.pwmDutyCycle == 100){
		turnOnPinDuration(actuationInfo.pin, actuationInfo.timeOn);
	// pX on ZZZ%
	} else if (actuationInfo.timeOn == -1 && actuationInfo.pwmDutyCycle != 0 && actuationInfo.pwmDutyCycle != 100){
		setPWMPin(actuationInfo.pin, actuationInfo.pwmDutyCycle, 0);
	// pX on YY ZZZ%
	} else if (actuationInfo.timeOn != -1 && actuationInfo.pwmDutyCycle != 100){
		setPWMPin(actuationInfo.pin, actuationInfo.pwmDutyCycle, actuationInfo.timeOn);
	} else {
		handleGenericMessages(inputBuffer); // should never reach this, but is implemented for potentially unknown errors.
	}
}

void handleUserInputs() {

	// Read user input
	usb_cdc_rx((uint8_t *)inputBuffer);

	// Check if there is new input
	if (inputBuffer[0] == '\0') {
		return;
	}

	struct actuationInfo actuationInfo = parseAndValidateInput(inputBuffer);

	if (!actuationInfo.isInputValid){
		handleGenericMessages(inputBuffer);
		return;
	}
	actuatePins(actuationInfo);
}

void autoOff() {
	uint64_t now = HAL_GetTick();
	for (int i = 0; i < 4; i++) {
		if ((now - actuationStart[i]) > actuationDuration[i]
				&& actuationDuration[i] != 0) {
			turnOffPin(i);
		}
	}
}

void handleButtonPress()
{
    // Button ports
    GPIO_TypeDef* button_ports[] = { Btn_2_GPIO_Port, Btn_1_GPIO_Port, Btn_4_GPIO_Port, Btn_3_GPIO_Port };
    const uint16_t buttonPins[] = {Btn_2_Pin, Btn_1_Pin, Btn_4_Pin, Btn_3_Pin};

	for (int i=0; i<4; i++)
	{
		if (HAL_GPIO_ReadPin(button_ports[i], buttonPins[i]) == 0){
			pinWrite(i, SET);
		} else if (HAL_GPIO_ReadPin(button_ports[i], buttonPins[i]) == 1){
			if (port_state[i]==1){
				unsigned long now = HAL_GetTick();
				int duration = actuationDuration[i] - (int)(now - actuationStart[i]);
				setPWMPin(i, ccr_states[i], duration);
			} else {
				pinWrite(i, RESET);
			}

		}
	}
}


void checkButtonPress(){
	uint64_t now = HAL_GetTick();
	if (now - lastCheckButtonTime > tsButton){
		handleButtonPress();
		lastCheckButtonTime = HAL_GetTick();
	}
}

static void clearLineAndBuffer(){
	// Upon first write print line and reset circular buffer to ensure no faulty misreads occurs.
	USBnprintf("reconnected");
	usb_cdc_rx_flush();
	isFirstWrite=false;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_USB_DEVICE_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_TIM5_Init();
  /* USER CODE BEGIN 2 */
  ADCMonitorInit(&hadc1, ADCBuffer, sizeof(ADCBuffer)/sizeof(ADCBuffer[0]));
  HAL_TIM_Base_Start_IT(&htim2);
  HAL_TIM_Base_Start_IT(&htim5);

  // Turn on PWM on channels with default always off
  HAL_TIM_PWM_Start(&htim5,TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim5,TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim5,TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim5,TIM_CHANNEL_4);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
      handleUserInputs();
      ADCMonitorLoop(printResult);

      // Turn off pins if they have run for requested time
      autoOff();
      checkButtonPress();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 6;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = 3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = 4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = 5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_9;
  sConfig.Rank = 6;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 399;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_ENABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM5_Init(void)
{

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 15;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 999;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim5) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim5, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim5, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim5, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim5, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

  /* USER CODE END TIM5_Init 2 */
  HAL_TIM_MspPostInit(&htim5);

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, CounterClkWise_Pin|ClkWise_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : Emerg_Off_Pin Btn_2_Pin Btn_3_Pin Btn_4_Pin
                           Btn_5_Pin Btn_6_Pin */
  GPIO_InitStruct.Pin = Emerg_Off_Pin|Btn_2_Pin|Btn_3_Pin|Btn_4_Pin
                          |Btn_5_Pin|Btn_6_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : CounterClkWise_Pin ClkWise_Pin */
  GPIO_InitStruct.Pin = CounterClkWise_Pin|ClkWise_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : Btn_1_Pin */
  GPIO_InitStruct.Pin = Btn_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(Btn_1_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
