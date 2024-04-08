/*!
** @file   LightController_tests.cpp
** @author Matias
** @date   08/04/2024
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

/* Fakes */
#include "fake_StmGpio.h"
#include "fake_stm32xxxx_hal.h"
#include "fake_USBprint.h"


/* Real supporting units */
#include "CAProtocol.c"
#include "CAProtocolStm.c"

/* UUT */
#include "LightController.c"

using ::testing::Contains;
using ::testing::ElementsAre;
using namespace std;

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

/* Allow a range of single line container tests */
#define EXPECT_READ_USB(x) { \
    vector<string>* ss = hostUSBread(); \
    EXPECT_THAT(*ss, (x)); \
    delete ss; \
}

/* Allow a range of single line container tests */
#define EXPECT_FLUSH_USB(x) { \
    vector<string>* ss = hostUSBread(true); \
    EXPECT_THAT(*ss, (x)); \
    delete ss; \
}


/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class LightControllerTest: public ::testing::Test 
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        LightControllerTest()
        {
            /* OTP code to allow initialisation of the board to pass */
            /* TODO: Make this use a define for the board release, so that tests fail if the someone
            ** forgets to update the version numbers */
            BoardInfo bi = {
                .v2 = {
                    .otpVersion = OTP_VERSION_2,
                    .boardType  = LightController,
                    .subBoardType = 0,
                    .reserved = {0},
                    .pcbVersion = {
                        .major = 1,
                        .minor = 1
                    },
                    .productionDate = 0
                }
            };
            HAL_otpWrite(&bi);
            forceTick(0);
            hostUSBConnect();
        }

        void writeLightControllerMessage(const char * msg)
        {
            hostUSBprintf(msg);
            LightControllerLoop(bootMsg);
        }

        void simTick(int numTicks = 1)
        {
            for(int i = tickCounter + 1; i <= tickCounter + numTicks; i++) 
            {
                HAL_TIM_PeriodElapsedCallback(&htim2);
            }
            tickCounter = tickCounter + numTicks;
        }

        void goToTick(int tickDest) {
            while(tickCounter < tickDest) {
                simTick();
            }

            // Reset counter
            if (tickDest == 0){
                while (tickCounter < TICK_COUNTER_OVERFLOW){
                    simTick();
                }
                tickCounter = 0;
            }
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/
        TIM_HandleTypeDef htim2;
        TIM_HandleTypeDef htim5;
        WWDG_HandleTypeDef hwwdg;
        const char * bootMsg = "Boot Unit Test";
        const int TICK_COUNTER_OVERFLOW = 256;
        int tickCounter = 0;
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(LightControllerTest, CorrectBoardParams)
{
    LightControllerInit(&htim2, &htim5, &hwwdg);

    /* Basic test, was everything OK?  */
    EXPECT_FALSE(bsGetField(BS_VERSION_ERROR_Msk));

    /* This should force a print on the USB bus */
    HAL_TIM_PeriodElapsedCallback(&htim5);

    /* Check the printout is correct */
    EXPECT_READ_USB(Contains("0, 0, 0, 0x0"));
}

TEST_F(LightControllerTest, WrongBoardParams)
{
    // Set wrong minor version
    BoardInfo bi = {
        .v2 = {
            .otpVersion = OTP_VERSION_2,
            .boardType  = LightController,
            .subBoardType = 0,
            .reserved = {0},
            .pcbVersion = {
                .major = 1,
                .minor = 0
            },
            .productionDate = 0
        }
    };
    HAL_otpWrite(&bi);

    LightControllerInit(&htim2, &htim5, &hwwdg);

    /* Minor version does not add up. Produce error.  */
    EXPECT_TRUE(bsGetField(BS_VERSION_ERROR_Msk));

    /* This should force a print on the USB bus */
    HAL_TIM_PeriodElapsedCallback(&htim5);

    /* Check that it prints the status error code */
    EXPECT_FLUSH_USB(Contains("0x84000000"));
    

    /* Reset to correct params */
    bsClearField(BS_VERSION_ERROR_Msk);
    bsClearError(BS_VERSION_ERROR_Msk);
    bi.v2.pcbVersion.minor=1;
    HAL_otpWrite(&bi);
    LightControllerInit(&htim2, &htim5, &hwwdg);
    EXPECT_FALSE(bsGetField(BS_VERSION_ERROR_Msk));
    HAL_TIM_PeriodElapsedCallback(&htim5);
    EXPECT_FLUSH_USB(ElementsAre("\r", "0, 0, 0, 0x0"));

    /* Run with wrong board type */
    bi.v2.boardType = AC_Board;
    HAL_otpWrite(&bi);
    LightControllerInit(&htim2, &htim5, &hwwdg);
    EXPECT_TRUE(bsGetField(BS_VERSION_ERROR_Msk));
    HAL_TIM_PeriodElapsedCallback(&htim5);
    EXPECT_FLUSH_USB(ElementsAre("\r", "0x84000000"));
}


TEST_F(LightControllerTest, printSerial) 
{
    LightControllerInit(&htim2, &htim5, &hwwdg);
    /* Note: usb RX buffer is flushed during the first loop, so a single loop must be done before
    ** printing anything */
    LightControllerLoop(bootMsg);
    writeLightControllerMessage("Serial\n");
    
    EXPECT_FLUSH_USB(ElementsAre(
        "\r", 
        "Boot Unit Test\r", 
        "Serial Number: 000\r", 
        "Product Type: LightController\r", 
        "Sub Product Type: 0\r", 
        "MCU Family: Unknown 0x  0 Rev 0\r", 
        "Software Version: (null)\r", 
        "Compile Date: (null)\r", 
        "Git SHA: (null)\r", 
        "PCB Version: 1.1\r"
    ));
}

TEST_F(LightControllerTest, printStatus) 
{
    LightControllerInit(&htim2, &htim5, &hwwdg);
    /* Note: usb RX buffer is flushed during the first loop, so a single loop must be done before
    ** printing anything */
    LightControllerLoop(bootMsg);
    writeLightControllerMessage("Status\n");
    
    EXPECT_FLUSH_USB(ElementsAre(
        "\r",
        "Boot Unit Test\r",
        "Board Status:\r",
        "The board is operating normally.\r",  
        "Port 1: On: 0\r",
        "Port 2: On: 0\r",
        "Port 3: On: 0\r",
        "\r",
        "End of board status. \r"
    ));
}


/* Note: this test uses some knowledge of internals of LightController.c (e.g. white box), which isn't 
** perfect */
TEST_F(LightControllerTest, GpioInit) 
{
    auto expectStmNull = [](StmGpio* stm) {
        EXPECT_EQ(stm->set,    nullptr);
        EXPECT_EQ(stm->get,    nullptr);
        EXPECT_EQ(stm->toggle, nullptr);
    };

    auto expectStmNotNull = [](StmGpio* stm) {
        ASSERT_NE(stm->set,    nullptr);
        ASSERT_NE(stm->get,    nullptr);
        ASSERT_NE(stm->toggle, nullptr);
    };

    /* The GPIOs should be null prior to init function */
    for(int i = 0; i < LED_CHANNELS*NO_COLORS; i++) expectStmNull(ChCtrl[i]);

    LightControllerInit(&htim2, &htim5, &hwwdg);

    /* The GPIOs should be instatiated after init function */
    for(int i = 0; i < LED_CHANNELS*NO_COLORS; i++) expectStmNotNull(ChCtrl[i]);

    /* The GPIOs should be off after init */
    for(int i = 0; i < LED_CHANNELS*NO_COLORS; i++) EXPECT_FALSE(stmGetGpio(*ChCtrl[i]));
}

TEST_F(LightControllerTest, InputHandler) 
{
    LightControllerInit(&htim2, &htim5, &hwwdg);
    /* Note: usb RX buffer is flushed during the first loop, so a single loop must be done before
    ** printing anything */
    LightControllerLoop(bootMsg);
    HAL_TIM_PeriodElapsedCallback(&htim5);

    // Initial flush
    EXPECT_FLUSH_USB(ElementsAre(
        "\r",
        "Boot Unit Test\r",
        "0, 0, 0, 0x0"
    ));


    /* ----- VALID INPUTS ----- */
    writeLightControllerMessage("p1 ABAB10\n");
    HAL_TIM_PeriodElapsedCallback(&htim5);
    EXPECT_FLUSH_USB(ElementsAre(
        "\r",
        "abab10, 0, 0, 0x1"
    ));

    writeLightControllerMessage("p2 105020\n");
    HAL_TIM_PeriodElapsedCallback(&htim5);
    EXPECT_FLUSH_USB(ElementsAre(
        "\r",
        "abab10, 105020, 0, 0x3"
    ));

    writeLightControllerMessage("p3 FFFFFF\n");
    HAL_TIM_PeriodElapsedCallback(&htim5);
    EXPECT_FLUSH_USB(ElementsAre(
        "\r",
        "abab10, 105020, ffffff, 0x7"
    ));

    // Reset all ports
    writeLightControllerMessage("p1 000000\n");
    writeLightControllerMessage("p2 000000\n");
    writeLightControllerMessage("p3 000000\n");
    HAL_TIM_PeriodElapsedCallback(&htim5);
    EXPECT_FLUSH_USB(ElementsAre(
        "\r",
        "0, 0, 0, 0x0"
    ));

    /* ----- INVALID INPUTS ----- */

    /* For the misreads we do not need to call the printtimer callback
       since the MISREAD messages are pushed to the USB by the standard
       protocol.*/

    // Invalid port   
    writeLightControllerMessage("p4 FFFFFF\n");
    EXPECT_FLUSH_USB(ElementsAre(
        "\r",
        "MISREAD: p4 FFFFFF"
    ));

    writeLightControllerMessage("p0 FFFFFF\n");
    EXPECT_FLUSH_USB(ElementsAre(
        "\r",
        "MISREAD: p0 FFFFFF"
    ));

    // Wrong hex code (too short)
    writeLightControllerMessage("p1 13908\n");
    EXPECT_FLUSH_USB(ElementsAre(
        "\r",
        "MISREAD: p1 13908"
    ));

    // Wrong hex code (too long)
    writeLightControllerMessage("p1 13908CC\n");
    EXPECT_FLUSH_USB(ElementsAre(
        "\r",
        "MISREAD: p1 13908CC"
    ));

    // Wrong hex code (non valid hex characters)
    writeLightControllerMessage("p1 ACACFM\n");
    EXPECT_FLUSH_USB(ElementsAre(
        "\r",
        "MISREAD: p1 ACACFM"
    ));
}

TEST_F(LightControllerTest, LEDSwitching)
{
    LightControllerInit(&htim2, &htim5, &hwwdg);
    LightControllerLoop(bootMsg);

    /* --- Turn on p1 minimum amount of time for each RGB component --- */
    writeLightControllerMessage("p1 010101\n");
    LightControllerLoop(bootMsg);

    // LED switching interrupt 
    goToTick(1);

    // First three GPIOs should be on and the rest should be off
    for(int i = 0; i < 3; i++) EXPECT_TRUE(stmGetGpio(*ChCtrl[i]));
    for(int i = 3; i < LED_CHANNELS*NO_COLORS; i++) EXPECT_FALSE(stmGetGpio(*ChCtrl[i]));

    goToTick(2);
    for(int i = 0; i < LED_CHANNELS*NO_COLORS; i++) EXPECT_FALSE(stmGetGpio(*ChCtrl[i]));

    // Reset counter
    goToTick(0);

    // Check that it keeps switching correctly after overflow
    goToTick(1);

    // First three GPIOs should be on and the rest should be off
    for(int i = 0; i < 3; i++) EXPECT_TRUE(stmGetGpio(*ChCtrl[i]));
    for(int i = 3; i < LED_CHANNELS*NO_COLORS; i++) EXPECT_FALSE(stmGetGpio(*ChCtrl[i]));


    /* --- Turn on p1 white LED all time --- */
    goToTick(0);
    writeLightControllerMessage("p1 FFFFFF\n");
    LightControllerLoop(bootMsg);

    // LED switching interrupt 
    goToTick(100);
    for(int i = 0; i < LED_CHANNELS*NO_COLORS; i++){
        if (i == 3){
            EXPECT_TRUE(stmGetGpio(*ChCtrl[i]));
            continue;
        }
        EXPECT_FALSE(stmGetGpio(*ChCtrl[i]));
    } 


    /* --- Turn on p2 RGB --- */
    goToTick(0);
    writeLightControllerMessage("p1 000000\n");

    // 0xAA = 170
    writeLightControllerMessage("p2 AAAAAA\n");
    LightControllerLoop(bootMsg);
    goToTick(170);
    for(int i = 0; i < LED_CHANNELS*NO_COLORS; i++){
        if (i >= 4 && i < 7){
            EXPECT_TRUE(stmGetGpio(*ChCtrl[i]));
            continue;
        }
        EXPECT_FALSE(stmGetGpio(*ChCtrl[i]));
    } 

    goToTick(171);
    for(int i = 0; i < LED_CHANNELS*NO_COLORS; i++) EXPECT_FALSE(stmGetGpio(*ChCtrl[i]));


    /* --- Turn on p3 white while p2 is running --- */
    goToTick(0);
    writeLightControllerMessage("p3 FFFFFF\n");
    LightControllerLoop(bootMsg);

    goToTick(170);
    for(int i = 0; i < LED_CHANNELS*NO_COLORS; i++){
        if ((i >= 4 && i < 7) || i == 11){
            EXPECT_TRUE(stmGetGpio(*ChCtrl[i]));
            continue;
        }
        EXPECT_FALSE(stmGetGpio(*ChCtrl[i]));
    } 

    // p2 should turn off, but p3 white should still be on
    goToTick(171);
    for(int i = 0; i < LED_CHANNELS*NO_COLORS - 1; i++) EXPECT_FALSE(stmGetGpio(*ChCtrl[i]));
    EXPECT_TRUE(stmGetGpio(*ChCtrl[11]));
} 