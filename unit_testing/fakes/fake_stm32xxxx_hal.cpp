#include <chrono>

#include "fake_stm32xxxx_hal.h"

using namespace std::chrono;

/***************************************************************************************************
** PRIVATE MEMBERS
***************************************************************************************************/

static bool force_tick = false;
static uint32_t next_tick = 0;

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void forceTick(uint32_t next_val)
{
    force_tick = true;
    next_tick = next_val;
}

uint32_t HAL_GetTick(void) 
{
    if(!force_tick)
    {
        return duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
    }
    else 
    {
        force_tick = false;
        return next_tick;
    }
}