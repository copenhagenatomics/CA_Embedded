/*
 * Temperature.c
 * Description: reads temperature from devices connected to SPI.
 */

#include <stdbool.h>
#include <stdio.h>

#include "stm32f4xx_hal.h"

#include "main.h"
#include "Temperature.h"
#include "systemInfo.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "USBprint.h"
#include "time32.h"
#include "StmGpio.h"
#include "ADS1120.h"
#include "FLASH_readwrite.h"
#include "pcbversion.h"

// Local functions
static void calibrateTypeInput(int noOfCalibrations, const CACalibration* calibrations);
static void calibrateReadWrite(bool write);
static void printTempHeader();
static void printTempStatus();

// Local variables
static CAProtocolCtx caProto =
{
        .undefined = HALundefined,
        .printHeader = printTempHeader,
        .printStatus = printTempStatus,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = calibrateTypeInput,
        .calibrationRW = calibrateReadWrite,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL
};

// Set all SPI pins high to be enable for communication
#define NO_SPI_DEVICES 5
#define CALIMEMSIZE NO_SPI_DEVICES*2
static ADS1120Device ads1120[ NO_SPI_DEVICES ];
float portCalVal[NO_SPI_DEVICES*2][2];

static void printTempHeader()
{
    CAPrintHeader();
    calibrateReadWrite(false);
}

static void printTempStatus()
{
    static char buf[600] = { 0 };
    int len = 0;
    uint32_t tempStatus = bsGetStatus();

    for (int i = 0; i < NO_SPI_DEVICES; i++)
    {
        if (tempStatus & TEMP_ADS1120_x_Error_Msk(i))
        {
            len += snprintf(&buf[len], sizeof(buf) - len, 
                "Communication lost to the ADS1120 chip that measures temperature on port %d and %d.\r\n", i*2, i*2+1);
        }
    }
    writeUSB(buf, len);
}

static void initConnection(ADS1120Device *ads1120, int channel)
{
    // Configure the device.
    int ret = ADS1120Init(ads1120);
    if (ret != 0) {
        // If connection could not be established to chip
        // set temperatures to 10010 to indicate miscommunication
        ads1120->data.internalTemp = 10010;
        ads1120->data.chA = 10010;
        ads1120->data.chB = 10010;

        // The TEMP_ADS1120_Error_Msk maps to the bit relating to the
        // 0th ADS1120 on the temperature board. Hence, we shift the index
        // according to the relevant chip having an error.
        bsSetErrorRange(ret << (channel*2), TEMP_ADS1120_x_Error_Msk(channel));
    }
    else
    {
        bsClearField(TEMP_ADS1120_x_Error_Msk(channel));
    }
}

static void initSpiDevices(SPI_HandleTypeDef* hspi)
{
    // Initialise Chip Select pin
    stmGpioInit(&ads1120[0].cs, CS1_GPIO_Port, CS1_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&ads1120[1].cs, CS2_GPIO_Port, CS2_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&ads1120[2].cs, CS3_GPIO_Port, CS3_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&ads1120[3].cs, CS4_GPIO_Port, CS4_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&ads1120[4].cs, CS5_GPIO_Port, CS5_Pin, STM_GPIO_OUTPUT);

    // Initialise Data Ready input pin
    stmGpioInit(&ads1120[0].drdy, DRDY1_GPIO_Port, DRDY1_Pin, STM_GPIO_INPUT);
    stmGpioInit(&ads1120[1].drdy, DRDY2_GPIO_Port, DRDY2_Pin, STM_GPIO_INPUT);
    stmGpioInit(&ads1120[2].drdy, DRDY3_GPIO_Port, DRDY3_Pin, STM_GPIO_INPUT);
    stmGpioInit(&ads1120[3].drdy, DRDY4_GPIO_Port, DRDY4_Pin, STM_GPIO_INPUT);
    stmGpioInit(&ads1120[4].drdy, DRDY5_GPIO_Port, DRDY5_Pin, STM_GPIO_INPUT);

    for (int i=0; i < NO_SPI_DEVICES; i++) {
        stmSetGpio(ads1120[i].cs, true); // CS selects chip when low
        ads1120[i].hspi = hspi;
    }

    // Write 2 dummy byte on SPI wire. Yes, this seems VERY strange
    // since no CS is made so no receiver. For some reason it is needed
    // to kick the STM32 SPI interface before the first real transmission.
    // After this initial write everything seems to work just fine.
    // This could be related to the note in the documentation section 8.5.6
    // where two bytes with DIN held low should be sent after each read of data.
    uint8_t dummy[2] = { 0 };
    HAL_SPI_Transmit(hspi, dummy, 2, 1);

    // Now configure the devices.
    for (int i=0; i < NO_SPI_DEVICES; i++)
    {
        initConnection(&ads1120[i], i);
    }
}

static void monitorBoardStatus()
{
    for (int i=0; i < NO_SPI_DEVICES; i++)
    {
        // Try to re-establish connection to ADS1120.
        // if the connection is broken
        if ((bsGetStatus() & TEMP_ADS1120_x_Error_Msk(i)) != 0)
        {
            initConnection(&ads1120[i], i);
            // If there are no more errors left then clear the error bit.
            if ((bsGetStatus() & TEMP_No_Error_Msk) == 0)
            {
                bsClearField(BS_ERROR_Msk);
            }
        } 
    }
}

