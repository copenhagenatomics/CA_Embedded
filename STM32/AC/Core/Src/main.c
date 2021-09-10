/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 *
 * ToDo Code Flow Overview (make functions and comments)
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
#include "circular_buffer.h"
#include "usbd_cdc_if.h"
#include "handleGenericMessages.h"
#include "inputValidation.h"
#include "pinActuation.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define DMA_BUF_SIZE 	5
#define ADC_CHANNELS	5
#define PORTS	4
#define TEMP_CHANNEL	0
#define ADC_CHANNEL_BUF_SIZE	100 // tbd 4kHz sampling means 25ms of data stored
#define CIRCULAR_BUFFER_SIZE 64
#define ULONG_MAX 0xFFFFFFFFUL

//-------------F4xx UID--------------------
#define ID1 (*(unsigned long *)0x1FFF7A10)
#define ID2 (*(unsigned long *)0x1FFF7A14)
#define ID3 (*(unsigned long *)0x1FFF7A18)

// ***** PRODUCT INFO *****
/*
 * Versions:
 * 1.0 Beta.
 * 1.1 Skipped.
 * 1.2 Fix HEX print for serial number.
 * 1.3 Add print heatsink temperature.
 */

char softwareVersion[] = "1.4";
char productType[] = "AC Board";
char mcuFamily[] = "STM32F401";
char pcbVersion[] = "V5.6";
char compileDate[] = __DATE__ " " __TIME__;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */

/* ADC handling */
uint16_t ADC_result[DMA_BUF_SIZE];
volatile int16_t ADC_Buffer[ADC_CHANNELS][ADC_CHANNEL_BUF_SIZE]; // array for all ADC readings, filled by DMA.
uint16_t current_calibration[ADC_CHANNELS];
uint16_t dmaBufIndex = 0;

/*ADC-to-current calibration values*/
float current_scalar = 0.0125;
float current_bias = -0.1187;//-0.058;

/*ADC-to-temperature calibration values*/
float temp_scalar = 0.0806;

/* General */
unsigned long lastPrintTime = 0;
int tsButton = 100;

float current[PORTS] = { 0 };
float boardTemperature = 0;

char inputBuffer[1024 * sizeof(uint8_t)];


bool isFirstWrite = true;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void printHeader()
{
    char buf[250] = { 0 };
    int len = 0;
    len  = snprintf(&buf[len], sizeof(buf) - len, "Serial Number: %lX%lX%lX", ID1, ID2, ID3);
    len += snprintf(&buf[len], sizeof(buf) - len, "Product Type: %s", productType);
    len += snprintf(&buf[len], sizeof(buf) - len, "Software Version: %s", softwareVersion);
    len += snprintf(&buf[len], sizeof(buf) - len, "Compile Date: %s", compileDate);
    len += snprintf(&buf[len], sizeof(buf) - len, "MCU Family: %s", mcuFamily);
    len += snprintf(&buf[len], sizeof(buf) - len, "PCB Version: %s", pcbVersion);
    USBnprintf(buf);
}

void popBuff() { // move the ADC_result to the ADC_Buffer. TempSense (PA0) ADC_Buffer[0], P1(PA2) ADC_Buffer[1], P2(PA4) ADC_Buffer[2], P3(PA6) ADC_Buffer[4], P4(PB0) ADC_Buffer[5]

	for (uint8_t var = 0; var < DMA_BUF_SIZE; var++) {
		// Subtract calibration of current readings
		ADC_Buffer[var][dmaBufIndex] = ADC_result[var]
				- current_calibration[var];
	}

	dmaBufIndex++;

	if (dmaBufIndex == ADC_CHANNEL_BUF_SIZE) {
		dmaBufIndex = 0;
	}

	//(dmaBufIndex % ADC_CHANNEL_BUF_SIZE == 0) && (dmaBufIndex = 0);

}

void current_init() {	// not sure we want to tare calibration here..

	uint8_t i = 0;
	uint16_t j = 0;
	uint64_t sum;

	for (i = 1; i < ADC_CHANNELS; i++) { // finding the average of each channel array to subtract from the readings
		sum = 0;
		for (j = 0; j < ADC_CHANNEL_BUF_SIZE; j++) {
			sum = sum + ADC_Buffer[i][j];
		}
		current_calibration[i] = (sum / ADC_CHANNEL_BUF_SIZE);
	}
}

float ADCtoCurrent(float adc_val) {
	return current_scalar * adc_val + current_bias;
}

float ADCtoTemperature(float adc_val) {
	return temp_scalar * adc_val;
}

