#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>

#include "stm32f4xx_hal.h"

#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "FLASH_readwrite.h"
#include "systemInfo.h"
#include "USBprint.h"
#include "time32.h"

#include "flowChip.h"
#include "honeywellZephyrI2C.h"
#include "pcbversion.h"

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void flowChipUsr(const char *input);
static void flowChipStatus();
static void calibrateSensor(int noOfCalibrations, const CACalibration* calibrations);
static void calibrationRW(bool write);

// Local variables
static I2C_HandleTypeDef *hi2c = NULL;
static WWDG_HandleTypeDef *hwwdg_ = NULL;
static CRC_HandleTypeDef *hcrc_ = NULL;

static uint16_t SLPM = 0;
static double accumulatedFlow = 0;
static float offset = 0;
const  uint16_t validSLPM[] = { 10, 15, 20, 50, 100, 200, 300 };

static CAProtocolCtx caProto =
{
        .undefined = flowChipUsr,
        .printHeader = CAPrintHeader,
        .printStatus = flowChipStatus,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = calibrateSensor,
        .calibrationRW = calibrationRW,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL
};

/*!
** @brief Verbose prints the flowchip status
*/
static void flowChipStatus() {
    static char buf[100] = { 0 };
    int len = 0;

    if (bsGetField(FLOWCHIP_WARNING_NO_CAL_Msk))
    {
        len += snprintf(&buf[len], sizeof(buf) - len, "The device is not calibrated\r\n");
    }

    if (bsGetField(FLOWCHIP_ERROR_WRONG_OTP_Msk)) {
        len += snprintf(&buf[len], sizeof(buf) - len, "OTP is out of date. Update to OTP version %u", OTP_VERSION_2);
    }

    writeUSB(buf, len);
}

static void flowChipUsr(const char *inputBuffer)
{
    if (strncmp("reset", inputBuffer, 5) == 0)
        accumulatedFlow = 0;
    else
        HALundefined(inputBuffer);
}

void uploadData()
{
    assert(hi2c != NULL);

    float flowData = 0;

    if ((BS_VERSION_ERROR_Msk | FLOWCHIP_ERROR_WRONG_OTP_Msk) & bsGetStatus()) {
        USBnprintf("0x%08" PRIx32, bsGetStatus());
        return;
    }

    HAL_StatusTypeDef ret = honeywellZephyrRead(hi2c, &flowData);
    
    double flow = 10000; /* Default value in case of error */
    
    if (ret == HAL_OK)
    {
        flow = flowData * SLPM + offset;

        if (fabs(flow) > 0.02)
            accumulatedFlow += flow/600.0; // L/PM. This loop is called every 1/10sec => divide by 600.
    }
    /* No board status output as this error is captured by the flow going to 10000 */

    USBnprintf("%0.2f, %0.2f, 0x%08" PRIx32, flow, accumulatedFlow, bsGetStatus()); // print over USB
}

// Read calibration offset from FLASH
void calibrationInit()
{
    if (readFromFlashCRC(hcrc_, (uint32_t) FLASH_ADDR_CAL, (uint8_t*) &offset, sizeof(offset)) != 0)
    {
        bsSetField(FLOWCHIP_WARNING_NO_CAL_Msk);
    }
}

// Calibrate the flow sensor to remove offset.
static void calibrateSensor(int noOfCalibrations, const CACalibration* calibrations)
{
    if (noOfCalibrations != 1 || calibrations[0].port != 1)
    {
        USBnprintf("Invalid calibration format. Standard format: CAL 1,targetFlow,0");
        return;
    }

    // Get latest flow reading
    float flow = 0;
    HAL_StatusTypeDef ret = honeywellZephyrRead(hi2c, &flow);

    if (ret != HAL_OK)
    {
        USBnprintf("Could not communicate with sensor. Try again...");
        return;
    }

    offset += calibrations[0].alpha - flow;

    calibrationRW(true); 
}

// Store values in FLASH.
static void calibrationRW(bool write)
{
    if (write) {
        if (writeToFlashCRC(hcrc_, (uint32_t) FLASH_ADDR_CAL, (uint8_t*) &offset, sizeof(offset)) == 0)
        {
            USBnprintf("Calibration was not stored in FLASH");
            bsClearField(FLOWCHIP_WARNING_NO_CAL_Msk);
        }
    }
    else {
        USBnprintf("Calibration offset: %.2f", offset);
    }
}

HAL_StatusTypeDef flowChipInit(I2C_HandleTypeDef *hi2c_, WWDG_HandleTypeDef *hwwdg, CRC_HandleTypeDef *hcrc)
{
    hi2c = hi2c_;
    hwwdg_ = hwwdg;
    hcrc_ = hcrc;

    uint32_t serialNB = 0;

    initCAProtocol(&caProto, usbRx);

    HAL_StatusTypeDef ret = honeywellZephyrSerial(hi2c, &serialNB);
    if (ret); // TBD: What should be done with the serial??. Why read it during init ??

    if(-1 == boardSetup(GasFlow, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR})) {
        return HAL_BUSY;
    }

    /* Return value ignored as it should have just been successfully read in the previous 
    ** function */
    BoardInfo info;
    (void) HAL_otpRead(&info);

    const unsigned int noOfSLPM = sizeof(validSLPM) / sizeof(validSLPM[0]);
    if ((info.otpVersion == OTP_VERSION_2) && (info.v2.subBoardType < noOfSLPM)) {
        SLPM = validSLPM[info.v2.subBoardType];
    }
    else {
        bsSetError(FLOWCHIP_ERROR_WRONG_OTP_Msk);
    }

    calibrationInit();

    return ret;
}

void flowChipLoop(const char* bootMsg)
{
    static uint32_t timeStamp = 0;
    static const uint32_t tsUpload = 100;

    CAhandleUserInputs(&caProto, bootMsg); // always allow DFU upload.

    // Upload data every "tsUpload" ms.
    if (tdiff_u32(HAL_GetTick(), timeStamp) >= tsUpload)
    {
        timeStamp = HAL_GetTick();

        if (isUsbPortOpen()) {
            uploadData();
        }
    }
}