static void getPeripheralTemperatures()
{
    for (int i=0; i < NO_SPI_DEVICES; i++)
    {
        // Try to re-establish connection to ADS1120.
        // If the connection is broken
        if ((bsGetStatus() & TEMP_ADS1120_x_Error_Msk(i)) != 0) continue;

        // Get measurements
        float *calPtr = &portCalVal[i*2][0];
        int ret = ADS1120Loop(&ads1120[i], calPtr);

        // If the return value is different from 0 - set an error.
        if (ret != 0)
        {
            bsSetErrorRange(ret << (i*2), TEMP_ADS1120_x_Error_Msk(i));
        }
    }
}

static float getInternalTemperature()
{
    int count = 0;
    double internalTemp = 0;
    for (int i = 0; i < NO_SPI_DEVICES; i++)
    {
        // Do not include internal temperature of chip if
        // connection could not be established to ADS1120 chip.
        // Reconnection is handled in updateTempAndStates()
        if ((bsGetStatus() & TEMP_ADS1120_x_Error_Msk(i)) != 0) continue;

        internalTemp += ads1120[i].data.internalTemp;
        count++;
    }
    return internalTemp/count;
}

static void enableWWDG()
{
    static int isWWDGEnabled = 0;
    if (!isWWDGEnabled)
    {
        isWWDGEnabled = 1;
        __HAL_RCC_WWDG_CLK_ENABLE(); 
    }
}

static SPI_HandleTypeDef* hspi = NULL;
static WWDG_HandleTypeDef* hwwdg = NULL;
static CRC_HandleTypeDef* hcrc = NULL;
void InitTemperature(SPI_HandleTypeDef* hspi_, WWDG_HandleTypeDef* hwwdg_, CRC_HandleTypeDef* hcrc_)
{
    boardSetup(Temperature, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR});
    initCAProtocol(&caProto, usbRx);

    hspi = hspi_;
    hwwdg = hwwdg_;
    hcrc = hcrc_;

    initSensorCalibration();
    initSpiDevices(hspi);
}

void LoopTemperature(const char* bootMsg)
{
    static uint32_t timeStamp = 0;
    static const uint32_t tsUpload = 100;

    CAhandleUserInputs(&caProto, bootMsg);

    // Check the status off the board
    monitorBoardStatus();
    // Measure temperatures
    getPeripheralTemperatures();
    float internalTemp = getInternalTemperature();

    // Upload data every "tsUpload" ms.
    if (tdiff_u32(HAL_GetTick(), timeStamp) >= tsUpload)
    {
        timeStamp = HAL_GetTick();
        HAL_WWDG_Refresh(hwwdg);
        
        if (!isUsbPortOpen())
        {
            return;
        }

        // Enable wwdg now that print frequency has stabilised.
        enableWWDG(); 

        USBnprintf("%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, 0x%x"
                , ads1120[0].data.chA, ads1120[0].data.chB
                , ads1120[1].data.chA, ads1120[1].data.chB
                , ads1120[2].data.chA, ads1120[2].data.chB
                , ads1120[3].data.chA, ads1120[3].data.chB
                , ads1120[4].data.chA, ads1120[4].data.chB
                , internalTemp, bsGetStatus());
    }
}

void initSensorCalibration()
{
    if (readFromFlashCRC(hcrc, (uint32_t) FLASH_ADDR_CAL, (uint8_t *) portCalVal, sizeof(portCalVal)) != 0)
    {
        // If nothing is stored in FLASH default to type K thermocouple
        for (int i = 0; i < NO_SPI_DEVICES*2; i++)
        {
            portCalVal[i][0] = TYPE_K_DELTA;
            portCalVal[i][1] = TYPE_K_CJ_DELTA;
        }
    }
}

static void calibrateTypeInput(int noOfCalibrations, const CACalibration* calibrations)
{
    __HAL_RCC_WWDG_CLK_DISABLE();
    for (int count = 0; count < noOfCalibrations; count++)
    {
        if (1 <= calibrations[count].port && calibrations[count].port <= 10)
        {
            portCalVal[calibrations[count].port-1][0] = calibrations[count].alpha;
            portCalVal[calibrations[count].port-1][1] = calibrations[count].beta;
        }
    }
    // Update automatically when receiving new calibration values
    calibrateReadWrite(true);
    __HAL_RCC_WWDG_CLK_ENABLE();
}

static void calibrateReadWrite(bool write)
{
    if (write)
    {
        if (writeToFlashCRC(hcrc, (uint32_t) FLASH_ADDR_CAL, (uint8_t *) portCalVal, sizeof(portCalVal)) != 0) 
        { 
            USBnprintf("Calibration was not stored in FLASH"); 
        }
    }
    else
    {
        char buf[512];
        int len = 0;
        for (int i = 0; i < NO_SPI_DEVICES*2; i++)
        {
            if (i == 0)
            {
                len += snprintf(&buf[len], sizeof(buf), "Calibration: CAL");
            }
            len += snprintf(&buf[len], sizeof(buf) - len, " %d,%.10f,%.10f", i+1, portCalVal[i][0], portCalVal[i][1]);
        }
        len += snprintf(&buf[len], sizeof(buf) - len, "\r\n");
        writeUSB(buf, len);
    }
}

