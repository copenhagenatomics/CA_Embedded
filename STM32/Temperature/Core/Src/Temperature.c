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
#include "StmGpio.h"

//Board Specific Defines
#define TEMP_VALUES 11

// Forward declare static functions.
static void printHeader(); // Needed for CaProtocol callback.

// Local variables
static CAProtocolCtx caProto =
{
        .undefined = HALundefined,
        .printHeader = CAPrintHeader,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL,
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL
};

/* Actuation pin outs */
static StmGpio spiGpio[TEMP_VALUES-1];

static void handleUserInputs()
{
    char inputBuffer[CIRCULAR_BUFFER_SIZE];
    usb_cdc_rx((uint8_t*) inputBuffer);
    inputCAProtocol(&caProto, inputBuffer);
}

// Set all SPI pins high to be enable for communication
static void GPIO_INIT()
{
    GPIO_TypeDef* const gpioBlk[TEMP_VALUES-1] = { SEL0_GPIO_Port, SEL1_GPIO_Port, SEL2_GPIO_Port,
                                                   SEL3_GPIO_Port, SEL4_GPIO_Port, SEL5_GPIO_Port,
                                                   SEL6_GPIO_Port, SEL7_GPIO_Port, SEL8_GPIO_Port,
                                                   SEL9_GPIO_Port};
    const uint16_t gpioPin[TEMP_VALUES-1] = { SEL0_Pin, SEL1_Pin, SEL2_Pin,
                                              SEL3_Pin, SEL4_Pin, SEL5_Pin,
                                              SEL6_Pin, SEL7_Pin, SEL8_Pin,
                                              SEL9_Pin};

    for (int i=0; i < (TEMP_VALUES-1); i++)
    {
        stmGpioInit(&spiGpio[i], gpioBlk[i], gpioPin[i]);
        spiGpio[i].set(&spiGpio[i], true); // Default is CS high.
    }
}

static float temperatures[TEMP_VALUES] = {0}; // array where all temperatures are stored.
static void readTemperatures(SPI_HandleTypeDef* hspi, I2C_HandleTypeDef* hi2c)
{
    float junction_temperatures[TEMP_VALUES] = {0}; // Internal junction temp is not used.

    // Read from thermocouple ports
    for (int i = 0; i < TEMP_VALUES-1; i++)
    {
        Max31855_Read(hspi, &spiGpio[i], &temperatures[i], &junction_temperatures[i]);
    }

    // On board temperature
    if (si7051Temp(hi2c, &temperatures[TEMP_VALUES-1]) != HAL_OK)
        temperatures[TEMP_VALUES-1] = 10000;
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
