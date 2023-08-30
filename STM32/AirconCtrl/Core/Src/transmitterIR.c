/*
 * transmitterIR.c
 *
 *  Created on: Jun 28, 2022
 *      Author: matias
 */

/***************************************************************************************************
** INCLUDES
***************************************************************************************************/

#include <stdbool.h>

#include "transmitterIR.h"
#include "StmGpio.h"
#include "main.h"
#include "USBprint.h"

/***************************************************************************************************
** STATIC FUNCTION PROTOTYPES
***************************************************************************************************/

static bool startSendingTempUpdate(int temp, bool isNewController);
static void sendCommand();
static void setupSignalTimer(uint32_t period, uint32_t interval);
static void setupStartBit();
static void setupHighBit();
static void setupLowBit();

/***************************************************************************************************
** STATIC VARIABLES
***************************************************************************************************/

static TIM_HandleTypeDef* timFreqCarrier = NULL;
static TIM_HandleTypeDef* timSignal = NULL;

static int currentTemp = 0;

/*!
** @brief Set of flags for controlling sending of IR message
*/
static struct {
    uint32_t cmdTimeStamp;

    /* Used to indicate how many bits will be sent */
    uint8_t len_bits;

    /* Used to indicate a command is ready to be sent */
    uint8_t isCommandIssued: 1;

    /* Indicates the start bit has been sent */
    uint8_t isAddressSent: 1;

    /* Used to indicate that the body of the command is ready to be sent */
    uint8_t isCommReady: 1;

    /* Used to indicate which controller version is being used */
    uint8_t isNewController: 1;

    /* Used to indicate a temperature or off message */
    uint8_t isOffMessage: 1;
} commandState = {0, 0, false, false, false, false, false};

/*!
** @brief Holds the main body of the command
*/
static union IRCommand {
    struct {
        uint32_t address;
        uint32_t tempData;
        uint32_t miscStates;
        uint32_t checksum;
    }; // struct used for altering data
    uint32_t command[4]; // command used for sending
    uint16_t command_u16[8]; /* Alternative mode of addressing command for AC2 */
} IRCommand= {.address = IR_ADDRESS, .tempData = TEMP_18, .miscStates = FAN_HIGH, .checksum = CRC18};

static uint32_t tempCodes[2][8] = 
{
    {TEMP_18, TEMP_19, TEMP_20, TEMP_21, TEMP_22, TEMP_23, TEMP_24, TEMP_25},
    {AC2_TEMP_18, AC2_TEMP_19, AC2_TEMP_20, AC2_TEMP_21, AC2_TEMP_22, AC2_TEMP_23, AC2_TEMP_24, AC2_TEMP_25},
};
static uint32_t crcCodes[8] = {CRC18, CRC19, CRC20, CRC21, CRC22, CRC23, CRC24, CRC25};

/***************************************************************************************************
** STATIC FUNCTIONS
***************************************************************************************************/

/*!
** @brief Sets up the signal timer period and interval
**
** @param period   The period (in counts) that the timer should run for
** @param interval The interval at which the timer should switch from on to off
*/
static void setupSignalTimer(uint32_t period, uint32_t interval)
{
    TIM3->ARR = period;
    TIM3->CCR2 = interval;
}

/*!
** @brief Sets up the signal timer for the start bit
**
** Changes the length of the start bit, depending on what controller version is being requested
*/
static void setupStartBit()
{
    uint32_t period   = !commandState.isNewController ? START_BIT_ARR : START_BIT_ARR_AC2;
    uint32_t interval = !commandState.isNewController ? START_BIT_CCR : START_BIT_CCR_AC2;
    setupSignalTimer(period, interval);
}

/*!
** @brief Sets up the signal timer for the high bit
**
** Changes the length of the high bit, depending on what controller version is being requested
*/
static void setupHighBit()
{
    uint32_t period   = !commandState.isNewController ? HIGH_BIT_ARR : HIGH_BIT_ARR_AC2;
    uint32_t interval = !commandState.isNewController ? HIGH_BIT_CCR : HIGH_BIT_CCR_AC2;
    setupSignalTimer(period, interval);
}

/*!
** @brief Sets up the signal timer for the low bit
**
** Changes the length of the low bit, depending on what controller version is being requested
*/
static void setupLowBit()
{
    uint32_t period   = !commandState.isNewController ? LOW_BIT_ARR : LOW_BIT_ARR_AC2;
    uint32_t interval = !commandState.isNewController ? LOW_BIT_CCR : LOW_BIT_CCR_AC2;
    setupSignalTimer(period, interval);
}

