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
#include <stdbool.h>
#include "FLASH_readwrite.h"

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

static float internalTemperature()
{
    int count = 0;
    double internalTemp = 0;
    for (int i = 0; i < NO_SPI_DEVICES; i++)
    {
        // Do not include internal temperature of chip if
        // connection could not be established to ADS1120 chip.
        if (((bsGetStatus() >> 2*i) & 0x0F) != 0) continue;

        internalTemp += ads1120[i].data.internalTemp;
        count++;
    }
    return internalTemp/count;
}


static SPI_HandleTypeDef* hspi = NULL;
static WWDG_HandleTypeDef* hwwdg = NULL;
static CRC_HandleTypeDef* hcrc = NULL;
void InitTemperature(SPI_HandleTypeDef* hspi_, WWDG_HandleTypeDef* hwwdg_, CRC_HandleTypeDef* hcrc_)
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

    hspi = hspi_;
	hwwdg = hwwdg_;
	hcrc = hcrc_;

    initSensorCalibration();
}

void LoopTemperature(const char* bootMsg)
{
    static uint32_t timeStamp = 0;
    static const uint32_t tsUpload = 100;
    static bool isFirstWrite = true;

    CAhandleUserInputs(&caProto, bootMsg);

    uint32_t boardState = bsGetStatus();
    for (int i=0; i < NO_SPI_DEVICES && hspi != NULL && !isFirstWrite; i++)
    {
    	// Try to re-establish connection to ADS1120.
    	if (((boardState >> 2*i) & 0x0F) != 0)
        {
            initConnection(&ads1120[i], i);
            // If there are no more errors left then clear the error bit.
            if (bsGetStatus() == 0)
            {
                bsClearField(BS_ERROR_Msk);
            }
            continue;
        } 

        float *calPtr = &portCalVal[i*2][0];
        ADS1120Loop(&ads1120[i], calPtr);
    }
    float internalTemp = internalTemperature();

    // Upload data every "tsUpload" ms.
    if (tdiff_u32(HAL_GetTick(), timeStamp) >= tsUpload)
    {
        timeStamp = HAL_GetTick();
    	HAL_WWDG_Refresh(hwwdg);

        if (isComPortOpen())
        {
            if (isFirstWrite)
            {
                __HAL_RCC_WWDG_CLK_ENABLE(); // Enable wwdg now that print frequency has stabilised.
                if (hspi != NULL)
                    initSpiDevices(hspi);
                isFirstWrite = false;
                return;
            }

            if (hspi == NULL) {
                USBnprintf("This Temperature SW require PCB version >= 5.2 where SPI devices are attached");
            }

			USBnprintf("%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, 0x%x"
					, ads1120[0].data.chA, ads1120[0].data.chB
					, ads1120[1].data.chA, ads1120[1].data.chB
					, ads1120[2].data.chA, ads1120[2].data.chB
					, ads1120[3].data.chA, ads1120[3].data.chB
					, ads1120[4].data.chA, ads1120[4].data.chB
					, internalTemp, bsGetStatus());
        }
    }

    if (!isComPortOpen())
        isFirstWrite=true;
}

void initSensorCalibration()
{
	readFromFlashSafe(hcrc, 0, sizeof(portCalVal), (uint8_t *) portCalVal);
	if (*((uint8_t*) portCalVal) == 0xFF)
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
        writeToFlashSafe(hcrc, 0, sizeof(portCalVal), (uint8_t *) portCalVal);
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

