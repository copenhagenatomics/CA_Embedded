#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "ADCMonitor.h"
#include "systemInfo.h"
#include "USBprint.h"
#include "pressure.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "StmGpio.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define ADC_CHANNELS			8   // Channels: Pressure 1 - 6, VCC_FB, VCC_RAW_FB 
#define ADC_CHANNEL_BUF_SIZE	400 // 4kHz sampling rate

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2]; // array for all ADC readings, filled by DMA.

static float pressure[ADC_CHANNELS];    // port pressure readings
static float volts[ADC_CHANNELS];       // port voltage readings
static float ADCMeans[ADC_CHANNELS];    // ADC mean readings adjusted with portCalVal
static float ADCMeansRaw[ADC_CHANNELS]; // ADC mean readings

// Definition in calibration.h
FlashCalibration cal;

BoardInfo board;
StmGpio ledCtrl;

static int loggingMode = 0;

// Local functions
static void printHeader();
static void printPressureStatus();
static void calibrateSensorOrBoard(int noOfCalibrations, const CACalibration* calibrations);
static void calibrationInfoRW(bool write);
static void logMode(int port);

// Local variables
static CAProtocolCtx caProto =
{
    .undefined = HALundefined,
    .printHeader = printHeader, // Need this since calibrations should be included.
    .printStatus = printPressureStatus,
    .jumpToBootLoader = HALJumpToBootloader,
    .calibration = calibrateSensorOrBoard,
    .calibrationRW = calibrationInfoRW,
    .logging = logMode,
    .otpRead = CAotpRead,
    .otpWrite = NULL
};

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

static void printHeader()
{
    CAPrintHeader();
    calibrationInfoRW(false);
}

static void printPressureStatus()
{
    static char buf[600] = { 0 };
    int len = 0;

    for (int i = 0; i < 6; i++)
    {
        if (bsGetField(PORT_MEASUREMENT_TYPE(i)))
        {
            len += snprintf(&buf[len], sizeof(buf) - len, "Port %d measures current [4-20mA]\r\n", i+1);
        }
        else
        {
            len += snprintf(&buf[len], sizeof(buf) - len, "Port %d measures voltage [0-5V]\r\n", i+1);
        }
    }

    if (bsGetField(VCC_ERROR_Msk))
    {
        len += snprintf(&buf[len], sizeof(buf) - len, "VCC is: %.2f. It should be >=5.05V \r\n", volts[6]);
    }

    if (bsGetField(VCC_RAW_ERROR_Msk))
    {
        len += snprintf(&buf[len], sizeof(buf) - len, "VCC raw is: %.2f. It should be >=4.6V \r\n", volts[7]);
    }
    writeUSB(buf, len);
}

static void calibrationInfoRW(bool write)
{
    calibrationRW(write, &cal, sizeof(cal));
}

static void logMode(int port)
{
    if (port >= 0 && port < 3)
    {
        loggingMode = port;
    }
}

static void calibrateSensorOrBoard(int noOfCalibrations, const CACalibration* calibrations)
{
    // Overload of threshold value to switch between sensor calibration
    // and board calibration mode.
    //		calibrations->threshold == 0 -> sensor calibration
    // 		calibrations->threshold == 1 -> board port calibration
    if (calibrations->threshold == 2)
    {
        if (loggingMode != 1)
        {
            USBnprintf("To calibrate board, first enter voltLogging mode by typing: 'LOG p1'");
            return;
        }
        calibrateBoard(noOfCalibrations, calibrations, &cal, ADCMeansRaw, sizeof(cal));
    }
    else
    {
        calibrateSensor(noOfCalibrations, calibrations, &cal, sizeof(cal));
    }
}

static void ADCtoPressure(float *adcMeans, int noOfChannels)
{
    // Convert from ADC means to pressure via its calibration function
    for (int channel = 0; channel < noOfChannels; channel++)
    {
        pressure[channel] = adcMeans[channel] * cal.sensorCalVal[channel*2] + cal.sensorCalVal[channel*2+1];
    }

    // 6th port is used for LED indication if subtype != 0
    if (board.v2.subBoardType != 0)
        pressure[5] = 0;
}

static void ADCtoVolt(float *adcMeans, int noOfChannels)
{
    // Convert from ADC means to volt
    for (int channel = 0; channel < noOfChannels; channel++)
    {
        float adcScaled = adcMeans[channel]/(ADC_MAX+1);

        /*  VCC and VCC raw have a different voltage divider network to enable measuring above 5V.
         *  Hence, we divide by MAX_VCC_IN (=5.49V) rather than MAX_VIN 
         */ 
        volts[channel] = (channel <= 5) ? adcScaled*MAX_VIN : adcScaled*MAX_VCC_IN;  
    }
}

