/*!
 * @file    Temperature.c
 * @brief   This file contains the main program of Temperature
 * @date    2022
 * @author  Anders Gnistrup
 */

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ADS1120.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "StmGpio.h"
#include "Temperature.h"
#include "USBprint.h"
#include "calibration.h"
#include "githash.h"
#include "main.h"
#include "pcbversion.h"
#include "stm32f4xx_hal.h"
#include "systemInfo.h"
#include "temperatureMeas.h"
#include "time32.h"
#include "uptime.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

// Value printed for the ports of an ADS1120 that does not communicate
#define TEMPERATURE_COMM_ERROR 10010.0f

// RTD boards: supply voltage of the RTD bias network
#define RTD_SUPPLY_VOLTAGE 3.3f  // [V]

#define BIAS_RES_SUB_1 2.7e3f  // [ohm] - Bias resistance for subtype 1
#define BIAS_RES_SUB_2 1.8e3f  // [ohm] - Bias resistance for subtype 2

typedef struct _gpio {
    GPIO_TypeDef* port;
    uint16_t pin;
} gpio_t;

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void printUptime(const char* input);
static void printHeader();
static void printStatus();
static void printStatusDef();
static void printOutputDef();

static void calibration(int noOfCalibrations, const CACalibration* calibrations);
static void readCalibrationFromRAM();
static void readCalibrationFromFlash();
static void writeCalibrationToFlash();

static void initPinLayout(pcbVersion ver);
static void initConnection(ADS1120Device* ads1120, int channel);
static void initSpiDevices(SPI_HandleTypeDef* hspi);
static void monitorBoardStatus();
static void getPeripheralTemperatures();
static void getInternalTemperature();
static float portTemperature(int port, float millivolt, float internalTemp);
static void printValues();

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static CAProtocolCtx caProto = {.undefined                = HALundefined,
                                .printHeader              = printHeader,
                                .printStatus              = printStatus,
                                .printStatusDef           = printStatusDef,
                                .printOutputDef           = printOutputDef,
                                .jumpToBootLoader         = HALJumpToBootloader,
                                .calibration              = calibration,
                                .readCalibrationFromRAM   = readCalibrationFromRAM,
                                .readCalibrationFromFlash = readCalibrationFromFlash,
                                .writeCalibrationToFlash  = writeCalibrationToFlash,
                                .logging                  = NULL,
                                .otpRead                  = CAotpRead,
                                .uptime                   = printUptime};

// External ADCs - Each ADC handles 2 ports
static ADS1120Device ads1120[NO_SPI_DEVICES];
static gpio_t cs[NO_SPI_DEVICES];
static gpio_t drdy[NO_SPI_DEVICES];

// Calibration
static FlashCalibration_t cal;

// Temperature readings
static float temperatures[NO_PORTS];  // [degC] - Temperature of each port
static float internalTemp = 0.0f;     // [degC] - Board internal temperature

// Subtype
// 0 = thermocouple
// 1 = RTD with 2.7 kOhm bias resistors
// 2 = RTD with 1.8 kOhm bias resistors
static SubBoardType subtype = 0;
static float biasRes        = BIAS_RES_SUB_1;

// Print buffer shared
static char buf[600];

/***************************************************************************************************
** PRIVATE FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
 * @brief Interface with uptime feature
 */
static void printUptime(const char* input) {
    uptime_inputHandler(input, printHeader);
}

/*!
 * @brief Definition of what is printed when the 'Serial' command is received
 */
static void printHeader() {
    CAPrintHeader();
    readCalibrationFromRAM();
}

/*!
 * @brief Definition of status information when the 'Status' command is received
 */
static void printStatus() {
    int len = 0;

    for (int i = 0; i < NO_SPI_DEVICES; i++) {
        if (bsGetField(TEMP_ADS1120_x_Error_Msk(i))) {
            CA_SNPRINTF(buf, len,
                        "Communication lost to the ADS1120 chip that measures temperature on port "
                        "%d and %d.\r\n",
                        i * 2, i * 2 + 1);
        }
    }

    writeUSB(buf, len);
}

/*!
 * @brief Definition of status information when the 'StatusDef' command is received
 */
