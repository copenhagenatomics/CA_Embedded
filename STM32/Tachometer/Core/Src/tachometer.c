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

/* Macro to make setting pin layouts quicker */
#define SET_GPIO_PINS(x, a, b, c, d, e, f) \
    do { \
        x[0] = a; \
        x[1] = b; \
        x[2] = c; \
        x[3] = d; \
        x[4] = e; \
        x[5] = f; \
    } while(0)

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

/* For determining pin layout */
/* Channels are different on different board versions */ 
static pcbVersion ver;
static uint8_t gpio_pins[NUM_CHANNELS] = {0};

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void computeTachoFrequency(int channel);
static void printFrequencies();
static void resetFlow();
static void initGpio(void);

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

/*!
** @brief Sets up the STM32 GPIO low-level HAL for the tachometer
**
** Different PCB versions have different pin layouts
*/
static void initGpio(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    if(ver.major == 3 && ver.minor == 1) {
        GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
        SET_GPIO_PINS(gpio_pins, GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5);
    }
    else if(ver.major == 3 && ver.minor == 2) {
        GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
        SET_GPIO_PINS(gpio_pins, GPIO_PIN_5, GPIO_PIN_4, GPIO_PIN_3, GPIO_PIN_2, GPIO_PIN_1, GPIO_PIN_0);
    }
    else if(ver.major == 3 && ver.minor >= 3) {
        GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_6|GPIO_PIN_7;
        SET_GPIO_PINS(gpio_pins, GPIO_PIN_1, GPIO_PIN_7, GPIO_PIN_6, GPIO_PIN_4, GPIO_PIN_3, GPIO_PIN_2);
    }
    else {
        bsSetError(BS_VERSION_ERROR_Msk);
        return;
    }

    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    if(ver.major == 3 && ver.minor >= 3) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        GPIO_InitStruct.Pin = GPIO_PIN_1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }

    /* Enable interrupts */
    if(ver.minor <= 2) {
        HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    }

    HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);

    HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);

    HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);

    HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);

    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
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
    
    for(int i = 0; i < NUM_CHANNELS; i++) {
        if (gpio_pins[i] == GPIO_Pin) {
            channel = i;
            break;
        }
    }

    computeTachoFrequency(channel);
}

void tachoInputLoop(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
}

void tachoInputInit(TIM_HandleTypeDef* htimIC, TIM_HandleTypeDef* printTim)
{
    /* Since this is sensors only, it isn't dangerous just to continue setup */
    (void) boardSetup(BOARD, PCB, BS_SYSTEM_ERRORS_Msk);
    initCAProtocol(&caProto, usbRx);

    /* If this fails, the version error flag will have been set previously */
    (void) getPcbVersion(&ver);

    initGpio();

    _tachoTim = htimIC;
    HAL_TIM_Base_Start(_tachoTim);

    _printTim = printTim;
    HAL_TIM_Base_Start_IT(_printTim);
}