static void printPorts(float *portValues)
{
    USBnprintf("%0.6f, %0.6f, %0.6f, %0.6f, %0.6f, %0.6f, 0x%x",
                *portValues, *(portValues+1), *(portValues+2), *(portValues+3),
                *(portValues+4), *(portValues+5), bsGetStatus());
}

static void switchLED()
{
    static const float PRESSURIZED_LEVEL = 0.15;
    bool pressurized = false;
    // Exclude the VCC and VCC raw channels
    for (int i = 0; i < ADC_CHANNELS-2; i++)
        if (pressure[i] > PRESSURIZED_LEVEL && cal.sensorCalVal[i*2] != 1)
            pressurized = true;

    stmSetGpio(ledCtrl, pressurized);
}

static void updateBoardStatus()
{
    // Check whether a port is set to measure voltage (=0) or current (=1)
    for (int i = 0; i<6; i++)
    {
        (cal.measurementType[i]) ? bsSetField(PORT_MEASUREMENT_TYPE(i)) : bsClearField(PORT_MEASUREMENT_TYPE(i));
    }

    // Check input voltages 
    (volts[6] <= MIN_VCC) ? bsSetError(VCC_ERROR_Msk) : bsClearField(VCC_ERROR_Msk);
    (volts[7] <= MIN_RAW_VCC)  ? bsSetError(VCC_RAW_ERROR_Msk) : bsClearField(VCC_RAW_ERROR_Msk);

    setBoardVoltage(volts[6]);
    if (bsGetField(VCC_ERROR_Msk) || bsGetField(VCC_RAW_ERROR_Msk))
    {
        bsSetError(BS_UNDER_VOLTAGE_Msk);
    }
    else
    {
        bsClearField(BS_UNDER_VOLTAGE_Msk);
    }

    bsClearError(PRESSURE_ERROR_Msk);
}

static void adcCallback(int16_t *pData, int noOfChannels, int noOfSamples)
{
    if (!isUsbPortOpen())
        return;

    /* If the version is incorrect only print out status code */
    if (bsGetStatus() & BS_VERSION_ERROR_Msk)
    {
        USBnprintf("0x%x", bsGetStatus());
        return;
    }

    for (int channel = 0; channel < noOfChannels; channel++)
    {
        ADCMeansRaw[channel] = ADCMean(pData, channel);
        ADCMeans[channel] = ADCMeansRaw[channel]*cal.portCalVal[channel];
    }

    // We exclude the last two channels (VCC and VCC raw) when converting the ADC to pressure.
    ADCtoPressure(ADCMeans, noOfChannels-2);
    ADCtoVolt(ADCMeans, noOfChannels);

    if (loggingMode == 0)
    {
        printPorts(pressure);
    }
    else if (loggingMode == 1)
    {
        printPorts(volts);
    }
    else if (loggingMode == 2)
    {
        printPorts(ADCMeans);
    }

    // Turn on LED when attached that turns on when pressure > pressurizedLevel.
    // Only relevant for modified boards with subBoardType = 1.
    if (board.v2.subBoardType == 1)
        switchLED();
}

static void gpioInit()
{
    stmGpioInit(&ledCtrl, LED1_GPIO_Port, LED1_Pin, STM_GPIO_OUTPUT);
    stmSetGpio(ledCtrl, false);
}

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void pressureInit(ADC_HandleTypeDef *hadc, CRC_HandleTypeDef *hcrc)
{
    initCAProtocol(&caProto, usbRx);

    // Pin out has changed from PCB V3.2 - older versions need other software.
    boardSetup(Pressure, (pcbVersion){3, 2});

    /* Any reading error will already have been dealt with by boardSetup */
    (void) HAL_otpRead(&board);

    // subBoardType == 1 has an extra LED attached that turns on when pressure > pressurizedLevel.
    // NOTE: The LED is attached to the 6th port on the board. The board has to have its 6th port
    // soldered directly on to the LED pin trace and the ADC circuit for the 6th port has to be
    // cut off for it to work correctly.
    if (board.v2.subBoardType == 1)
        gpioInit();

    ADCMonitorInit(hadc, ADCBuffer, sizeof(ADCBuffer) / sizeof(int16_t));
    calibrationInit(hcrc, &cal, sizeof(cal));
}

void pressureLoop(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
    updateBoardStatus();
    ADCMonitorLoop(adcCallback); // Monitoring loop ensures print speed of 100ms/write
}
