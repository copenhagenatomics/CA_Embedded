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
#include "HeatCtrl.c"
#include "ADCmonitor.c"
#include "CAProtocol.c"
#include "CAProtocolStm.c"
#include "CAProtocolACDC.c"

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

class ACBoard: public ::testing::Test 
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        ACBoard()
        {
            hadc.Init.NbrOfConversion = 5;

            /* OTP code to allow initialisation of the board to pass */
            /* TODO: Make this use a define for the board release, so that tests fail if the someone
            ** forgets to update the version numbers */
            BoardInfo bi = {
                .v2 = {
                    .otpVersion = OTP_VERSION_2,
                    .boardType  = AC_Board,
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

        void writeAcMessage(const char * msg)
        {
            hostUSBprintf(msg);
            ACBoardLoop(bootMsg);
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
                ACBoardLoop(bootMsg);
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

TEST_F(ACBoard, CorrectBoardParams)
{
    ACBoardInit(&hadc, &hwwdg);

    /* Basic test, was everything OK?  */
    EXPECT_FALSE(bsGetStatus() && BS_VERSION_ERROR_Msk);

    /* This should force a print on the USB bus */
    ACBoardLoop(bootMsg);
    HAL_ADC_ConvHalfCpltCallback(&hadc);
    ACBoardLoop(bootMsg);

    /* Check the printout is correct */
    EXPECT_READ_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 0.00, 0x0"));
}

TEST_F(ACBoard, printStatus) 
{
    ACBoardInit(&hadc, &hwwdg);
    /* Note: usb RX buffer is flushed during the first loop, so a single loop must be done before
    ** printing anything */
    ACBoardLoop(bootMsg);
    writeAcMessage("Status\n");
    

    EXPECT_FLUSH_USB(ElementsAre(
        "\r", 
        "Boot Unit Test\r", 
        "Start of board status:\r", 
        "The board is operating normally.\r", 
        "Fan     On: 0\r", 
        "Port 0: On: 0, PWM percent: 0\r", 
        "Port 1: On: 0, PWM percent: 0\r", 
        "Port 2: On: 0, PWM percent: 0\r", 
        "Port 3: On: 0, PWM percent: 0\r", 
        "\r", 
        "End of board status. \r"
    ));
    
    writeAcMessage("fan on\n");
    writeAcMessage("p2 on 1\n");
    writeAcMessage("p4 on 1\n");
    writeAcMessage("Status\n");
    
    EXPECT_FLUSH_USB(ElementsAre( 
        "\r", 
        "Start of board status:\r", 
        "The board is operating normally.\r", 
        "Fan     On: 1\r",
        "Port 0: On: 0, PWM percent: 0\r",
        "Port 1: On: 1, PWM percent: 100\r",
        "Port 2: On: 0, PWM percent: 0\r",
        "Port 3: On: 1, PWM percent: 100\r",
        "\r", 
        "End of board status. \r"
    ));

    /* It would be nice to be easily able to test PWM too, but this would require mocking/faking 
    ** heatCtrl module */
}

TEST_F(ACBoard, printSerial) 
{
    ACBoardInit(&hadc, &hwwdg);
    /* Note: usb RX buffer is flushed during the first loop, so a single loop must be done before
    ** printing anything */
    ACBoardLoop(bootMsg);
    writeAcMessage("Serial\n");
    
    EXPECT_FLUSH_USB(ElementsAre(
        "\r", 
        "Boot Unit Test\r", 
        "Serial Number: 000\r", 
        "Product Type: AC Board\r", 
        "Sub Product Type: 0\r", 
        "MCU Family: Unknown 0x  0 Rev 0\r", 
        "Software Version: 0\r", 
        "Compile Date: 0\r", 
        "Git SHA: 0\r", 
        "PCB Version: 6.4\r"
    ));
}

/* Note: this test uses some knowledge of internals of ACBoard.c (e.g. white box), which isn't 
** perfect */
TEST_F(ACBoard, fanInput) 
{
    ACBoardInit(&hadc, &hwwdg);
    ACBoardLoop(bootMsg);

    EXPECT_FALSE(isFanForceOn);
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    writeAcMessage("fan on\n");
    EXPECT_TRUE(isFanForceOn);
    EXPECT_TRUE(stmGetGpio(fanCtrl));

    writeAcMessage("fan off\n");
    EXPECT_FALSE(isFanForceOn);
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    writeAcMessage("fan wrong\n");
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

    ACBoardInit(&hadc, &hwwdg);

    expectStmNotNull(&fanCtrl);
    for(int i = 0; i < AC_BOARD_NUM_PORTS; i++) expectStmNotNull(&heaterPorts[i].heater);

    /* The GPIO should turn on immediately if a normal message is sent */
    EXPECT_FALSE(stmGetGpio(heaterPorts[0].heater));

    ACBoardLoop(bootMsg);
    writeAcMessage("p1 on 1\n");
    EXPECT_TRUE(stmGetGpio(heaterPorts[0].heater));

    /* The GPIO should turn on at the next PWM cycle if a % message is sent */
    EXPECT_FALSE(stmGetGpio(heaterPorts[1].heater));
    writeAcMessage("p2 on 2 50%\n");
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
    writeAcMessage("p3 on 3 25%\n");
    EXPECT_FALSE(stmGetGpio(heaterPorts[2].heater));
    goToTick(2999);

    EXPECT_FALSE(stmGetGpio(heaterPorts[2].heater));
    goToTick(3000);
    EXPECT_TRUE(stmGetGpio(heaterPorts[2].heater));
}

/* Stacked switching basically checked in heatCtrl tests */

TEST_F(ACBoard, InvalidCommands)
{
    ACBoardInit(&hadc, &hwwdg);
    ACBoardLoop(bootMsg);
    hostUSBread(true); /* Flush USB buffer */

    /* OK messages */
    writeAcMessage("p1 on 1\n");
    EXPECT_READ_USB(IsEmpty());
    writeAcMessage("p2 on 1\n");
    EXPECT_READ_USB(IsEmpty());
    writeAcMessage("p3 on 1\n");
    EXPECT_READ_USB(IsEmpty());
    writeAcMessage("p4 on 1\n");
    EXPECT_READ_USB(IsEmpty());

    /* Fail messages */
    writeAcMessage("p1 on\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: p1 on -1"));
    writeAcMessage("p2 on -1\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: p2 on -1"));
    writeAcMessage("p3 on 10%\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: p3 on -1"));
    writeAcMessage("p4 on 80%\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: p4 on -1"));
    writeAcMessage("all on\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: all on"));
}

