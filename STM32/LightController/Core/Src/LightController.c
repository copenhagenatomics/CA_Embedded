/*
 * LightController.c
 *
 *  Created on: Aug 12, 2022
 *      Author: matias
 */

#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ADCMonitor.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "LightController.h"
#include "StmGpio.h"
#include "USBprint.h"
#include "pcbversion.h"
#include "systemInfo.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define ADC_CHANNELS         1     // Number of ADC channels used on the STM32
#define ADC_CHANNEL_BUF_SIZE 100   // 1 kHz sampling  rate -> 10 Hz
#define ADC_MAX              4095  // 12-bit
#define ANALOG_REF_VOLTAGE   3.3f  // [V]
#define ADC_RATIO            (ANALOG_REF_VOLTAGE / (ADC_MAX + 1))

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void LightControllerStatus();
static void LightControllerStatusDef();
static bool isInputValid(const char *input, int *channel, unsigned int *rgb);
static int handleInput(unsigned int rgb, uint8_t *red, uint8_t *green, uint8_t *blue);

static void updateLEDCtrl(int channel, unsigned int red, unsigned int green, unsigned int blue,
                          int whiteOn);
static void controlLEDStrip(const char *input);
static void updateLEDs();
static void checkTimeOut();
static void updateStatus();
static void switchTestState();
static bool controlLightTest();
static void initPWMs(TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim2, TIM_HandleTypeDef *htim3,
                     TIM_HandleTypeDef *htim4);
static void adcToFloat(int16_t *pData);
static void adcCallback(int16_t *pData, int noOfChannels, int noOfSamples);

/***************************************************************************************************
** PRIVATE VARIABLES
***************************************************************************************************/

// Circular buffer
static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2];

static float voltage24V = 0;

// PWM timers
static TIM_HandleTypeDef *timers[LED_CHANNELS*NO_COLORS];
static uint32_t channels[LED_CHANNELS*NO_COLORS];

static uint32_t rgbwControl[LED_CHANNELS*NO_COLORS] = {0};
static uint32_t rgbs[LED_CHANNELS] = {0, 0, 0};

// For timeout
static uint32_t lastCmdTime = 0;

// Variables for keeping track of the time elapsed to control the LED test
static uint32_t timer_start = 0;
static uint32_t time_now = 0;

// States
typedef enum {
    OFF,
    RED,
    GREEN,
    BLUE,
    WHITE
}test_mode_state;

static bool isInTest = false;
static test_mode_state testState = OFF;

// Buffer shared by adcCallback, status and statusDef
static char buf[600];

static CAProtocolCtx caProto = {.undefined        = controlLEDStrip,
                                .printHeader      = CAPrintHeader,
                                .printStatus      = LightControllerStatus,
                                .printStatusDef   = LightControllerStatusDef,
                                .jumpToBootLoader = HALJumpToBootloader,
                                .calibration      = NULL,
                                .calibrationRW    = NULL,
                                .logging          = NULL,
                                .otpRead          = CAotpRead,
                                .otpWrite         = NULL};

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

static void LightControllerStatus() {
    int len = 0;

    for (int i = 0; i < LED_CHANNELS; i++) {
        CA_SNPRINTF(buf, len, "Port %d: On: %" PRIu32 "\r\n", i + 1,
                    bsGetField(LIGHT_PORT_STATUS_Msk(i)));
    }
    writeUSB(buf, len);
}

static void LightControllerStatusDef() {
    int len = 0;

    for (int i = 0; i < LED_CHANNELS; i++) {
        len += snprintf(&buf[len], sizeof(buf) - len, "0x%08" PRIu32 ",Status port %d\r\n",
                        (uint32_t)LIGHT_PORT_STATUS_Msk(i), i + 1);
    }
    writeUSB(buf, len);
}

static bool isInputValid(const char *input, int *channel, unsigned int *rgb)
{
    // If test command is entered start the colour test
    // If any other input is entered stop it again
    if (strcmp(input, "test") == 0) 
    {
        isInTest = true;
        return true;
    }
    else
    {
        // Reset the colours when disabling isInTest
        testState = OFF;
        isInTest = false;
    }
    
    if (sscanf(input, "p%d %x", channel, rgb) != 2)
        return false;
    
    if (*channel <= 0 || *channel > LED_CHANNELS)
        return false;

    // If rgb is 0 shut off all colors 
    if (*rgb == 0)
        return true;

    // Check the RGB format is exactly 6 hex characters long
    char* idx = strchr((char*)input, ' ');
    if (strlen(&idx[1]) != 6)
        return false;

    // Check input is valid hex format
    for (int i = 0; i<6; i++)
    {
        if(!isxdigit((unsigned char)input[3+i]))
        {
            return false;
        }
    }

    lastCmdTime = HAL_GetTick();
    return true;
}

