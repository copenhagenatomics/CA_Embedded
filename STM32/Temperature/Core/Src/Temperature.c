/*!
 * @file    Temperature.c
 * @brief   This file contains the main program of Temperature
 * @date    2022
 * @author  Anders Gnistrup
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ADS1120.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "FLASH_readwrite.h"
#include "StmGpio.h"
#include "Temperature.h"
#include "USBprint.h"
#include "main.h"
#include "pcbversion.h"
#include "stm32f4xx_hal.h"
#include "systemInfo.h"
#include "time32.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

// Set all SPI pins high to be enable for communication
#define NO_SPI_DEVICES 5
#define CALIMEMSIZE    (NO_SPI_DEVICES * 2)

typedef struct _gpio {
    GPIO_TypeDef* port;
    uint16_t pin;
} gpio_t;

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void printTempHeader();
static void printTempStatus();
static void printTempStatusDef();
static void printTempOutputDef();

static void initPinLayout(pcbVersion ver);
static void initConnection(ADS1120Device* ads1120, int channel);
static void initSpiDevices(SPI_HandleTypeDef* hspi);
static void monitorBoardStatus();
static void getPeripheralTemperatures();
static float getInternalTemperature();

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static CAProtocolCtx caProto = {.undefined        = HALundefined,
                                .printHeader      = printTempHeader,
                                .printStatus      = printTempStatus,
                                .printStatusDef   = printTempStatusDef,
                                .printOutputDef   = printTempOutputDef,
                                .jumpToBootLoader = HALJumpToBootloader,
                                .calibration      = calibrateTypeInput,
                                .calibrationRW    = calibrateReadWrite,
                                .logging          = NULL,
                                .otpRead          = CAotpRead,
                                .otpWrite         = NULL};

static ADS1120Device ads1120[NO_SPI_DEVICES];
static float portCalVal[NO_SPI_DEVICES * 2][2];
SubBoardType subtype = 0;

static char buf[600] = {0};  // Shared by printTempStatus and printTempStatusDef

static SPI_HandleTypeDef* hspi   = NULL;
static WWDG_HandleTypeDef* hwwdg = NULL;
static CRC_HandleTypeDef* hcrc   = NULL;

static gpio_t cs[NO_SPI_DEVICES];
static gpio_t drdy[NO_SPI_DEVICES];

/***************************************************************************************************
** PRIVATE FUNCTION DEFINITIONS
***************************************************************************************************/

static void printTempHeader() {
    CAPrintHeader();
    calibrateReadWrite(false);
}

static void printTempStatus() {
    int len             = 0;
    uint32_t tempStatus = bsGetStatus();

    for (int i = 0; i < NO_SPI_DEVICES; i++) {
        if (tempStatus & TEMP_ADS1120_x_Error_Msk(i)) {
            CA_SNPRINTF(buf, len,
                        "Communication lost to the ADS1120 chip that measures temperature on port "
                        "%d and %d.\r\n",
                        i * 2, i * 2 + 1);
        }
    }
    writeUSB(buf, len);
}

static void printTempStatusDef() {
    int len = 0;

    for (int i = 0; i < NO_SPI_DEVICES; i++) {
        CA_SNPRINTF(buf, len, "0x%08" PRIx32 ",Status ADC %u\r\n",
                    (uint32_t)TEMP_ADS1120_x_Error_Msk(i), i + 1);
    }
    writeUSB(buf, len);
}

/*!
** @brief   Print the output definition for the temperature board.
*/
static void printTempOutputDef() {
    int len = 0;

    for (int i = 1; i <= NO_SPI_DEVICES; i++) {
        CA_SNPRINTF(buf, len, "Temperature p%d,degC\r\n", 2 * i - 1);
        CA_SNPRINTF(buf, len, "Temperature p%d,degC\r\n", 2 * i);
    }
    CA_SNPRINTF(buf, len, "Internal Temperature,degC\r\n");

    writeUSB(buf, len);
}

static void initPinLayout(pcbVersion ver) {
    cs[0] = (gpio_t){GPIOB, GPIO_PIN_13};
    cs[1] = (gpio_t){GPIOB, GPIO_PIN_15};
    cs[2] = (gpio_t){GPIOA, GPIO_PIN_15};
    cs[3] = (gpio_t){GPIOB, GPIO_PIN_4};
    cs[4] = (gpio_t){GPIOB, GPIO_PIN_8};

    drdy[0] = (gpio_t){GPIOB, GPIO_PIN_14};
    drdy[1] = (gpio_t){GPIOA, GPIO_PIN_8};
    drdy[2] = (gpio_t){GPIOB, GPIO_PIN_3};
    drdy[3] = (gpio_t){GPIOB, GPIO_PIN_5};
    drdy[4] = (gpio_t){GPIOB, GPIO_PIN_9};

    if (ver.major >= 6 && ver.minor >= 2) {
        cs[3]   = (gpio_t){GPIOB, GPIO_PIN_5};
        drdy[2] = (gpio_t){GPIOB, GPIO_PIN_4};
        drdy[3] = (gpio_t){GPIOB, GPIO_PIN_6};
    }
}

static void initConnection(ADS1120Device* ads1120, int channel) {
    // Configure the device.
    int ret = ADS1120Init(ads1120);
    if (ret != 0) {
        // If connection could not be established to chip
        // set temperatures to 10010 to indicate miscommunication
        ads1120->data.internalTemp = 10010;
        ads1120->data.chA          = 10010;
        ads1120->data.chB          = 10010;

        // The TEMP_ADS1120_Error_Msk maps to the bit relating to the
        // 0th ADS1120 on the temperature board. Hence, we shift the index
        // according to the relevant chip having an error.
        bsSetErrorRange(ret << (channel * 2), TEMP_ADS1120_x_Error_Msk(channel));
    }
    else {
        bsClearField(TEMP_ADS1120_x_Error_Msk(channel));
    }
}

