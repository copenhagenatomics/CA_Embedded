/*
 * ADCmonitor.c
 *
 *  Created on: 25 Aug 2021
 *      Author: agp
 *  Description:
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <math.h>
#include <ADCMonitor.h>
#include "USBprint.h"

static struct {
    uint32_t   length;
    int16_t   *pData;        // DMA buffer
    uint32_t   noOfChannels; // No of channels for each sample
    uint32_t   noOfSamples;  // No of Samples in each channel per interrupt, half buffer.

    enum {
        NotAvailable,
        First,
        Second
    } activeBuffer;
} ADCMonitorData;

void ADCMonitorInit(ADC_HandleTypeDef* hadc, int16_t *pData, uint32_t length)
{
    // Save internal data.
    ADCMonitorData.pData           = pData;
    ADCMonitorData.length          = length;
    ADCMonitorData.noOfChannels    = hadc->Init.NbrOfConversion;
    ADCMonitorData.noOfSamples     = length / (2 * hadc->Init.NbrOfConversion);

    // Write the registers
    HAL_ADC_Start_DMA(hadc, (uint32_t *) pData, length);
}

void ADCMonitorLoop(ADCCallBack callback)
{
    static int lastBuffer = NotAvailable;

    if (ADCMonitorData.activeBuffer != lastBuffer)
    {
        lastBuffer = ADCMonitorData.activeBuffer;
        int16_t *pData = (ADCMonitorData.activeBuffer == First)
                ? ADCMonitorData.pData : &ADCMonitorData.pData[ADCMonitorData.length / 2];
        callback(pData, ADCMonitorData.noOfChannels, ADCMonitorData.noOfSamples);
    }
}

double ADCrms(const int16_t *pData, uint16_t channel)
{
    if (ADCMonitorData.activeBuffer == NotAvailable ||
        pData == NULL ||
        channel >= ADCMonitorData.noOfChannels)
    {
        return 0;
    }

    uint64_t sum = 0;
    for (uint32_t sampleId = 0; sampleId < ADCMonitorData.noOfSamples; sampleId++)
    {
        const int16_t mul = pData[sampleId*ADCMonitorData.noOfChannels + channel];
        sum += (mul * mul); // add squared values to sum
    }

    return sqrt(sum / ADCMonitorData.noOfSamples);
}

double ADCMean(const int16_t *pData, uint16_t channel)
{
    if (ADCMonitorData.activeBuffer == NotAvailable ||
        pData == NULL ||
        channel >= ADCMonitorData.noOfChannels)
    {
        return 0;
    }

    uint64_t sum = 0;
    for (uint32_t sampleId = 0; sampleId < ADCMonitorData.noOfSamples; sampleId++)
    {
        sum += pData[sampleId*ADCMonitorData.noOfChannels + channel];
    }

    return (sum / ADCMonitorData.noOfSamples);
}

double ADCAbsMean(const int16_t *pData, uint16_t channel)
{
    if (ADCMonitorData.activeBuffer == NotAvailable ||
        pData == NULL ||
        channel >= ADCMonitorData.noOfChannels)
    {
        return 0;
    }

    uint64_t sum = 0;
    for (uint32_t sampleId = 0; sampleId < ADCMonitorData.noOfSamples; sampleId++)
    {
        sum += abs(pData[sampleId*ADCMonitorData.noOfChannels + channel]);
    }

    return (sum / ADCMonitorData.noOfSamples);
}

uint16_t ADCmax(const int16_t *pData, uint16_t channel)
{
    if (ADCMonitorData.activeBuffer == NotAvailable ||
        pData == NULL ||
        channel >= ADCMonitorData.noOfChannels)
    {
        return 0;
    }

    uint16_t max = 0;
    for (uint32_t sampleId = 0; sampleId < ADCMonitorData.noOfSamples; sampleId++)
    {
        int16_t sample = pData[sampleId*ADCMonitorData.noOfChannels + channel];
        if (max < sample)
            max = sample;
    }
    return max;
}

void ADCSetOffset(int16_t* pData, int16_t offset, uint16_t channel)
{
    if (ADCMonitorData.activeBuffer == NotAvailable ||
        pData == NULL ||
        channel >= ADCMonitorData.noOfChannels)
    {
        return;
    }

    for (uint32_t sampleId = 0; sampleId < ADCMonitorData.noOfSamples; sampleId++)
    {
        // No need to addjust for overflow since ADC is 12 bits.
        pData[sampleId*ADCMonitorData.noOfChannels + channel] += offset;
    }
}

int16_t sineRBegin(const int16_t* pData, uint16_t channel)
{
    // RMS (and other) should be done on full waves of sine.
    // Procedure: Take first point, pData[channel] and find mathcing value
    // and gradient from end.
    const uint16_t firstPoint = pData[channel];
    const uint32_t noOfChannels = ADCMonitorData.noOfChannels;
    const uint32_t noOfSamples = ADCMonitorData.noOfSamples;
    const bool raise = pData[noOfChannels + channel] > firstPoint;

    uint32_t end = noOfChannels * (noOfSamples-2) + channel;
    for (;end > noOfChannels; end -= noOfChannels)
    {
        const int16_t current = pData[end];
        const int16_t prev = pData[end-noOfChannels];
        const bool isRaise = current > prev;

        if (isRaise != raise)
            continue; // Wrong section of sine wave.

        if (((raise && (firstPoint <= current && firstPoint > prev)) ||
            (!raise && (firstPoint <= prev    && firstPoint > current))))
        {
            return end;
        }
    }
    return channel; // Invalid sine curve.
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
    ADCMonitorData.activeBuffer = First;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    ADCMonitorData.activeBuffer = Second;
}
