#include <stdio.h>
#include "stm32f4xx_hal.h"
#include <ADCMonitor.h>
#include <math.h>
#include <assert.h>

#define NO_SAMPLES 120

// HW depended functions
void HAL_ADC_Start_DMA(ADC_HandleTypeDef* hadc, uint32_t* pData, uint32_t Length)
{
}

static void generate4Sine(int16_t* pData, int offset, int freq)
{
    for (int i = 0; i<NO_SAMPLES; i++)
    {
        pData[4*i+0] = 2041 + (2041.0 * sin( (i*freq + offset      )/180.0 * M_PI));
        pData[4*i+1] = 2041 + (2041.0 * sin( (i*freq + offset + 120)/180.0 * M_PI));
        pData[4*i+2] = 2041 + (2041.0 * sin( (i*freq + offset + 240)/180.0 * M_PI));
        pData[4*i+3] = (42 + i) & 0xFFFF;
    }
}

// Helper function to be used during debug.
static void debugSine(const int16_t* pData, int channel)
{
    for (int i = 0; i<NO_SAMPLES; i++)
    {
        if ((i) % 10 == 0)
            printf("%4d: ", i);

        printf("%4d ",pData[4*i+channel]);
        if ((i+1) % 10 == 0)
            printf("\n");
    }
}

const int testSine()
{
    // Create an array used for buffer data.
    int16_t pData[NO_SAMPLES*4*2];
    generate4Sine(pData, 0, 10);

    ADC_HandleTypeDef dommy = { { 4 } };
    ADCMonitorInit(&dommy, pData, NO_SAMPLES*4*2);

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

int main(int argc, char *argv[])
{
    int line = 0;
    if (line = testSine()) {
        printf("TestSine failed at line %d", line);
    }
}
