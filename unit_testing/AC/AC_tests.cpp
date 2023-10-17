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
                EXPECT_NE(heatCtrlAdd(&heaterGpios[i], &heaterButtons[i]), nullptr);
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
        EXPECT_EQ(getPWMPinPercent(i), 0);
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

TEST_F(ACHeaterCtrl, turnOnPinDuration)
{
    const int durations[MAX_NO_HEATERS] = {PWM_PERIOD_MS, 2 * PWM_PERIOD_MS, 3 * PWM_PERIOD_MS, 4 * PWM_PERIOD_MS};

    /* Give each heater a different duration */
    forceTick(0);
    for(int i = 0; i < MAX_NO_HEATERS; i++)
    {
        turnOnPinDuration(i, durations[i]);
    }

    /* Check all the heaters GPIO are only turned on until their duration runs out */
    for(int i = 0; i < durations[3] + 1; i++)
    {
        forceTick(i);
        heaterLoop();

        for(int j = 0; j < MAX_NO_HEATERS; j++)
        {
            EXPECT_EQ(heaterGpios[j].state == PIN_SET, i <= durations[j]) << "Heater " << j << " at time " << i;
        }
    }
}

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

TEST_F(ACHeaterCtrl, adjustPWMDown) 
{
    static const int STAGGER = 5;
    int start_pwm[MAX_NO_HEATERS] = {0};

    /* Setup staggered PWMs on different pins */
    for(int i = 0; i < MAX_NO_HEATERS; i++) 
    {
        start_pwm[i] = STAGGER * (i + 1);
        setPWMPin(i, start_pwm[i], 0);
    }

    /* + 1 added to ensure going below 0 is tested for every channel, including the last one */
    for(int i = 0; i < STAGGER * MAX_NO_HEATERS + 1; i++)
    {
        for(int j = 0; j < MAX_NO_HEATERS; j++)
        {
            if(start_pwm[j] > i)
            {
                EXPECT_EQ(getPWMPinPercent(j), STAGGER * (j + 1) - i) << "Heater " << j;
            }
            else
            {
                EXPECT_EQ(getPWMPinPercent(j), 0) << "Heater " << j;
            }
            
        }

        adjustPWMDown();
    }
}

/* getPWMPinPercent is sort of tested implicitly by other tests */

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
        /* Reset relevant variables to 0 */
        allOff();

        /* Setup PWMs */
        forceTick(PWM_PERIOD_MS);
        for(int j = 0; j < MAX_NO_HEATERS; j++)
        {
            setPWMPin(j, pcts[i][j + 2], -1);
        }

        /* Run for a few periods, checking that the number of GPIOs enabled is never outside the 
        ** min/max range */
        for(int j = PWM_PERIOD_MS; j < 4 * PWM_PERIOD_MS; j++)
        {
            forceTick(j);
            heaterLoop();

            /* Count on GPIOs */
            int onGpios = 0;
            for(int k = 0; k < MAX_NO_HEATERS; k++)
            {
                /* Check nothing weird has happened to the PWM (for debug trace) */
                EXPECT_EQ(getPWMPinPercent(k), pcts[i][k + 2]) << "Heater " << k << ", time " << j << ", pct set " << i;
                onGpios = heaterGpios[k].state == PIN_SET ? onGpios + 1 : onGpios;
            }

            /* Check the number of activated outputs is within bounds */
            EXPECT_GE(onGpios, pcts[i][0]) << "Pct set " << i << ", time " << j;
            EXPECT_LE(onGpios, pcts[i][1]) << "Pct set " << i << ", time " << j;
        }
    }
}
