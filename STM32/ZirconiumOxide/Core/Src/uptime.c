#include <inttypes.h>
#include <string.h>

#include "uptime.h"
#include "time32.h"
#include "ZrO2Sensor.h"
#include "activeHeatCtrl.h"
#include "FLASH_readwrite.h"
#include "USBprint.h"
#include "systemInfo.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define UPDATE_INTERVAL_SESSION 60000        // Update states every minute (ms per minute)
#define UPDATE_INTERVAL_FLASH   86400000     // Store states in FLASH every day (ms per day)

/***************************************************************************************************
** PRIVATE FUNCTION PROTOTYPES
***************************************************************************************************/

static void loadUptime();
static void storeUptime();

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static CRC_HandleTypeDef* hcrc = NULL;
Uptime uptime = {0};

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

static void loadUptime()
{
    // Read in stored uptime after power cycling
    Uptime tmp = {0};
    if (readFromFlashCRC(hcrc, (uint32_t) FLASH_ADDR_UPTIME, (uint8_t*) &tmp, sizeof(tmp)) != 0)
    {
        return;
    }
    uptime = tmp;
}

static void storeUptime()
{
    if (writeToFlashCRC(hcrc, (uint32_t) FLASH_ADDR_UPTIME, (uint8_t *) &uptime, sizeof(uptime)) != 0)
    {
        USBnprintf("Uptime not stored in FLASH.");
    }
}

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

Uptime getUptime()
{
    return uptime;
}

void updateUptime()
{
    static uint32_t timestamp = 0;
    static uint32_t lastUpdate = 0;
    static uint32_t lastFlashUpdate = 0;

    timestamp = HAL_GetTick();
    if (tdiff_u32(timestamp, lastUpdate) >= UPDATE_INTERVAL_SESSION)
    {
        lastUpdate = timestamp;
        uptime.board_uptime++;
        uptime.high_sensor_uptime = (isSensorOn(0))     ? uptime.high_sensor_uptime + 1 : uptime.high_sensor_uptime;
        uptime.high_heater_uptime = (isSensorHeated(0)) ? uptime.high_heater_uptime + 1 : uptime.high_heater_uptime;
        uptime.low_sensor_uptime  = (isSensorOn(1))     ? uptime.low_sensor_uptime + 1 : uptime.low_sensor_uptime;
        uptime.low_heater_uptime  = (isSensorHeated(1)) ? uptime.low_heater_uptime + 1 : uptime.low_heater_uptime;
    }

    if (tdiff_u32(timestamp, lastFlashUpdate) >= (UPDATE_INTERVAL_FLASH - 1000))
    {
        bsSetField(BS_FLASH_ONGOING_Msk);
    }

    if (tdiff_u32(timestamp, lastFlashUpdate) >= UPDATE_INTERVAL_FLASH)
    {
        lastFlashUpdate = timestamp;
        storeUptime();
        bsClearField(BS_FLASH_ONGOING_Msk);
    }
}

void initUptime(CRC_HandleTypeDef* _hcrc)
{
    hcrc = _hcrc;
    loadUptime();
}