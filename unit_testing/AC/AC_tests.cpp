/*!
** @file   AC_tests.cpp
** @author Luke W
** @date   12/10/2023
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
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
#include "CAProtocolACDC.c"
#include "faultHandlers.c"

/* Prevents attempting to access non-existent linker script variables */
#define FLASH_ADDR_FAULT ((uint32_t) 0U)

#include "flashHandler.c"

/* UUT */
#include "ACBoard.c"

using ::testing::AnyOf;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class ACBoard: public CaBoardUnitTest
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        ACBoard() : CaBoardUnitTest(ACBoardLoop, AC_Board, {LATEST_MAJOR, LATEST_MINOR}) {
            hadc.Init.NbrOfConversion = 8;

            /* Prevent fault info triggering automatically at startup */
            faultInfo_t tmp = {.fault = NO_FAULT};
            writeToFlash(FLASH_ADDR_FAULT, (uint8_t*)&tmp, sizeof(faultInfo_t));
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
            ACBoardLoop(bootMsg);
        }

        void setPowerStatus(bool state)
        {
            ACBoardInit(&hadc);
            powerStatus.state = state;
            for (int i = 0; i < 1000; i++)
            {
                updateBoardStatus();
            }
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/
        
        ADC_HandleTypeDef hadc;
        
        SerialStatusTest sst = {
            .boundInit = bind(ACBoardInit, &hadc),
            .testFixture = this
        };
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(ACBoard, CorrectBoardParams) {
    setPowerStatus(true);

    goldenPathTest(sst, "-0.0100, -0.0100, -0.0100, -0.0100, -50.00, -50.00, -50.00, -50.00, 0x00000000");
}

TEST_F(ACBoard, printStatus) {
    setPowerStatus(true);

    statusPrintoutTest(sst, {"Fan     On: 0\r", 
                             "Port 0: On: 0, PWM percent: 0\r", 
                             "Port 1: On: 0, PWM percent: 0\r", 
                             "Port 2: On: 0, PWM percent: 0\r", 
                             "Port 3: On: 0, PWM percent: 0\r",
                             "Power   On: 1\r"});

    setPowerStatus(false);
    writeBoardMessage("fan on\n");
    writeBoardMessage("p2 on 1\n");
    writeBoardMessage("p4 on 1\n");
    writeBoardMessage("Status\n");
    
    EXPECT_FLUSH_USB(ElementsAre( 
        "\r", 
        "Start of board status:\r",
        "Fan     On: 1\r",
        "Port 0: On: 0, PWM percent: 0\r",
        "Port 1: On: 1, PWM percent: 100\r",
        "Port 2: On: 0, PWM percent: 0\r",
        "Port 3: On: 1, PWM percent: 100\r",
        "Power   On: 0\r",
        "\r", 
        "End of board status. \r"
    ));

    /* It would be nice to be easily able to test PWM too, but this would require mocking/faking 
    ** heatCtrl module */
}

TEST_F(ACBoard, printStatusDef) {
    statusDefPrintoutTest(sst, "0x7e000020,System errors\r", 
                          {"0x00000020,Mains not-connected error\r", 
                           "0x00000010,Port 4 switching state\r", 
                           "0x00000008,Port 3 switching state\r", 
                           "0x00000004,Port 2 switching state\r", 
                           "0x00000002,Port 1 switching state\r", 
                           "0x00000001,Fan state\r"});
}

TEST_F(ACBoard, printSerial) {
    serialPrintoutTest(sst, "AC Board");
}

TEST_F(ACBoard, incorrectBoardParams) {
    incorrectBoardTest(sst);
}

/* Note: this test uses some knowledge of internals of ACBoard.c (e.g. white box), which isn't 
** perfect */
TEST_F(ACBoard, fanInput) 
{
    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);

    EXPECT_FALSE(isFanForceOn);
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    writeBoardMessage("fan on\n");
    EXPECT_TRUE(isFanForceOn);
    EXPECT_TRUE(stmGetGpio(fanCtrl));

    writeBoardMessage("fan off\n");
    EXPECT_FALSE(isFanForceOn);
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    writeBoardMessage("fan wrong\n");
    EXPECT_FALSE(isFanForceOn);
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    EXPECT_READ_USB(Contains("MISREAD: fan wrong"));
}

