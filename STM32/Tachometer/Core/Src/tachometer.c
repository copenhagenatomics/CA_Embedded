/*! 
** @file    tachometer.c
** @brief   This file contains the main program of tachometer 
** @date:   7/05/2024
** @author: Luke W
*/

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#include "tachometer.h"
#include "USBprint.h"
#include "systemInfo.h"
#include "CAProtocolStm.h"
#include "CAProtocol.h"
#include "pcbversion.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define TIMCLOCK    64000000
#define PRESCALAR   64

/* Minimum frequency is arbitrary, and could be lowered if there is a demand. It exists to ensure it 
** is possible to timeout to 0 Hz if edges stop coming */
#define MIN_FREQ    1.0

/* Maximum frequency is limited by hardware on the board. As of V3.2, this is ~1.5 kHz */

#define NUM_CHANNELS 6
#define CH1_GPIO    GPIO_PIN_5
#define CH2_GPIO    GPIO_PIN_4
#define CH3_GPIO    GPIO_PIN_3
#define CH4_GPIO    GPIO_PIN_2
#define CH5_GPIO    GPIO_PIN_1
#define CH6_GPIO    GPIO_PIN_0

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static float freq[NUM_CHANNELS] = {0};

static CAProtocolCtx caProto =
{
    .undefined = HALundefined,
    .printHeader = CAPrintHeader,
    .printStatus = NULL,
    .jumpToBootLoader = HALJumpToBootloader,
    .calibration = NULL,
    .calibrationRW = NULL,
    .logging = NULL,
    .otpRead = CAotpRead,
    .otpWrite = NULL
};

static struct {
    bool        notFirstEdge;
    uint32_t    prevTimCount;
    uint32_t    nowTimCount;
    float       avgFreq;
    uint32_t    numSamples;
} timData[NUM_CHANNELS] = {0};

TIM_HandleTypeDef* _printTim = NULL;
TIM_HandleTypeDef* _tachoTim = NULL;

static const float REF_CLOCK = TIMCLOCK / PRESCALAR;

static const BoardType  BOARD = Tachometer;
static const pcbVersion PCB   = {BREAKING_MAJOR, BREAKING_MINOR};

static bool reverse = false;

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void computeTachoFrequency(int channel);
static void printFrequencies();
static void resetFlow();

/***************************************************************************************************
** PRIVATE FUNCTION DEFINITIONS
***************************************************************************************************/

static void computeTachoFrequency(int channel)
{
    /* On the first edge, don't do anything, as a real frequency cannot be calculated with just one 
    ** edge */
    if (!timData[channel].notFirstEdge)
    {

        timData[channel].prevTimCount = __HAL_TIM_GET_COUNTER(_tachoTim);
        timData[channel].notFirstEdge = true;    
    }
    /* On subsequent edges, the calculate the frequency based on the time difference between the last
    ** edge and this one. Update a rolling average with the new frequency information. The average is 
    ** reset at every print so that the device remains responsive at low frequencies. The minimum 
    ** frequency is set to 1 Hz. */
    else
    {
        timData[channel].nowTimCount = __HAL_TIM_GET_COUNTER(_tachoTim);

        uint32_t diff = 0;
        if (timData[channel].nowTimCount >= timData[channel].prevTimCount) {
            diff = timData[channel].nowTimCount - timData[channel].prevTimCount;
        }
        else {
            diff = (0xFFFFFFFF - timData[channel].prevTimCount) + timData[channel].nowTimCount;
        }

        /* Prevent divide by 0 */
        diff = diff != 0 ? diff : 1;

        float nextFreq = REF_CLOCK / diff;
        timData[channel].avgFreq = (timData[channel].avgFreq * timData[channel].numSamples + nextFreq) / (timData[channel].numSamples + 1);
        freq[channel] = timData[channel].avgFreq;

        /* Save the last edge for the next calculation */
        timData[channel].prevTimCount = timData[channel].nowTimCount;
        timData[channel].numSamples++;
    }
}

static void printFrequencies()
{
    if (!isUsbPortOpen())
        return;

    if(bsGetStatus() & BS_VERSION_ERROR_Msk) {
        USBnprintf("0x%08x", bsGetStatus());
        return;
    }

    USBnprintf("%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, 0x%08x", freq[0], freq[1], freq[2], freq[3], freq[4], freq[5], bsGetStatus());
}

static void resetFlow()
{
    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        /* Resets the frequency to 0 if it falls below the minimum frequency. This is to prevent 
        ** the tachometer getting stuck if the edges stop */
        uint32_t now    = __HAL_TIM_GET_COUNTER(_tachoTim);
        float currFreq  = REF_CLOCK / (now - timData[i].prevTimCount);
        
        if(currFreq < MIN_FREQ) {
            freq[i] = 0;
        }

        /* Maintains the previous average so that slow freq are not lost at each print 
        ** interval, but minimises its weight against any subsequent measurements for fast 
        ** measurements */
        timData[i].numSamples = 1;
    }
}

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim == _printTim)
    {
        resetFlow();
        printFrequencies();
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    int channel = 0;
    
    switch(GPIO_Pin) {
        case CH1_GPIO:  channel = 0;
                        break;
        case CH2_GPIO:  channel = 1;
                        break;
        case CH3_GPIO:  channel = 2;
                        break;
        case CH4_GPIO:  channel = 3;
                        break;
        case CH5_GPIO:  channel = 4;
                        break;
        case CH6_GPIO:  channel = 5;
                        break;
        default:        return;
    }

    computeTachoFrequency(reverse ? 5 - channel : channel);
}

void tachoInputLoop(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
}

void tachoInputInit(TIM_HandleTypeDef* htimIC, TIM_HandleTypeDef* printTim)
{
    /* Since this is sensors only, it isn't dangerous just to continue setup */
    (void) boardSetup(BOARD, PCB);
    initCAProtocol(&caProto, usbRx);

    /* If this fails, the version error flag will have been set previously */
    pcbVersion ver;
    (void) getPcbVersion(&ver);

    /* Channels are reversed on older versions of the board */ 
    if(ver.major == 3 && ver.minor < 2) {
        reverse = true;
    }

    _tachoTim = htimIC;
    HAL_TIM_Base_Start(_tachoTim);

    _printTim = printTim;
    HAL_TIM_Base_Start_IT(_printTim);
}
