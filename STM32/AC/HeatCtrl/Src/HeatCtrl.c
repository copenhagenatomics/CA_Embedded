#include <time32.h>
#include "HeatCtrl.h"

#define MAX_DURATION ((uint32_t) -1)
#define MAX_TIMEOUT  60000 // Auto regulation time out from overheat prevention mode

typedef struct HeatCtrl
{
    // pwm data.
    uint32_t pwmBegin;   // Start of PWM period (for PWM generation)
    uint32_t pwmPeriod;  // Period (in mSec) for a full PWM signal
    uint8_t  pwmPercent; // Percentage of the PWM signal where is should be high.

    StmGpio *heater;
    StmGpio *button;

    // timeStamp begin of period. Used to safety shut off after a duration
    uint32_t periodBegin;
    uint32_t periodDuration;
} HeatCtrl;

static HeatCtrl heaters[MAX_NO_HEATERS];
static int noOfHeaters = 0;

HeatCtrl* heatCtrlAdd(StmGpio *heater, StmGpio * button)
{
    if (noOfHeaters >= MAX_NO_HEATERS)
        return NULL;

    HeatCtrl *ctx = &heaters[noOfHeaters];
    noOfHeaters++;

    ctx->periodDuration = 0;            // Default is off.
    ctx->pwmPeriod   = PWM_PERIOD_MS;   // default value, 1 seconds.
    ctx->pwmPercent  = 0;               // Default is off.
    ctx->pwmBegin    = 0;               // Default is off.

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
        if (tdiff > pCtrl->periodDuration)
        {
            // Turn of heater since duration is done.
            pCtrl->pwmPercent = 0;
            updateHeaterPhaseControl();
        }

        // Use modules '%' to get the on/off section in PWM period.
        // If percent is 0, heat shall be off (period is invalid)
        tdiff = tdiff_u32(now, pCtrl->pwmBegin);
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
        ctx->periodDuration = 0;
    }
    updateHeaterPhaseControl();
}

void allOn()
{
    for(HeatCtrl *ctx = heaters; ctx < &heaters[noOfHeaters]; ctx++)
    {
        ctx->pwmPercent = 100;
        ctx->periodDuration = MAX_DURATION;
    }
    updateHeaterPhaseControl();
}

void turnOffPin(int pin)
{
    if (pin >= 0 && pin < noOfHeaters)
    {
        HeatCtrl *ctx = &heaters[pin];
        ctx->pwmPercent = 0;
        ctx->periodDuration = 0;
        updateHeaterPhaseControl();
    }
}

void turnOnPin(int pin)
{
    if (pin >= 0 && pin < noOfHeaters)
    {
        HeatCtrl *ctx = &heaters[pin];
        ctx->pwmPercent = 100;
        ctx->periodDuration = MAX_DURATION;
        updateHeaterPhaseControl();
    }
}

void turnOnPinDuration(int pin, int duration_ms)
{
    if (pin >= 0 && pin < noOfHeaters)
    {
        HeatCtrl *ctx = &heaters[pin];
        ctx->pwmPercent = 100;
        ctx->periodDuration = (duration_ms >= 0) ? duration_ms : MAX_DURATION; // Negative value means forever.
        ctx->periodBegin = HAL_GetTick();
        updateHeaterPhaseControl();
    }
}

void setPWMPin(int pin, int pwmPct, int duration_ms)
{
    if (pin >= 0 && pin < noOfHeaters && pwmPct >= 0 && pwmPct <= 100)
    {
        HeatCtrl *ctx = &heaters[pin];
        ctx->pwmPercent = pwmPct;
        ctx->periodDuration = (duration_ms >= 0) ? duration_ms : MAX_DURATION; // Negative value means forever.
        ctx->periodBegin = HAL_GetTick();
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
            ctx->periodDuration = (ctx->periodDuration != MAX_DURATION) ? MAX_TIMEOUT : MAX_DURATION;

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
** channels) by modifying the "pwmBegin" variable for each heater
*/
void updateHeaterPhaseControl() 
{
    /* Start from one period ago, so everything is time aligned */
    const uint32_t pwmBegin = HAL_GetTick() - PWM_PERIOD_MS;
    uint32_t totalPeriod = 0;
    
    for(HeatCtrl *ctx = heaters; ctx < &heaters[noOfHeaters]; ctx++)
    {
        ctx->pwmBegin = pwmBegin + totalPeriod;

        /* If the totalPeriod reaches the end of the period, wrap it around to the beginning so 
        ** that none of the heaters are more than one second delayed in starting */
        totalPeriod += (ctx->pwmPercent * ctx->pwmPeriod) / 100;
        if(totalPeriod > PWM_PERIOD_MS)
        {
            totalPeriod -= PWM_PERIOD_MS;
        }
    }
}