/* Note: this test uses some knowledge of internals of ACBoard.c (e.g. white box), which isn't 
** perfect */
TEST_F(ACBoard, GpioInit) 
{
    auto expectStmNull = [](StmGpio* stm) {
        EXPECT_EQ(stm->set,    nullptr);
        EXPECT_EQ(stm->get,    nullptr);
        EXPECT_EQ(stm->toggle, nullptr);
    };

    auto expectStmNotNull = [](StmGpio* stm) {
        EXPECT_NE(stm->set,    nullptr);
        EXPECT_NE(stm->get,    nullptr);
        EXPECT_NE(stm->toggle, nullptr);
    };

    expectStmNull(&fanCtrl);
    for(int i = 0; i < AC_BOARD_NUM_PORTS; i++) expectStmNull(&heaterPorts[i].heater);

    ACBoardInit(&hadc);

    expectStmNotNull(&fanCtrl);
    for(int i = 0; i < AC_BOARD_NUM_PORTS; i++) expectStmNotNull(&heaterPorts[i].heater);

    /* The GPIO should turn on immediately if a normal message is sent */
    EXPECT_FALSE(stmGetGpio(heaterPorts[0].heater));

    ACBoardLoop(bootMsg);
    writeBoardMessage("p1 on 1\n");
    EXPECT_TRUE(stmGetGpio(heaterPorts[0].heater));

    /* The GPIO should turn on at the next PWM cycle if a % message is sent */
    EXPECT_FALSE(stmGetGpio(heaterPorts[1].heater));
    writeBoardMessage("p2 on 2 50%\n");
    EXPECT_FALSE(stmGetGpio(heaterPorts[1].heater));
    
    /* Check PWM starts at beginning of next cycle */
    goToTick(1000);
    EXPECT_TRUE(stmGetGpio(heaterPorts[1].heater));

    /* Still on all the way to 50%? */
    goToTick(1499);
    EXPECT_TRUE(stmGetGpio(heaterPorts[1].heater));

    /* Turns off at 50% */
    goToTick(1500);
    EXPECT_FALSE(stmGetGpio(heaterPorts[1].heater));

    /* Doesn't turn on again (duration set to 2 secs at t=0) */
    goToTick(2000);
    EXPECT_FALSE(stmGetGpio(heaterPorts[1].heater));

    /* Quick test "next cycle turn on" works elsewhere within PWM period */
    goToTick(2750);
    EXPECT_FALSE(stmGetGpio(heaterPorts[2].heater));
    writeBoardMessage("p3 on 3 25%\n");
    EXPECT_FALSE(stmGetGpio(heaterPorts[2].heater));
    goToTick(2999);

    EXPECT_FALSE(stmGetGpio(heaterPorts[2].heater));
    goToTick(3000);
    EXPECT_TRUE(stmGetGpio(heaterPorts[2].heater));
}

/* Stacked switching basically checked in heatCtrl tests */

TEST_F(ACBoard, InvalidCommands)
{
    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);
    hostUSBread(true); /* Flush USB buffer */

    /* OK messages */
    writeBoardMessage("p1 on 1\n");
    EXPECT_READ_USB(IsEmpty());
    writeBoardMessage("p2 on 1\n");
    EXPECT_READ_USB(IsEmpty());
    writeBoardMessage("p3 on 1\n");
    EXPECT_READ_USB(IsEmpty());
    writeBoardMessage("p4 on 1\n");
    EXPECT_READ_USB(IsEmpty());

    /* Fail messages */
    writeBoardMessage("p1 on\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: p1 on -1"));
    writeBoardMessage("p2 on -1\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: p2 on -1"));
    writeBoardMessage("p3 on 10%\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: p3 on -1"));
    writeBoardMessage("p4 on 80%\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: p4 on -1"));
    writeBoardMessage("all on\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: all on"));
}

