/*!
** @file   LightController_tests.cpp
** @author Matias
** @date   08/04/2024
*/

/* CA unit tests */
#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Real supporting units */
#include "ADCmonitor.c"
#include "CAProtocol.c"
#include "CAProtocolStm.c"

/* UUT */
#include "LightController.c"

using ::testing::Contains;
using ::testing::ElementsAre;

using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class LightControllerTests : public CaBoardUnitTest {
   protected:
    /*******************************************************************************************
    ** METHODS
    *******************************************************************************************/
    LightControllerTests()
        : CaBoardUnitTest(&LightControllerLoop, LightController, {LATEST_MAJOR, LATEST_MINOR}) {
        hadc1.Init.NbrOfConversion = 1;
    }

    void simTick() {
        // ADC buffer should be half full every 100 ms
        if (tickCounter != 0) {
            if (tickCounter % 200 == 0) {
                HAL_ADC_ConvCpltCallback(&hadc1);
            }
            else if (tickCounter % 200 == 100) {
                HAL_ADC_ConvHalfCpltCallback(&hadc1);
            }
        }
        LightControllerLoop(bootMsg);
    }

    void setVoltage(int16_t adcVal) {
        int ch_dma_len = ADC_CHANNEL_BUF_SIZE * 2;

        for (int i = 0; i < ch_dma_len; i++) {
            ((int16_t*)hadc1.dma_address)[i] = adcVal;
        }
    }

    /*******************************************************************************************
    ** MEMBERS
    *******************************************************************************************/

    ADC_HandleTypeDef hadc1 = {0};
    TIM_HandleTypeDef htim1 = {.Instance = TIM1};
    TIM_HandleTypeDef htim2 = {.Instance = TIM2};
    TIM_HandleTypeDef htim3 = {.Instance = TIM3};
    TIM_HandleTypeDef htim4 = {.Instance = TIM4};

    SerialStatusTest sst = {
        .boundInit   = bind(LightControllerInit, &hadc1, &htim1, &htim2, &htim3, &htim4),
        .testFixture = this};
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(LightControllerTests, incorrectBoard) {
    incorrectBoardTest(sst);
}

TEST_F(LightControllerTests, incorrectBoardVersion) {
    incorrectBoardVersionTest(sst);
}

TEST_F(LightControllerTests, testPrintStatus) {
    statusPrintoutTest(sst, {"The board is operating normally.\r", "Port 1: On: 0\r",
                             "Port 2: On: 0\r", "Port 3: On: 0\r"});
}

TEST_F(LightControllerTests, testPrintStatusDef) {
    statusDefPrintoutTest(
        sst, "0x7e000000,System errors\r",
        {"0x00000001,Status port 1\r", "0x00000002,Status port 2\r", "0x00000004,Status port 3\r"});
}

TEST_F(LightControllerTests, testPrintSerial) {
    serialPrintoutTest(sst, "LightController");
}

TEST_F(LightControllerTests, InputHandler) {
    sst.boundInit();
    simTicks(100);
    (void)hostUSBread(true);
    setVoltage(681);  // 24 V

    /* ----- VALID INPUTS ----- */
    writeBoardMessage("p1 ABAB10\n");
    simTicks(100);
    EXPECT_FLUSH_USB(ElementsAre("abab10, 000000, 000000, 0x00000001\r"));

    writeBoardMessage("p2 105020\n");
    simTicks(100);
    EXPECT_FLUSH_USB(ElementsAre("abab10, 105020, 000000, 0x00000003\r"));

    writeBoardMessage("p3 ffffff\n");
    simTicks(100);
    EXPECT_FLUSH_USB(ElementsAre("abab10, 105020, ffffff, 0x00000007\r"));

    // Reset all ports
    writeBoardMessage("p1 000000\n");
    writeBoardMessage("p2 000000\n");
    writeBoardMessage("p3 000000\n");
    simTicks(100);
    EXPECT_FLUSH_USB(ElementsAre("000000, 000000, 000000, 0x00000000\r"));

    /* ----- INVALID INPUTS ----- */

    /* For the misreads we do not need to call the printtimer callback
       since the MISREAD messages are pushed to the USB by the standard
       protocol.*/

    // Invalid port
    writeBoardMessage("p4 FFFFFF\n");
    EXPECT_FLUSH_USB(ElementsAre("MISREAD: p4 FFFFFF\r"));

    writeBoardMessage("p0 0xFFFFFF\n");
    EXPECT_FLUSH_USB(ElementsAre("MISREAD: p0 0xFFFFFF\r"));

    // Wrong hex code (too short)
    writeBoardMessage("p1 13908\n");
    EXPECT_FLUSH_USB(ElementsAre("MISREAD: p1 13908\r"));

    // Wrong hex code (too long)
    writeBoardMessage("p1 13908CC\n");
    EXPECT_FLUSH_USB(ElementsAre("MISREAD: p1 13908CC\r"));

    // Wrong hex code (non valid hex characters)
    writeBoardMessage("p1 ACACFM\n");
    EXPECT_FLUSH_USB(ElementsAre("MISREAD: p1 ACACFM\r"));
}

TEST_F(LightControllerTests, LEDSwitchingTimeout) {
    sst.boundInit();
    simTicks(100);
    (void)hostUSBread(true);
    setVoltage(681);  // 24 V

    /* --- Turn on white LED for p1-3  --- */
    writeBoardMessage("p1 FFFFFF\n");
    writeBoardMessage("p2 FFFFFF\n");
    writeBoardMessage("p3 FFFFFF\n");
    simTicks(100);
    EXPECT_FLUSH_USB(ElementsAre("ffffff, ffffff, ffffff, 0x00000007\r"));

    simTicks(29800);
    (void)hostUSBread(true);
    simTicks(100); // Just before timeout
    EXPECT_FLUSH_USB(ElementsAre("ffffff, ffffff, ffffff, 0x00000007\r"));

    simTicks(100); // Just after timeout
    EXPECT_FLUSH_USB(ElementsAre("000000, 000000, 000000, 0x00000000\r"));
}
