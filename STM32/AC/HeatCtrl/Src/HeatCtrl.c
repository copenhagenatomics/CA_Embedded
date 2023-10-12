#include <time32.h>
#include "HeatCtrl.h"

#define MAX_DURATION ((uint32_t) -1)
#define MAX_TIMEOUT  60000 // Auto regulation time out from overheat prevention mode

typedef struct HeatCtrl
{
    // pwm data.
    uint32_t pwmDuration;
    uint32_t pwmPeriod;  // Period (in mSec) for a full PWM signal
    uint8_t  pwmPercent; // Percentage of the PWM signal where is should be high.

    StmGpio *heater;
    StmGpio *button;

    // timeStamp begin of period. Used to safety shut off and PWM signal generation
    uint32_t periodBegin;
} HeatCtrl;

static HeatCtrl heaters[MAX_NO_HEATERS];
static int noOfHeaters = 0;

HeatCtrl* heatCtrlAdd(StmGpio *heater, StmGpio * button)
{
    if (noOfHeaters >= MAX_NO_HEATERS)
        return NULL;

    HeatCtrl *ctx = &heaters[noOfHeaters];
    noOfHeaters++;

    ctx->pwmDuration = 0;     // Default is off.
    ctx->pwmPeriod   = PWM_PERIOD_MS;  // default value, 1 seconds.
    ctx->pwmPercent  = 0;     // Default is off.

    ctx->heater = heater;
    ctx->button = button;

    ctx->periodBegin = HAL_GetTick();
    return ctx;
}

void heaterLoop()
{
    uint32_t now = HAL_GetTick();

    for(HeatCtrl *pCtrl = heaters; pCtrl < &heaters[noOfHeaters]; pCtrl++)
    {
        uint32_t tdiff = tdiff_u32(now, pCtrl->periodBegin);
        if (tdiff > pCtrl->pwmDuration)
        {
            // Turn of heater since duration is done.
            pCtrl->pwmPercent = 0;
        }

        // Use modules '%' to get the on/off section in PWM period.
        // If percent is 0, heat shall be off (period is invalid)
        if (pCtrl->pwmPercent == 0)
        {
            pCtrl->heater->set(pCtrl->heater, false);
        }
        else
        {
            uint32_t periodOn = (pCtrl->pwmPercent * pCtrl->pwmPeriod) / 100;
            tdiff = tdiff % pCtrl->pwmPeriod;

            pCtrl->heater->set(pCtrl->heater, tdiff <= periodOn);
        }
    }
}

// Interface functions
void allOff()
{
    for(HeatCtrl *ctx = heaters; ctx < &heaters[noOfHeaters]; ctx++)
    {
        ctx->pwmPercent = 0;
        ctx->pwmDuration = 0;
    }
    updateHeaterPhaseControl();
}

void allOn()
{
    for(HeatCtrl *ctx = heaters; ctx < &heaters[noOfHeaters]; ctx++)
    {
        ctx->pwmPercent = 100;
        ctx->pwmDuration = MAX_DURATION;
    }
    updateHeaterPhaseControl();
}

void turnOffPin(int pin)
{
    if (pin >= 0 && pin < noOfHeaters)
    {
        HeatCtrl *ctx = &heaters[pin];
        ctx->pwmPercent = 0;
        ctx->pwmDuration = 0;
        updateHeaterPhaseControl();
    }
}

void turnOnPin(int pin)
{
    if (pin >= 0 && pin < noOfHeaters)
    {
        HeatCtrl *ctx = &heaters[pin];
        ctx->pwmPercent = 100;
        ctx->pwmDuration = MAX_DURATION;
        updateHeaterPhaseControl();
    }
}

void turnOnPinDuration(int pin, int duration_ms)
{
    if (pin >= 0 && pin < noOfHeaters)
    {
        HeatCtrl *ctx = &heaters[pin];
        ctx->pwmPercent = 100;
        ctx->pwmDuration = (duration_ms >= 0) ? duration_ms : MAX_DURATION; // Negative value means forever.
        updateHeaterPhaseControl();
    }
}

void setPWMPin(int pin, int pwmPct, int duration_ms)
{
    if (pin >= 0 && pin < noOfHeaters && pwmPct >= 0 && pwmPct <= 100)
    {
        HeatCtrl *ctx = &heaters[pin];
        ctx->pwmPercent = pwmPct;
        ctx->pwmDuration = (duration_ms >= 0) ? duration_ms : MAX_DURATION; // Negative value means forever.
        updateHeaterPhaseControl();
    }
}

void adjustPWMDown()
{
    for(HeatCtrl *ctx = heaters; ctx < &heaters[noOfHeaters]; ctx++)
    {
        if (ctx->pwmPercent >= 1)
        {
            ctx->pwmPercent -= 1;
            // If the overheat prevention state has been enabled then extend the pwm duration
            // such that the board tries to keep the maximal attainable temperature
            // However, the board should ultimately go into safe mode by shutting off
            // if no new commands are received in case of loss of communication.
            ctx->pwmDuration = (ctx->pwmDuration != MAX_DURATION) ? MAX_TIMEOUT : MAX_DURATION;

            updateHeaterPhaseControl();
        }
    }
}

uint8_t getPWMPinPercent(int pin)
{
    if (pin >= 0 && pin < noOfHeaters)
    {
        HeatCtrl *ctx = &heaters[pin];
        return ctx->pwmPercent;
    }
    // In the case of passing -1 (i.e. targeting all ports) return 0
    // As there are only options for setting target to 100 or 0 for all
    // 0 is the always safe option.
    return 0;
}

/*!
** @brief Aligns phase control of all PWM'd heaters
**
** Aligns the start and end of the "on" pulse of all channels (excluding "always on" or "always off"
** channels) by modifying the "periodBegin" variable for each heater
*/
void updateHeaterPhaseControl() 
{
    uint32_t period_begin = HAL_GetTick();

    /* Starting at the end and working backwards means all the periodBegins are set BACK in time, 
    ** instead of forwards. This means the tdiff_u32() function will produce normal results */
    for(HeatCtrl *ctx = &heaters[noOfHeaters]; ctx >= heaters; ctx--)
    {
        period_begin -= (ctx->pwmPercent * ctx->pwmPeriod) / 100;
        ctx->periodBegin = period_begin;
    }
}