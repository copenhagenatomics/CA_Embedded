/*
 * Temperature.c
 * Description: reads temperature from devices connected to SPI.
 */

#include "Temperature.h"
#include "systemInfo.h"
#include "usb_cdc_fops.h"
#include <CAProtocol.h>
#include <CAProtocolStm.h>
#include "si7051.h"
#include "USBprint.h"
#include "max31855.h"
#include "time32.h"

// ***** PRODUCT INFO *****
char productType[] = "Temperature";
char mcuFamily[] = "STM32F401";
char pcbVersion[] = "V4.3";

//Board Specific Defines
#define TEMP_VALUES 11

// Forward declare static functions.
static void printHeader(); // Needed for CaProtocol callback.

// Local variables
static CAProtocolCtx caProto =
{
        .undefined = HALundefined,
        .printHeader = printHeader,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL,
        .calibrationRW = NULL,
        .logging = NULL
};

static float temperatures[TEMP_VALUES] = {0}; // array where all temperatures are stored.
static float junction_temperatures[TEMP_VALUES] = {0}; // array where all temperatures are stored.

/* Actuation pin outs */
static GPIO_TypeDef* const gpio_temp[TEMP_VALUES-1] = { SEL0_GPIO_Port, SEL1_GPIO_Port, SEL2_GPIO_Port,
                                                        SEL3_GPIO_Port, SEL4_GPIO_Port, SEL5_GPIO_Port,
                                                        SEL6_GPIO_Port, SEL7_GPIO_Port, SEL8_GPIO_Port,
                                                        SEL9_GPIO_Port};

static const uint16_t pins_temp[TEMP_VALUES-1] = { SEL0_Pin, SEL1_Pin, SEL2_Pin,
                                                   SEL3_Pin, SEL4_Pin, SEL5_Pin,
                                                   SEL6_Pin, SEL7_Pin, SEL8_Pin,
                                                   SEL9_Pin};

static void printHeader()
{
    USBnprintf(systemInfo(productType, mcuFamily, pcbVersion));
}

static void handleUserInputs()
{
    char inputBuffer[CIRCULAR_BUFFER_SIZE];
    usb_cdc_rx((uint8_t*) inputBuffer);
    inputCAProtocol(&caProto, inputBuffer);
}

// Set all SPI pins high to be enable for communication
static void GPIO_INIT()
{
    for (int i = 0; i<TEMP_VALUES-1; i++){
        HAL_GPIO_WritePin(gpio_temp[i], pins_temp[i], SET);
    }
}

static void readTemperatures(SPI_HandleTypeDef* hspi, I2C_HandleTypeDef* hi2c)
{
    // Read from thermocouple ports
    for (int i = 0; i < TEMP_VALUES-1; i++){
        HAL_GPIO_WritePin(gpio_temp[i],pins_temp[i],RESET);       // Low State to enable SPI Communication
        Max31855_Read_Temp(hspi, &temperatures[i], &junction_temperatures[i]);
        HAL_GPIO_WritePin(gpio_temp[i],pins_temp[i],SET);         // High State to disable SPI Communication
    }

    // On board temperature
    HAL_Delay(1);
    if (si7051Temp(hi2c, &temperatures[TEMP_VALUES-1]) != HAL_OK) {
        temperatures[TEMP_VALUES-1] = 10000;
    }
}

static void printTemperatures(void)
{
    USBnprintf("%0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f",
            temperatures[0], temperatures[1], temperatures[2], temperatures[3], temperatures[4],
            temperatures[5], temperatures[6], temperatures[7], temperatures[8], temperatures[9], temperatures[10]);
}

static void clearLineAndBuffer()
{
    // Upon first write print line and reset circular buffer to ensure no faulty misreads occurs.
    USBnprintf("reconnected");
    usb_cdc_rx_flush();
}

static SPI_HandleTypeDef* hspi = NULL;
static I2C_HandleTypeDef* hi2c = NULL;
void InitTemperature(SPI_HandleTypeDef* hspi_, I2C_HandleTypeDef* hi2c_)
{
    initCAProtocol(&caProto);

    hspi = hspi_;
    hi2c = hi2c_;
    GPIO_INIT();
}

void LoopTemperature()
{
    static uint32_t timeStamp = 0;
    static const uint32_t tsUpload = 100;
    static bool isFirstWrite = true;

    handleUserInputs(); // always allow DFU upload.
    if (hspi == NULL || hi2c == NULL)
        return;

    // Upload data every "tsUpload" ms.
    if (tdiff_u32(HAL_GetTick(),timeStamp) > tsUpload)
    {
        timeStamp = HAL_GetTick();
        if (isComPortOpen())
        {
            if (isFirstWrite)
            {
                clearLineAndBuffer();
                isFirstWrite = false;
            }
            readTemperatures(hspi, hi2c);
            printTemperatures();
        }
    }

    if (!isComPortOpen())
    {
        isFirstWrite=true;
    }
}