static void printStatusDef() {
    int len = 0;

    for (int i = 0; i < NO_SPI_DEVICES; i++) {
        CA_SNPRINTF(buf, len, "0x%08" PRIx32 ",Status ADC %u\r\n",
                    (uint32_t)TEMP_ADS1120_x_Error_Msk(i), i + 1);
    }

    writeUSB(buf, len);
}

/*!
 * @brief Definition of output printed when the 'OutputDef' command is received
 */
static void printOutputDef() {
    int len = 0;

    for (int i = 1; i <= NO_SPI_DEVICES; i++) {
        CA_SNPRINTF(buf, len, "Temperature p%d,degC\r\n", 2 * i - 1);
        CA_SNPRINTF(buf, len, "Temperature p%d,degC\r\n", 2 * i);
    }
    CA_SNPRINTF(buf, len, "Internal Temperature,degC\r\n");

    writeUSB(buf, len);
}

/*!
 * @brief Applies the calibration sent by the user
 * @param noOfCalibrations Number of calibrations
 * @param calibrations Pointer to the calibration structure
 */
static void calibration(int noOfCalibrations, const CACalibration* calibrations) {
    calibrate(noOfCalibrations, calibrations, &cal, subtype != 0);
}

/*!
 * @brief Prints the calibration stored in RAM
 * @note  Used when "Serial" or "CAL r" is received
 */
static void readCalibrationFromRAM() {
    printCal(&cal);
}

/*!
 * @brief Prints the calibration stored in flash
 * @note  Used when "CAL m" is received
 */
static void readCalibrationFromFlash() {
    readCalFromFlash();
}

/*!
 * @brief Write the calibration stored in RAM into flash
 * @note  Used when "CAL w" is received
 */
static void writeCalibrationToFlash() {
    writeCalToFlash(&cal, sizeof(cal));
}

/*!
 * @brief Initializes pin layout
 * @param ver Board version
 */
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

/*!
 * @brief Initializes ADC connection
 * @param ads1120 ADC handler
 * @param channel
 */
static void initConnection(ADS1120Device* ads1120, int channel) {
    // Configure the device.
    int ret = ADS1120Init(ads1120);
    if (ret != 0) {
        // If connection could not be established to chip, the temperatures of its ports are
        // set to TEMPERATURE_COMM_ERROR to indicate miscommunication (see
        // getPeripheralTemperatures)

        // The TEMP_ADS1120_Error_Msk maps to the bit relating to the
        // 0th ADS1120 on the temperature board. Hence, we shift the index
        // according to the relevant chip having an error.
        bsSetErrorRange(ret << (channel * 2), TEMP_ADS1120_x_Error_Msk(channel));
    }
    else {
        bsClearField(TEMP_ADS1120_x_Error_Msk(channel));
    }
}

/*!
 * @brief Initializes connection to all ADCs
 * @param hspi SPI handler
 */
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
    }
}

/*!
 * @brief Updates status fields
 */
static void monitorBoardStatus() {
    for (int i = 0; i < NO_SPI_DEVICES; i++) {
        // Try to re-establish connection to ADS1120.
        // if the connection is broken
        if (bsGetField(TEMP_ADS1120_x_Error_Msk(i)) != 0) {
            initConnection(&ads1120[i], i);
        }
    }
    bsClearError(TEMP_ERRORS_Msk);
}

/*!
 * @brief Converts the voltage measured on a port into a temperature, according to its calibration
 * @param port Index of the port
 * @param volt Voltage at the input of the ADS1120 [V], NAN if nothing is connected
 * @param adcTemp Temperature of the ADS1120, used as cold-junction temperature [degC]
 * @return Temperature [degC], or TEMPERATURE_ERROR if it cannot be computed
 */
static float portTemperature(int port, float volt, float adcTemp) {
    if (isnan(volt)) {
        return TEMPERATURE_ERROR;
    }

    // Thermocouple
    if (subtype == 0) {
        return voltToDeg(1e3f * volt, cal.port[port].type, adcTemp);
    }

    // RTD in a bias network. The resistance includes the leads, which are compensated
    if (volt >= RTD_SUPPLY_VOLTAGE) {
        return TEMPERATURE_ERROR;
    }

    float ohm = 2.0f * biasRes * volt / (RTD_SUPPLY_VOLTAGE - volt);
    return resToDeg(ohm, cal.port[port].type, cal.port[port].leadRes);
}

/*!
 * @brief Updates temperature readings
 */
