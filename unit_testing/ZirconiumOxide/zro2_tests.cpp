/*!
** @file   zro2_tests.cpp
** @author Matias
** @date   01/08/2024
*/
#include <inttypes.h>
#include <string.h>

extern "C" {
    uint32_t _FlashAddrCal = 0;
    uint32_t _FlashAddrUptime = 0;
}

#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Fakes */
#include "fake_stm32xxxx_hal.h"
#include "fake_USBprint.h"
#include "fake_StmGpio.h"

#include "uptime.h"

/* Real supporting units */
#include "stm32f4xx_hal_gpio.c"
#include "ADCmonitor.c"
#include "CAProtocol.c"
#include "CAProtocolStm.c"
#include "calibration.c"
#include "ZrO2Sensor.c"
#include "activeHeatCtrl.c"
#include "AD5227x.c"
#include "uptime.c"

/* UUT */
#include "oxygen.c"

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class ZrO2Test: public CaBoardUnitTest
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        ZrO2Test() : CaBoardUnitTest(oxygenLoop, ZrO2Oxygen, {LATEST_MAJOR, LATEST_MINOR}) {
            hadc.Init.NbrOfConversion = 7;
        }

        void simTick()
        {
            if(tickCounter != 0 && (tickCounter % 100 == 0)) {
                if(tickCounter % 200 == 0) {
                    HAL_ADC_ConvCpltCallback(&hadc);
                }
                else {
                    HAL_ADC_ConvHalfCpltCallback(&hadc);
                }
            }
            oxygenLoop(bootMsg);
        }

        void setAdcBufferChannel(int ch, int16_t val) {
            /* Set Current and Voltage channels to */
            int n = hadc.Init.NbrOfConversion;
            int ch_dma_len = ADC_CHANNEL_BUF_SIZE * 2;

            for(int i = 0; i < ch_dma_len; i++) {
                ((int16_t*)hadc.dma_address)[ch + i * n] = val;
            }
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/
        
        ADC_HandleTypeDef hadc;
        TIM_HandleTypeDef adcTimer;
        TIM_HandleTypeDef loopTimer;
        CRC_HandleTypeDef hcrc;

        SerialStatusTest sst = {
            .boundInit = bind(oxygenInit, &hadc, &adcTimer, &loopTimer, &hcrc),
            .testFixture = this,
        };
};

TEST_F(ZrO2Test, CorrectBoardParams) {
    goldenPathTest(sst, "-1.00, -1.00, -1.00, 0.00, 0.00, 0x00000000");
}

TEST_F(ZrO2Test, incorrectBoard) {
    incorrectBoardTest(sst);
}

TEST_F(ZrO2Test, printSerial) {
    serialPrintoutTest(sst, "ZrO2Oxygen", "Calibration: CAL 1,13,-1500.00,81.699997,-0.00496 2,13,0.00,0.305000,0.00000 9.10,9.10\r");
}

TEST_F(ZrO2Test, printStatus) {
    statusPrintoutTest(sst, {
        "High range sensor is: Off\r", 
        "Low range sensor is: Off\r",
        "Oxygen level is below measurement range: No\r",
        "High range sensor has heater error: No\r", 
        "Low range sensor has heater error: No\r",
        "Board total on time: 0 min.\r", 
        "High range sensor total on time: 0 min.\r", 
        "High range heater total on time: 0 min.\r", 
        "Low range sensor total on time: 0 min.\r", 
        "Low range heater total on time: 0 min.\r",
    });
}

TEST_F(ZrO2Test, testUptime) {
    // Initialise board
    oxygenInit(&hadc, &adcTimer, &loopTimer, &hcrc);
    
    uint32_t tickUpdateVal = 60000; // 1 minute of tick (ms)
    uint32_t minutes_per_day = 1440;
    
    for (uint32_t i = 0; i<=minutes_per_day; i++)
    {
        // Increment ticker value 1 min at a time until
        // it has reached one day of run time
        forceTick(tickUpdateVal*i);
        oxygenLoop(bootMsg);
        writeBoardMessage("Status\n");
        usbFlush();
        
        // i = number of minutes simulated
        string uptimeOutput = "Board total on time: " + std::to_string(i) + " min.\r";

        // Check the board total time updates every minute
        EXPECT_FLUSH_USB(Contains(uptimeOutput));
    }

    // Read from FLASH and see that it has been stored
    // oxygenInit calls loadUptime which loads the uptime stored in FLASH
    Uptime uptime = {0};
    (void) readFromFlashCRC(&hcrc, (uint32_t) FLASH_ADDR_UPTIME, (uint8_t*) &uptime, sizeof(uptime));
    EXPECT_EQ(uptime.board_uptime, 1440);
}