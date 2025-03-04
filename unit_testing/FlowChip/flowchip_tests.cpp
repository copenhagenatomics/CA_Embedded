/*!
** @file   AC_tests.cpp
** @author Luke W
** @date   12/10/2023
*/

#include <inttypes.h>

extern "C" {
    uint32_t _FlashAddrCal = 0;
}

#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Fakes */
#include "fake_stm32xxxx_hal.h"
#include "fake_USBprint.h"
#include "FLASH_readwrite.h"

/* Real supporting units */
#include "CAProtocol.c"
#include "CAProtocolStm.c"

/* UUT */
#include "flowChip.c"

using ::testing::AnyOf;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class FlowChipBoard: public CaBoardUnitTest
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        FlowChipBoard() : CaBoardUnitTest(flowChipLoop, GasFlow, {LATEST_MAJOR, LATEST_MINOR}) { }

        void simTick() {
            flowChipLoop(bootMsg);
        }

        void storeCalibrationInFlash() {
            uint32_t offset = 0;
            writeToFlash(FLASH_ADDR_CAL, (uint8_t*) &offset, sizeof(uint32_t));
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/
        
        I2C_HandleTypeDef hi2c;
        WWDG_HandleTypeDef hwwdg;
        CRC_HandleTypeDef hcrc;

        SerialStatusTest sst = {
            .boundInit = bind(flowChipInit, &hi2c, &hwwdg, &hcrc),
            .testFixture = this,
        };
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(FlowChipBoard, CorrectBoardParams) {
    // Store calibration value to remove status bit warning
    storeCalibrationInFlash();
    goldenPathTest(sst, "0.00, 0.00, 0x00000000");
}

TEST_F(FlowChipBoard, NoCalibration) {
    // Don't store calibration to ensure the warning is set
    goldenPathTest(sst, "0.00, 0.00, 0x00000001");
}

TEST_F(FlowChipBoard, incorrectBoard) {
    incorrectBoardTest(sst, 101);
}

TEST_F(FlowChipBoard, printStatusOk) {
    // Store calibration value to remove status bit warning
    storeCalibrationInFlash();
    statusPrintoutTest(sst, {});
}

TEST_F(FlowChipBoard, printSerial) {
    serialPrintoutTest(sst, "GasFlow");
}

TEST_F(FlowChipBoard, wrongotp) {
    // Store calibration value to remove status bit warning
    storeCalibrationInFlash();

    /* Update OTP with incorrect board number */
    bi.otpVersion = OTP_VERSION_1;
    bi.v1.boardType = GasFlow;
    bi.v1.pcbVersion.major = BREAKING_MAJOR;
    bi.v1.pcbVersion.minor = BREAKING_MINOR;
    HAL_otpWrite(&bi);

    flowChipInit(&hi2c, &hwwdg, &hcrc);

    /* This should force a print on the USB bus */
    goToTick(101);

    /* Check the printout is correct */
    EXPECT_FLUSH_USB(Contains("0x80000002"));

    writeBoardMessage("Status\n");
    EXPECT_FLUSH_USB(ElementsAre(
        "\r", 
        "Start of board status:\r", 
        "OTP is out of date. Update to OTP version 2\r",
        "End of board status. \r"
    ));
}
