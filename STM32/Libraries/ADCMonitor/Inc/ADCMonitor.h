#ifndef _ADCMONITOR_H_
#define _ADCMONITOR_H_

#include <stdint.h>
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Callback function from ADCMonitorLoop.
 * The format of the buffer is [ CH0{s0}, CH1{s0},,,, CHN{s0},
 *                               CH0{s1}, CH1{s1},,,, CHN{s1},
 *                               ...........
 *                               CH0{sM}, CH2{sM},,,, CHN{SN} ], N,M =[0:inf]
 * SInce this modules does not has a fixed format the buffer is a one dimensional array and
 * each sample is fetched using pData[SampleNo * noOfChannls + channelNumber] */
typedef void (*ADCCallBack)(int16_t *pBuffer, int noOfChannels, int noOfSamples);

// ADC Monitor initialisation function. MUST be called before call to any other function
// Must only be called once.
void ADCMonitorInit(ADC_HandleTypeDef* hadc, int16_t *pData, uint32_t Length);

// must be called from from while(1) loop to get info about new buffer via
// ADCCallBack function. If new buffer, callback is called, else nothing is done.
void ADCMonitorLoop(ADCCallBack cb);

// perform a Cumulative moving average on data in buffer.
// Note, data is altereed in buffer.
// @param preveous calculated cma
// @param k is the length of cumulative buffer.
int16_t cmaAvarage(int16_t *pData, uint16_t channel, int16_t cma, int k);

// Standard helper function where noOfChannles/NoOfSamples is
// used from ADCMonitorInit. Can be used for skeleton locally.
double ADCMean(const int16_t *pData, uint16_t channel);
double ADCAbsMean(const int16_t *pData, uint16_t channel);
double ADCrms(const int16_t *pData, uint16_t channel);
uint16_t ADCmax(const int16_t *pData, uint16_t channel);

// @Description Set offset on each sample in channel, Xi = Xi + offset
// @Param pData Pointer to buffer from callback function
// @Param offset to adjust channel
// @param channel Channel in sample Data to adjust.
// Note, no check for overflow since ADC is 12 bit's
void ADCSetOffset(int16_t* pData, int16_t offset, uint16_t channel);

// Find Sample start/begin of a half sine curve.
// @Return statuc Start/end of sample Index (not pointer offset).
typedef struct SineWave {
    uint32_t begin;
    uint32_t end;
} SineWave;
SineWave sineWave(const int16_t* pData, uint32_t noOfChannels, uint32_t noOfSamples, uint16_t channel);

#ifdef __cplusplus
}
#endif

#endif // _ADCMONITOR_H_