static void initSpiDevices(SPI_HandleTypeDef* hspi) {
    for (int i = 0; i < NO_SPI_DEVICES; i++) {
        // Initialise Chip Select pin
        stmGpioInit(&ads1120[i].cs, cs[i].port, cs[i].pin, STM_GPIO_OUTPUT);

        // Initialise Data Ready input pin
        stmGpioInit(&ads1120[i].drdy, drdy[i].port, drdy[i].pin, STM_GPIO_INPUT);

        stmSetGpio(ads1120[i].cs, true);  // CS selects chip when low
        ads1120[i].hspi = hspi;
    }

    // Write 2 dummy byte on SPI wire. Yes, this seems VERY strange
    // since no CS is made so no receiver. For some reason it is needed
    // to kick the STM32 SPI interface before the first real transmission.
    // After this initial write everything seems to work just fine.
    // This could be related to the note in the documentation section 8.5.6
    // where two bytes with DIN held low should be sent after each read of data.
    uint8_t dummy[2] = {0};
    HAL_SPI_Transmit(hspi, dummy, 2, 1);

    // Now configure the devices.
    for (int i = 0; i < NO_SPI_DEVICES; i++) {
        initConnection(&ads1120[i], i);
        ads1120[i].sensor_type = (subtype == 1 || subtype == 2) ? ST_RTD : ST_THERMOCOUPLE;
    }
}

static void monitorBoardStatus() {
    for (int i = 0; i < NO_SPI_DEVICES; i++) {
        // Try to re-establish connection to ADS1120.
        // if the connection is broken
        if ((bsGetStatus() & TEMP_ADS1120_x_Error_Msk(i)) != 0) {
            initConnection(&ads1120[i], i);
            // If there are no more errors left then clear the error bit.
            if ((bsGetStatus() & TEMP_ERRORS_Msk) == 0) {
                bsClearField(BS_ERROR_Msk);
            }
        }
    }
}

static void getPeripheralTemperatures() {
    for (int i = 0; i < NO_SPI_DEVICES; i++) {
        // Try to re-establish connection to ADS1120.
        // If the connection is broken
        if ((bsGetStatus() & TEMP_ADS1120_x_Error_Msk(i)) != 0) {
            continue;
        }

        // Get measurements
        float* calPtr = &portCalVal[i * 2][0];
        int ret       = ADS1120Loop(&ads1120[i], calPtr);

        // If the return value is different from 0 - set an error.
        if (ret != 0) {
            bsSetErrorRange(ret << (i * 2), TEMP_ADS1120_x_Error_Msk(i));
        }
    }
}

static float getInternalTemperature() {
    int count           = 0;
    double internalTemp = 0;
    for (int i = 0; i < NO_SPI_DEVICES; i++) {
        // Do not include internal temperature of chip if
        // connection could not be established to ADS1120 chip.
        // Reconnection is handled in updateTempAndStates()
        if ((bsGetStatus() & TEMP_ADS1120_x_Error_Msk(i)) != 0) {
            continue;
        }

        internalTemp += ads1120[i].data.internalTemp;
        count++;
    }
    return internalTemp / count;
}

static void printValues() {
    if (!isUsbPortOpen()) {
        return;
    }

    if (bsGetField(BS_VERSION_ERROR_Msk)) {
        USBnprintf("0x%08" PRIx32 "\r\n", bsGetStatus());
        return;
    }

    int len = 0;

    for (uint32_t i = 0; i < NO_SPI_DEVICES; i++) {
        CA_SNPRINTF(buf, len, "%.2f, %.2f, ", ads1120[i].data.chA, ads1120[i].data.chB);
    }
    CA_SNPRINTF(buf, len, "%.2f, 0x%08" PRIx32 "\r\n", internalTemp, bsGetStatus());
    

    USBnprintf(
        "%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, 0x%08" PRIx32 "\r\n",
        ads1120[0].data.chA, ads1120[0].data.chB, ads1120[1].data.chA, ads1120[1].data.chB,
        ads1120[2].data.chA, ads1120[2].data.chB, ads1120[3].data.chA, ads1120[3].data.chB,
        ads1120[4].data.chA, ads1120[4].data.chB, internalTemp, bsGetStatus());
}

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

void InitTemperature(SPI_HandleTypeDef* hspi_, CRC_HandleTypeDef* hcrc_, const char* bootMsg) {
    
    initCAProtocol(&caProto, usbRx);

    if (boardSetup(Temperature, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}, TEMP_ERRORS_Msk) != 0) {
        return;
    }

    hspi  = hspi_;
    hcrc  = hcrc_;

    pcbVersion ver;
    getPcbVersion(&ver);
    initPinLayout(ver);

    /* Get subtype for checking if this is an RTD board or not */
    getBoardInfo(NULL, &subtype);

    initSpiDevices(hspi);
}

void LoopTemperature(const char* bootMsg) {
    static uint32_t timeStamp      = 0;
    static const uint32_t tsUpload = 100;

    CAhandleUserInputs(&caProto, bootMsg);

    // Check the status off the board
    monitorBoardStatus();
    // Measure temperatures
    getPeripheralTemperatures();
    float internalTemp = getInternalTemperature();

    // Upload data every "tsUpload" ms.
    if (tdiff_u32(HAL_GetTick(), timeStamp) >= tsUpload) {
        timeStamp = HAL_GetTick();

        
    }
}
