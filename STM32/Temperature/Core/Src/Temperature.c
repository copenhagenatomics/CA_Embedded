/*
 * Temperature.c
 * Description: reads temperature from devices connected to SPI.
 */

#include "Temperature.h"
#include "systemInfo.h"
#include "usb_cdc_fops.h"
#include <CAProtocol.h>
#include <CAProtocolStm.h>
#include "USBprint.h"
#include "time32.h"
#include "StmGpio.h"
#include "ADS1120.h"

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

static void handleUserInputs()
{
    char inputBuffer[CIRCULAR_BUFFER_SIZE];
    usb_cdc_rx((uint8_t*) inputBuffer);
    inputCAProtocol(&caProto, inputBuffer);
}

// Set all SPI pins high to be enable for communication
#define NO_SPI_DEVICES 5
static ADS1120Device ads1120[ NO_SPI_DEVICES ];
static int initSpiDevices(SPI_HandleTypeDef* hspi)
{
    // Initialise Chip Select pin
    stmGpioInit(&ads1120[0].cs, CS1_GPIO_Port, CS1_Pin);
    stmGpioInit(&ads1120[1].cs, CS2_GPIO_Port, CS2_Pin);
    stmGpioInit(&ads1120[2].cs, CS3_GPIO_Port, CS3_Pin);
    stmGpioInit(&ads1120[3].cs, CS4_GPIO_Port, CS4_Pin);
    stmGpioInit(&ads1120[4].cs, CS5_GPIO_Port, CS5_Pin);

    // Initialise Data Ready input pin
    stmGpioInit(&ads1120[0].drdy, DRDY1_GPIO_Port, DRDY1_Pin);
    stmGpioInit(&ads1120[1].drdy, DRDY2_GPIO_Port, DRDY2_Pin);
    stmGpioInit(&ads1120[2].drdy, DRDY3_GPIO_Port, DRDY3_Pin);
    stmGpioInit(&ads1120[3].drdy, DRDY4_GPIO_Port, DRDY4_Pin);
    stmGpioInit(&ads1120[4].drdy, DRDY5_GPIO_Port, DRDY5_Pin);

    for (int i=0; i < NO_SPI_DEVICES; i++) {
        stmSetGpio(ads1120[i].cs, true); // CS selects chip when low
        ads1120[i].hspi = hspi;
    }

    // Write a dummy byte on SPI wire. Yes, this seems VERY strange
    // since no CS is made so no receiver. For some reason it is needed
    // to kick the STM32 SPI interface before the first real transmission.
    // After this initial write everything seems to work just fine.
    uint8_t dummy = 0xAA;
    HAL_SPI_Transmit(hspi, &dummy, 1, 1);

    // Now configure the devices.
    int err = 0;
    for (int i=0; i < NO_SPI_DEVICES; i++)
    {
        int ret = ADS1120Configure(&ads1120[i]);
        if (ret != 0) {
            err |= ret << (4*i);
        }
    }
    return err;
}

static void clearLineAndBuffer()
{
    // Upon first write print line and reset circular buffer to ensure no faulty misreads occurs.
    USBnprintf("reconnected");
    usb_cdc_rx_flush();
}

static SPI_HandleTypeDef* hspi = NULL;
void InitTemperature(SPI_HandleTypeDef* hspi_)
{
    hspi = hspi_;
    initCAProtocol(&caProto);
}

void LoopTemperature()
{
    static int spiErr = 0;
    static uint32_t timeStamp = 0;
    static const uint32_t tsUpload = 100;
    static bool isFirstWrite = true;

    handleUserInputs(); // always allow DFU upload.

    // Upload data every "tsUpload" ms.
    if (tdiff_u32(HAL_GetTick(), timeStamp) > tsUpload)
    {
        timeStamp = HAL_GetTick();

        if (isComPortOpen())
        {
            if (isFirstWrite)
            {
                spiErr = initSpiDevices(hspi);
                clearLineAndBuffer();
                isFirstWrite = false;
            }

            if (spiErr == 0)
            {
                ADS1120_data data = { 0 };

                if (ADS1120Read(&ads1120[0], &data) == 0)
                    USBnprintf("Data %x %x %x", data.chA, data.chB, data.internalTemp);
                else
                    USBnprintf("Failed to read data");
            }
            else
            {
                USBnprintf("Failed to initialise ADS1120 device %X. This SW require PCB version >= 1.52", spiErr);
            }
        }
    }

    if (!isComPortOpen())
    {
        isFirstWrite=true;
    }
}
