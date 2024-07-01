/*
 * LightController.c
 *
 *  Created on: Aug 12, 2022
 *      Author: matias
 */

#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <inttypes.h>

#include "LightController.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "systemInfo.h"
#include "USBprint.h"
#include "StmGpio.h"


static StmGpio Ch1_R;
static StmGpio Ch1_G;
static StmGpio Ch1_B;
static StmGpio Ch1_W;
static StmGpio Ch2_R;
static StmGpio Ch2_G;
static StmGpio Ch2_B;
static StmGpio Ch2_W;
static StmGpio Ch3_R;
static StmGpio Ch3_G;
static StmGpio Ch3_B;
static StmGpio Ch3_W;

static StmGpio* ChCtrl[12] = {&Ch1_R, &Ch1_G, &Ch1_B, &Ch1_W,
                              &Ch2_R, &Ch2_G, &Ch2_B, &Ch2_W,
                              &Ch3_R, &Ch3_G, &Ch3_B, &Ch3_W};

static unsigned int rgbwControl[LED_CHANNELS*NO_COLORS] = {0};
static unsigned int rgbs[LED_CHANNELS] = {0, 0, 0};

static TIM_HandleTypeDef* loopTimer = NULL;
static TIM_HandleTypeDef* ledUpdateTimer = NULL;
static WWDG_HandleTypeDef* hwwdg_ = NULL;

static void controlLEDStrip(const char *input);
static void LightControllerStatus();

// variables for keeping track of the time elapsed to control the LED test
int timerstart = 0;
int time_now = 0;
// states
typedef enum {
    OFF,
    RED,
    GREEN,
    BLUE,
    WHITE
}state;

bool teststate = false;
state partystate = OFF;


static CAProtocolCtx caProto =
{
        .undefined = controlLEDStrip,
        .printHeader = CAPrintHeader,
        .printStatus = LightControllerStatus,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL,
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL
};