double rmsCurrent(uint8_t channel) { // calculate all currents.

	// RMS (current board example)
	uint16_t k = 0;
	uint64_t sum = 0;
	int16_t cur_val = 0;

	for (k = 0; k < ADC_CHANNEL_BUF_SIZE; k++) { // sum squares of values zero to preBuf
		cur_val = ADC_Buffer[channel][k];
		sum = sum + (cur_val * cur_val); // add squared values to sum
	}
	float adc_avg = sqrt(sum / ADC_CHANNEL_BUF_SIZE);
	return ADCtoCurrent(adc_avg);
}

double tempAvg(){

	uint64_t sum = 0;
	int16_t cur_val = 0;

	for (uint16_t k = 0; k < ADC_CHANNEL_BUF_SIZE; k++) { // sum squares of values zero to preBuf
		cur_val = ADC_Buffer[TEMP_CHANNEL][k];
		sum += cur_val; // add squared values to sum
	}
	float adc_avg = sum / ADC_CHANNEL_BUF_SIZE;
	return ADCtoTemperature(adc_avg);
}

void printCurrentArray()
{
    USBnprintf("%.2f, %.2f, %.2f, %.2f, %.2f",
            rmsCurrent(0), rmsCurrent(1), rmsCurrent(2), rmsCurrent(3), tempAvg());
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
		}// pX on YY
	} else if (actuationInfo.timeOn != -1 && actuationInfo.pwmDutyCycle == 100){
		turnOnPinDuration(actuationInfo.pin, actuationInfo.timeOn);
	// pX on ZZZ%
	} else if (actuationInfo.timeOn == -1 && actuationInfo.pwmDutyCycle != 0 && actuationInfo.pwmDutyCycle != 100){
		setPWMPin(actuationInfo.pin, actuationInfo.pwmDutyCycle, actuationInfo.timeOn);
	// pX on YY ZZZ%
	} else if (actuationInfo.timeOn != -1 && actuationInfo.pwmDutyCycle != 100){
		setPWMPin(actuationInfo.pin, actuationInfo.pwmDutyCycle, actuationInfo.timeOn);
	} else {
		handleGenericMessages(inputBuffer); // should never reach this, but is implemented for potentially unknown errors.
	}
}

void handleUserInputs() {

	// Read user input
	circular_read_command(cbuf, (uint8_t*) inputBuffer);

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

void clearLineAndBuffer(){
	// Upon first write print line and reset circular buffer to ensure no faulty misreads occurs.
	USBnprintf("reconnected");
	circular_buf_reset(cbuf);
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
  MX_USB_DEVICE_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

    HAL_ADC_Start_DMA(&hadc1, (uint32_t *) ADC_result, DMA_BUF_SIZE);

	HAL_TIM_Base_Start_IT(&htim2);

	HAL_Delay(30);  // Takes 25ms for the buffer to fill up which is necessary for correct current calibration. Added 5ms for security.
	current_init();
	circularBufferInit();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	int tsUpload = 100;
	while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		handleUserInputs();

		// see if 100ms has passed
		if ((HAL_GetTick() - lastPrintTime) > tsUpload && isComPortOpen) {
			lastPrintTime = HAL_GetTick();

			// Upon first write print line and reset circular buffer to ensure no faulty misreads occurs.
			if (isFirstWrite){
				clearLineAndBuffer();
			}

			printCurrentArray(); // calc and print currents ( 100ms check inside here.)
		}

		// Toggle pins if needed when in pwm mode
		actuatePWM();

		// Turn off pins if they have run for requested time
		autoOff();

		checkButtonPress();

		if (!isComPortOpen){
			isFirstWrite=true;
		}
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
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
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
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
  hadc1.Init.NbrOfConversion = 5;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = 3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = 4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = 5;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, ctrl1_Pin|ctrl2_Pin|ctrl3_Pin|ctrl4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED_Pin|powerCut_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : ctrl1_Pin ctrl2_Pin ctrl3_Pin ctrl4_Pin */
  GPIO_InitStruct.Pin = ctrl1_Pin|ctrl2_Pin|ctrl3_Pin|ctrl4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_Pin powerCut_Pin */
  GPIO_InitStruct.Pin = LED_Pin|powerCut_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : btn4_Pin */
  GPIO_InitStruct.Pin = btn4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(btn4_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : btn3_Pin btn2_Pin btn1_Pin */
  GPIO_InitStruct.Pin = btn3_Pin|btn2_Pin|btn1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
	/* This is called after the conversion is completed */
	popBuff();
}
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
	while (1) {
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