/*!
** @brief Sets up the timer to transmit the next bit
*/
static void sendCommand()
{
    static int sendBitIdx = 0;
    static int wordIdx = 0;
    static int numRepeats = 0;

    if (!commandState.isAddressSent)
    {
        commandState.isAddressSent = true;
        setupStartBit();
        return;
    }

    /* End of packet */
    int bits = wordIdx * 32 + sendBitIdx;
    if (bits >= commandState.len_bits)
    {
        turnOffLED();

        // Repeat command 'NUM_COMMAND_REPEATS' times to minimize risk of command
        // not being received correctly
        if (!commandState.isNewController) 
        {
            setupSignalTimer(START_BIT_ARR, 0);

            if (numRepeats++ < NUM_COMMAND_REPEATS) 
            {
                startSendingTempUpdate(currentTemp, false);
            }
            else
            {
                numRepeats = 0;
                startSendingTempUpdate(currentTemp, true);
            }
        }
        else
        {
            setupSignalTimer(START_BIT_ARR_AC2, 0);

            /* For the new controller, each packet consists of the same message sent twice */
            if (numRepeats++ < (2 * NUM_COMMAND_REPEATS))
            {
                /* Hackity hack: to prevent the double packets being hampered by the minimum send 
                ** time, (see HAL_TIM_PWM_PulseFinishedCallback()), take a copy of the original time
                ** stamp here and re-apply it if necessary */
                uint32_t tmpTimestamp = commandState.cmdTimeStamp;
                startSendingTempUpdate(currentTemp, true);
                if (numRepeats % 2) /* e.g. odd numbers */
                {
                    commandState.cmdTimeStamp = tmpTimestamp;

                    /* In between "double packets" a shorter rest period is observed on the second 
                    ** AC controller */
                    setupSignalTimer(START_BIT_REST_AC2, 0);
                }
            }
            else
            {
                /* Finishes the transmission */
                numRepeats = 0;
                commandState.isCommandIssued = false;
            }
        }

        wordIdx = 0;
        sendBitIdx = 0;
        return;
    }

    // After overload of counter the new PWM settings will be set 
    // that codes for a logical 1 and 0 respectively.
    IRCommand.command[wordIdx] & (1 << (31-sendBitIdx)) ? setupHighBit() : setupLowBit();

    if (++sendBitIdx % 32 == 0)
    {
        sendBitIdx = 0;
        wordIdx++;
    }
}

/*!
** @brief Updates message packet with temperature value and prepares to send
*/
static bool startSendingTempUpdate(int temp, bool isNewController)
{
    commandState.isNewController = isNewController;

    if (!commandState.isNewController) 
    {
        switch(temp)
        {
            case 0:     IRCommand.tempData = AC_OFF;
                        IRCommand.miscStates = FAN_HIGH;
                        IRCommand.checksum = CRC_OFF;
                        break;
            case 5:     IRCommand.tempData = TEMP_5;
                        IRCommand.checksum = CRC5;
                        IRCommand.miscStates = FAN_HIGH_5;
                        break;
            case 30:    IRCommand.tempData = TEMP_30;
                        IRCommand.checksum = CRC30;
                        IRCommand.miscStates = FAN_HIGH;
                        break;
            default:    IRCommand.tempData = tempCodes[0][temp-18];
                        IRCommand.checksum = crcCodes[temp-18];
                        IRCommand.miscStates = FAN_HIGH;
                        break;
        }
        commandState.len_bits = MSG_LEN_BITS;
    }
    else
    {
        switch(temp)
        {
            case 0:     IRCommand.command_u16[1] = AC2_OFF;
                        IRCommand.command_u16[2] = AC2_OFF_SWING_END;
                        break;
            case 5:     return false;
            case 30:    IRCommand.command_u16[1] = AC2_FAN_MODE_2;
                        IRCommand.command_u16[2] = AC2_TEMP_30;
                        break;
            default:    IRCommand.command_u16[1] = AC2_FAN_MODE_2;
                        IRCommand.command_u16[2] = (uint16_t) tempCodes[1][temp-18];
                        break;
        }
        IRCommand.command_u16[0] = AC2_TEMP_COMMAND;
        commandState.len_bits = AC2_MSG_LEN_BITS;
    }

    commandState.isCommandIssued = true;
    commandState.isAddressSent = false;
    commandState.isCommReady = false;
    commandState.cmdTimeStamp = HAL_GetTick();
    currentTemp = temp;

    return true;
}

/***************************************************************************************************
** PUBLIC
***************************************************************************************************/

/*!
** @brief Turns the LED PWM on or off, depending on how far through the signal timer the count is.
**
** This function must be called as frequently as possible to maintain accurate timing.
*/
void pwmGPIO()
{
    if (!commandState.isCommReady)
    {
        return;
    }

    TIM3->CNT < TIM3->CCR2 ? turnOnLED() : turnOffLED();
}

/*!
** @brief Returns the current temperature
*/
void getACStates(int * tempState)
{
    *tempState = currentTemp;
}

/*!
** @brief Starts sending a temperature update with the first controller version
*/
bool updateTemperatureIR(int temp)
{
    /* Guarding against invalid temperatures */
    if ((temp < 18 || temp > 25) && temp != 5 && temp != 30)
    {
        return false;
    }

    return startSendingTempUpdate(temp, false);
}

/*!
** @brief Starts sending the commands to turn off the aircon
*/
void turnOffAC()
{
    startSendingTempUpdate(0, false);
}

/*!
** @brief Starts the PWM for the IR LED
*/
void turnOnLED()
{
    HAL_TIM_PWM_Start(timFreqCarrier, TIM_CHANNEL_1);
}

/*!
** @brief Stops the PWM for the IR LED
*/
void turnOffLED()
{
    HAL_TIM_PWM_Stop(timFreqCarrier, TIM_CHANNEL_1);
}

/*!
** @brief Callback called when timer rolls over
**
** Runs the main state machine of the module:
** <img src="aircon_ctrl_ir_states.svg"/>
*/
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (!commandState.isCommandIssued || (HAL_GetTick() - commandState.cmdTimeStamp) < 100)
    {
        return;
    }

    if (commandState.isAddressSent && !commandState.isCommReady)
    {
        commandState.isCommReady = true;
        return;
    }

    if (htim == timSignal)
    {
        sendCommand();
    }
}

/*!
** @brief Initisalise IR transmitter by starting timer
*/
void initTransmitterIR(TIM_HandleTypeDef *timFreqCarrier_, TIM_HandleTypeDef *timSignal_)
{
    HAL_TIM_PWM_Start_IT(timSignal_, TIM_CHANNEL_2);
    timSignal = timSignal_;
    timFreqCarrier = timFreqCarrier_;
}

