/*!
** @file   ACTenChannel_tests.cpp
** @author Matias
** @date   21/11/2024
*/

#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Fakes */
#include "fake_StmGpio.h"
#include "fake_stm32xxxx_hal.h"
#include "fake_USBprint.h"

/* Real supporting units */
#include "HeatCtrl.c"
#include "ADCmonitor.c"
#include "CAProtocol.c"
#include "CAProtocolStm.c"

/* UUT */
#include "ACTenChannel.c"

using ::testing::AnyOf;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class ACTenCh: public CaBoardUnitTest 
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        ACTenCh() : CaBoardUnitTest(&ACTenChannelLoop, ACTenChannel, {LATEST_MAJOR, LATEST_MINOR}) {
            hadc1.Init.NbrOfConversion = 10;
        }

        void simTick()
        {
            if(tickCounter != 0 && (tickCounter % 100 == 0)) {
                if(tickCounter % 200 == 0) {
                    HAL_ADC_ConvCpltCallback(&hadc1);
                }
                else {
                    HAL_ADC_ConvHalfCpltCallback(&hadc1);
                }
            }
            ACTenChannelLoop(bootMsg);
        }

        void setAdcBufferChannel(int ch, int16_t val) {
            /* According to default calibration, 2048 yields ~0 current */
            int n = hadc1.Init.NbrOfConversion;
            int ch_dma_len = ADC_CHANNEL_BUF_SIZE * 2;

            for(int i = 0; i < ch_dma_len; i++) {
                ((int16_t*)hadc1.dma_address)[ch + i * n] = val;
            }
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/
        
        ADC_HandleTypeDef hadc1 = {0};

        TIM_HandleTypeDef htim2 = {
            .Instance = TIM2
        };

        TIM_HandleTypeDef htim5 = {
            .Instance = TIM5
        };

        SerialStatusTest sst = {
            .boundInit = bind(&ACTenChannelInit, &hadc1, &htim2, &htim5),
            .testFixture = this,
        };
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(ACTenCh, goldenPath)
{
    sst.boundInit();

    goldenPathTest(sst, "-0.0100, -0.0100, -0.0100, -0.0100, -0.0100, -0.0100, -0.0100, -0.0100, -0.0100, -0.0100, 0x00000000");    
}

TEST_F(ACTenCh, printStatusDef) {
    statusDefPrintoutTest(sst, "0x7e000000,System errors\r", {
        "0x00000001,Port 0 switching state\r", 
        "0x00000002,Port 1 switching state\r", 
        "0x00000004,Port 2 switching state\r", 
        "0x00000008,Port 3 switching state\r", 
        "0x00000010,Port 4 switching state\r", 
        "0x00000020,Port 5 switching state\r", 
        "0x00000040,Port 6 switching state\r", 
        "0x00000080,Port 7 switching state\r", 
        "0x00000100,Port 8 switching state\r",
        "0x00000200,Port 9 switching state\r"
    });
}

TEST_F(ACTenCh, printStatus) {
    statusPrintoutTest(sst, {
        "The board is operating normally.\r", 
        "Port 0: On: 0, PWM percent: 0\r", 
        "Port 1: On: 0, PWM percent: 0\r", 
        "Port 2: On: 0, PWM percent: 0\r", 
        "Port 3: On: 0, PWM percent: 0\r", 
        "Port 4: On: 0, PWM percent: 0\r", 
        "Port 5: On: 0, PWM percent: 0\r", 
        "Port 6: On: 0, PWM percent: 0\r", 
        "Port 7: On: 0, PWM percent: 0\r", 
        "Port 8: On: 0, PWM percent: 0\r", 
        "Port 9: On: 0, PWM percent: 0\r",
    });

    writeBoardMessage("p2 on 1\n");
    writeBoardMessage("p8 on 1\n");
    writeBoardMessage("Status\n");

    EXPECT_FLUSH_USB(ElementsAre(
        "\r", 
        "Start of board status:\r", 
        "The board is operating normally.\r",
        "Port 0: On: 0, PWM percent: 0\r", 
        "Port 1: On: 1, PWM percent: 100\r", 
        "Port 2: On: 0, PWM percent: 0\r", 
        "Port 3: On: 0, PWM percent: 0\r", 
        "Port 4: On: 0, PWM percent: 0\r", 
        "Port 5: On: 0, PWM percent: 0\r", 
        "Port 6: On: 0, PWM percent: 0\r", 
        "Port 7: On: 1, PWM percent: 100\r", 
        "Port 8: On: 0, PWM percent: 0\r", 
        "Port 9: On: 0, PWM percent: 0\r",
        "\r", 
        "End of board status. \r"
    ));
}

TEST_F(ACTenCh, printSerial) {
    /* Default calibration string */
    serialPrintoutTest(sst, "ACTenChannel");
}

TEST_F(ACTenCh, UsbTimeout)
{
    static const int TEST_LENGTH_MS = 10000;
    static const int TIMEOUT_LENGTH_MS = 5000;

    ACTenChannelInit(&hadc1, &htim2, &htim5);
    ACTenChannelLoop(bootMsg);

    writeBoardMessage("all on 60\n");

    for(int i = 0; i < TEST_LENGTH_MS / 10; i++)
    {
        if(i == 0.25 * TEST_LENGTH_MS / 10) {
            hostUSBDisconnect();
        }

        goToTick(i * 10);

        if(i <= (0.25 * TEST_LENGTH_MS / 10 + TIMEOUT_LENGTH_MS / 10)) {
            for (int j = 0; j < AC_TEN_CH_NUM_PORTS; j++)
            {
                ASSERT_TRUE(stmGetGpio(heaterPorts[j])) << i;
            }
        }
        else {
            for (int j = 0; j < AC_TEN_CH_NUM_PORTS; j++)
            {
                ASSERT_FALSE(stmGetGpio(heaterPorts[j])) << i;
            }
        }
    }
}
