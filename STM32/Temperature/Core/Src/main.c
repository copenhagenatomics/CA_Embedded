/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  *
  *Temperature Board
  *Temperature
 *ToDo
 * 1. Functioning SPI Comm. - Ready to test.
 * 2. SI7051 Temp.
 * 3. JunctAvg Temp.
 * 4. USB comm and Serial in. - Ready to test.
 *
 * Versions:
 * 1.0	Stable. WIP
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "handleGenericMessages.h"
#include "circular_buffer.h"
#include "usbd_cdc_if.h"
#include "si7051.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

//-------------F4xx UID--------------------
#define ID1 (*(unsigned long *)0x1FFF7A10)
#define ID2 (*(unsigned long *)0x1FFF7A14)
#define ID3 (*(unsigned long *)0x1FFF7A18)

// ***** PRODUCT INFO *****
char softwareVersion[] = "1.1";
char productType[] = "Temperature";
char mcuFamily[] = "STM32F401";
char pcbVersion[] = "V4.3";
char compileDate[] = __DATE__ " " __TIME__;

//Board Specific Defines
#define TEMP_VALUES 11

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

//error codes
#define	FAULT_OPEN		10000.0
#define	FAULT_SHORT_GND	10001.0
#define	FAULT_SHORT_VCC	10002.0

//Port number
#define PORT_NUMBER 11

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */

char inputBuffer[1024*sizeof(uint8_t)];
float temperatures[TEMP_VALUES] = {0}; // array where all temperatures are stored.
float junction_temperatures[TEMP_VALUES] = {0}; // array where all temperatures are stored.


int tsUpload = 100;

bool isFirstWrite = true;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
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

void handleInput() { // list all board specific commands.

	circular_read_command(cbuf, (uint8_t *) inputBuffer);

	// Check if there is new input
	if (inputBuffer[0]=='\0'){
		return;
	}

	handleGenericMessages(inputBuffer); //handles serial, jumpToBootloader (DMA), misreads.

}

void GPIO_INIT(void) {	// set all Chip Select pins high at startup.
	  HAL_GPIO_WritePin(SEL0_GPIO_Port, SEL0_Pin, 1);
	  HAL_GPIO_WritePin(SEL1_GPIO_Port, SEL1_Pin, 1);
	  HAL_GPIO_WritePin(SEL2_GPIO_Port, SEL2_Pin, 1);
	  HAL_GPIO_WritePin(SEL3_GPIO_Port, SEL3_Pin, 1);
	  HAL_GPIO_WritePin(SEL4_GPIO_Port, SEL4_Pin, 1);
	  HAL_GPIO_WritePin(SEL5_GPIO_Port, SEL5_Pin, 1);
	  HAL_GPIO_WritePin(SEL6_GPIO_Port, SEL6_Pin, 1);
	  HAL_GPIO_WritePin(SEL7_GPIO_Port, SEL7_Pin, 1);
	  HAL_GPIO_WritePin(SEL8_GPIO_Port, SEL8_Pin, 1);
	  HAL_GPIO_WritePin(SEL9_GPIO_Port, SEL9_Pin, 1);
}


void Max31855_Read_Temp(float *temp_probe, float *temp_junction){

	int  temperature=0;

	uint8_t DATARX[4];
	uint32_t rawData_bitString_temp;
	uint16_t rawData_bitString_junc;
	//read 4 bytes from SPI bus
	//Note that chip selecting is handled elsewhere
	HAL_SPI_Receive(&hspi1,DATARX,4,1000);

	//Merge 4 bytes into 1
	rawData_bitString_temp = DATARX[0]<<24 | DATARX[1]<<16 | DATARX[2] << 8 | DATARX[3];

	 // Checks for errors in the probe
	if(DATARX[3] & 0x01){
		*temp_probe = FAULT_OPEN;
	}
	else if(DATARX[3] & 0x02){
		*temp_probe = FAULT_SHORT_GND;
	}
	else if(DATARX[3] & 0x04){
		*temp_probe = FAULT_SHORT_VCC;
	}
	else{ //No errors
		//reading temperature
		temperature = rawData_bitString_temp >> 18;

		// Check for negative temperature
		if ((DATARX[0]&(0x80))>>7)
		{
			temperature = (DATARX[0] << 6) | (DATARX[1] >> 2);
			temperature&=0b01111111111111;
			temperature^=0b01111111111111;
		}

		// Convert to Degree Celsius
		*temp_probe = temperature * 0.25;
	}

    //calculate juntion temperature
	rawData_bitString_junc = DATARX[2] << 8 | DATARX[3];
	rawData_bitString_junc >>= 4;
	*temp_junction = (rawData_bitString_junc & 0x000007FF);

	if (rawData_bitString_junc & 0x00000800)
	{
		// 2's complement operation
		// Invert
		rawData_bitString_junc = ~rawData_bitString_junc;
		// Ensure operation involves lower 11-bit only
		*temp_junction = rawData_bitString_junc & 0x000007FF;
		// Add 1 to obtain the positive number
		*temp_junction += 1;
		// Make temperature negative
		*temp_junction *= -1;
	}

	*temp_junction = *temp_junction * 0.0625;
}


