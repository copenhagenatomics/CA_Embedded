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

/* UUT */
#include "ACBoard.c"

using ::testing::AnyOf;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
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

        }

        void EXPECT_USB_CONTAINS(const char * search)
        {
            vector<string>* ss = hostUSBread();
            EXPECT_THAT(*ss, Contains(std::string(search)));
            delete ss;
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/

};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(ACBoard, printStatus) 
{
    GpioInit();

    printAcStatus();
    vector<string>* ss = hostUSBread(true);
    EXPECT_THAT(*ss, ElementsAre(
        "Fan     On: 0\r",
        "Port 0: On: 0, PWM percent: 0\r",
        "Port 1: On: 0, PWM percent: 0\r",
        "Port 2: On: 0, PWM percent: 0\r",
        "Port 3: On: 0, PWM percent: 0\r"
    ));
    delete ss;


    stmSetGpio(fanCtrl, true);
    stmSetGpio(heaterPorts[1].heater, true);
    stmSetGpio(heaterPorts[3].heater, true);
    printAcStatus();
    ss = hostUSBread(true);
    EXPECT_THAT(*ss, ElementsAre(
        "Fan     On: 1\r",
        "Port 0: On: 0, PWM percent: 0\r",
        "Port 1: On: 1, PWM percent: 0\r",
        "Port 2: On: 0, PWM percent: 0\r",
        "Port 3: On: 1, PWM percent: 0\r"
    ));
    delete ss;

    /* It would be nice to be easily able to test PWM too, but this would require mocking/faking 
    ** heatCtrl module */
}

TEST_F(ACBoard, fanInput) 
{
    GpioInit();

    EXPECT_FALSE(isFanForceOn);
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    userInput("fan on");
    EXPECT_TRUE(isFanForceOn);
    EXPECT_TRUE(stmGetGpio(fanCtrl));

    userInput("fan off");
    EXPECT_FALSE(isFanForceOn);
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    userInput("fan wrong");
    EXPECT_FALSE(isFanForceOn);
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    vector<string>* ss = hostUSBread();
    EXPECT_THAT(*ss, Contains(std::string("MISREAD: fan wrong")));
    delete ss;
}

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
    
    GpioInit();

    expectStmNotNull(&fanCtrl);
    for(int i = 0; i < AC_BOARD_NUM_PORTS; i++) expectStmNotNull(&heaterPorts[i].heater);
}
