#include <time32.h>
#include "HeatCtrl.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define NO_NEW_PCT   0xFFU

#define MAX_TIMEOUT  60000 // Auto regulation time out from overheat prevention mode

/***************************************************************************************************
** TYPEDEFS
***************************************************************************************************/

typedef struct HeatCtrl
{
    // pwm data.
    uint32_t pwmBegin;   // Start of PWM period (for PWM generation)
    uint32_t pwmPeriod;  // Period (in mSec) for a full PWM signal
    uint8_t  pwmPercent; // Percentage of the PWM signal where is should be high.
    uint8_t  pwmNextPct; // PWM percentage to set at the beginning of the next period.

    StmGpio *heater;

    // timeStamp begin of period. Used to safety shut off after a duration
    uint32_t periodBegin;
    uint32_t periodDuration;
} HeatCtrl;

/***************************************************************************************************
** PRIVATE VARIABLES
***************************************************************************************************/

static HeatCtrl heaters[MAX_NO_HEATERS];
static int noOfHeaters = 0;

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

void setPwmPercent(HeatCtrl* ctx, uint32_t pct);
void updateHeaterPhaseControl();

/***************************************************************************************************
** FUNCTION DEFINITIONS
***************************************************************************************************/

HeatCtrl* heatCtrlAdd(StmGpio *heater)
{
    if (noOfHeaters >= MAX_NO_HEATERS)
        return NULL;

    HeatCtrl *ctx = &heaters[noOfHeaters];
    noOfHeaters++;

    ctx->periodDuration = 0;            // Default is off.
    ctx->pwmPeriod   = PWM_PERIOD_MS;   // default value, 1 seconds.
    ctx->pwmPercent  = 0;               // Default is off.
    ctx->pwmBegin    = 0;               // Default is off.
    ctx->pwmNextPct  = NO_NEW_PCT;      // Default is no update.

    ctx->heater = heater;

    ctx->periodBegin = HAL_GetTick();
    return ctx;
}

void heaterLoop()
{
    static uint32_t prev = 0;
    uint32_t now = HAL_GetTick();

    /* If the PWM period has changed (the period is 1000 ms (ticks) long, and started at 0), then
    ** an update to the PWM percentage can take place, if there is one */
    bool newPeriod = (prev / 1000) != (now / 1000);
    prev = now;

    for(HeatCtrl *pCtrl = heaters; pCtrl < &heaters[noOfHeaters]; pCtrl++)
    {
        uint32_t tdiff = tdiff_u32(now, pCtrl->periodBegin);
        if (tdiff >= pCtrl->periodDuration)
        {
            // Turn off heater since duration is done.
            setPwmPercent(pCtrl, 0);

            /* Also prevent any pending changes from taking effect */
            pCtrl->pwmNextPct = NO_NEW_PCT;
        }

        /* Update the PWM percent if it is a new period */
        if (newPeriod && (pCtrl->pwmNextPct != NO_NEW_PCT))
        {
            setPwmPercent(pCtrl, pCtrl->pwmNextPct);
            pCtrl->pwmNextPct = NO_NEW_PCT;
        }

        /* If percent is 0, heat shall be off (period is invalid) */
        if (pCtrl->pwmPercent == 0)
        {
            pCtrl->heater->set(pCtrl->heater, false);
        }
        /* Otherwise, use modules '%' to get the on/off section in PWM period. */
        else
        {
            tdiff = tdiff_u32(now, pCtrl->pwmBegin);
            uint32_t periodOn = (pCtrl->pwmPercent * pCtrl->pwmPeriod) / 100;
            tdiff = tdiff % pCtrl->pwmPeriod;

            pCtrl->heater->set(pCtrl->heater, tdiff < periodOn);
        }
    }
}

// Interface functions
void allOff()
{
    for(int i = 0; i < noOfHeaters; i++)
    {
        turnOffPin(i);
    }
}

/*!
** @brief Turns on all ports for the specfied duration
**
** @param[in] duration_ms The length of time to keep all ports on, in ms
*/
void allOn(int duration_ms)
{
    for(int i = 0; i < noOfHeaters; i++)
    {
        turnOnPin(i, duration_ms);
    }
}

