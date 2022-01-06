/*
 * inputValidation.c
 *
 *  Created on: Aug 26, 2021
 *      Author: matias
 */
#include <stdbool.h>
#include "string.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "inputValidation.h"

int maxPWM = 999;

static int getNumberDigits(int inputNumber)
{
    int numberOfDigits = 0;

    if (inputNumber == 0)
    {
        return 1;
    }

    while (inputNumber != 0)
    {
        inputNumber = inputNumber / 10;
        numberOfDigits++;
    }
    return numberOfDigits;
}

static bool isInputInt(char *inputBuffer, int startIdx, int endIdx)
{
    int idx = startIdx;
    while (idx <= endIdx)
    {
        if (!isdigit((int )inputBuffer[idx]))
        {
            if (inputBuffer[idx] == ' ' && idx != startIdx)
            {
                break;
            }
            return false;
        }
        idx++;
    }
    return true;
}

struct actuationInfo parseAndValidateInput(char *inputBuffer)
{
    /*
     * Valid commands are:

     all on - turn all ports on indefinitely
     all off - turn all ports off
     pX on - turn off port number X
     pX off - turn on port number X indefinitely 'always on'
     pX on YY - turn on port number X for YY seconds
     pX on ZZZ% - turn on port number X on ZZ percent of the time using PWM 'always on'
     pX on YY ZZZ% - turn on port number X for YY seconds ZZ percent of the time using PWM

     Any other command
     */
    size_t len = strlen(inputBuffer);

    struct actuationInfo info;
    info.isInputValid = false;

    // all on command
    if (strstr(inputBuffer, "all off") != NULL)
    {
        info.pin = -1;
        info.pwmDutyCycle = 0;
        info.timeOn = -1;
        info.isInputValid = true;
        return info;
    }

    // all off command
    if (strstr(inputBuffer, "all on") != NULL)
    {
        info.pin = -1;
        info.pwmDutyCycle = 100;
        info.timeOn = -1;
        info.isInputValid = true;
        return info;
    }

    // All other valid formats starts with 'pX '
    if (inputBuffer[0] != 'p' || inputBuffer[2] != ' ')
    {
        return info;
    }

    // Check if X in pX is int and valid
    if (!isdigit((int )inputBuffer[1]) || atoi(&inputBuffer[1]) > 4
            || atoi(&inputBuffer[1]) == 0)
    {
        return info;
    }
    else
    {
        info.pin = atoi(&inputBuffer[1]) - 1; // User uses 1-indexed ports
    }

    // 'pX on' command
    if (len == 5 && strstr(inputBuffer, "on") != NULL)
    {
        info.pwmDutyCycle = 100;
        info.timeOn = -1;
        info.isInputValid = true;
        return info;
    }

    // 'pX off' command
    if (len == 6 && strstr(inputBuffer, "off") != NULL)
    {
        info.pwmDutyCycle = 0;
        info.timeOn = -1;
        info.isInputValid = true;
        return info;
    }

    // All other valid formats has ' ' at index 5 and contains keyword "on"
    if (inputBuffer[5] != ' ' && strstr(inputBuffer, "on") != NULL)
    {
        return info;
    }

    // 'pX on YY' command
    if (len <= 8 && inputBuffer[len - 1] != '%')
    {

        // Check all characters from index 6 an onwards are int.
        if (!isInputInt(inputBuffer, 6, len - 1))
        {
            return info;
        }
        // Save user specified actuation time of pin
        char timeOn[3] = { 0 };
        memcpy(timeOn, &inputBuffer[6], len - 6 + 1);

        info.pwmDutyCycle = 100;
        //info.timeOn = (int)strtol(*timeOn,(char **)NULL, 10) * 1000;
        info.timeOn = atoi(timeOn) * 1000;
        info.isInputValid = true;
        return info;
    }

    // All other valid commands ends with '%'
    if (inputBuffer[len - 1] != '%')
    {
        return info;
    }

    // 'pX on ZZZ%'
    if (strlen(inputBuffer) <= 10)
    {
        // Check all characters from index 6 an onwards are int.
        if (!isInputInt(inputBuffer, 6, len - 2))
        {
            return info;
        }

        // Save user specified pwm of pin
        char pwmPct[4] = { 0 };
        memcpy(pwmPct, &inputBuffer[6], len - 2 - 5);
        int pwmPctTmp = atoi(pwmPct);
        int pwmDutyCycle = (int) (pwmPctTmp / 100.0f * maxPWM);

        info.pwmDutyCycle = pwmDutyCycle;
        info.timeOn = -1;
        info.isInputValid = true;
        return info;
    }

    // 'pX on YY ZZZ%'
    if (strlen(inputBuffer) <= 13)
    {

        // Check all characters from index 6 an onwards are int.
        if (!isInputInt(inputBuffer, 6, 7))
        {
            return info;
        }

        // Save user specified actuation time of pin
        char timeOn[3] = { 0 };
        memcpy(timeOn, &inputBuffer[6], 2);
        info.timeOn = atoi(timeOn) * 1000;

        int lenTimeOn = getNumberDigits(info.timeOn / 1000);

        // Check all characters from % start and onwards are int.
        if (!isInputInt(inputBuffer, 6 + lenTimeOn + 1, len - 2))
        {
            return info;
        }

        // Save user specified pwm of pin
        char pwmPct[4] = { 0 };
        memcpy(pwmPct, &inputBuffer[6 + lenTimeOn + 1],
                len - 2 - lenTimeOn - 5);
        int pwmPctTmp = atoi(pwmPct);

        if (pwmPctTmp > 100)
        {
            return info;
        }

        int pwmDutyCycle = (int) (pwmPctTmp / 100.0f * maxPWM);

        info.pwmDutyCycle = pwmDutyCycle;
        info.isInputValid = true;
        return info;
    }
    return info;
}

