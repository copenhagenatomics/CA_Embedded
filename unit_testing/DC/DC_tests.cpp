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
#include "ADCmonitor.c"
#include "CAProtocol.c"
#include "CAProtocolStm.c"

/* UUT */
#include "DCBoard.c"

using ::testing::AnyOf;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class DCBoard: public ::testing::Test 
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        DCBoard()
        {
            hadc.Init.NbrOfConversion = 5;

            /* OTP code to allow initialisation of the board to pass */
            /* TODO: Make this use a define for the board release, so that tests fail if the someone
            ** forgets to update the version numbers */
            BoardInfo bi = {
                .v2 = {
                    .otpVersion = OTP_VERSION_2,
                    .boardType  = DC_Board,
                    .subBoardType = 0,
                    .reserved = {0},
                    .pcbVersion = {
                        .major = 6,
                        .minor = 4
                    },
                    .productionDate = 0
                }
            };
            HAL_otpWrite(&bi);
            forceTick(0);
            hostUSBConnect();
        }

        void writeDcMessage(const char * msg)
        {
            hostUSBprintf(msg);
            DCBoardLoop(bootMsg);
        }

        void simTick(int numTicks = 1)
        {
            for(int i = tickCounter + 1; i <= tickCounter + numTicks; i++) 
            {
                forceTick(i);
                if(i != 0 && (i % 100 == 0)) {
                    if(i % 200 == 0) {
                        HAL_ADC_ConvCpltCallback(&hadc);
                    }
                    else {
                        HAL_ADC_ConvHalfCpltCallback(&hadc);
                    }
                }
                DCBoardLoop(bootMsg);
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
        
        ADC_HandleTypeDef hadc;
        WWDG_HandleTypeDef hwwdg;
        const char * bootMsg = "Boot Unit Test";
        int tickCounter = 0;
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(DCBoard, CorrectBoardParams)
{
    DCBoardInit(&hadc, &hwwdg);

    /* Basic test, was everything OK?  */
    EXPECT_FALSE(bsGetStatus() && BS_VERSION_ERROR_Msk);

    /* This should force a print on the USB bus */
    DCBoardLoop(bootMsg);
    HAL_ADC_ConvHalfCpltCallback(&hadc);
    DCBoardLoop(bootMsg);

    /* Check the printout is correct */
    EXPECT_READ_USB(Contains("-6.25, -6.25, -6.25, -6.25, -6.25, -6.25, 0x0"));
}