void turnOffPin(int pin)
{
    if (pin >= 0 && pin < noOfHeaters)
    {
        HeatCtrl *ctx = &heaters[pin];
        ctx->periodDuration = 0;
        setPwmPercent(ctx, 0);
    }
}

/*!
** @brief Turns on one port for the specfied duration
**
** @param[in] pin         The port to enable
** @param[in] duration_ms The length of time to keep all the port on, in ms
*/
void turnOnPin(int pin, int duration_ms)
{
    if (pin >= 0 && pin < noOfHeaters)
    {
        HeatCtrl *ctx = &heaters[pin];
        /* Negative values always interpreted as 0 - safety measure  */
        ctx->periodDuration = (duration_ms >= 0) ? duration_ms : 0;
        ctx->periodBegin = HAL_GetTick();
        setPwmPercent(ctx, 100);
    }
}

/*!
** @brief Turns on one port for the specfied duration with a PWM value
**
** @param[in] pin         The port to enable
** @param[in] pwmPct      The pwmPct to use
** @param[in] duration_ms The length of time to keep all the port on, in ms
**
** PWM has an effective resolution of 100 Hz, as the AC can only be enabled / disabled at 0 
** crossings. Setting a PWM always takes effect on the next PWM period, not immediately.
*/
void setPWMPin(int pin, int pwmPct, int duration_ms)
{
    if (pin >= 0 && pin < noOfHeaters && pwmPct >= 0 && pwmPct <= 100)
    {
        HeatCtrl *ctx = &heaters[pin];
        /* Negative values always interpreted as 0 - safety measure */
        ctx->periodDuration = (duration_ms >= 0) ? duration_ms : 0;
        ctx->periodBegin = HAL_GetTick();
        ctx->pwmNextPct = pwmPct;
    }
}

/*!
** @brief Reduces the PWM, and extends duration to maintain energy delivered
**
** Reduces the PWM by 1%, and extends duration of "on-time" (up to a maximum) by the inverse of the
** percentage drop. This means the board tries to keep the maximal attainable temperature.
** However, the board should ultimately go into safe mode by shutting off if no new commands are 
** received in case of loss of communication.
*/
void adjustPWMDown()
{
    for(HeatCtrl *ctx = heaters; ctx < &heaters[noOfHeaters]; ctx++)
    {
        if (ctx->pwmPercent > 1)
        {
            uint8_t new_pct = ctx->pwmPercent - 1;
            float duration_scaler = ((float) ctx->pwmPercent) / new_pct;

            /* Extend time on to match lost PWM pct, and keep equal energy delivered */
            ctx->periodDuration = duration_scaler * ctx->periodDuration;
            ctx->periodDuration = (ctx->periodDuration > MAX_TIMEOUT) ? MAX_TIMEOUT : ctx->periodDuration;
            setPwmPercent(ctx, new_pct);
        }
        else
        {
            ctx->periodDuration = 0;
            setPwmPercent(ctx, 0);
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

/***************************************************************************************************
** PRIVATE FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
** @brief Aligns phase control of all PWM'd heaters
**
** Aligns the start and end of the "on" pulse of all channels (excluding "always on" or "always off"
** channels) by modifying the "pwmBegin" variable for each heater
*/
void updateHeaterPhaseControl() 
{
    uint32_t totalPeriod = 0;
    
    for(HeatCtrl *ctx = heaters; ctx < &heaters[noOfHeaters]; ctx++)
    {
        ctx->pwmBegin = totalPeriod;

        /* If the totalPeriod reaches the end of the period, wrap it around to the beginning so 
        ** that none of the heaters are more than one second delayed in starting */
        totalPeriod += (ctx->pwmPercent * ctx->pwmPeriod) / 100;
        if(totalPeriod >= PWM_PERIOD_MS)
        {
            totalPeriod -= PWM_PERIOD_MS;
        }
    }
}

/*!
** @brief Sets the heaters pwmPercent member
**
** If the new percentage matches the old percentage, does not re-organise the PWM stacking.
**
** @param[inout] ctx Pointer to heater to modify
** @param[in]    pct New on-percent to apply
*/
void setPwmPercent(HeatCtrl* ctx, uint32_t pct)
{
    if(ctx->pwmPercent != pct)
    {
        ctx->pwmPercent = pct;
        updateHeaterPhaseControl();
    }
}