/*!
** @file   saltleak_tests.cpp
** @author Luke W
** @date   23/07/2024
*/

#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Fakes */
#include "fake_StmGpio.h"
#include "fake_stm32xxxx_hal.h"
#include "fake_USBprint.h"

/* Real supporting units */
#include "ADCmonitor.c"
#include "CAProtocol.c"
#include "CAProtocolStm.c"

/* UUT */
#include "saltleakLoop.c"

using ::testing::AnyOf;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class SaltLeakBoard: public CaBoardUnitTest
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        SaltLeakBoard() : CaBoardUnitTest(saltleakLoop, SaltLeak, {LATEST_MAJOR, LATEST_MINOR}) {
            hadc.Init.NbrOfConversion = ADC_CHANNELS;
        }

        void simTick()
        {
            if(tickCounter != 0 && (tickCounter % 100 == 0)) {
                /* The timer runs at 10 Hz, if started */
                if(HAL_TIM_Base_GetState(&hboosttim) == HAL_TIM_STATE_BUSY) {
                    HAL_TIM_PeriodElapsedCallback(&hboosttim);
                }

                /* Printout also runs at 10 Hz, but via alternating ADC buffers */
                if(tickCounter % 200 == 0) {
                    HAL_ADC_ConvCpltCallback(&hadc);
                }
                else {
                    HAL_ADC_ConvHalfCpltCallback(&hadc);
                }
            }
            saltleakLoop(bootMsg);
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/

        ADC_HandleTypeDef hadc;
        TIM_HandleTypeDef hboosttim = {
            .Instance = TIM5,
        };
        WWDG_HandleTypeDef hwwdg;

        SerialStatusTest sst = {
            .boundInit = bind(saltleakInit, &hadc, &hboosttim, &hwwdg),
            .testFixture = this,
        };
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(SaltLeakBoard, CorrectBoardParams) {
    goldenPathTest(sst, "0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0x00000002");
}

TEST_F(SaltLeakBoard, printStatusOk) 
{
    statusPrintoutTest(sst, {
        "Boost mode active: 0\r", 
        "Boost mode pin: 0\r",
    });
}

TEST_F(SaltLeakBoard, incorrectBoard) {
    incorrectBoardTest(sst);
}

TEST_F(SaltLeakBoard, printSerial) {
    serialPrintoutTest(sst, "SaltLeak");
}

TEST_F(SaltLeakBoard, Boostmode) {
    saltleakInit(&hadc, &hboosttim, &hwwdg);

    /* Boost mode doesn't take effect until the next 100 ms interval. Send the message at 999 to 
    ** make everything aligned to 1 s */
    goToTick(999);
    EXPECT_TRUE(stmGetGpio(boostGpio));
    writeBoardMessage("boost 1 2\n");

    /* Boost GPIO set to "ON" by default, so entering boost mode starts by turning the GPIO off */
    goToTick(1100);
    EXPECT_FALSE(stmGetGpio(boostGpio));
    EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0x00000001"));
    goToTick(1999);
    EXPECT_FALSE(stmGetGpio(boostGpio));
    EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0x00000001"));

    goToTick(3000);
    EXPECT_TRUE(stmGetGpio(boostGpio));
    EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0x00000003"));
    goToTick(3999);
    EXPECT_TRUE(stmGetGpio(boostGpio));
    EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0x00000003"));

    goToTick(4000);
    EXPECT_FALSE(stmGetGpio(boostGpio));
    EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0x00000001"));
}