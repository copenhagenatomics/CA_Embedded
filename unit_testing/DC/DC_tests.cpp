/*!
** @file   DC_tests.cpp
** @author Luke W
** @date   12/10/2023
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Fakes */
#include "fake_StmGpio.h"
#include "fake_stm32xxxx_hal.h"
#include "fake_USBprint.h"

/* Real supporting units */
#include "ADCmonitor.c"
#include "CAProtocol.c"
#include "CAProtocolStm.c"
#include "CAProtocolACDC.c"

/* UUT */
#include "DCBoard.c"

using ::testing::Contains;
using ::testing::ElementsAre;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class DCBoard: public CaBoardUnitTest
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        DCBoard() : CaBoardUnitTest(DCBoardLoop, DC_Board, {LATEST_MAJOR, LATEST_MINOR}) {
            hadc.Init.NbrOfConversion = ADC_CHANNELS;
        }

        void writeDcMessage(const char * msg)
        {
            hostUSBprintf(msg);
            DCBoardLoop(bootMsg);
        }

        void simTick()
        {
            if(tickCounter != 0 && (tickCounter % 100 == 0)) {
                if(tickCounter % 200 == 0) {
                        HAL_ADC_ConvCpltCallback(&hadc);
                    }
                    else {
                        HAL_ADC_ConvHalfCpltCallback(&hadc);
                    }
                }
                DCBoardLoop(bootMsg);
        }

        void setADCChannelBuffer(int channel, int value) {
            /* Fill channel with ADC value */
            for(uint32_t i = 0; i < hadc.dma_length/ADC_CHANNELS; i++) {
                *((int16_t*)hadc.dma_address + (ADC_CHANNELS*i + channel)) = value;
            }
        }

        /* Normal board setup */
        void dcSetup() {
            DCBoardInit(&hadc, &hwwdg);
        
            /* Fill the current buffer with a sensible value
            ** According to default calibration, 2048 yields ~0 current */
            for (int i = 0; i < ADC_CHANNELS-1; i++) {
                setADCChannelBuffer(i, 2048);
            }

            /* Fill the input voltage buffer with value resembling 24V input */
            setADCChannelBuffer(INPUT_V_CHANNEL_IDX, 800);

            /* All buttons automatically in "off" position */
            for(int i = 0; i < ACTUATIONPORTS; i++) {
                stmSetGpio(buttonGpio[i], true);
            }
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/
        
        ADC_HandleTypeDef hadc;
        WWDG_HandleTypeDef hwwdg;
        const char * bootMsg = "Boot Unit Test";

        SerialStatusTest sst = {
            .boundInit = bind(DCBoardInit, &hadc, &hwwdg),
            .testFixture = this
        };
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(DCBoard, CorrectBoardParams)
{
    dcSetup();

    goldenPathTest(sst, "0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x00000000");
}

TEST_F(DCBoard, incorrectBoard) {
    incorrectBoardTest(sst);
}

TEST_F(DCBoard, printSerial) 
{
    serialPrintoutTest(sst, "DC Board");
}

TEST_F(DCBoard, printStatus) 
{
    statusPrintoutTest(sst, {"Port 0: On: 0, PWM percent: 0\r", 
        "Port 1: On: 0, PWM percent: 0\r", 
        "Port 2: On: 0, PWM percent: 0\r", 
        "Port 3: On: 0, PWM percent: 0\r", 
        "Port 4: On: 0, PWM percent: 0\r", 
                             "Port 5: On: 0, PWM percent: 0\r"});
    
    writeDcMessage("p2 on\n");
    writeDcMessage("p4 on\n");
    writeDcMessage("p6 on\n");
    writeDcMessage("Status\n");
    
    EXPECT_FLUSH_USB(ElementsAre( 
        "\r", 
        "Start of board status:\r", 
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

/* Grey box - has knowledge of adc to voltage function */
TEST_F(DCBoard, status24v) 
{
    dcSetup();

    for (int i = 0; i < 4096; i++)
    {
        /* Test input voltage buffer with ADC values from under to over voltage  */
        setADCChannelBuffer(INPUT_V_CHANNEL_IDX, i);
        double inputVoltage = adcToInputVoltage(i); 
        /* Get the next printout */
        simTicks(200);

        if (inputVoltage < UNDER_VOLTAGE_THRESHOLD)
        {
            EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0xa0000000"));
        }
        else if (inputVoltage > OVER_VOLTAGE_THRESHOLD)
        {
            EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x90000000"));
        }
        else
        {
            EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x00000000"));
        }

    }
    // /* Fill buffer with ADC value resembling under voltage */
    // setADCChannelBuffer(INPUT_V_CHANNEL_IDX, UNDER_VOLTAGE_ADC-1);

    // /* Note: usb RX buffer is flushed during the first loop, so a single loop must be done before
    // ** printing anything */
    // goToTick(200);
    // EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0xa0000000"));

    // writeDcMessage("Status\n");

    // EXPECT_FLUSH_USB(ElementsAre(
    //     "\r", 
    //     "Start of board status:\r", 
    //     "Under voltage. The board operates at too low voltage of 0.00V. Check power supply.\r",
    //     "Port 0: On: 0, PWM percent: 0\r", 
    //     "Port 1: On: 0, PWM percent: 0\r", 
    //     "Port 2: On: 0, PWM percent: 0\r", 
    //     "Port 3: On: 0, PWM percent: 0\r", 
    //     "Port 4: On: 0, PWM percent: 0\r", 
    //     "Port 5: On: 0, PWM percent: 0\r", 
    //     "\r", 
    //     "End of board status. \r"
    // ));

    // /* Fill buffer with ADC value resembling over voltage */
    // setADCChannelBuffer(INPUT_V_CHANNEL_IDX, OVER_VOLTAGE_ADC+1);
    // goToTick(400);
    // EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0xb0000000"));
}

/* Grey box - uses getTimerCCR() */
TEST_F(DCBoard, portsNoTimeout) 
{
    dcSetup();

    goToTick(1);
    
    char cmd[100] = {0};
    for(int i = 0; i <= ACTUATIONPORTS + 1; i++) {
        sprintf(cmd, "p%u on\n", i);
        writeDcMessage(cmd);

        if(i == 0 || i == (ACTUATIONPORTS + 1)) {
            for(int j = 0; j < ACTUATIONPORTS; j++) {
                ASSERT_EQ(*getTimerCCR(j), 0) << "j = " << j;
            }
            sprintf(cmd, "MISREAD: Invalid Pin: %d", i);
            EXPECT_FLUSH_USB(Contains(cmd));
        }
        else {
            for(int j = 1; j <= ACTUATIONPORTS; j++) {
                if(j != i) {
                    ASSERT_EQ(*getTimerCCR(j-1), 0) << "j = " << j;
                }
                else {
                    ASSERT_EQ(*getTimerCCR(j-1), 999) << "j = " << j;
                }
            }
        }

        writeDcMessage("all off\n");
    }
}

/* Grey box - uses getTimerCCR() */
TEST_F(DCBoard, portsPct) 
{
    dcSetup();
    goToTick(1);
    
    char cmd[100] = {0};
    for(int i = 0; i <= ACTUATIONPORTS + 1; i++) {
        int pct = (i+1) * 10;
        int pct_ccr = (pct * 999) / 100;
        sprintf(cmd, "p%u on %u%%\n", i, pct);
        writeDcMessage(cmd);

        if(i == 0 || i == (ACTUATIONPORTS + 1)) {
            for(int j = 0; j < ACTUATIONPORTS; j++) {
                ASSERT_EQ(*getTimerCCR(j), 0) << "j = " << j;
            }
            sprintf(cmd, "MISREAD: Invalid Pin: %d", i);
            EXPECT_FLUSH_USB(Contains(cmd));
        }
        else {
            for(int j = 1; j <= ACTUATIONPORTS; j++) {
                if(j != i) {
                    ASSERT_EQ(*getTimerCCR(j-1), 0) << "j = " << j;
                }
                else {
                    ASSERT_EQ(*getTimerCCR(j-1), pct_ccr) << "j = " << j;
                }
            }
        }

        writeDcMessage("all off\n");
    }
}

/* Grey box - uses getTimerCCR() */
TEST_F(DCBoard, portsTimeout) 
{
    dcSetup();
    goToTick(1);
    
    char cmd[100] = {0};
    for(int i = 0; i <= ACTUATIONPORTS + 1; i++) {
        int timeout_secs = i+1;
        int timeout_ticks = timeout_secs * 1000;
        sprintf(cmd, "p%u on %u\n", i, timeout_secs);
        writeDcMessage(cmd);

        if(i == 0 || i == (ACTUATIONPORTS + 1)) {
            for(int j = 0; j < ACTUATIONPORTS; j++) {
                ASSERT_EQ(*getTimerCCR(j), 0) << "j = " << j << ", tick = " << tickCounter;
            }
            sprintf(cmd, "MISREAD: Invalid Pin: %d", i);
            EXPECT_FLUSH_USB(Contains(cmd));
        }
        else {
            simTicks(timeout_ticks);
            for(int j = 1; j <= ACTUATIONPORTS; j++) {
                if(j != i) {
                    ASSERT_EQ(*getTimerCCR(j-1), 0) << "j = " << j << ", tick = " << tickCounter;
                }
                else {
                    ASSERT_EQ(*getTimerCCR(j-1), 999) << "j = " << j << ", tick = " << tickCounter;
                    simTicks();
                    ASSERT_EQ(*getTimerCCR(j-1), 0) << "j = " << j << ", tick = " << tickCounter;
                }
            }
        }

        writeDcMessage("all off\n");
    }
}

/* Grey box - uses getTimerCCR() */
TEST_F(DCBoard, onboardButtons) 
{
    dcSetup();
    goToTick(1);

    for(int i = 0; i < ACTUATIONPORTS; i++) {
        /* All ports initially off */
        for(int j = 0; j < ACTUATIONPORTS; j++) {
            ASSERT_EQ(*getTimerCCR(j), 0) << "j = " << j;
        }

        /* Press a button */
        stmSetGpio(buttonGpio[i], false);
        simTicks(100); /* Should respond within 100 ms */

        /* Only the requested port should be on */
        for(int j = 0; j < ACTUATIONPORTS; j++) {
            if(j != i) {
                ASSERT_EQ(*getTimerCCR(j), 0) << "j = " << j;    
            } 
            else {
                ASSERT_EQ(*getTimerCCR(j), 999) << "j = " << j;
            }
        }

        /* Check status bit updated correctly. Could take up to 100 ms for another print */
        simTicks(100);
        switch(i) {
            case 0: EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x00000001"));
                    break;
            case 1: EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x00000002"));
                    break;
            case 2: EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x00000004"));
                    break;
            case 3: EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x00000008"));
                    break;
            case 4: EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x00000010"));
                    break;
            case 5: EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x00000020"));
                    break;
            default: FAIL();
        }

        /* Release button */
        stmSetGpio(buttonGpio[i], true);
        simTicks(100); /* Should respond within 100 ms */
    }
}   

/* Grey box - uses getTimerCCR() */
TEST_F(DCBoard, onboardButtonsOff) 
{
    dcSetup();
    goToTick(1);

    for(int i = 0; i < ACTUATIONPORTS; i++) {
        /* All ports initially on */
        writeDcMessage("all on 5\n");

        for(int j = 0; j < ACTUATIONPORTS; j++) {
            ASSERT_EQ(*getTimerCCR(j), 999) << "j = " << j;
        }

        /* Put one port on a timer */
        char cmd[100] = {0};
        sprintf(cmd, "p%u on %u\n", i+1, 1);
        writeDcMessage(cmd);

        /* Press a button */
        stmSetGpio(buttonGpio[i], false);
        simTicks(100); /* Should respond within 100 ms */

        /* There should be no effect */
        for(int j = 0; j < ACTUATIONPORTS; j++) {
            ASSERT_EQ(*getTimerCCR(j), 999) << "j = " << j;    
        }

        simTicks(100); /* Could take up to 100 ms for another print */
        /* Check status bit updated correctly */
        EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x0000003f")); 

        /* Release button */
        stmSetGpio(buttonGpio[i], true);
        simTicks(100); /* Should respond within 100 ms */

        /* Timer port should turn off 1 second after the start of the test */
        simTicks(700);
        ASSERT_EQ(*getTimerCCR(i), 999) << "i = " << i;   
        simTicks();
        ASSERT_EQ(*getTimerCCR(i), 0) << "i = " << i;   
    }
}   

/* Grey box - uses getTimerCCR() */
TEST_F(DCBoard, onboardButtonsPortDurationExpiresDuringOnPeriod) 
{
    dcSetup();
    goToTick(1);

    for(int i = 0; i < ACTUATIONPORTS; i++) {
        /* All ports initially on */
        writeDcMessage("all on 5\n");

        for(int j = 0; j < ACTUATIONPORTS; j++) {
            ASSERT_EQ(*getTimerCCR(j), 999) << "j = " << j;
        }

        /* Put one port on a shorter timer */
        char cmd[100] = {0};
        sprintf(cmd, "p%u on %u\n", i+1, 1);
        writeDcMessage(cmd);

        /* Press a button */
        stmSetGpio(buttonGpio[i], false);
        simTicks(100); /* Should respond within 100 ms */

        /* There should be no effect */
        for(int j = 0; j < ACTUATIONPORTS; j++) {
            ASSERT_EQ(*getTimerCCR(j), 999) << "j = " << j;    
        }

        simTicks(100); /* Could take up to 100 ms for another print */
        /* Check status bit updated correctly */
        EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x0000003f")); 

        /* Timer port should time out after 1 second but not turn off because button is pressed */
        simTicks(800);
        simTicks();
        ASSERT_EQ(*getTimerCCR(i), 999) << "i = " << i;   

        /* Release button */
        stmSetGpio(buttonGpio[i], true);
        simTicks(100); /* Should respond within 100 ms */
        ASSERT_EQ(*getTimerCCR(i), 0) << "i = " << i;   
    }
}   

TEST_F(DCBoard, testCurrentBuffer) {
    dcSetup();
    goToTick(100);

    EXPECT_FLUSH_USB(Contains("0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x00000000"));

    for(int h = 0; h < 4096; h++) {
        
        /* Fill with current */
        for (int i = 0; i < ADC_CHANNELS-1; i++) {
            setADCChannelBuffer(i, h);
        }

        /* Get the next printout */
        simTicks(100);

        char buf[100] = {0};
        double curr = ((h / 4096.0) * 3.3 - 1.65) / 0.264;
        sprintf(buf, "%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, 0x00000000", 
                curr, curr, curr, curr, curr, curr);
        EXPECT_FLUSH_USB(Contains(buf));
    }
} 

/* Test that ports are shut off when disconnecting USB */
TEST_F(DCBoard, testPortShutOffAtUSBDisconnect) 
{
    dcSetup();
    goToTick(1);

    writeDcMessage("all on 20\n");

    /* All ports should be on */
    for(int j = 0; j < ACTUATIONPORTS; j++) {
        ASSERT_EQ(*getTimerCCR(j), 999) << "j = " << j;    
    }

    goToTick(5000);
    /* All ports should still be on */
    for(int j = 0; j < ACTUATIONPORTS; j++) {
        ASSERT_EQ(*getTimerCCR(j), 999) << "j = " << j;    
    }

    goToTick(20002);
    /* All ports should be shut off after 20 seconds */
    for(int j = 0; j < ACTUATIONPORTS; j++) {
        ASSERT_EQ(*getTimerCCR(j), 0) << "j = " << j;    
    }

    // Reset timer and turn on ports on again
    writeDcMessage("all on 20\n");

    /* All ports should be on */
    for(int j = 0; j < ACTUATIONPORTS; j++) {
        ASSERT_EQ(*getTimerCCR(j), 999) << "j = " << j;    
    }

    /* Disconnect USB */
    hostUSBDisconnect();
    goToTick(22500);
     /* Ports should not turn off until 5 seconds after USB disconnect */
    for(int j = 0; j < ACTUATIONPORTS; j++) {
        ASSERT_EQ(*getTimerCCR(j), 999) << "j = " << j;    
    }

    // Takes a 100 ticks to go to the ADC callback function
    goToTick(25100);
    /* Ports should not turn off until 5 seconds after USB disconnect */
    for(int j = 0; j < ACTUATIONPORTS; j++) {
        ASSERT_EQ(*getTimerCCR(j), 0) << "j = " << j;    
    }
}