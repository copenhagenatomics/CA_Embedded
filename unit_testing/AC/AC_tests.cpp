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

/* getPWMPinPercent not tested due to instrumentation issues. It is sort of tested implicitly by 
** other tests */

TEST_F(ACHeaterCtrl, updateHeaterPhaseControl)
{
    static const int PWM_PERIOD = heaters[MAX_NO_HEATERS-1].pwmPeriod;
    static const int TICK_TIME  = MAX_NO_HEATERS * PWM_PERIOD;

    forceTick(TICK_TIME);
    turnOnTest();

    /* Note: some dubious practice here. HeatCtrl is supposed to be private, but by including the .c
    ** file (instead of .h of UUT), we've got around that. */

    for(int i = 0; i < MAX_NO_HEATERS; i++)
    {
        EXPECT_LE(heaters[i].periodBegin, TICK_TIME);

        /* Since all heaters are on 100% of the time, their periodBegins should be aligned in time*/
        EXPECT_EQ(heaters[i].periodBegin % PWM_PERIOD, TICK_TIME % PWM_PERIOD);
    }

    allOff();

    /* Tryout a few random combinations to make sure the alignment is correct. TODO: use RNG */
    uint8_t pcts[5][4] = 
    {
        { 54, 89, 12, 03},
        { 05, 78, 12, 65},
        { 82, 02, 10, 00},
        {100, 00, 00, 10},
        { 12, 23, 16, 07},
    };

    /* For each combination, set the PWMs for each pin one-by-one. Check the on-periods are always
    ** aligned back-to-back, and that the number of overlapping "stacks" from the beginning of the 
    ** first PWM period is the minimum achievable for a particular set of PWMs */
    for(int i = 0; i < 5; i++)
    {
        /* Reset relevant variables to 0 */
        allOff();
        int pct_count = 0;

        /* Set heater PWM one-by-one */
        for(int j = 0; j < MAX_NO_HEATERS; j++)
        {
            forceTick(TICK_TIME);
            setPWMPin(j, pcts[i][j], 0);
            pct_count += pcts[i][j];

            int stack_count = 0;
            uint32_t pb = heaters[0].periodBegin;
            for(int k = 0; k < MAX_NO_HEATERS; k++)
            {
                uint32_t pct_to_period = heaters[k].pwmPercent * PWM_PERIOD / 100;

                /* Everything should be set back in time */
                EXPECT_LE(heaters[k].periodBegin, TICK_TIME);

                /* Check each on-period aligns with the next heater */
                if(k < MAX_NO_HEATERS - 1)
                {
                    EXPECT_EQ(heaters[k].periodBegin + pct_to_period, heaters[k+1].periodBegin);
                }

                /* If the on-period overlaps a max-period boundary, increment the stack_cout */
                uint32_t normal_pb = heaters[k].periodBegin - pb;
                if(normal_pb / PWM_PERIOD != (normal_pb + pct_to_period) / PWM_PERIOD)
                {
                    stack_count++;
                }
            }

            /* Verify the stack count matches the percent count stack (this should be the minimum 
            ** number of stacks to achieve the desired PWM settings ) */
            EXPECT_EQ(stack_count, pct_count / 100) << stack_count << ", " << pct_count;
        }
    }
}