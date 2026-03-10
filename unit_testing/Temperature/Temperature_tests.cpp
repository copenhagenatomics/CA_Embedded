/*!
** @file   Temperature_tests.cpp
** @author Luke W
** @date   05/11/2024
*/

#include <inttypes.h>

extern "C" {
    uint32_t _FlashAddrCal = 0;
}

#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Fakes */
#include "fake_StmGpio.h"
#include "fake_stm32xxxx_hal.h"
#include "fake_USBprint.h"
#include "fake_ADS1120.h"

/* Real supporting units */
#include "CAProtocol.c"
#include "CAProtocolStm.c"
#include "crc.c"

/* UUT */
#include "Temperature.c"

using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class TemperatureBoardTest: public CaBoardUnitTest
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        TemperatureBoardTest() : CaBoardUnitTest(&LoopTemperature, Temperature, {LATEST_MAJOR, LATEST_MINOR}) {}

        void simTick() {
            LoopTemperature(bootMsg);
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/
        
        SPI_HandleTypeDef hspi = {
            .Instance = SPI1,
        };
        WWDG_HandleTypeDef hwwdg = {
            .Instance = WWDG,
        };
        CRC_HandleTypeDef hcrc = {
            .Instance = CRC,
        };
        
        SerialStatusTest sst = {
            .boundInit = bind(InitTemperature, &hspi, &hwwdg, &hcrc),
            .testFixture = this,
        };
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(TemperatureBoardTest, goldenPath) {
    sst.boundInit();
    for(int i = 0; i < NO_SPI_DEVICES; i++) {
        setTemp(&ads1120[i], 24.00, 26.00, 25.00);
    }

    goldenPathTest(sst, "24.00, 26.00, 24.00, 26.00, 24.00, 26.00, 24.00, 26.00, 24.00, 26.00, 25.00, 0x00000000");
}

TEST_F(TemperatureBoardTest, incorrectBoard) {
    incorrectBoardTest(sst);
}

TEST_F(TemperatureBoardTest, printStatus) {
    statusPrintoutTest(sst,{"The board is operating normally.\r"});
}

TEST_F(TemperatureBoardTest, printStatusDef) {
    statusDefPrintoutTest(sst,
        {"0x7e0003ff,System errors\r"},
        {"0x00000003,Status ADC 1\r",
         "0x0000000c,Status ADC 2\r",
         "0x00000030,Status ADC 3\r",
         "0x000000c0,Status ADC 4\r",
         "0x00000300,Status ADC 5\r"});
}

TEST_F(TemperatureBoardTest, printSerial) {
    /* Default calibration string */
    serialPrintoutTest(sst, "Temperature", "Calibration: CAL 1,0.0000412760,0.0000407300 2,0.0000412760,0.0000407300 3,0.0000412760,0.0000407300 4,0.0000412760,0.0000407300 5,0.0000412760,0.0000407300 6,0.0000412760,0.0000407300 7,0.0000412760,0.0000407300 8,0.0000412760,0.0000407300 9,0.0000412760,0.0000407300 10,0.0000412760,0.0000407300\r");
}