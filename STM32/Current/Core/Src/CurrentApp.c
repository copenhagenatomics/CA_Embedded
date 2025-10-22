/*!
 * @file    CurrentApp.c
 * @brief   This file contains the main program of Current board
 * @date    15/10/2021
 * @author  agp
*/

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <inttypes.h>

#include "stm32f4xx_hal.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "ADCMonitor.h"
#include "FLASH_readwrite.h"
#include "systemInfo.h"
#include "USBprint.h"
#include "StmGpio.h"
#include "array-math.h"
#include "pcbversion.h"
#include "CurrentApp.h"
#include "main.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define OMEGA_TO_HZ       (ADC_F_S / (2.0 * M_PI))
#define ROCOF_TO_HZ_PER_S (ADC_F_S * ADC_F_S / (2.0 * M_PI))

#define PLL_ALPHA     0.002f                      // [-]  - PLL magnitude smoothing factor
#define ROCOF_ALPHA   0.001f                      // [-]  - Rocof smoothing factor
#define PLL_MAX_HZ    150.0f                      // [Hz] - Max PLL frequency
#define PLL_MAX_OMEGA (PLL_MAX_HZ / OMEGA_TO_HZ)  // [rad/sample] - Max PLL angular speed
#define PLL_KP        5.0e-3f                     // [rad/sample] - PLL proportional gain
#define PLL_KI        1.5e-3f                     // [rad/sample] - PLL integral gain

#define MIN_CUR_RMS 0.5f // [Arms] - Min RMS current for PLL

typedef enum {
    Stopped,
    Forward,
    Reverse
} PhaseDirection_t;

typedef struct _PLL {
    float theta;        // [rad]        - Phase estimation
    float omega;        // [rad/sample] - Angular speed estimation
    float omegaFilt;    // [rad/sample] - Filtered angular speed estimation
    float integrator;   // [1/sample]   - Integrator
    float amp;          // [-]          - Fundamental amplitude estimation
    float inPhaseFilt;  // [-]          - In phase component
    float quadFilt;     // [-]          - Quadrature component
    float rocof;        // [rad/(sample·sample)]
} PLL_t;

typedef struct
{
    double rms;
    double maBuffer[MOVING_AVERAGE_LENGTH];
    moving_avg_cbuf_t maBufferHandler;
    PLL_t pll;
} PhaseData_t;

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void CurrentPrintHeader();

static void pllReset(PLL_t *pll);
static void pllStep(PLL_t *pll, float x);
static void pDataToValues(int16_t *pData, int noOfChannels, int noOfSamples);
static void updateAdcAmps();
static double adcToCurrent(double adcRMS, int channel);
static double adcToFaultOhm(double adcValue, double adc_vsupply);

static void ADCcalibrationRW(bool wr);
static void ADCcalibration(int noOfCalibrations, const CACalibration* calibrations);

static void getDirection(const int16_t *pData, int noOfChannels, int noOfSamples);
static void calculateCurrent(int16_t *pData, int noOfChannels, int noOfSamples);

static void printCurrent();
static void adcPrinter();
static void adcLogger(int16_t *pData, int noOfChannels, int noOfSamples);
static void logging(int port);

static void updateBoardStatus();

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static CRC_HandleTypeDef* hcrc_ = NULL;

static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2];
static ADCCallBack adcCbFunc = calculateCurrent;

static struct
{
    PhaseData_t phases[NUM_CURRENT_CHANNELS];
    double fault;
    PhaseDirection_t dir;
} currentData = {};

static struct
{
    double transformerRatio;
} adcToAmps[NUM_CURRENT_CHANNELS];

static StmGpio faultEnable;

static struct
{
    int channel;     // Channel to be logged.
    size_t len;      // Length of current data.
    size_t printed;  // number of samples printed to console/user.
    int16_t log[ADC_CHANNEL_BUF_SIZE * 4]; // Log two seconds.
} adcData = { 0, 0, 0, { 0 } };

static CAProtocolCtx caProto =
{
    .undefined = HALundefined,
    .printHeader = CurrentPrintHeader,
    .printStatus = NULL,
    .printStatusDef = NULL,
    .jumpToBootLoader = HALJumpToBootloader,
    .calibration = ADCcalibration,
    .calibrationRW = ADCcalibrationRW,
    .logging = logging,
    .otpRead = CAotpRead,
    .otpWrite = NULL
};

static double vSupply = 0.0;

/***************************************************************************************************
** FUNCTION DEFINITIONS
***************************************************************************************************/

static void CurrentPrintHeader()
{
    CAPrintHeader();
    ADCcalibrationRW(false);
}

