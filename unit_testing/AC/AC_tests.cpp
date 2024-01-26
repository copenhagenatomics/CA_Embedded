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

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/

};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

/*!
** @brief 
*/
TEST_F(ACBoard, userInputs) 
{
    GpioInit();
    
    EXPECT_FALSE(isFanForceOn);

    userInput("fan on");
    EXPECT_TRUE(isFanForceOn);

    userInput("fan off");
    EXPECT_FALSE(isFanForceOn);

    userInput("fan wrong");
    EXPECT_FALSE(isFanForceOn);

    /* TODO: read from file */
}