void popTemp(int port) {

	// toggles through each chip, reads probe and junction temperature.
	switch (port) {

	case 0:
		HAL_GPIO_WritePin(SEL0_GPIO_Port,SEL0_Pin,GPIO_PIN_RESET);       // Low State to enable SPI Communication
		Max31855_Read_Temp(&temperatures[port], &junction_temperatures[port]);
		HAL_GPIO_WritePin(SEL0_GPIO_Port,SEL0_Pin,GPIO_PIN_SET);         // High State to disable SPI Communication
		break;

	case 1:
		HAL_GPIO_WritePin(SEL1_GPIO_Port,SEL1_Pin,GPIO_PIN_RESET);
		Max31855_Read_Temp(&temperatures[port], &junction_temperatures[port]);
		HAL_GPIO_WritePin(SEL1_GPIO_Port,SEL1_Pin,GPIO_PIN_SET);
		break;

	case 2:
		HAL_GPIO_WritePin(SEL2_GPIO_Port,SEL2_Pin,GPIO_PIN_RESET);
		Max31855_Read_Temp(&temperatures[port], &junction_temperatures[port]);
		HAL_GPIO_WritePin(SEL2_GPIO_Port,SEL2_Pin,GPIO_PIN_SET);
		break;

	case 3:
		HAL_GPIO_WritePin(SEL3_GPIO_Port,SEL3_Pin,GPIO_PIN_RESET);
		Max31855_Read_Temp(&temperatures[port], &junction_temperatures[port]);
		HAL_GPIO_WritePin(SEL3_GPIO_Port,SEL3_Pin,GPIO_PIN_SET);
		break;

	case 4:
		HAL_GPIO_WritePin(SEL4_GPIO_Port,SEL4_Pin,GPIO_PIN_RESET);
		Max31855_Read_Temp(&temperatures[port], &junction_temperatures[port]);
		HAL_GPIO_WritePin(SEL4_GPIO_Port,SEL4_Pin,GPIO_PIN_SET);
		break;

	case 5:
		HAL_GPIO_WritePin(SEL5_GPIO_Port,SEL5_Pin,GPIO_PIN_RESET);
		Max31855_Read_Temp(&temperatures[port], &junction_temperatures[port]);
		HAL_GPIO_WritePin(SEL5_GPIO_Port,SEL5_Pin,GPIO_PIN_SET);
		break;

	case 6:
		HAL_GPIO_WritePin(SEL6_GPIO_Port,SEL6_Pin,GPIO_PIN_RESET);
		Max31855_Read_Temp(&temperatures[port], &junction_temperatures[port]);
		HAL_GPIO_WritePin(SEL6_GPIO_Port,SEL6_Pin,GPIO_PIN_SET);
		break;

	case 7:
		HAL_GPIO_WritePin(SEL7_GPIO_Port,SEL7_Pin,GPIO_PIN_RESET);
		Max31855_Read_Temp(&temperatures[port], &junction_temperatures[port]);
		HAL_GPIO_WritePin(SEL7_GPIO_Port,SEL7_Pin,GPIO_PIN_SET);
		break;

	case 8:
		HAL_GPIO_WritePin(SEL8_GPIO_Port,SEL8_Pin,GPIO_PIN_RESET);
		Max31855_Read_Temp(&temperatures[port], &junction_temperatures[port]);
		HAL_GPIO_WritePin(SEL8_GPIO_Port,SEL8_Pin,GPIO_PIN_SET);
		break;

	case 9:
		HAL_GPIO_WritePin(SEL9_GPIO_Port,SEL9_Pin,GPIO_PIN_RESET);
		Max31855_Read_Temp(&temperatures[port], &junction_temperatures[port]);
		HAL_GPIO_WritePin(SEL9_GPIO_Port,SEL9_Pin,GPIO_PIN_SET);
		HAL_Delay(1);
		break;

	case 10:
		temperatures[port]=si7051Temp(&hi2c1);
		break;

	/*
	case 11:
	//Change PORT_NUMBER to 12
	//Average Max31855 juction temps. Can serve as an alternative to si7051 if this fails.
		float sum = 0;

		for (int i = 0; i < 10; i++){
			sum =+ junction_temperatures[i];
		}
		temperatures[port] = sum/10;
		break;

*/
	}

}

void printTemperatures(void)
{
    USBnprintf("%0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f",
            temperatures[0], temperatures[1], temperatures[2], temperatures[3], temperatures[4],
            temperatures[5], temperatures[6], temperatures[7], temperatures[8], temperatures[9], temperatures[10]);
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

	unsigned long timeStamp = 0;
	int port;

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
  MX_I2C1_Init();
  MX_USB_DEVICE_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  GPIO_INIT();
  circularBufferInit();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	// Upload data every "tsUpload" ms.
	if (HAL_GetTick() - timeStamp > tsUpload)
	{
	    timeStamp = HAL_GetTick();
	    if (isComPortOpen)
	    {
	        if (isFirstWrite)
	            clearLineAndBuffer();
	        printTemperatures();
	    }
	}

	for (port = 0; port < PORT_NUMBER; port++) {	// for loop to run function that circulates through port switch cases updating temperatures[]
		popTemp(port);
	}

	handleInput();

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
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4|SEL4_Pin|SEL5_Pin|SEL6_Pin
                          |SEL7_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED_Pin|SEL0_Pin|SEL1_Pin|SEL2_Pin
                          |SEL3_Pin|SEL8_Pin|SEL9_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA4 SEL4_Pin SEL5_Pin SEL6_Pin
                           SEL7_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_4|SEL4_Pin|SEL5_Pin|SEL6_Pin
                          |SEL7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_Pin SEL0_Pin SEL1_Pin SEL2_Pin
                           SEL3_Pin SEL8_Pin SEL9_Pin */
  GPIO_InitStruct.Pin = LED_Pin|SEL0_Pin|SEL1_Pin|SEL2_Pin
                          |SEL3_Pin|SEL8_Pin|SEL9_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