/*!
 * @brief Resets PLL
 * @param pll PLL handler
*/
static void pllReset(PLL_t *pll) {
    pll->amp         = 0.0;
    pll->inPhaseFilt = 0.0;
    pll->quadFilt    = 0.0;
    pll->integrator  = 0.0;
    pll->omega       = 0.0;
    pll->omegaFilt   = 0.0;
    pll->rocof       = 0.0;
    pll->theta       = 1.0;  // Different from 0 so it can starts again
}

/*!
 * @brief Updates PLL
 * @note  Works by fitting an internal oscillator to the input signal
 * @param pll PLL handler
 * @param newSample New ADC value
*/
static void pllStep(PLL_t *pll, float newSample) {
    static float previousOmegaFilt = 0.0;

    // In phase and quadrature signal computation
    float inPhase = newSample * cosf(pll->theta);
    float quad    = newSample * sinf(pll->theta);

    // Filtering
    pll->inPhaseFilt = (1.0 - PLL_ALPHA) * pll->inPhaseFilt + PLL_ALPHA * inPhase;
    pll->quadFilt   = (1.0 - PLL_ALPHA) * pll->quadFilt + PLL_ALPHA * quad;

    // Amplitude estimation
    pll->amp = 2.0 * sqrtf(pll->inPhaseFilt * pll->inPhaseFilt + pll->quadFilt * pll->quadFilt);

    // PI controller that brings quad to 0, so the estimation corresponds to reality
    if (pll->amp > 1.0e-2f) {
        quad /= pll->amp;
    }
    float P = PLL_KP * quad;
    float I = PLL_KI * quad;
    float output = P + pll->integrator + I;

    // Anti wind-up
    if (output > PLL_MAX_OMEGA) {
        output = PLL_MAX_OMEGA;
    }
    else if (output < -PLL_MAX_OMEGA) {
        output = -PLL_MAX_OMEGA;
    }
    else {
        pll->integrator += I;
    }
    pll->omega = output;

    // Filtering
    // Absolute value as PLL can lock on opposite side
    previousOmegaFilt = pll->omegaFilt;
    pll->omegaFilt = (1.0 - PLL_ALPHA) * pll->omegaFilt + PLL_ALPHA * fabsf(pll->omega);
    pll->theta += pll->omega;

    // Wrapping
    while (pll->theta >= 2.0 * M_PI) {
        pll->theta -= 2.0 * M_PI;
    }
    while (pll->theta < 0.0) {
        pll->theta += 2.0 * M_PI;
    }

    // Rate of change of frequency
    float raw_rocof = pll->omegaFilt - previousOmegaFilt;
    pll->rocof = (1.0 - ROCOF_ALPHA) * pll->rocof + ROCOF_ALPHA * raw_rocof;
}

static void pDataToValues(int16_t *pData, int noOfChannels, int noOfSamples)
{
    static struct {
        unsigned int delayCount; // Old value should be reported every second until error is gone.
        double oldFault;         // Fault measured when fault switch on
    } faultChannel = { 0, 0 };

    for (int ch = 0; ch < NUM_CURRENT_CHANNELS; ch++)
    {
        /*
         * True RMS current calculation for each phase with moving average
         *
         * Step 1: Calculation of begin/end indexes of the current sinewave
         * Step 2: Removal of offset with computed indexes (the current sinewave is centered around 0)
         * Step 3: Computation of RMS value based on computed indexes
         * Step 4: Moving average
         */
        SineWaveIndexes_t indexes = sineWave(pData, noOfChannels, noOfSamples, ch);
        ADCSetOffset(pData, -ADCMeanLimited(pData, ch, indexes), ch);
        double current = adcToCurrent(ADCTrueRms(pData, ch, indexes), ch);
        currentData.phases[ch].rms = maMean(&currentData.phases[ch].maBufferHandler, current);

        // Don't run the PLL if the RMS is too low
        if (currentData.phases[ch].rms >= MIN_CUR_RMS) {
            // Frequency and fundamental amplitude estimation
            for (int i = 0; i < noOfSamples; i++) {
                pllStep(&currentData.phases[ch].pll, pData[i * noOfChannels + ch]);
            }
        }
        else {
            pllReset(&currentData.phases[ch].pll);
        }
    }

    if (faultChannel.delayCount == 0)
    {
        currentData.fault = faultChannel.oldFault = adcToFaultOhm(ADCMean(pData, 3), ADCMean(pData,4));
        if (currentData.fault < 1500)
        {
            // Make a delay before reading the sensor again.
            faultChannel.delayCount = 7;

            if(NULL != faultEnable.set)
            {
                stmSetGpio(faultEnable, false);
            }
        }
    }
    else
    {
        currentData.fault = faultChannel.oldFault;
        faultChannel.delayCount--;
        if ((faultChannel.delayCount == 0) && (NULL != faultEnable.set))
        {
            stmSetGpio(faultEnable, true);
        }
    }
}