static int handleInput(unsigned int rgb, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    *red = (rgb >> 16) & 0xFF;
    *green = (rgb >> 8) & 0xFF;
    *blue = rgb & 0xFF;
    return (rgb == 0xFFFFFF) ? 1 : 0; // If 0xFFFFFF turn on the white led
}


static void updateLEDCtrl(int channel, unsigned int red, unsigned int green, unsigned int blue, int whiteOn)
{
    if (whiteOn)
    {
        rgbwControl[channel*NO_COLORS] 	   = 0;
        rgbwControl[channel*NO_COLORS + 1] = 0;
        rgbwControl[channel*NO_COLORS + 2] = 0;
        rgbwControl[channel*NO_COLORS + 3] = MAX_PWM; 
        return;
    }

    rgbwControl[channel*NO_COLORS] 	   = red;
    rgbwControl[channel*NO_COLORS + 1] = green;
    rgbwControl[channel*NO_COLORS + 2] = blue;
    rgbwControl[channel*NO_COLORS + 3] = 0; 
}

// Update LED strip with user input colors
static void controlLEDStrip(const char *input)
{
    int port = 1;
    unsigned int rgb = 0x000000;

    // If the SW Version does not match,
    // do not allow user to control PWMs.
    if (bsGetField(BS_VERSION_ERROR_Msk))
    {
        USBnprintf("SW mismatch. Rejecting MCU PWM control.\r\n");
        return;
    }

    if (!isInputValid(input, &port, &rgb))
    {
        HALundefined(input);
        return;
    }

    int channel = port - 1;
    uint8_t red, green, blue;
    int ret = handleInput(rgb, &red, &green, &blue);
    
    updateLEDCtrl(channel, red, green, blue, ret);
    (rgb != 0x0) ? bsSetField(LIGHT_PORT_STATUS_Msk(channel)) : bsClearField(LIGHT_PORT_STATUS_Msk(channel));
    rgbs[channel] = rgb;
}

static void updateLEDs() {
    for (uint32_t i = 0; i < LED_CHANNELS * NO_COLORS; i++) {
        // Directly applies 8 bit value as PWM resolution (ARR) is 255
        __HAL_TIM_SET_COMPARE(timers[i], channels[i], rgbwControl[i]);
    }
}

static void checkTimeOut() {
    // If more than ACTUATION_TIMEOUT has passed since last command turn off output.
    if ((HAL_GetTick() - lastCmdTime) >= ACTUATION_TIMEOUT) {
        for (uint32_t i = 0; i < LED_CHANNELS * NO_COLORS; i++) {
            rgbwControl[i] = 0;

            if (i < LED_CHANNELS) {
                rgbs[i] = 0;
                bsClearField(LIGHT_PORT_STATUS_Msk(i));
            }
        }
    }
}

static void updateStatus() {
    static const float MIN_VOLTAGE = 21.0;  // [V]

    setBoardVoltage(voltage24V);
    bsUpdateError(BS_UNDER_VOLTAGE_Msk, voltage24V < MIN_VOLTAGE, BS_SYSTEM_ERRORS_Msk);
}

static void switchTestState()
{
    // Turn on every colour for 2 seconds during the test
    switch(testState){  
        
        case OFF: {
            timer_start = HAL_GetTick();
            time_now = timer_start;
            controlLightTest();
            break;
        }
        case RED: {
            time_now = HAL_GetTick();
            if ((time_now - timer_start) > 2000){
                controlLightTest();
            }
            break;
        }
        case GREEN: {
            time_now = HAL_GetTick();
            if ((time_now - timer_start) > 4000){
                controlLightTest();
            }
            break;
        }
        case BLUE: {
            time_now = HAL_GetTick();
            if ((time_now - timer_start) > 6000){
                controlLightTest();
            }
            break;
        }
        case WHITE: {
            time_now = HAL_GetTick();
            if ((time_now - timer_start) > 8000){
                controlLightTest(); 
            }      
            break;   
        }
        default: {
            isInTest = false;
            testState = OFF;          
        }
    }
}

