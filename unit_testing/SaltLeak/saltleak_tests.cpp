/*!
** @file   saltleak_tests.cpp
** @author Luke W
** @date   23/07/2024
*/

/* Linker script variables */
#include <inttypes.h>
extern "C" {
    uint32_t _FlashAddrCal = 0;
}

/* CA unit tests */
#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Fakes */
#include "fake_StmGpio.h"

/* Real supporting units */
#include "ADCmonitor.c"
#include "CAProtocol.c"
#include "CAProtocolStm.c"
#include "calibration.c"
#include "crc.c"
#include "systeminfo.c"
#include "time32.c"

/* UUT */
#include "saltleakLoop.c"

using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class SaltLeakBoard : public CaBoardUnitTest {
   protected:
    /*******************************************************************************************
    ** METHODS
    *******************************************************************************************/
    SaltLeakBoard() : CaBoardUnitTest(saltleakLoop, SaltLeak, {LATEST_MAJOR, LATEST_MINOR}) {
        hadc.Init.NbrOfConversion = ADC_CHANNELS;
    }

    void simTick() {
        if (tickCounter != 0 && (tickCounter % 100 == 0)) {
            /* ADC cicular buffer is calling callback function at 10 Hz */
            if (tickCounter % 200 == 0) {
                HAL_ADC_ConvCpltCallback(&hadc);
            }
            else {
                HAL_ADC_ConvHalfCpltCallback(&hadc);
            }
        }
        saltleakLoop(bootMsg);
    }

    void setAdcBufferChannel(int ch, int16_t val){
        int n = hadc.Init.NbrOfConversion;
        int ch_dma_len = ADC_CHANNEL_BUF_SIZE * 2;

        for (int i = 0; i < ch_dma_len; i++) {
            ((int16_t*) hadc.dma_address)[ch + i * n] = val;
        }
    }

    /*******************************************************************************************
    ** MEMBERS
    *******************************************************************************************/

    ADC_HandleTypeDef hadc = { 0 };
    CRC_HandleTypeDef hcrc;

    SerialStatusTest sst = {
        .boundInit = bind(saltleakInit, &hadc, &hcrc),
        .testFixture = this,
    };
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(SaltLeakBoard, CorrectBoardParams) {
    goldenPathTest(sst, "-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 0.00, 0xa0005fff");
}

TEST_F(SaltLeakBoard, testPrintStatus) {
    statusPrintoutTest(sst, {
        "Boost error: NO\r",
        "Boost switch mode active: NO\r",
        "Boost mode pin: OFF\r",
        "Sensor 1 state: NOT SENSING\r",
        "Sensor 2 state: NOT SENSING\r",
        "Sensor 3 state: NOT SENSING\r",
        "Sensor 4 state: NOT SENSING\r",
        "Sensor 5 state: NOT SENSING\r",
        "Sensor 6 state: NOT SENSING\r",
    });
}

TEST_F(SaltLeakBoard, testPrintStatusDef) {
    statusDefPrintoutTest(sst,
        "0x7e004000,System errors\r",
        {
        "0x00004000,Boost error\r",
        "0x00002000,Boost switch mode active\r",
        "0x00001000,Boost mode pin\r",
        "0x00000c00,Sensor 1 state\r",
        "0x00000300,Sensor 2 state\r",
        "0x000000c0,Sensor 3 state\r",
        "0x00000030,Sensor 4 state\r",
        "0x0000000c,Sensor 5 state\r",
        "0x00000003,Sensor 6 state\r"
    });
}

TEST_F(SaltLeakBoard, incorrectBoard) { incorrectBoardTest(sst); }

TEST_F(SaltLeakBoard, printSerial) {
    serialPrintoutTest(
        sst, "SaltLeak",
        "Calibration: CAL 1,6.200,100.000 2,18.000,12.443 3,6.200,100.000 4,18.000,12.443 "
        "5,6.200,100.000 6,18.000,12.443 7,6.200,100.000 8,18.000,12.443 9,6.200,100.000 "
        "10,18.000,12.443 11,6.200,100.000 12,18.000,12.443 13,12.443,0\r");
}

TEST_F(SaltLeakBoard, boostMode) {
    saltleakInit(&hadc, &hcrc);

    // Initial state, boost mode OFF, but boost pin ON by default
    goToTick(999);
    EXPECT_TRUE(stmGetGpio(BoostEn));
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 0.00, 0xa0005fff"));

    // Start switching sequence
    writeBoardMessage("boost 1 2\n");

    // Boost mode activated, but boost pin still ON
    goToTick(1000);
    EXPECT_FALSE(stmGetGpio(BoostEn));
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 0.00, 0xa0007fff"));

    // GPIO turns OFF
    goToTick(1100);
    EXPECT_FALSE(stmGetGpio(BoostEn));
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 0.00, 0xa0002fff"));

    goToTick(2900);
    EXPECT_FALSE(stmGetGpio(BoostEn));
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 0.00, 0xa0002fff"));

    // GPIO turns ON
    goToTick(3000);
    EXPECT_TRUE(stmGetGpio(BoostEn));
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 0.00, 0xa0002fff"));

    // Status updated
    goToTick(3100);
    EXPECT_TRUE(stmGetGpio(BoostEn));
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 0.00, 0xa0007fff"));

    // GPIO turns OFF
    goToTick(4000);
    EXPECT_FALSE(stmGetGpio(BoostEn));
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 0.00, 0xa0007fff"));

    // Status updated
    goToTick(4100);
    EXPECT_FALSE(stmGetGpio(BoostEn));
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 0.00, 0xa0002fff"));

    // Stops siwtching sequence
    writeBoardMessage("switch off\n");

    // Switch mode turns OFF and GPIO turns ON (default behavior)
    goToTick(4200);
    EXPECT_TRUE(stmGetGpio(BoostEn));
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 0.00, 0xa0000fff"));

    // Status updated
    goToTick(4300);
    EXPECT_TRUE(stmGetGpio(BoostEn));
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 0.00, 0xa0005fff"));
}

TEST_F(SaltLeakBoard, voltageFeedbackState) {
    saltleakInit(&hadc, &hcrc);

    goToTick(100);
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 0.00, 0xa0005fff"));

    // Boost voltage
    setAdcBufferChannel(6, 3858); // 48.01 V

    // Status update and voltage printed
    goToTick(200);
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 48.01, 0xa0001fff"));

    // VCC voltage
    setAdcBufferChannel(7, 3656); // 5.00 V

    // Status update and voltage printed
    goToTick(300);
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 48.01, 0x00001000"));

    // Boost voltage too low
    setAdcBufferChannel(6, 3536); // 44.00 V

    // Status update and voltage printed
    goToTick(400);
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 44.00, 0x80005fff"));
}

TEST_F(SaltLeakBoard, resistanceCalculation) {
    saltleakInit(&hadc, &hcrc);
    setAdcBufferChannel(6, 3858); // Boost voltage - 48.01 V
    setAdcBufferChannel(7, 3656); // VCC voltage - 5.00 V

    setAdcBufferChannel(2, 560); // Sensor voltage - 6.97 V - Very high resistance (~52 MOhm)
    goToTick(100);
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, 51752.21, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 48.01, 0x000010c0"));

    setAdcBufferChannel(2, 1607); // Sensor voltage - 20.0 V - Medium resistance (~23 kOhm)
    goToTick(200);
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, 23.48, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 48.01, 0x000010c0"));

    setAdcBufferChannel(2, 2867); // Sensor voltage - 35.68 V - Very low resistance (~20 Ohm) - Leak
    goToTick(300);
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, 0.02, -1.00, -1.00, -1.00, 0, 0, 1, 0, 0, 0, 48.01, 0x00001080"));

    setAdcBufferChannel(2, 3215); // Sensor voltage - 40.0 V - Broken sensor
    goToTick(400);
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 48.01, 0x00001040"));

    setAdcBufferChannel(2, 402); // Sensor voltage - 5.0 V - Broken sensor
    goToTick(500);
    EXPECT_FLUSH_USB(Contains("-1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 0, 0, 0, 0, 0, 0, 48.01, 0x00001040"));
}
