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
            hadc.Init.NbrOfConversion = 6;

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
                        .major = 3,
                        .minor = 1
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

        void setDcCurrentBuffer() {
            /* According to default calibration, 2048 yields ~0 current */
            for(int i = 0; i < hadc.dma_length; i++) {
                *((int16_t*)hadc.dma_address + i) = 2048;
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
        
    /* Enable 24V power and fill the current buffer with a sensible value */
    stmSetGpio(sense24v, true);
    setDcCurrentBuffer();

    /* Basic test, was everything OK?  */
    EXPECT_FALSE(bsGetStatus() && BS_VERSION_ERROR_Msk);

    /* This should force a print on the USB bus */
    DCBoardLoop(bootMsg);
    HAL_ADC_ConvHalfCpltCallback(&hadc);
    DCBoardLoop(bootMsg);

    /* Check the printout is correct */
    EXPECT_READ_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x0"));
}

TEST_F(DCBoard, printSerial) 
{
    DCBoardInit(&hadc, &hwwdg);

    /* Enable 24V power and fill the current buffer with a sensible value */
    stmSetGpio(sense24v, true);
    setDcCurrentBuffer();

    /* Note: usb RX buffer is flushed during the first loop, so a single loop must be done before
    ** printing anything */
    DCBoardLoop(bootMsg);
    writeDcMessage("Serial\n");
    
    EXPECT_FLUSH_USB(ElementsAre(
        "\r", 
        "Boot Unit Test\r", 
        "Serial Number: 000\r", 
        "Product Type: DC Board\r", 
        "Sub Product Type: 0\r", 
        "MCU Family: Unknown 0x  0 Rev 0\r", 
        "Software Version: 0\r", 
        "Compile Date: 0\r", 
        "Git SHA: 0\r", 
        "PCB Version: 3.1\r"
    ));
}

TEST_F(DCBoard, printStatus) 
{
    DCBoardInit(&hadc, &hwwdg);

    /* Enable 24V power and fill the current buffer with a sensible value */
    stmSetGpio(sense24v, true);
    setDcCurrentBuffer();

    /* Note: usb RX buffer is flushed during the first loop, so a single loop must be done before
    ** printing anything */
    DCBoardLoop(bootMsg);
    writeDcMessage("Status\n");
    

    EXPECT_FLUSH_USB(ElementsAre(
        "\r", 
        "Boot Unit Test\r", 
        "Board Status:\r", 
        "The board is operating normally.\r", 
        "Port 0: On: 0, PWM percent: 0\r", 
        "Port 1: On: 0, PWM percent: 0\r", 
        "Port 2: On: 0, PWM percent: 0\r", 
        "Port 3: On: 0, PWM percent: 0\r", 
        "Port 4: On: 0, PWM percent: 0\r", 
        "Port 5: On: 0, PWM percent: 0\r", 
        "\r", 
        "End of board status. \r"
    ));
    
    writeDcMessage("p2 on\n");
    writeDcMessage("p4 on\n");
    writeDcMessage("p6 on\n");
    writeDcMessage("Status\n");
    
    EXPECT_FLUSH_USB(ElementsAre( 
        "\r", 
        "Board Status:\r", 
        "The board is operating normally.\r", 
        "Port 0: On: 0, PWM percent: 0\r",
        "Port 1: On: 1, PWM percent: 999\r",
        "Port 2: On: 0, PWM percent: 0\r",
        "Port 3: On: 1, PWM percent: 999\r",
        "Port 4: On: 0, PWM percent: 0\r",
        "Port 5: On: 1, PWM percent: 999\r",
        "\r", 
        "End of board status. \r"
    ));
}
