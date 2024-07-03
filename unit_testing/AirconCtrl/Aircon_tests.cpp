/*!
** @file   AC_tests.cpp
** @author Luke W
** @date   12/10/2023
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

/* Fakes */
#include "fake_StmGpio.h"
#include "fake_stm32xxxx_hal.h"
#include "fake_USBprint.h"

/* Real supporting units */
#include "transmitterIR.c"
#include "CAProtocol.c"
#include "CAProtocolStm.c"

/* UUT */
#include "airconCtrl.c"

using ::testing::AnyOf;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class AirconBoard: public ::testing::Test 
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        AirconBoard()
        {
            /* OTP code to allow initialisation of the board to pass */
            HAL_otpWrite(&bi);
            forceTick(0);
            hostUSBConnect();
        }

        void writeAcMessage(const char * msg)
        {
            hostUSBprintf(msg);
            airconCtrlLoop(bootMsg);
        }

        void simTick(int numTicks = 1)
        {
            for(int i = tickCounter + 1; i <= tickCounter + numTicks; i++) 
            {
                forceTick(i);
                if(i != 0 && (i % 100 == 0)) {
                    HAL_TIM_PeriodElapsedCallback(&hlooptim);
                }
                airconCtrlLoop(bootMsg);
            }
            tickCounter = tickCounter + numTicks;
        }

        void goToTick(int tickDest) {
            while(tickCounter < tickDest) {
                simTick();
            }
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/
        
        /* Default OTP code */
        BoardInfo bi = {
            .v2 = {
                .otpVersion = OTP_VERSION_2,
                .boardType  = AirCondition,
                .subBoardType = 0,
                .reserved = {0},
                .pcbVersion = {
                    .major = BREAKING_MAJOR,
                    .minor = BREAKING_MINOR
                },
                .productionDate = 0
            }
        };

        TIM_HandleTypeDef hlooptim = {
            .Instance = TIM2,
        };
        TIM_HandleTypeDef hctxtim = {
            .Instance = TIM1,
        };
        WWDG_HandleTypeDef hwwdg;
        const char * bootMsg = "Boot Unit Test";
        int tickCounter = 0;
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(AirconBoard, CorrectBoardParams)
{
    airconCtrlInit(&hctxtim, &hlooptim, &hwwdg);

    /* This should force a print on the USB bus */
    goToTick(100);

    /* Check the printout is correct */
    EXPECT_READ_USB(Contains("0, 0x00000000"));
}

TEST_F(AirconBoard, printStatusOk) 
{
    airconCtrlInit(&hctxtim, &hlooptim, &hwwdg);
    /* Note: usb RX buffer is flushed during the first loop, so a single loop must be done before
    ** printing anything */
    airconCtrlLoop(bootMsg);
    writeAcMessage("Status\n");

    EXPECT_FLUSH_USB(ElementsAre(
        "\r", 
        "Boot Unit Test\r", 
        "Start of board status:\r", 
        "The board is operating normally.\r",
        "\r",
        "End of board status. \r"
    ));
}

TEST_F(AirconBoard, incorrectBoard) {
    /* Update OTP with incorrect board number */
    if(BREAKING_MINOR != 0) {
        bi.v2.pcbVersion.minor = BREAKING_MINOR - 1;
    }
    else if(BREAKING_MAJOR != 0) {
        bi.v2.pcbVersion.major = BREAKING_MAJOR - 1;
    }
    else {
        FAIL() << "Oldest PCB Version must be at least v0.1";
    }
    
    HAL_otpWrite(&bi);

    airconCtrlInit(&hctxtim, &hlooptim, &hwwdg);

    /* This should force a print on the USB bus */
    goToTick(100);

    /* Check the printout is correct */
    EXPECT_READ_USB(Contains("0x84000000"));
}

TEST_F(AirconBoard, printSerial) 
{
    airconCtrlInit(&hctxtim, &hlooptim, &hwwdg);
    /* Note: usb RX buffer is flushed during the first loop, so a single loop must be done before
    ** printing anything */
    airconCtrlLoop(bootMsg);
    writeAcMessage("Serial\n");
    
    EXPECT_FLUSH_USB(ElementsAre(
        "\r", 
        "Boot Unit Test\r", 
        "Serial Number: 000\r", 
        "Product Type: AirCondition\r", 
        "Sub Product Type: 0\r", 
        "MCU Family: Unknown 0x  0 Rev 0\r", 
        "Software Version: 0\r", 
        "Compile Date: 0\r", 
        "Git SHA: 0\r", 
        "PCB Version: 1.8\r"
    ));
}
