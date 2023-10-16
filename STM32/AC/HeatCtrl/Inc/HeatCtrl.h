#pragma once

#include <stdint.h>
#include "StmGpio.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define MAX_NO_HEATERS 4
#define PWM_PERIOD_MS  1000

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

// Loop handling
struct HeatCtrl* heatCtrlAdd(StmGpio *out, StmGpio * button);
void heaterLoop();

// Interface handling
void allOff();
void allOn();
void turnOffPin(int pin);
void turnOnPin(int pin);
void turnOnPinDuration(int pin, int duration);
void setPWMPin(int pin, int pwmPct, int duration);
void adjustPWMDown();
uint8_t getPWMPinPercent(int pin);