static void getPeripheralTemperatures() {
    for (int i = 0; i < NO_SPI_DEVICES; i++) {
        // Try to re-establish connection to ADS1120.
        // If the connection is broken
        if (bsGetField(TEMP_ADS1120_x_Error_Msk(i))) {
            temperatures[i * 2]     = TEMPERATURE_COMM_ERROR;
            temperatures[i * 2 + 1] = TEMPERATURE_COMM_ERROR;
            continue;
        }

        // Get measurements
        int ret = ADS1120Loop(&ads1120[i]);

        // If the return value is different from 0 - set an error.
        if (ret != 0) {
            bsSetErrorRange(ret << (i * 2), TEMP_ADS1120_x_Error_Msk(i));
        }

        float chipTemp          = (float)ads1120[i].data.internalTemp;
        temperatures[i * 2]     = portTemperature(i * 2, ads1120[i].data.chA, chipTemp);
        temperatures[i * 2 + 1] = portTemperature(i * 2 + 1, ads1120[i].data.chB, chipTemp);
    }
}

/*!
 * @brief Updates internal temperature
 */
static void getInternalTemperature() {
    int count = 0;
    float sum = 0;
    for (int i = 0; i < NO_SPI_DEVICES; i++) {
        // Do not include internal temperature of chip if
        // connection could not be established to ADS1120 chip.
        if (bsGetField(TEMP_ADS1120_x_Error_Msk(i)) != 0) {
            continue;
        }

        sum += (float)ads1120[i].data.internalTemp;
        count++;
    }

    internalTemp = (count != 0) ? sum / count : TEMPERATURE_COMM_ERROR;
}

/*!
 * @brief Print function called at 10 Hz
 */
static void printValues() {
    if (!isUsbPortOpen()) {
        return;
    }

    if (bsGetField(BS_VERSION_ERROR_Msk)) {
        USBnprintf("0x%08" PRIx32 "\r\n", bsGetStatus());
        return;
    }

    int len = 0;

    for (int i = 0; i < NO_PORTS; i++) {
        CA_SNPRINTF(buf, len, "%.2f, ", temperatures[i]);
    }
    CA_SNPRINTF(buf, len, "%.2f, 0x%08" PRIx32 "\r\n", internalTemp, bsGetStatus());

    writeUSB(buf, len);
}

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
 * @brief Initialization function
 * @param hspi SPI handler
 * @param hcrc CRC handler
 * @param bootMsg Boot message
 */
void InitTemperature(SPI_HandleTypeDef* hspi, CRC_HandleTypeDef* hcrc, const char* bootMsg) {
    // Enables communication
    initCAProtocol(&caProto, usbRx);

    // Uptime
    (void)uptime_init(hcrc, 0, NULL, bootMsg, GIT_VERSION);

    // Board type and PCB version check
    if (boardSetup(Temperature, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}, TEMP_ERRORS_Msk) !=
        0) {
        return;
    }

    // PCB version and subtype
    pcbVersion ver;
    getPcbVersion(&ver);
    getBoardInfo(NULL, &subtype);

    // Only subtypes 0,1 and 2 are supported
    if (subtype > 2) {
        bsSetError(BS_VERSION_ERROR_Msk);
        return;
    }
    if (subtype == 2) {
        biasRes = BIAS_RES_SUB_2;  // Default bias resistance already set for subtype 1
    }

    // Calibration
    calibrationInit(hcrc, &cal, sizeof(cal), subtype != 0);

    // External ADCs initialization
    initPinLayout(ver);
    initSpiDevices(hspi);
}

/*!
 * @brief Loop function called in while(1)
 * @param bootMsg Boot message
 */
void LoopTemperature(const char* bootMsg) {
    static const uint32_t PRINT_PERIOD = 100;  // [ms] - 10 Hz output
    static uint32_t timeStamp          = 0;

    // Always allow DFU upload
    CAhandleUserInputs(&caProto, bootMsg);

    // Handles uptime
    uptime_update();

    if (!bsGetField(BS_VERSION_ERROR_Msk)) {
        // Check the status off the board
        monitorBoardStatus();
        // Measure temperatures
        getPeripheralTemperatures();
        getInternalTemperature();
    }

    // Prints data line
    if (tdiff_u32(HAL_GetTick(), timeStamp) >= PRINT_PERIOD) {
        timeStamp = HAL_GetTick();
        printValues();
    }
}
