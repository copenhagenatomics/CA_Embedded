/*!
** @file   Temperature_tests.cpp
** @author Luke W
** @date   05/11/2024
*/

/* Linker script variables */
#include <inttypes.h>
extern "C" {
uint32_t _FlashAddrCal    = 0;
uint32_t _FlashAddrUptime = 0;
}

#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Fakes */
#include "fake_ADS1120.h"
#include "fake_StmGpio.h"

/* Real supporting units */
#include "CAProtocol.c"
#include "CAProtocolStm.c"
#include "calibration.c"
#include "crc.c"
#include "temperatureMeas.c"
#include "time32.c"
#include "uptime.c"

/* UUT */
#include "Temperature.c"

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

using namespace std;

/***************************************************************************************************
** OUTPUT STRINGS
***************************************************************************************************/

// Default flash calibration values, before any CAL command has been sent - for thermocouple boards
static const string CAL_DEFAULT_TC =
    "CAL 1,0.00,0,3 2,0.00,0,3 3,0.00,0,3 4,0.00,0,3 5,0.00,0,3 6,0.00,0,3 7,0.00,0,3 8,0.00,0,3 "
    "9,0.00,0,3 10,0.00,0,3\r";

// Calibration string containing only valid calibrations - for thermocouple boards
static const string CAL_VALID_TC =
    "CAL 1,0.00,0,0 2,0.00,0,1 3,0.00,0,2 4,0.00,0,4 5,0.00,0,5 6,0.00,0,6 7,0.00,0,7 8,0.00,0,0 "
    "9,0.00,0,1 10,0.00,0,2\r";

// Calibration string containing only invalid calibrations - for thermocouple boards
static const string CAL_INVALID_TC =
    "CAL 1,1.00,0,0 2,1.00,0,1 3,1.00,0,2 4,1.00,0,4 5,1.00,0,5 6,1.00,0,6 7,1.00,0,7 8,1.00,0,0 "
    "9,1.00,0,1 10,1.00,0,2\r";

// Default flash calibration values, before any CAL command has been sent - for RTD boards
static const string CAL_DEFAULT_RTD =
    "CAL 1,0.00,0,8 2,0.00,0,8 3,0.00,0,8 4,0.00,0,8 5,0.00,0,8 6,0.00,0,8 7,0.00,0,8 8,0.00,0,8 "
    "9,0.00,0,8 10,0.00,0,8\r";

// Calibration string containing only valid calibrations - for RTD boards
static const string CAL_VALID_RTD =
    "CAL 1,1.00,0,8 2,0.00,0,9 3,1.00,0,8 4,0.00,0,9 5,1.00,0,8 6,0.00,0,9 7,1.00,0,8 8,0.00,0,9 "
    "9,1.00,0,8 10,0.00,0,9\r";

// Calibration string containing only invalid calibrations - for RTD boards
static const string CAL_INVALID_RTD =
    "CAL 1,0.00,0,0 2,0.00,0,1 3,0.00,0,2 4,0.00,0,4 5,0.00,0,5 6,0.00,0,6 7,0.00,0,7 8,-1.00,0,8 "
    "9,1000.00,0,9 10,-100.00,0,8\r";

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class TemperatureBoardTest : public CaBoardUnitTest {
   protected:
    /*******************************************************************************************
    ** METHODS
    *******************************************************************************************/
    TemperatureBoardTest()
        : CaBoardUnitTest(&LoopTemperature, Temperature, {LATEST_MAJOR, LATEST_MINOR}) {}

    void simTick() { LoopTemperature(bootMsg); }

    /* Initialises the board and runs a loop, to flush the buffers */
    void initBoard() {
        sst.boundInit();
        _loopFunction(bootMsg);
        (void)hostUSBread(true);
    }

    /*******************************************************************************************
    ** MEMBERS
    *******************************************************************************************/

    SPI_HandleTypeDef hspi = {
        .Instance = SPI1,
    };
    CRC_HandleTypeDef hcrc = {
        .Instance = CRC,
    };

    SerialStatusTest sst = {
        .boundInit   = bind(InitTemperature, &hspi, &hcrc, bootMsg),
        .testFixture = this,
    };
};

/***************************************************************************************************
** GENERIC CA TESTS
***************************************************************************************************/

TEST_F(TemperatureBoardTest, incorrectBoard) {
    incorrectBoardTest(sst);
}

TEST_F(TemperatureBoardTest, incorrectBoardVersion) {
    incorrectBoardVersionTest(sst);
}