static void LightControllerStatus()
{
     static char buf[600] = { 0 };
    int len = 0;

    for (int i = 0; i < LED_CHANNELS; i++)
    {
        len += snprintf(&buf[len], sizeof(buf) - len, "Port %d: On: %" PRIu32 "\r\n", 
                        i+1, (bsGetStatus() & LIGHT_PORT_STATUS_Msk(i)) >> i);
    }
    writeUSB(buf, len);
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

bool isInputValid(const char *input, int *channel, unsigned int *rgb)
{
    // If PARTY command is entered start the colour test
    // If any other input is entered stop it again
    if (strcmp(input, "PARTY") == 0) {
        teststate = true;
        return true;
    }
    else{
        // Reset the colours when disabling teststate
        if(teststate=true){
        int red = 0;
        int green = 0;
        int blue = 0;
        int whiteon = false;
        for (int channel = 0;channel < 3;channel++){
            updateLEDCtrl(channel,red,green,blue,whiteon);
        };
        }
        teststate = false;
    }


    
    if (sscanf(input, "p%d %x", channel, rgb) != 2)
        return false;
    
    if (*channel <= 0 || *channel > LED_CHANNELS)
        return false;

    // Check the RGB format is exactly 6 hex characters long
    char* idx = index((char*)input, ' ');
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

    uint8_t red = (*rgb >> 16) & 0xFF;
    if (red < 0 || red > MAX_PWM)
        return false;

    uint8_t green = (*rgb >> 8) & 0xFF;
    if (green < 0 || green > MAX_PWM)
        return false;

    uint8_t blue = *rgb & 0xFF;
    if (blue < 0 || blue > MAX_PWM)
        return false;

    return true;
}

static int handleInput(unsigned int rgb, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    *red = (rgb >> 16) & 0xFF;
    *green = (rgb >> 8) & 0xFF;
    *blue = rgb & 0xFF;
    return (rgb == 0xFFFFFF) ? 1 : 0; // If 0xFFFFFF turn on the white in other ports
}


// Update LED strip with user input colors
static void controlLEDStrip(const char *input)
{
    int port = 1;
    unsigned int rgb = 0x000000;

    // If the SW Version does not match,
    // do not allow user to control GPIOs.
    if (bsGetField(BS_VERSION_ERROR_Msk))
    {
        USBnprintf("SW mismatch. Rejecting MCU GPIO control.");
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

static void updateLEDs()
{
    // Interrupt speed of ledUpdateTimer is 20kHz.
    // For each 10kHz update the counter incremented until 256 and reset
    // Hence, the update speed of the led channels are 25kHz/256 ~= 98Hz.
    static int count = 0;
    for (int i = 0; i < 12; i++)
    {
        stmSetGpio(*ChCtrl[i], (count < rgbwControl[i]));
    }

    count = (count + 1 <= MAX_PWM) ? count + 1 : 0;
}

static void printStates()
{
    if (!isUsbPortOpen())
        return;

    if (bsGetField(BS_VERSION_ERROR_Msk))
    {
        USBnprintf("0x%x", bsGetStatus());
        return;
    }

    USBnprintf("%x, %x, %x, 0x%x", rgbs[0], rgbs[1], rgbs[2], bsGetStatus());
}

static void initGpio()
{
    stmGpioInit(&Ch1_R, Ch1_Ctrl_R_GPIO_Port, Ch1_Ctrl_R_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&Ch1_G, Ch1_Ctrl_G_GPIO_Port, Ch1_Ctrl_G_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&Ch1_B, Ch1_Ctrl_B_GPIO_Port, Ch1_Ctrl_B_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&Ch1_W, Ch1_Ctrl_W_GPIO_Port, Ch1_Ctrl_W_Pin, STM_GPIO_OUTPUT);

    stmGpioInit(&Ch2_R, Ch2_Ctrl_R_GPIO_Port, Ch2_Ctrl_R_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&Ch2_G, Ch2_Ctrl_G_GPIO_Port, Ch2_Ctrl_G_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&Ch2_B, Ch2_Ctrl_B_GPIO_Port, Ch2_Ctrl_B_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&Ch2_W, Ch2_Ctrl_W_GPIO_Port, Ch2_Ctrl_W_Pin, STM_GPIO_OUTPUT);

    stmGpioInit(&Ch3_R, Ch3_Ctrl_R_GPIO_Port, Ch3_Ctrl_R_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&Ch3_G, Ch3_Ctrl_G_GPIO_Port, Ch3_Ctrl_G_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&Ch3_B, Ch3_Ctrl_B_GPIO_Port, Ch3_Ctrl_B_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&Ch3_W, Ch3_Ctrl_W_GPIO_Port, Ch3_Ctrl_W_Pin, STM_GPIO_OUTPUT);

    for (int i = 0; i < LED_CHANNELS*NO_COLORS; i++)
    {
        stmSetGpio(*ChCtrl[i], false);
    }
}

bool controlLightTest()
{
    // If state is OFF switch it to RED and set all 3 channels to display red
    if (partystate == OFF){
        partystate = RED;
        int red = 0xFF;
        int green = 0;
        int blue = 0;
        int whiteon = false;
        for (int channel = 0;channel < 3;channel++){
            updateLEDCtrl(channel,red,green,blue,whiteon);
        };
        return true;
    }
    // If state is RED switch it to GREEN and set all 3 channels to display green
    else if (partystate == RED){
        partystate = GREEN;   
        int red = 0;
        int green = 0xFF;
        int blue = 0;
        int whiteon = false;
        for (int channel = 0;channel < 3;channel++){
            updateLEDCtrl(channel,red,green,blue,whiteon);
        };   
        return true;  
    }
    // If state is GREEN switch it to BLUE and set all 3 channels to display blue
    else if (partystate == GREEN){
        partystate = BLUE;
        int red = 0;
        int green = 0;
        int blue = 0xFF;
        int whiteon = false;
        for (int channel = 0;channel < 3;channel++){
            updateLEDCtrl(channel,red,green,blue,whiteon);
        };
        return true;
    }
    // If state is BLUE switch it to WHITE and set all 3 channels to display white
    else if (partystate == BLUE){
        partystate = WHITE;
    
        int red = 0;
        int green = 0;
        int blue = 0;
        int whiteon = true;
        for (int channel = 0;channel < 3;channel++){
            updateLEDCtrl(channel,red,green,blue,whiteon);
        };
        return true;
    }
    // If state is WHITE switch it to OFF and set all 3 channels to display nothing and turn off the test mode
    else if (partystate == WHITE){
        partystate = OFF;
        teststate = false;
        int red = 0;
        int green = 0;
        int blue = 0;
        int whiteon = false;
        for (int channel = 0;channel < 3;channel++){
            updateLEDCtrl(channel,red,green,blue,whiteon);
        };
        return true;
    }
    
    return true;
}

// Callback: timer has rolled over
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == ledUpdateTimer)
    {
        updateLEDs();
        return;
    }

    // loopTimer corresponds to htim5 which triggers with a frequency of 10Hz
    if (htim == loopTimer)
    {
        HAL_WWDG_Refresh(hwwdg_);
        printStates();
        return;
    }
}

// Initialize board
void LightControllerInit(TIM_HandleTypeDef *htim2, TIM_HandleTypeDef *htim5, WWDG_HandleTypeDef *hwwdg)
{
    boardSetup(LightController, (pcbVersion){1, 1});
    initCAProtocol(&caProto, usbRx);

    initGpio();

    HAL_TIM_Base_Start_IT(htim2);
    HAL_TIM_Base_Start_IT(htim5);

    ledUpdateTimer = htim2;
    loopTimer = htim5;

    hwwdg_ = hwwdg;
}

// Main loop - Board only reacts on user inputs
void LightControllerLoop(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);

    if (teststate == true){
    
    // Turn on every colour for 2 second during the test
        switch(partystate){  

            case OFF: 
                timerstart = HAL_GetTick();
                time_now = timerstart;
                controlLightTest();
                break;

            case RED:
                time_now = HAL_GetTick();
                if (time_now > (timerstart + 2000)){
                    controlLightTest();
                }
                break;
            

            case GREEN:
                time_now = HAL_GetTick();

                if (time_now > timerstart + 4000){
                    controlLightTest();
                }
                break;
            

            case BLUE:
                time_now = HAL_GetTick();

                if (time_now > timerstart + 6000){
                    controlLightTest();
                }
                break;
            

            case WHITE:
                time_now = HAL_GetTick();

                if (time_now > timerstart + 8000){
                    controlLightTest(); 
                }      
                break;             
        }
    }
}
