/*!
** @file   heatCtrl_tests.cpp
** @author Luke W
** @date   12/10/2023
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "fake_stm32xxxx_hal.h"
#include "fake_StmGpio.h"

/* Real supporting units */
#include "faultHandlers.c"

/* UUT */
#include "HeatCtrl.c"

using ::testing::AnyOf;
using ::testing::AllOf;

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
                stmGpioInit(&heaterGpios[i], (GPIO_TypeDef*)0, 0, STM_GPIO_OUTPUT);
                EXPECT_NE(heatCtrlAdd(&heaterGpios[i], &heaterButtons[i]), nullptr);
            }

            /* Always start tests from the same point in time */
            forceTick(0);
        }

        /*!
        ** @brief Turns all the heaters on to max and checks it worked
        */
        void turnOnTest()
        {
            allOn(60000);

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

/*!
** @brief Test that if all ports turn on immediately when "allOn" is sent
*/
TEST_F(ACHeaterCtrl, allOn) 
{
    /* Check everything is off by default */
    for(int i = 0; i < MAX_NO_HEATERS; i++) 
    {
        EXPECT_EQ(getPWMPinPercent(i), 0);
    }

    turnOnTest();
}

/*!
** @brief Test that if all ports turn off immediately when "allOff" is sent
*/
TEST_F(ACHeaterCtrl, allOff) 
{
    turnOnTest();

    allOff();

    for(int i = 0; i < MAX_NO_HEATERS; i++) 
    {
        EXPECT_EQ(getPWMPinPercent(i), 0);
    }
}

/*!
** @brief Test that a port turns off immediately when "turnOffPin" is sent
*/
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

/*!
** @brief Test that a port turns on immediately when "turnOnPin" is sent
*/
TEST_F(ACHeaterCtrl, turnOnPin) 
{
    /* Turn off pins one by one and check they turn off (and nothing else happens) */
    for(int i = 0; i < MAX_NO_HEATERS; i++) 
    {
        /* Note: allOff is tested in a different test, so we can probably rely on the 
        ** outcome */
        allOff(); 
        turnOnPin(i, 1); /* Note: minimum positive number for duration */
        for(int j = 0; j < MAX_NO_HEATERS; j++)
        {
            EXPECT_EQ(getPWMPinPercent(j), j == i ? 100 : 0);
        }
    }
}

/*!
** @brief Test that a port turns off after "duration" when "turnOnPin" is sent with a duration
*/
TEST_F(ACHeaterCtrl, turnOnPinDuration)
{
    const int durations[MAX_NO_HEATERS] = {PWM_PERIOD_MS, 2 * PWM_PERIOD_MS, 3 * PWM_PERIOD_MS, 4 * PWM_PERIOD_MS};

    /* Give each heater a different duration */
    for(int i = 0; i < MAX_NO_HEATERS; i++)
    {
        turnOnPin(i, durations[i]);
    }

    /* Check all the heaters GPIO are only turned on until their duration runs out */
    for(int i = 0; i < durations[3] + 1; i++)
    {
        forceTick(i);
        heaterLoop();

        for(int j = 0; j < MAX_NO_HEATERS; j++)
        {
            ASSERT_EQ(heaterGpios[j].state == PIN_SET, i < durations[j]) << "Heater " << j << " at time " << i;
        }
    }
}

/*!
** @brief Test that a port turns on with a given PWM at the beginning of the next period
*/
TEST_F(ACHeaterCtrl, setPWMPinNextPeriod) 
{
    for(int i = 0; i < MAX_NO_HEATERS; i++) 
    {
        /* Test the function works for a range of PWMs, JIC there is an issue e.g. with 0 or 100 */
        for(int j = 0; j <= 100; j+= 10) 
        {
            /* Start at a new tick boundary */
            uint32_t startTick = (i + 1) * j * 1000;
            forceTick(startTick);    
            heaterLoop();

            setPWMPin(i, j, 2000);

            /* Expect that the PWMPinPercentage doesn't change straightaway */
            for(int k = 1; k <= 1000; k++)
            {
                /* 0 is a special case as the default state is 0 */
                if(j != 0) 
                {
                    ASSERT_NE(getPWMPinPercent(i), j) << "Heater " << i << " at time " << k;
                }

                forceTick(startTick + k);    
                heaterLoop();
            }

            /* Verify that by the next period, only the required heater has changed */
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

/*!
** @brief Test that a port turns on with a given PWM at the beginning of the next period, unless 
**        the duration means it should already have turned off again
*/
TEST_F(ACHeaterCtrl, setPWMPinDuration)
{
    for(int i = 0; i < MAX_NO_HEATERS; i++) 
    {
        forceTick(PWM_PERIOD_MS - 1);    
        heaterLoop();
        setPWMPin(i, 50, 0);

        /* Since duration is 0, PWM pin should change, either now OR on the next period */
        ASSERT_NE(getPWMPinPercent(i), 50) << "Heater " << i;

        forceTick(PWM_PERIOD_MS);    
        heaterLoop();
        ASSERT_NE(getPWMPinPercent(i), 50) << "Heater " << i;

        setPWMPin(i, 50, 2000);
        forceTick(2 * PWM_PERIOD_MS);    
        heaterLoop();
        ASSERT_EQ(getPWMPinPercent(i), 50) << "Heater " << i;

        forceTick(3 * PWM_PERIOD_MS + 1);
        heaterLoop();   
        ASSERT_EQ(getPWMPinPercent(i), 0) << "Heater " << i;
    }
}

/* getPWMPinPercent is sort of tested implicitly by other tests */

/*!
** @brief Test that updateHeaterPhaseControl stacks correctly
*/
TEST_F(ACHeaterCtrl, updateHeaterPhaseControl)
{
    /* Tryout a few random combinations to make sure the alignment is correct. TODO: use RNG */
    uint8_t pcts[5][6] = 
    {
        /* Min GPIOs, Max GPIOs, 4x PWMs */
        {2, 3,  54, 89, 12, 95},
        {1, 2,  05, 78, 12, 65},
        {0, 1,  82, 02, 10, 00},
        {1, 2, 100, 00, 00, 10},
        {0, 1,  12, 23, 16, 07},
    };

    /* For each combination, set the PWMs, then run through a few PWM cycles to make sure that more tha */
    for(int i = 0; i < 5; i++)
    {
        static const int LEN_TEST = 4 * PWM_PERIOD_MS;

        /* Reset relevant variables to 0 */
        forceTick(0);
        heaterLoop();
        allOff();

        /* Setup PWMs */
        for(int j = 0; j < MAX_NO_HEATERS; j++)
        {
            setPWMPin(j, pcts[i][j + 2], LEN_TEST);
        }

        forceTick(PWM_PERIOD_MS);
        heaterLoop();

        /* Run for a few periods, checking that the number of GPIOs enabled is never outside the 
        ** min/max range */
        for(int j = PWM_PERIOD_MS; j < LEN_TEST; j++)
        {
            forceTick(j);
            heaterLoop();

            /* Count on GPIOs */
            int onGpios = 0;
            for(int k = 0; k < MAX_NO_HEATERS; k++)
            {
                /* Check nothing weird has happened to the PWM (for debug trace) */
                ASSERT_EQ(getPWMPinPercent(k), pcts[i][k + 2]) << "Heater " << k << ", time " << j << ", pct set " << i;
                onGpios = heaterGpios[k].state == PIN_SET ? onGpios + 1 : onGpios;
            }

            /* Check the number of activated outputs is within bounds */
            ASSERT_GE(onGpios, pcts[i][0]) << "Pct set " << i << ", time " << j;
            ASSERT_LE(onGpios, pcts[i][1]) << "Pct set " << i << ", time " << j;
        }
    }
}

/*!
** @brief Checks that the on-periods don't get messed up if the host SW re-asserts the same PWM
*/
TEST_F(ACHeaterCtrl, resetPwmMidperiod)
{
    /* Setup PWMs */
    for(int j = 0; j < MAX_NO_HEATERS; j++)
    {
        setPWMPin(j, 10, 2000);
    }

    heaterLoop();
    forceTick(1000);
    heaterLoop();

    /* Run for half a period, to make sure the action is as it is supposed to be */
    for(int j = 0; j < 500; j++)
    {
        forceTick(j);
        heaterLoop();

        if(j < 400)
        {
            ASSERT_THAT(PIN_SET, AnyOf(heaterGpios[0].state, heaterGpios[1].state, 
                                       heaterGpios[2].state, heaterGpios[3].state));
        }
        else
        {
            ASSERT_THAT(PIN_RESET, AllOf(heaterGpios[0].state, heaterGpios[1].state,
                                         heaterGpios[2].state, heaterGpios[3].state));
        }
    }

    /* Reset the same PWM */
    for(int j = 0; j < MAX_NO_HEATERS; j++)
    {
        setPWMPin(j, 10, 2000);
    }

    /* Verify the PWM doesn't restart */
    /* Run for the second half a period, to make sure the pins are not re-enabled */
    for(int j = 500; j < 1000; j++)
    {
        forceTick(j);
        heaterLoop();

        EXPECT_THAT(PIN_RESET, AllOf(heaterGpios[0].state, heaterGpios[1].state,
                                     heaterGpios[2].state, heaterGpios[3].state));
    }
}

/*!
** @brief Checks that turning on an output after setting the PWM doesn't revert to the PWM
*/
TEST_F(ACHeaterCtrl, turnOnRaceCondition) {
    /* Repeat test for all pins */
    for (int i = 0; i < MAX_NO_HEATERS; i++) {
        forceTick(0);
        heaterLoop();
        setPWMPin(i, 10, 2000);

        heaterLoop();
        forceTick(100);
        ASSERT_THAT(heaterGpios[i].state, PIN_RESET) << "Heater " << i;
        turnOnPin(i, 2000);
        heaterLoop();
        ASSERT_THAT(heaterGpios[i].state, PIN_SET) << "Heater " << i;

        /* Run until 1 ms after the PWM would turn off, if it had taken priority */
        for (int j = 101; j < 1101; j++) {
            forceTick(j);
            heaterLoop();

            ASSERT_THAT(heaterGpios[i].state, PIN_SET) << "Heater " << i << " at time " << j;
        }
    }
}

/*!
** @brief Checks that turning off an output after setting the PWM doesn't revert to the PWM
*/
TEST_F(ACHeaterCtrl, turnOffRaceCondition) {
    /* Repeat test for all pins */
    for (int i = 0; i < MAX_NO_HEATERS; i++) {
        forceTick(0);
        heaterLoop();
        setPWMPin(i, 10, 2000);

        heaterLoop();
        forceTick(100);
        ASSERT_THAT(heaterGpios[i].state, PIN_RESET) << "Heater " << i;
        turnOffPin(i);
        heaterLoop();
        ASSERT_THAT(heaterGpios[i].state, PIN_RESET) << "Heater " << i;

        /* Run until 1 ms after the PWM would turn off, if it had taken priority */
        for (int j = 101; j < 1001; j++) {
            forceTick(j);
            heaterLoop();

            ASSERT_THAT(heaterGpios[i].state, PIN_RESET) << "Heater " << i << " at time " << j;
        }
    }
}