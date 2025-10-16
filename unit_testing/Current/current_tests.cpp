/*!
** @file   current_tests.cpp
** @author Luke W
** @date   23/07/2024
*/

#include <string>
#include <sstream>
#include <vector>
#include <inttypes.h>

extern "C" {
    uint32_t _FlashAddrCal = 0;
}

#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Fakes */
#include "fake_stm32xxxx_hal.h"
#include "fake_StmGpio.h"
#include "fake_USBprint.h"

/* Real supporting units */
#include "crc.c"
#include "systeminfo.c"
#include "array-math.c"
#include "CAProtocol.c"
#include "CAProtocolStm.c"
#include "ADCmonitor.c"

/* UUT */
#include "CurrentApp.c"

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsSupersetOf;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class CurrentTest: public CaBoardUnitTest 
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        CurrentTest() : CaBoardUnitTest(currentAppLoop, Current, {LATEST_MAJOR, LATEST_MINOR}) {
            hadc.Init.NbrOfConversion = 5;
        }

        void simTick()
        {
            if(tickCounter != 0) {
                /* ADC sampling speed ~= 2048 Hz, buffer = 1024 * 2. Will fill in a second */
                if(tickCounter % 200 == 0) {
                    HAL_ADC_ConvCpltCallback(&hadc);
                }
                else if(tickCounter % 100 == 0) {
                    HAL_ADC_ConvHalfCpltCallback(&hadc);
                }
            }

            currentAppLoop(bootMsg);
        }

        void fillAdcChannel(int ch, uint16_t val) {
            /* According to default calibration, 2048 yields ~0 current */
            int n = hadc.Init.NbrOfConversion;
            
            int ch_dma_len = hadc.dma_length / n;
            for(int i = 0; i < ch_dma_len; i++) {
                *((int16_t*)hadc.dma_address + ch + n * i) = val;
            }
        }

        void generateSine(int ch, float freq, float amp, float fs, float offset) {
            int n = hadc.Init.NbrOfConversion;
            int ch_dma_len = hadc.dma_length / n;
            float angleStep = 2 * M_PI * freq / fs;

            for (int i = 0; i < ch_dma_len; i++) {
                *((int16_t*)hadc.dma_address + ch + n * i) = (int16_t)roundf(amp * sinf(angleStep * i) + offset);
            }
        }

        void generateSineRamp(int ch, float freqInit, float amp, float fs, float offset, float freqRamp) {
            int n = hadc.Init.NbrOfConversion;
            int ch_dma_len = hadc.dma_length / n;
            float freqStep = freqRamp / fs;

            for (int i = 0; i < ch_dma_len; i++) {
                float freq = freqInit + freqStep * i;
                float angleStep = 2.0 * M_PI * freq / fs;
                *((int16_t*)hadc.dma_address + ch + n * i) = (int16_t)roundf(amp * sinf(angleStep * i) + offset);
            }
        }

        void zeroCurrentAdcBuffer() {
            int n = hadc.Init.NbrOfConversion;
            
            /* According to default calibration:
            ** * 2048 yields ~0 current for phases
            ** * 0 yield 0 current for fault
            ** * 3504 yields 24V for AUX_FB */
            for(int ch = 0; ch < n; ch++) {
                int16_t fill_val = ch < 3 ? 2048 : (ch < 4 ? 0 : 3504);
                fillAdcChannel(ch, fill_val);
            }
        }

        /* Normal board setup */
        void currentSetup() {
            currentAppInit(&hadc, &hadctim, &hcrc);
        
            /* Fill the ADC buffer with null values */
            zeroCurrentAdcBuffer();
        }

        double getNthPrintoutChannel(string line, int n) {
            istringstream iss(line);
            string output;
            for(int i = 0; i <= n; i++) {
                std::getline(iss, output, ',');
            }
            return stod(output);
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/
        ADC_HandleTypeDef hadc;
        CRC_HandleTypeDef hcrc;
        TIM_HandleTypeDef hadctim;
        TIM_HandleTypeDef hlooptim;
        
        SerialStatusTest sst = {
            .boundInit = bind([this]{currentSetup();}),
            .testFixture = this,
        };
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(CurrentTest, goldenStartup) {
    goldenPathTest(sst, "0.00, 0.00, 0.00, 1000000.00, 0, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x00000000", 600);
}

TEST_F(CurrentTest, incorrectBoard) {
    incorrectBoardTest(sst, 600);
}

TEST_F(CurrentTest, printSerial) {
    serialPrintoutTest(sst, "Current", "Calibration: CAL 1,1.000000,0 2,1.000000,0 3,1.000000,0\r");
}

TEST_F(CurrentTest, printStatus) {
    currentSetup();
    /* Note: usb RX buffer is flushed during the first loop, so a single loop must be done before
    ** printing anything */
    currentAppLoop(bootMsg);
    writeBoardMessage("Status\n");
    
    EXPECT_FLUSH_USB(ElementsAre(
        "\r", 
        "Boot Unit Test\r", 
        "Start of board status:\r", 
        "Under voltage. The board operates at too low voltage of 0.00V. Check power supply.\r",
        "\r", 
        "End of board status. \r"
    ));

    goToTick(600);

    /* Flush the USB buffer so the next matcher works properly */
    (void) hostUSBread(true);

    writeBoardMessage("Status\n");
    EXPECT_FLUSH_USB(ElementsAre(
        "\r", 
        "Start of board status:\r", 
        "The board is operating normally.\r",
        "\r", 
        "End of board status. \r"
    ));

    /* Set the ADC buffer just below the undervoltage limit */
    int n = hadc.dma_length / hadc.Init.NbrOfConversion;
    for(int i = 0; i < n; i++) {
        *((int16_t*)hadc.dma_address + 4 + hadc.Init.NbrOfConversion * i) = 3210;
    }

    goToTick(2000);

    /* Flush the USB buffer so the next matcher works properly */
    (void) hostUSBread(true);

    writeBoardMessage("Status\n");
    EXPECT_FLUSH_USB(ElementsAre(
        "\r", 
        "Start of board status:\r", 
        "Under voltage. The board operates at too low voltage of 21.98V. Check power supply.\r",
        "\r", 
        "End of board status. \r"
    ));
}

TEST_F(CurrentTest, faultChannelCurrent) {
    goldenPathTest(sst, "0.00, 0.00, 0.00, 1000000.00, 0, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0x00000000", 600);
    /* Flush the USB buffer */
    (void) hostUSBread(true);

    /* 1 mA ~ 124 adc counts */
    fillAdcChannel(3, 124);
    
    /* Note: half second delay due to large ADC buffer */
    goToTick(1100);

    /* 1 mA at 24 V = 24k */
    vector<string> vs = hostUSBread(true);
    double fault = getNthPrintoutChannel(vs.back(), 3);
    EXPECT_NEAR(fault, 24000.00, 0.001*24000); /* 0.1% accuracy */

    /* Repeat with 4 and 20 mA */
    fillAdcChannel(3, 496);
    goToTick(1600);
    vs = hostUSBread(true);
    fault = getNthPrintoutChannel(vs.back(), 3);
    EXPECT_NEAR(fault, 6000.00, 0.001*6000); /* 0.1% accuracy */

    fillAdcChannel(3, 2482);
    goToTick(2100);
    vs = hostUSBread(true);
    fault = getNthPrintoutChannel(vs.back(), 3);
    EXPECT_NEAR(fault, 1200.00, 0.001*1200); /* 0.1% accuracy */
}

TEST_F(CurrentTest, frequencyMeasurement) {
    float fs     = 4000.0;  // Sampling frequency
    float amp    = 500.0;   // Typical amplitude
    float offset = 2047;    // To center the sine

    currentAppInit(&hadc, &hadctim, &hcrc);

    generateSine(0, 50.0, amp, fs, offset);
    generateSine(1, 75.6, amp, fs, offset);
    generateSine(2, 44.9, amp, fs, offset);
    goToTick(4000);
    vector<string> vs = hostUSBread(true);
    double freq = getNthPrintoutChannel(vs.back(), 5);
    EXPECT_NEAR(freq, 50.0, 0.01 * 50.0);
    freq = getNthPrintoutChannel(vs.back(), 6);
    EXPECT_NEAR(freq, 75.6, 0.01 * 75.6);
    freq = getNthPrintoutChannel(vs.back(), 7);
    EXPECT_NEAR(freq, 44.9, 0.01 * 44.9);

    generateSine(0, 51.0, amp, fs, offset);
    fillAdcChannel(1, 2400);
    goToTick(6000);
    vs = hostUSBread(true);
    freq = getNthPrintoutChannel(vs.back(), 5);
    EXPECT_NEAR(freq, 51.0, 0.1 * 51.0);
    freq = getNthPrintoutChannel(vs.back(), 6);
    EXPECT_NEAR(freq, 0.0, 0.001);
}

TEST_F(CurrentTest, rocofMeasurement) {
    float fs     = 4000.0;  // Sampling frequency
    float amp    = 500.0;   // Typical amplitude
    float offset = 2047;    // To center the sine

    currentAppInit(&hadc, &hadctim, &hcrc);

    // Ramps the frequency from 0 to 60 Hz
    float freqRamp = 10.0;
    float freqDiff = 60.0;
    int noOfSamples = ADC_F_S * freqDiff / freqRamp;
    int N = noOfSamples / (2 * ADC_CHANNEL_BUF_SIZE);
    int tick = 0;
    for (int i = 0; i < N; i++) {
        // Generates a sine with the frequency changing over time
        generateSineRamp(0, i * freqDiff / N, amp, fs, offset, freqRamp);
        tick += 200;
        goToTick(tick);
    }
    vector<string> vs = hostUSBread(true);
    double rocof = getNthPrintoutChannel(vs.back(), 8);
    EXPECT_NEAR(rocof, freqRamp, 0.1 * freqRamp);

    // Keeps constant frequency, the rocof should go to 0
    generateSine(0, 60.0, amp, fs, offset);
    fillAdcChannel(1, 2400);
    goToTick(tick + 1000);
    vs = hostUSBread(true);
    rocof = getNthPrintoutChannel(vs.back(), 8);
    EXPECT_NEAR(rocof, 0.0, 0.1);
}
