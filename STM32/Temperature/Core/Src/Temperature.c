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

// Local functions
static void temperatureUsrHandling(const char *input);

// Local variables
static CAProtocolCtx caProto =
{
        .undefined = temperatureUsrHandling,
        .printHeader = CAPrintHeader,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL,
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL
};

// Set all SPI pins high to be enable for communication
#define NO_SPI_DEVICES 5
#define CALIMEMSIZE NO_SPI_DEVICES*2
static uint8_t calibrationValues[CALIMEMSIZE];
static ADS1120Device ads1120[ NO_SPI_DEVICES ];

float portCalVal[NO_SPI_DEVICES*2][2];

static int initSpiDevices(SPI_HandleTypeDef* hspi)
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
    int err = 0;
    for (int i=0; i < NO_SPI_DEVICES; i++)
    {
        int ret = ADS1120Init(&ads1120[i]);
        if (ret != 0) {
            err |= ret << (4*i);
        }
    }
    return err;
}

static SPI_HandleTypeDef* hspi = NULL;
void InitTemperature(SPI_HandleTypeDef* hspi_)
{
    initCAProtocol(&caProto, usb_cdc_rx);

    BoardType board;
    if (getBoardInfo(&board, NULL) || board != Temperature)
        return;

    pcbVersion ver;
    if (getPcbVersion(&ver) || ver.major < 5)
        return;

    // SW versions lower than 5.2 are not supported
    if (ver.major == 5 && ver.minor < 2)
        return;

    initSensorCalibration();
    hspi = hspi_;
}

void LoopTemperature(const char* bootMsg)
{
    static int spiErr = 1;
    static uint32_t timeStamp = 0;
    static const uint32_t tsUpload = 100;
    static bool isFirstWrite = true;

    CAhandleUserInputs(&caProto, bootMsg);

    for (int i=0; i < NO_SPI_DEVICES && !spiErr && hspi != NULL; i++)
    {
        float *calPtr = &portCalVal[i*2][0];
        ADS1120Loop(&ads1120[i], calPtr);
    }

    // Upload data every "tsUpload" ms.
    if (tdiff_u32(HAL_GetTick(), timeStamp) > tsUpload)
    {
        timeStamp = HAL_GetTick();

        if (isComPortOpen())
        {
            if (isFirstWrite)
            {
                if (hspi != NULL)
                    spiErr = initSpiDevices(hspi);
                isFirstWrite = false;
            }

            if (hspi == NULL) {
                USBnprintf("This Temperature SW require PCB version >= 5.2 where SPI devices is attached");
                return;
            }

            if (!spiErr)
            {
                double internalTemp = (ads1120[0].data.internalTemp +
                                       ads1120[1].data.internalTemp +
                                       ads1120[2].data.internalTemp +
                                       ads1120[3].data.internalTemp +
                                       ads1120[4].data.internalTemp)/5;

                USBnprintf("%.2f, %.2f, %.2f, %.2f, %.2f%, %.2f, %.2f%, %.2f, %.2f%, %.2f, %.2f%"
                        , ads1120[0].data.chA, ads1120[0].data.chB
                        , ads1120[1].data.chA, ads1120[1].data.chB
                        , ads1120[2].data.chA, ads1120[2].data.chB
                        , ads1120[3].data.chA, ads1120[3].data.chB
                        , ads1120[4].data.chA, ads1120[4].data.chB
                        , internalTemp);
            }
            else
            {
                USBnprintf("Failed to initialise ADS1120 device %X. This SW require PCB version >= 5.2", spiErr);
            }
        }
    }

    if (!isComPortOpen())
    {
        isFirstWrite=true;
    }
}

bool isCalibrationInputValid(const char *inputBuffer)
{
    // Ensure all calibration values are set
    if (strlen(inputBuffer) != NO_SPI_DEVICES*2)
    {
        return false;
    }

    // Check all characters are valid calibration inputs
    for (int j = 0; j < strlen(inputBuffer); j++)
    {
    	// Support Type J and Type K only
        if (inputBuffer[j] != 'J' && inputBuffer[j] != 'K')
        {
            return false;
        }
    }
    return true;
}

void setSensorCalibration(int pinNumber, char type)
{
	if (type == 'J')
	{
	    portCalVal[pinNumber][0] = TYPE_J_DELTA;
	    portCalVal[pinNumber][1] = TYPE_J_CJ_DELTA;
	}
	else if (type == 'K')
	{
	    portCalVal[pinNumber][0] = TYPE_K_DELTA;
	    portCalVal[pinNumber][1] = TYPE_K_CJ_DELTA;
	}
}

void initSensorCalibration()
{
    for (int pinNumber = 0; pinNumber < NO_SPI_DEVICES*2; pinNumber++)
    {
    	// Default to type K thermocouple
    	setSensorCalibration(pinNumber, 'K');
    }
}

static void temperatureUsrHandling(const char *inputBuffer)
{
	if (isCalibrationInputValid(inputBuffer))
	{
		// Set the calibration type to the type specified by user.
		for (int pinNumber = 0; pinNumber < NO_SPI_DEVICES*2; pinNumber++)
		{
			setSensorCalibration(pinNumber, inputBuffer[pinNumber]);
		}
        strcpy((char*) calibrationValues, inputBuffer);
        USBnprintf("Calibration updated to: %s", inputBuffer);
	}
	else
	{
		HALundefined(inputBuffer);
	}
}