static bool controlLightTest()
{
    // If state is OFF switch it to RED and set all 3 channels to display red
    if (testState == OFF){
        testState = RED;
        for (int i = 0; i < 3; i++){
            //updateLEDCtrl(channel,red,green,blue,whiteon)
            updateLEDCtrl(i, 0xFF, 0, 0, false);
        };
        return true;
    }
    // If state is RED switch it to GREEN and set all 3 channels to display green
    else if (testState == RED){
        testState = GREEN;   
        for (int i = 0; i < 3; i++){
            //updateLEDCtrl(channel,red,green,blue,whiteon)
            updateLEDCtrl(i, 0, 0xFF, 0, false);
        };
        return true; 
    }
    // If state is GREEN switch it to BLUE and set all 3 channels to display blue
    else if (testState == GREEN){
        testState = BLUE;
        for (int i = 0; i < 3; i++){
            //updateLEDCtrl(channel,red,green,blue,whiteon)
            updateLEDCtrl(i, 0, 0, 0xFF, false);
        };
        return true;
    }
    // If state is BLUE switch it to WHITE and set all 3 channels to display white
    else if (testState == BLUE){
        testState = WHITE;
        for (int i = 0; i < 3; i++){
            //updateLEDCtrl(channel,red,green,blue,whiteon)
            updateLEDCtrl(i, 0, 0, 0, true);
        };
        return true;
    }
    // If state is WHITE switch it to OFF and set all 3 channels to display nothing and turn off the test mode
    else if (testState == WHITE){
        testState = OFF;
        isInTest = false;
        for (int i = 0; i < 3; i++){
            //updateLEDCtrl(channel,red,green,blue,whiteon)
            updateLEDCtrl(i, 0, 0, 0, false);
        };
        return true;
    }
    
    return false;
}

static void initPWMs(TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim2, TIM_HandleTypeDef *htim3,
                    TIM_HandleTypeDef *htim4) {
    // Led strip 1
    timers[0]   = htim2;
    timers[1]   = htim1;
    timers[2]   = htim1;
    timers[3]   = htim1;
    channels[0] = TIM_CHANNEL_3;
    channels[1] = TIM_CHANNEL_1;
    channels[2] = TIM_CHANNEL_2;
    channels[3] = TIM_CHANNEL_3;

    // Led strip 2
    timers[4]   = htim3;
    timers[5]   = htim3;
    timers[6]   = htim3;
    timers[7]   = htim3;
    channels[4] = TIM_CHANNEL_1;
    channels[5] = TIM_CHANNEL_2;
    channels[6] = TIM_CHANNEL_3;
    channels[7] = TIM_CHANNEL_4;

    // Led strip 3
    timers[8]    = htim4;
    timers[9]    = htim4;
    timers[10]   = htim2;
    timers[11]   = htim2;
    channels[8]  = TIM_CHANNEL_1;
    channels[9]  = TIM_CHANNEL_2;
    channels[10] = TIM_CHANNEL_4;
    channels[11] = TIM_CHANNEL_1;

    for (uint32_t i = 0; i < LED_CHANNELS * NO_COLORS; i++)
    {
        HAL_TIM_PWM_Start(timers[i], channels[i]);
    }

    HAL_TIM_Base_Start(htim1);
    HAL_TIM_Base_Start(htim3);
    HAL_TIM_Base_Start(htim4);
}

static void adcToFloat(int16_t *pData) {
    static const float SCALAR_24V = ADC_RATIO * 43.743; // Measured experimentally

    voltage24V = ADCMean(pData, 0) * SCALAR_24V;
}

static void adcCallback(int16_t *pData, int noOfChannels, int noOfSamples) {
    if (!isUsbPortOpen()) {
        return;
    }

    if (bsGetField(BS_VERSION_ERROR_Msk)) {
        USBnprintf("0x%08" PRIx32 "\r\n", bsGetStatus());
        return;
    }

    adcToFloat(pData);
    updateStatus();

    int len = 0;
    for (uint32_t i = 0; i < LED_CHANNELS; i++) {
        CA_SNPRINTF(buf, len, "%06" PRIx32 ", ", rgbs[i]);
    }

    CA_SNPRINTF(buf, len, "0x%08" PRIx32 "\r\n", bsGetStatus());
    writeUSB(buf, len);
}

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

// Initialize board
void LightControllerInit(ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim1,
                         TIM_HandleTypeDef *htim2, TIM_HandleTypeDef *htim3,
                         TIM_HandleTypeDef *htim4) {
    initCAProtocol(&caProto, usbRx);
    (void)HAL_TIM_Base_Start(htim2);  // ADC timer
    ADCMonitorInit(hadc, ADCBuffer, sizeof(ADCBuffer) / sizeof(ADCBuffer[0]));

    /* Don't initialise any outputs or act on them if the board isn't correct */
    if (boardSetup(LightController, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR},
                   BS_SYSTEM_ERRORS_Msk) != 0) {
        return;
    }

    initPWMs(htim1, htim2, htim3, htim4);
}

// Main loop - Board only reacts on user inputs
void LightControllerLoop(const char *bootMsg) {
    CAhandleUserInputs(&caProto, bootMsg); // Always allow DFU upload
    ADCMonitorLoop(adcCallback);

    if (bsGetField(BS_VERSION_ERROR_Msk)) {
        return;
    }

    checkTimeOut();
    if (isInTest) {
        switchTestState();
    }
    updateLEDs();
}
