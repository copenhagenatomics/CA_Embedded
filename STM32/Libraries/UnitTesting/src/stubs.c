#include <stdio.h>
#include "stm32f4xx_hal.h"
#include <ADCMonitor.h>
#include <math.h>
#include <assert.h>

// HW depended functions
void HAL_ADC_Start_DMA(ADC_HandleTypeDef* hadc, uint32_t* pData, uint32_t Length)
{
}

static void generate4Sine(int16_t* pData, int length, int offset, int freq)
{
    for (int i = 0; i<length; i++)
    {
        pData[4*i+0] = 2041 + (2041.0 * sin( (i*freq + offset      )/180.0 * M_PI));
        pData[4*i+1] = 2041 + (2041.0 * sin( (i*freq + offset + 120)/180.0 * M_PI));
        pData[4*i+2] = 2041 + (2041.0 * sin( (i*freq + offset + 240)/180.0 * M_PI));
        pData[4*i+3] = (42 + i) & 0xFFFF;
    }
}

// Helper function to be used during debug.
static void debugPData(const int16_t* pData, int length, int channel)
{
    for (int i = 0; i<length; i++)
    {
        if ((i) % 10 == 0)
            printf("%4d: ", i);

        printf("%4d ",pData[4*i+channel]);
        if ((i+1) % 10 == 0)
            printf("\n");
    }
}

int testSine()
{
    // Create an array used for buffer data.
    const int noOfSamples = 120;
    int16_t pData[noOfSamples*4*2];
    generate4Sine(pData, noOfSamples, 0, 10);

    ADC_HandleTypeDef dommy = { { 4 } };
    ADCMonitorInit(&dommy, pData, noOfSamples*4*2);

    SineWave s = sineWave(pData, 0);

    if (s.begin != 9 || s.end != 117)
        return __LINE__;
    s = sineWave(pData, 1);
    if (s.begin != 15 || s.end != 105)
        return __LINE__;
    s = sineWave(pData, 2);
    if (s.begin != 3 || s.end != 111)
        return __LINE__;
    return 0;
}

int testCMAverage()
{
    const int noOfSamples = 10;
    int16_t pData[noOfSamples*4*2];

    for (int i = 0; i<noOfSamples; i++)
    {
        pData[4*i] = (i % 10) * 20;
    }
    ADC_HandleTypeDef dommy = { { 4 } };
    ADCMonitorInit(&dommy, pData, noOfSamples*4*2);

    if (cmaAvarage(pData, 0, 85, 5) != 112)
        return __LINE__;

    return 0;
}

int main(int argc, char *argv[])
{
    int line = 0;
    if (line = testSine()) {
        printf("\nTestSine failed at line %d\n", line);
    }
    if (line = testCMAverage()) {
        printf("\nTestSine failed at line %d\n", line);
    }

}
