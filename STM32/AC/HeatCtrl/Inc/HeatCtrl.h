#pragma once

#include <stdint.h>
#include "StmGpio.h"

#define MAX_NO_HEATERS 4

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
