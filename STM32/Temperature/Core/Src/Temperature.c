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
#include <math.h>
#include "time32.h"
#include "StmGpio.h"
#include "ADS1120.h"
#include <stdbool.h>
#include "FLASH_readwrite.h"

// Local functions
static void calibrateTypeInput(int noOfCalibrations, const CACalibration* calibrations);
static void calibrateReadWrite(bool write);
static void TempPrintHeader();

// Local variables
static CAProtocolCtx caProto =
{
        .undefined = HALundefined,
        .printHeader = TempPrintHeader,
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
//** Transducer MVP
StmGpio cs10;
#define AVG_BUFFER_SIZE 500


static void TempPrintHeader()
{
	CAPrintHeader();
	// calibrateReadWrite(false);
}

static int initSpiDevices(SPI_HandleTypeDef* hspi)
{
    // Initialise Chip Select pin
    stmGpioInit(&ads1120[0].cs, CS1_GPIO_Port, CS1_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&ads1120[1].cs, CS2_GPIO_Port, CS2_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&ads1120[2].cs, CS3_GPIO_Port, CS3_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&ads1120[3].cs, CS4_GPIO_Port, CS4_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&ads1120[4].cs, CS5_GPIO_Port, CS5_Pin, STM_GPIO_OUTPUT);
    //** Transducer MVP
    stmGpioInit(&cs10, CS10_GPIO_Port, CS10_Pin, STM_GPIO_OUTPUT);

    // Initialise Data Ready input pin
    // stmGpioInit(&ads1120[0].drdy, DRDY1_GPIO_Port, DRDY1_Pin, STM_GPIO_INPUT);
    // stmGpioInit(&ads1120[1].drdy, DRDY2_GPIO_Port, DRDY2_Pin, STM_GPIO_INPUT);
    // stmGpioInit(&ads1120[2].drdy, DRDY3_GPIO_Port, DRDY3_Pin, STM_GPIO_INPUT);
    // stmGpioInit(&ads1120[3].drdy, DRDY4_GPIO_Port, DRDY4_Pin, STM_GPIO_INPUT);
    // stmGpioInit(&ads1120[4].drdy, DRDY5_GPIO_Port, DRDY5_Pin, STM_GPIO_INPUT);

    for (int i=0; i < NO_SPI_DEVICES; i++) {
        stmSetGpio(ads1120[i].cs, true); // CS selects chip when low
        // ads1120[i].hspi = hspi;
    }
    //** Transducer MVP
    stmSetGpio(cs10, true);


    //* Write 2 dummy byte on SPI wire. Yes, this seems VERY strange
    // since no CS is made so no receiver. For some reason it is needed
    // to kick the STM32 SPI interface before the first real transmission.
    // After this initial write everything seems to work just fine.
    // This could be related to the note in the documentation section 8.5.6
    // where two bytes with DIN held low should be sent after each read of data. *//
    // uint8_t dummy[2] = { 0 };
    // HAL_SPI_Transmit(hspi, dummy, 2, 1);

    // Now configure the devices.
     int err = 0;
    // for (int i=0; i < NO_SPI_DEVICES; i++)
    // {
    //     int ret = ADS1120Init(&ads1120[i]);
    //     if (ret != 0) {
    //         err |= ret << (4*i);

    //         // If connection could not be established to chip
    //         // set temperatures to 10010 to indicate miscommunication
    //     	ads1120[i].data.internalTemp = 10010;
    //         ads1120[i].data.chA = 10010;
    //         ads1120[i].data.chB = 10010;
    //     }
    // }
    return err;
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
    static int spiErr = 0;
    static uint32_t timeStamp = 0;
    static const uint32_t tsUpload = 100;
    static bool isFirstWrite = true;
    static uint8_t buff[4] = {};
    static int16_t avg_buff[AVG_BUFFER_SIZE] = {};
    static int16_t avg_pointer = 0;
    static double avg_press = 0.0;

    CAhandleUserInputs(&caProto, bootMsg);



    if (isComPortOpen())
    {
        HAL_StatusTypeDef halStatus = HAL_OK;
        int16_t temp_raw = 0, press = 0;
        float temp = 0.0f;

        if (isFirstWrite)
        {
            // __HAL_RCC_WWDG_CLK_ENABLE(); // Enable wwdg now that print frequency has stabilised.
            if (hspi != NULL)
                spiErr = initSpiDevices(hspi);
            isFirstWrite = false;
            return;
        }
        
        if (hspi != NULL) {
            //Get transducer data over SPI
            stmSetGpio(cs10, false);
            halStatus = HAL_SPI_Receive(hspi, buff, sizeof(buff), 10);
            stmSetGpio(cs10, true);

            // Conversion of data from transducer
            temp_raw = (buff[3]>>5) | (buff[2]<<3); // 11bit temperature value
            temp = temp_raw*(200/(pow(2,11)-1))-50; // Conversion formular can be foud in the datasheet

            press = (buff[1]) | ((buff[0]&0x1f)<<8) | (((~buff[0])&0x20)<<10) | (((~buff[0])&0x20)<<9) | (((~buff[0])&0x20)<<8); //14bit pressure value with inverse singed bit converted to 16bit integer

            avg_buff[avg_pointer] = press;
            avg_pointer = avg_pointer >= AVG_BUFFER_SIZE-1 ? 0 : avg_pointer + 1;
        }

        // Upload data every "tsUpload" ms.
        if (tdiff_u32(HAL_GetTick(), timeStamp) >= tsUpload)
        {
            timeStamp = HAL_GetTick();
            HAL_WWDG_Refresh(hwwdg);
            
            if (hspi == NULL) {
                USBnprintf("This Temperature SW require PCB version >= 5.2 where SPI devices are attached");
                buff[1] = 2U;
                return;
            }
            avg_press = 0;
            for(int i = 0; i<AVG_BUFFER_SIZE; i++)
                avg_press = avg_press + avg_buff[i]; 

            avg_press = avg_press/AVG_BUFFER_SIZE;
            avg_press = avg_press * 0.0343;

            USBnprintf("%0.2f, %0.1f", avg_press, temp);

        }
    }
    
    if (!isComPortOpen())
        isFirstWrite=true;

}

void initSensorCalibration()
{
	// readFromFlashSafe(hcrc, 0, sizeof(portCalVal), (uint8_t *) portCalVal);
	// if (*((uint8_t*) portCalVal) == 0xFF)
	// {
	// 	// If nothing is stored in FLASH default to type K thermocouple
	// 	for (int i = 0; i < NO_SPI_DEVICES*2; i++)
	// 	{
	// 		portCalVal[i][0] = TYPE_K_DELTA;
	// 		portCalVal[i][1] = TYPE_K_CJ_DELTA;
	// 	}
	// }
}

static void calibrateTypeInput(int noOfCalibrations, const CACalibration* calibrations)
{
	// __HAL_RCC_WWDG_CLK_DISABLE();
    // for (int count = 0; count < noOfCalibrations; count++)
    // {
    //     if (1 <= calibrations[count].port && calibrations[count].port <= 10)
    //     {
    //     	portCalVal[calibrations[count].port-1][0] = calibrations[count].alpha;
	// 		portCalVal[calibrations[count].port-1][1] = calibrations[count].beta;
    //     }
    // }
    // // Update automatically when receiving new calibration values
    // calibrateReadWrite(true);
    // __HAL_RCC_WWDG_CLK_ENABLE();
}

static void calibrateReadWrite(bool write)
{
    // if (write)
    //     writeToFlashSafe(hcrc, 0, sizeof(portCalVal), (uint8_t *) portCalVal);
    // else
    // {
    // 	char buf[512];
    // 	int len = 0;
    // 	for (int i = 0; i < NO_SPI_DEVICES*2; i++)
    // 	{
    // 		if (i == 0)
    // 		{
    // 			len += snprintf(&buf[len], sizeof(buf), "Calibration: CAL");
    // 		}
    // 		len += snprintf(&buf[len], sizeof(buf) - len, " %d,%.10f,%.10f", i+1, portCalVal[i][0], portCalVal[i][1]);
    // 	}
    // 	len += snprintf(&buf[len], sizeof(buf) - len, "\r\n");
	// 	writeUSB(buf, len);
    // }
}