// clang-format off
TEST_F(TemperatureBoardTest, goldenPath) {
    sst.boundInit();
    for(int i = 0; i < NO_SPI_DEVICES; i++) {
        setVoltage(&ads1120[i], 1e-3, 2e-3, 25.0);
    }

    goldenPathTest(sst,
        "49.48, 73.58, 49.48, 73.58, 49.48, 73.58, 49.48, 73.58, 49.48, 73.58, 25.00, 0x00000000\r",
    100);
}

TEST_F(TemperatureBoardTest, printStatus) {
    statusPrintoutTest(sst, {
        "The board is operating normally.\r"
    });
}

TEST_F(TemperatureBoardTest, printStatusDef) {
    statusDefPrintoutTest(sst, {"0x7e0003ff,System errors\r"}, {
        "0x00000003,Status ADC 1\r",
        "0x0000000c,Status ADC 2\r",
        "0x00000030,Status ADC 3\r",
        "0x000000c0,Status ADC 4\r",
        "0x00000300,Status ADC 5\r"
    });
}

TEST_F(TemperatureBoardTest, printOutputDef) {
    outputDefPrintoutTest(sst, {
        "Temperature p1,degC\r",
        "Temperature p2,degC\r",
        "Temperature p3,degC\r",
        "Temperature p4,degC\r",
        "Temperature p5,degC\r",
        "Temperature p6,degC\r",
        "Temperature p7,degC\r",
        "Temperature p8,degC\r",
        "Temperature p9,degC\r",
        "Temperature p10,degC\r",
        "Internal Temperature,degC\r"});
}
// clang-format on

TEST_F(TemperatureBoardTest, printSerial) {
    string calStr = "Calibration: " + CAL_DEFAULT_TC;
    serialPrintoutTest(sst, "Temperature", calStr.c_str());
}

TEST_F(TemperatureBoardTest, incorrectCommand) {
    incorrectCommandTest(sst);
}

TEST_F(TemperatureBoardTest, upTime) {
    uptimeTest(sst, (uintptr_t)&_FlashAddrUptime);
}

/***************************************************************************************************
** CALIBRATION TESTS
***************************************************************************************************/

TEST_F(TemperatureBoardTest, calibrationSubtype0) {
    // Subtype 0 - Thermocouple only
    calibrationTest(sst, CAL_DEFAULT_TC.c_str(), CAL_VALID_TC.c_str(), CAL_INVALID_TC.c_str());
}

TEST_F(TemperatureBoardTest, calibrationSubtype1) {
    // Subtype 1 - RTD only
    bi.v2.subBoardType = 1;
    HAL_otpWrite(&bi);
    calibrationTest(sst, CAL_DEFAULT_RTD.c_str(), CAL_VALID_RTD.c_str(), CAL_INVALID_RTD.c_str());
}

TEST_F(TemperatureBoardTest, calibrationSubtype2) {
    // Subtype 2 - RTD only
    bi.v2.subBoardType = 2;
    HAL_otpWrite(&bi);
    calibrationTest(sst, CAL_DEFAULT_RTD.c_str(), CAL_VALID_RTD.c_str(), CAL_INVALID_RTD.c_str());
}

TEST_F(TemperatureBoardTest, calibrationLinearTypeK) {
    initBoard();

    /* Linear calibration of the previous firmware versions is recognized as type K and J */
    writeBoardMessage("CAL 1,0.000057953,0.000052136\n");
    EXPECT_FLUSH_USB(ElementsAre("CAL 1 success\r"));
    writeBoardMessage("CAL r\n");
    EXPECT_FLUSH_USB(ElementsAre(HasSubstr("CAL 1,0.00,0,2 ")));

    writeBoardMessage("CAL 1,0.000041276,0.00004073 2,0.000041276,0.00004073\n");
    EXPECT_FLUSH_USB(ElementsAre("CAL 1 success\r", "CAL 2 success\r"));
    writeBoardMessage("CAL r\n");
    EXPECT_FLUSH_USB(ElementsAre(HasSubstr(CAL_DEFAULT_TC.c_str())));

    /* Other linear calibrations are refused, and the previous one is kept */
    writeBoardMessage("CAL 1,0.00005,0.00004 2,0.000041276,0.00004\n");
    EXPECT_FLUSH_USB(ElementsAre("CAL 1 failed\r", "CAL 2 failed\r"));
    writeBoardMessage("CAL r\n");
    EXPECT_FLUSH_USB(ElementsAre(HasSubstr(CAL_DEFAULT_TC.c_str())));
}

/***************************************************************************************************
** MEASUREMENT TESTS
***************************************************************************************************/