static void updateAdcAmps()
{
    if (readFromFlashCRC(hcrc_, (uint32_t) FLASH_ADDR_CAL, (uint8_t*) adcToAmps, sizeof(adcToAmps)) != 0)
    {
        // set default values.
        for (int i = 0; i < NUM_CURRENT_CHANNELS; i++)
        {
            adcToAmps[i].transformerRatio = 1;
        }
    }
}

static double adcToCurrent(double adcRMS, int channel)
{
    /* The current sensing is performed using the INA190A2IDDFR
     * Bidirectional current monitoring is computed using
     * Vout = (Iload * RSENSE * GAIN * PREGAIN) + Vbias
     *      => Iload = (Vout - Vbias) / (RSENSE * GAIN * PREGAIN)
     *  
     *  NOTE: Vbias is removed from the calculation below since the adcRMS measure
     *        is computed with the bias already subtracted
     */

    static const float GAIN = 50;             // The gain is 50V/V from INA190A2 device type
    static const float RSENSE = 0.02;         // Ohm
    static const float PREGAIN = 0.2507;      // Voltage divider gain

    float Vrms = adcRMS / ADC_RESOLUTION * ADC_V;

    // The transformer ratio defines the ratio between amps running through the phase
    // to the output of the current clamp
    return Vrms / (GAIN * PREGAIN * RSENSE) * adcToAmps[channel].transformerRatio;
}

static double adcToFaultOhm(double adcValue, double adc_vsupply)
{
    static const float FAULT_GAIN = 100; 
    static const float V_TO_A = 1; 
    float faultVoltage = adcValue / ADC_RESOLUTION * ADC_V;
    
    // The INA293B3QDBVRQ1 has 100V/A. A 1Ohm shunt means V_To_A = 1
    // => 1/(V_TO_A*FAULT_GAIN) A/V => 0.01 A/V 
    vSupply = adc_vsupply / ADC_RESOLUTION * VSUPPLY_RANGE;
    float faultCurrent = faultVoltage / (V_TO_A * FAULT_GAIN);

    /* Apply upper limit of 1 MOhm. This is to prevent divide by 0 */
    float faultResistance = 1E6;
    if(faultCurrent >= (VSUPPLY_EXPECTED / faultResistance))
    {
        faultResistance = vSupply / faultCurrent;
    }

    return faultResistance;
}

static void ADCcalibrationRW(bool wr)
{
    if (wr)
    {
        if (writeToFlashCRC(hcrc_, (uint32_t) FLASH_ADDR_CAL, (uint8_t *) adcToAmps, sizeof(adcToAmps)) != 0)
        {
            USBnprintf("Calibration was not stored in FLASH");
        }
    }
    else {
        char buf[100];
        int len = snprintf(buf, sizeof(buf), "Calibration: CAL 1,%lf,0 2,%lf,0 3,%lf,0\r\n",
                           adcToAmps[0].transformerRatio,
                           adcToAmps[1].transformerRatio,
                           adcToAmps[2].transformerRatio);
        writeUSB(buf, len);
    }
}

static void ADCcalibration(int noOfCalibrations, const CACalibration* calibrations)
{
    for (int count = 0; count < noOfCalibrations; count++)
    {
        if (1 <= calibrations[count].port && calibrations[count].port <= NUM_CURRENT_CHANNELS)
        {
            // Only alter calibration for Current phases, port 1,2,3.
            // Port 4 is the fault channel and should not be changed.
            const int port = calibrations[count].port - 1;
            adcToAmps[port].transformerRatio = calibrations[count].alpha;
        }
    }
    ADCcalibrationRW(true); // Always update in flash when receiving new calibration
}

static void getDirection(const int16_t *pData, int noOfChannels, int noOfSamples)
{
    // Find top of Phase A.
    unsigned int maxId = 0;
    for (int j = 0; j < noOfSamples - 2; j++)
    {
        if (pData[j*noOfChannels] > pData[maxId])
        {
            maxId = j*noOfChannels;
        }
    }

    // Get threshold from ADC Mean value.
    int16_t threshold = 50 + ADCMean(pData, 0);
    if (pData[maxId] < threshold)
    {
        currentData.dir = Stopped; // Current level too low, motor can't run
    }
    else
    {
        // Next phase and its slope
        maxId++;
        currentData.dir = (pData[maxId+noOfChannels] > pData[maxId]) ? Forward : Reverse;
    }
}

