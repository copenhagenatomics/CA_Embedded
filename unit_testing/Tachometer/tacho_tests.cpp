/*!
** @file   tacho_tests.cpp
** @author Luke W
** @date   08/05/2024
*/

#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Fakes */
#include "fake_stm32xxxx_hal.h"
#include "fake_USBprint.h"

/* Real supporting units */
#include "CAProtocol.c"
#include "CAProtocolStm.c"

/* UUT */
#include "tachometer.c"

using namespace ::testing;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class TachoUnitTest: public CaBoardUnitTest, public WithParamInterface<int>
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        TachoUnitTest() : CaBoardUnitTest(tachoInputLoop, Tachometer, {LATEST_MAJOR, LATEST_MINOR}) {}

        void simTick() {
            /* Required to print at 10 Hz */
            if((tickCounter != 0) && (tickCounter % 100 == 0)) {
                HAL_TIM_PeriodElapsedCallback(&testPrintTim);
            }

            tachoInputLoop(bootMsg);
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/

        TIM_HandleTypeDef testTachoTim = {
            .Instance = TIM2,
        };
        TIM_HandleTypeDef testPrintTim = {
            .Instance = TIM5,
        };

        SerialStatusTest sst = {
            .boundInit = bind(tachoInputInit, &testTachoTim, &testPrintTim),
            .testFixture = this,
        };
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(TachoUnitTest, CorrectBoardParams) {
    goldenPathTest(sst, "0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x00000000");
}

TEST_F(TachoUnitTest, incorrectBoard) {
    incorrectBoardTest(sst);
}

TEST_F(TachoUnitTest, printStatus) {
    statusPrintoutTest(sst, {});
}

TEST_F(TachoUnitTest, printSerial) {
    serialPrintoutTest(sst, "Tachometer");
}

INSTANTIATE_TEST_SUITE_P(tachoParamTests, TachoUnitTest, Range(0, NUM_CHANNELS));
TEST_P(TachoUnitTest, testTacho) {
    static const int MEGHZ_TO_KHZ = 1000;

    tachoInputInit(&testTachoTim, &testPrintTim);

    /* The pulse train will generate an event (e.g. rising edge) at decreasing frequencies */
    int len_pulse_train = 100;
    uint32_t pulse_train[len_pulse_train] = {0};
    pulse_train[0] = 0;
    for(int i = 1; i < len_pulse_train; i++) {
        pulse_train[i] = pulse_train[i-1] + 100 * i;
    }

    uint32_t gpio_param;
    switch(GetParam()) {
        case 0: gpio_param = GPIO_PIN_5;
                break;
        case 1: gpio_param = GPIO_PIN_4;
                break;
        case 2: gpio_param = GPIO_PIN_3;
                break;
        case 3: gpio_param = GPIO_PIN_2;
                break;
        case 4: gpio_param = GPIO_PIN_1;
                break;
        case 5: gpio_param = GPIO_PIN_0;
                break;
        default:FAIL();
    }

    for(int i = 0; i < len_pulse_train; i++) {
        /* Set the timer to the latest count and generate the event */
        testTachoTim.Instance->CNT = pulse_train[i];
        HAL_GPIO_EXTI_Callback(gpio_param);

        /* Ticks run at 10Hz frequency, but the timer is 1 MHz. Generate timer ticks at appropriate
        ** intervals */
        goToTick(pulse_train[i] / MEGHZ_TO_KHZ);

        tachoInputLoop(bootMsg);
    }

    /* Get data from USB into array */
    vector<string>* vs = hostUSBread(true);
    vector<vector<string>*>* os = new vector<vector<string>*>;
    for(int i = 0; i < 4; i++) {
        string s = vs->at(i+2);
        os->push_back(new vector<string>);
        
        /* Retrieve all values from the string into a 2D vector */
        for(int j = 0; j < NUM_CHANNELS; j++) {
            int idx = s.find(", "); 
            string sub = s.substr(0, idx);
            os->at(i)->push_back(sub);
            s = s.substr(idx + 2);
        }
    }

    /* Use assert to terminate the loop if something is wrong. Prevents the output being flooded 
    ** with wrong messages */
    for(int i = 0; i < NUM_CHANNELS; i++) {
        if(i == GetParam()) {
            EXPECT_EQ(os->at(0)->at(i), "976.66");
            EXPECT_EQ(os->at(1)->at(i), "226.83");
            EXPECT_EQ(os->at(2)->at(i), "147.95");
            ASSERT_EQ(os->at(3)->at(i), "122.12");
        }
        else {
            for( int j = 0; j < 4; j++) {
                ASSERT_EQ(os->at(j)->at(i), "0.00");
            }
        }
    }

    /* Memory leaks from VS/OS cleared by test runner */
} 

TEST_F(TachoUnitTest, tachoRollover) {
    tachoInputInit(&testTachoTim, &testPrintTim);
    /* Note: usb RX buffer is flushed during the first loop, so a single loop must be done before
    ** printing anything */
    tachoInputLoop(bootMsg);


    testTachoTim.Instance->CNT = 0xFFFFFFFE;
    HAL_GPIO_EXTI_Callback(GPIO_PIN_5);
    testTachoTim.Instance->CNT += 100000;
    HAL_GPIO_EXTI_Callback(GPIO_PIN_5);

    goToTick(100);

    EXPECT_READ_USB(Contains("10.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x00000000"));
}

TEST_F(TachoUnitTest, reverse_channels) {
    static const int MEGHZ_TO_KHZ = 1000;

    /* Specifically targeted at V3.1 */
    bi.v2.pcbVersion.major = 3;
    bi.v2.pcbVersion.minor = 1;
    HAL_otpWrite(&bi);

    tachoInputInit(&testTachoTim, &testPrintTim);

    /* The pulse train will generate an event (e.g. rising edge) at decreasing frequencies */
    int len_pulse_train = 100;
    uint32_t pulse_train[len_pulse_train] = {0};
    pulse_train[0] = 0;
    for(int i = 1; i < len_pulse_train; i++) {
        pulse_train[i] = pulse_train[i-1] + 100 * i;
    }

    uint32_t gpio_param = GPIO_PIN_5;
    for(int i = 0; i < len_pulse_train; i++) {
        /* Set the timer to the latest count and generate the event */
        testTachoTim.Instance->CNT = pulse_train[i];
        HAL_GPIO_EXTI_Callback(gpio_param);

        /* Ticks run at 10Hz frequency, but the timer is 1 MHz. Generate timer ticks at appropriate
        ** intervals */
        goToTick(pulse_train[i] / MEGHZ_TO_KHZ);

        tachoInputLoop(bootMsg);
    }

    /* Get data from USB into array */
    EXPECT_READ_USB(ElementsAre(
        "\r",
        "Boot Unit Test\r",
        "0.00, 0.00, 0.00, 0.00, 0.00, 976.66, 0x00000000\r",
        "0.00, 0.00, 0.00, 0.00, 0.00, 226.83, 0x00000000\r",
        "0.00, 0.00, 0.00, 0.00, 0.00, 147.95, 0x00000000\r",
        "0.00, 0.00, 0.00, 0.00, 0.00, 122.12, 0x00000000"
    ));

    /* Memory leaks from VS/OS cleared by test runner */
} 