TEST_F(ACBoard, UsbTimeout)
{
    static const int TEST_LENGTH_MS = 10000;
    static const int TIMEOUT_LENGTH_MS = 5000;

    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);
    writeBoardMessage("all on 60\n");

    for(int i = 0; i < TEST_LENGTH_MS / 10; i++)
    {
        if(i == 0.25 * TEST_LENGTH_MS / 10) {
            hostUSBDisconnect();
        }

        goToTick(i * 10);

        if(i < (0.25 * TEST_LENGTH_MS / 10 + TIMEOUT_LENGTH_MS / 10)) {
            ASSERT_TRUE(stmGetGpio(heaterPorts[0].heater)) << i;
            ASSERT_TRUE(stmGetGpio(heaterPorts[1].heater)) << i;
            ASSERT_TRUE(stmGetGpio(heaterPorts[2].heater)) << i;
            ASSERT_TRUE(stmGetGpio(heaterPorts[3].heater)) << i;
        }
        else {
            ASSERT_FALSE(stmGetGpio(heaterPorts[0].heater)) << i;
            ASSERT_FALSE(stmGetGpio(heaterPorts[1].heater)) << i;
            ASSERT_FALSE(stmGetGpio(heaterPorts[2].heater)) << i;
            ASSERT_FALSE(stmGetGpio(heaterPorts[3].heater)) << i;
        }
    }
}

TEST_F(ACBoard, heatsinkLoop) 
{
    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);

    setPowerStatus(true);

    EXPECT_FALSE(stmGetGpio(fanCtrl));

    const int TEMP_CHANNEL = 4;

    /* Fill the temperature buffer with ~54 degC - fan should stay off */
    for(unsigned i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + TEMP_CHANNEL + ADC_CHANNELS*i) = 1300;
    goToTick(100);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 54.78, -50.00, -50.00, -50.00, 0x00000000"));
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    /* Fill the temperature buffer with ~56 degC - fan should turn on */
    for(unsigned i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + TEMP_CHANNEL +  ADC_CHANNELS*i) = 1320;
    goToTick(200);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 56.39, -50.00, -50.00, -50.00, 0x00000000"));
    EXPECT_TRUE(stmGetGpio(fanCtrl));

    /* Fill the temperature buffer with ~51 degC - fan should remain on */
    /* Note: Status changes to 0x1 because fan pin is enabled. The previous message didn't contain 
    ** it because the printout and heatSink update are in the same function, and the heatsinkLoop 
    ** is outside the function */
    for(unsigned i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + TEMP_CHANNEL +  ADC_CHANNELS*i) = 1250;
    goToTick(300);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 50.75, -50.00, -50.00, -50.00, 0x00000001"));
    EXPECT_TRUE(stmGetGpio(fanCtrl));

    /* Fill the temperature buffer with ~49 degC - fan should turn off */
    for(unsigned i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + TEMP_CHANNEL +  ADC_CHANNELS*i) = 1228;
    goToTick(400);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 48.98, -50.00, -50.00, -50.00, 0x00000001"));
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    /* Fill the temperature buffer with ~71 degC - fan should turn on and PWM of heaters should be reduced */
    writeBoardMessage("p1 on 10\n");
    for(unsigned i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + TEMP_CHANNEL +  ADC_CHANNELS*i) = 1500;
    goToTick(600);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 70.90, -50.00, -50.00, -50.00, 0xc0000003"));
    EXPECT_TRUE(stmGetGpio(fanCtrl));
}

TEST_F(ACBoard, faultInfoPrintout) {
    faultInfo_t tmp = {.fault = HARD_FAULT};
    writeToFlash(FLASH_ADDR_FAULT, (uint8_t*)&tmp, sizeof(faultInfo_t));

    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);

    EXPECT_FLUSH_USB(ElementsAre(
        "\r", 
        "Boot Unit Test\r", 
        "Start of fault info\r", 
        "Last fault was: 1\r", 
        "CFSR was: 0x00000000\r", 
        "HFSR was: 0x00000000\r", 
        "MMFA was: 0x00000000\r", 
        "BFA was:  0x00000000\r", 
        "AFSR was: 0x00000000\r", 
        "Stack Frame was:\r", "0x00000000, 0x00000000, 0x00000000, 0x00000000\r", 
        "0x00000000, 0x00000000, 0x00000000, 0x00000000\r", 
        "End of fault info\r"
    ));
}