TEST_F(ACBoard, UsbTimeout)
{
    static const int TEST_LENGTH_MS = 10000;
    static const int TIMEOUT_LENGTH_MS = 5000;

    ACBoardInit(&hadc, &hwwdg);
    ACBoardLoop(bootMsg);
    writeAcMessage("all on 60\n");

    bool toggle = false;
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
    ACBoardInit(&hadc, &hwwdg);
    ACBoardLoop(bootMsg);

    EXPECT_FALSE(stmGetGpio(fanCtrl));

    /* Fill the temperature buffer with ~54 degC - fan should stay off */
    for(int i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + 5*i) = 680;
    goToTick(100);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 54.81, 0x0"));
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    /* Fill the temperature buffer with ~56 degC - fan should turn on */
    for(int i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + 5*i) = 700;
    goToTick(200);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 56.42, 0x0"));
    EXPECT_TRUE(stmGetGpio(fanCtrl));

    /* Fill the temperature buffer with ~51 degC - fan should remain on */
    /* Note: Status changes to 0x1 because fan pin is enabled. The previous message didn't contain 
    ** it because the printout and heatSink update are in the same function, and the heatsinkLoop 
    ** is outside the function */
    for(int i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + 5*i) = 630;
    goToTick(300);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 50.78, 0x1"));
    EXPECT_TRUE(stmGetGpio(fanCtrl));

    /* Fill the temperature buffer with ~49 degC - fan should turn off */
    for(int i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + 5*i) = 608;
    goToTick(400);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 49.00, 0x1"));
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    /* Fill the temperature buffer with ~71 degC - fan should turn on and PWM of heaters should be reduced */
    writeAcMessage("p1 on 10\n");
    for(int i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + 5*i) = 880;
    goToTick(600);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 70.93, 0xc0000003"));
    EXPECT_TRUE(stmGetGpio(fanCtrl));
    
    /* p1 was turned on 100 % for 10 secs at t=400. The temperature rise would not be detected until 
    ** t = 500. Since the simulated temperature doesn't change, the PWM will be adjusted down as 
    ** follows:
    ** * t =  2000, pwm = 99, duration = 10101 ms 
    ** * t =  4000, pwm = 98, duration = 10204 ms
    ** * t =  6000, pwm = 97, duration = 10309 ms
    ** * t =  8000, pwm = 96, duration = 10416 ms
    ** * t = 10000, pwm = 95, duration = 10525 ms
    **
    ** So starting from the original time of 400, we expect p1 to turn off at 10925 ms. Note: that 
    ** during the interval, the heater will turn off a few times temporarily as it starts to be 
    ** PWM'd */
    goToTick(10924);
    EXPECT_TRUE(stmGetGpio(heaterPorts[0].heater));

    goToTick(10925);
    EXPECT_FALSE(stmGetGpio(heaterPorts[0].heater));
}