static void calculateCurrent(int16_t *pData, int noOfChannels, int noOfSamples)
{
    getDirection(pData, noOfChannels, noOfSamples);
    pDataToValues(pData, noOfChannels, noOfSamples);
    printCurrent();
}

static void printCurrent()
{
    if (!isUsbPortOpen())
        return;

    if(bsGetStatus() & BS_VERSION_ERROR_Msk) {
        USBnprintf("0x%08" PRIx32, bsGetStatus());
        return;
    }

    USBnprintf("%.2f, %.2f, %.2f, %.2f, %d, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, 0x%08" PRIx32,
                currentData.phases[0].rms,
                currentData.phases[1].rms,
                currentData.phases[2].rms,
                currentData.fault,
                currentData.dir,
                currentData.phases[0].pll.omegaFilt * OMEGA_TO_HZ,
                currentData.phases[1].pll.omegaFilt * OMEGA_TO_HZ,
                currentData.phases[2].pll.omegaFilt * OMEGA_TO_HZ,
                currentData.phases[0].pll.rocof * ROCOF_TO_HZ_PER_S,
                currentData.phases[1].pll.rocof * ROCOF_TO_HZ_PER_S,
                currentData.phases[2].pll.rocof * ROCOF_TO_HZ_PER_S,
                bsGetStatus());
}

static void adcPrinter()
{
    char buf[20];

    if (adcData.printed == adcData.len)
        return; // No data to print

    while (adcData.printed<adcData.len && txAvailable() > sizeof(buf))
    {
        int len = sprintf(buf, "%u,", adcData.log[adcData.printed]);
        if (len > 0)
        {
            adcData.printed++;
            writeUSB(buf, len);
            if ((adcData.printed % 16) == 0)
            {
                writeUSB("\r\n", 2);
            }
        }
    }

    if (adcData.printed == sizeof(adcData.log)/sizeof(int16_t))
    {
        // Wait for ADC callback to generate new data
        writeUSB("\r\n", 2);
    }
}

static void adcLogger(int16_t *pData, int noOfChannels, int noOfSamples)
{
    for (int16_t* p = pData;
         adcData.len < sizeof(adcData.log)/sizeof(int16_t) && p < &pData[noOfChannels * noOfSamples];
         p += noOfChannels)
    {
        adcData.log[adcData.len] = *p;
        adcData.len++;
    }
    adcPrinter();
}

static void logging(int port)
{
    // Change callback for logging
    adcData.len = 0;
    adcData.printed = 0;
    if (port > 0 && port <= ADC_CHANNELS)
    {
        adcData.channel = port-1;
        adcCbFunc = adcLogger;
    }
    else
        adcCbFunc = calculateCurrent;
}

/*!
** @brief Checks vsupply against undervoltage
*/
static void updateBoardStatus()
{
    if(vSupply < VSUPPLY_UNDERVOLTAGE)
    {
        bsSetError(BS_UNDER_VOLTAGE_Msk);
    }
    else
    {
        bsClearField(BS_UNDER_VOLTAGE_Msk);
    }
    setBoardVoltage(vSupply);

    /* Clears the error bit if all other error bits are cleared */
    bsClearError(BS_SYSTEM_ERRORS_Msk);
}

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

void currentAppInit(ADC_HandleTypeDef* hadc, TIM_HandleTypeDef* adcTimer, CRC_HandleTypeDef* hcrc)
{
    initCAProtocol(&caProto, usbRx);

    hcrc_ = hcrc;

    ADCMonitorInit(hadc, ADCBuffer, sizeof(ADCBuffer)/sizeof(int16_t));

    for (int ch = 0; ch < NUM_CURRENT_CHANNELS; ch++)
    {
        maInit(&currentData.phases[ch].maBufferHandler, currentData.phases[ch].maBuffer, MOVING_AVERAGE_LENGTH);
        pllReset(&currentData.phases[ch].pll);
    }

    /* Don't initialise any outputs or act on them if the board isn't correct */
    if (-1 == boardSetup(Current, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}, 0))
    {
        return;
    }

    stmGpioInit(&faultEnable, Fault_Switch_GPIO_Port, Fault_Switch_Pin, STM_GPIO_OUTPUT);
    updateAdcAmps();

    // Start timers to begin a measure.
    stmSetGpio(faultEnable, true); // start reading the fault channel.
    HAL_TIM_Base_Start(adcTimer);
}

void currentAppLoop(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
    ADCMonitorLoop(adcCbFunc);
    updateBoardStatus();
}
