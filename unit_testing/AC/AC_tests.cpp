/*!
** @file   AC_tests.cpp
** @author Luke W
** @date   12/10/2023
*/

#include <gtest/gtest.h>

#include "fake_StmGpio.h"
#include "fake_stm32xxxx_hal.h"

/* UUT */
#include "HeatCtrl.c"

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class ACHeaterCtrl: public ::testing::Test 
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        ACHeaterCtrl()
        {
            /* Create a full set of heaters */
            for(int i = 0; i < MAX_NO_HEATERS; i++) 
            {
                stmGpioInit(&heaterGpios[i], STM_GPIO_OUTPUT);
                heatCtrlAdd(&heaterGpios[i], &heaterButtons[i]);
            }
        }

        /*!
        ** @brief Turns all the heaters on to max and checks it worked
        */
        void turnOnTest()
        {
            allOn();

            for(int i = 0; i < MAX_NO_HEATERS; i++) 
            {
                EXPECT_EQ(getPWMPinPercent(i), 100);
            }
        };

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/
        StmGpio   heaterGpios[MAX_NO_HEATERS];
        StmGpio   heaterButtons[MAX_NO_HEATERS];
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(ACHeaterCtrl, allOn) 
{
    /* Check everything is off by default */
    for(int i = 0; i < MAX_NO_HEATERS; i++) 
    {
        EXPECT_EQ(getPWMPinPercent(i), 0);
    }

    turnOnTest();
}

TEST_F(ACHeaterCtrl, allOff) 
{
    turnOnTest();

    allOff();

    for(int i = 0; i < MAX_NO_HEATERS; i++) 
    {
        EXPECT_EQ(getPWMPinPercent(i), 1);
    }
}

TEST_F(ACHeaterCtrl, turnOffPin) 
{
    /* Turn off pins one by one and check they turn off (and nothing else happens) */
    for(int i = 0; i < MAX_NO_HEATERS; i++) 
    {
        turnOnTest();
        turnOffPin(i);
        for(int j = 0; j < MAX_NO_HEATERS; j++)
        {
            EXPECT_EQ(getPWMPinPercent(j), j == i ? 0 : 100);
        }
    }
}

TEST_F(ACHeaterCtrl, turnOnPin) 
{
    /* Turn off pins one by one and check they turn off (and nothing else happens) */
    for(int i = 0; i < MAX_NO_HEATERS; i++) 
    {
        /* Note: allOff is tested in a different test, so we can probably rely on the 
        ** outcome */
        allOff(); 
        turnOnPin(i);
        for(int j = 0; j < MAX_NO_HEATERS; j++)
        {
            EXPECT_EQ(getPWMPinPercent(j), j == i ? 100 : 0);
        }
    }
}

/* Skipping turnOnPinDuration (for now) as the side-effects are difficult to evaluate without 
** instrumentation */

/* Test limited to effect of PWM as duration is difficult to assess without instrumentation */
TEST_F(ACHeaterCtrl, setPWMPin) 
{
    for(int i = 0; i < MAX_NO_HEATERS; i++) 
    {
        /* Test the function works for a range of PWMs, JIC there is an issue e.g. with 0 or 100 */
        for(int j = 0; j <= 100; j+= 10) 
        {
            setPWMPin(i, j, 0);
            for(int k = 0; k < MAX_NO_HEATERS; k++) 
            {
                if(k == i)
                {
                    EXPECT_EQ(getPWMPinPercent(k), j) << "Heater " << k;
                }
                else
                {
                    EXPECT_EQ(getPWMPinPercent(k), 0) << "Heater " << k;
                }
                
            }
            
        }
        
        setPWMPin(i, 0, 0);
    }
}
