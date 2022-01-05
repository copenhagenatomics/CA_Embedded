/*
 * Generic functions used the the STM system
 */

#include <stdio.h>

#include "stm32f4xx_hal.h"
#include "systemInfo.h"
#include "githash.h"
#include "HAL_otp.h"

// F4xx UID
#define ID1 *((unsigned long *) (UID_BASE))
#define ID2 *((unsigned long *) (UID_BASE + 4U))
#define ID3 *((unsigned long *) (UID_BASE + 8U))

static const char* mcuType()
{
    static char mcu[50] = { 0 }; // static to prevent allocate on stack.
    int len = 0;

    const DBGMCU_TypeDef* mcuType = DBGMCU;
    const uint16_t idCode = 0x00000FFF & mcuType->IDCODE;
    const uint16_t revCode = 0xFFFF & (mcuType->IDCODE >> 16);

    switch(idCode)
    {
    case 0x423: len += snprintf(&mcu[len], sizeof(mcu) -len, "STM32F401xB/C");        break;
    case 0x433: len += snprintf(&mcu[len], sizeof(mcu) -len, "STM32F401xD/E");        break;
    default:    len += snprintf(&mcu[len], sizeof(mcu) -len, "Unknown 0x%3X", idCode); break;
    }

    switch (revCode)
    {
    case 0x1000: len += snprintf(&mcu[len], sizeof(mcu) -len, " Rev A"); break;
    case 0x1001: len += snprintf(&mcu[len], sizeof(mcu) -len, " Rev Z"); break;
    default:     len += snprintf(&mcu[len], sizeof(mcu) -len, " Rev %4x", revCode); break;
    }

    return mcu;
}

static char* productType(uint8_t id)
{
    switch(id)
    {
    case 1:  return "AC Board";          break;
    case 2:  return "DC Board";          break;
    case 3:  return "Temperature";       break;
    case 4:  return "Current";           break;
    case 5:  return "FlowChip";          break;
    case 6:  return "humidityChip";      break;
    case 7:  return "Pressure";          break;
    case 8:  return "Salt Flow Board";   break;
    case 9:  return "SaltLeak";          break;
    case 10: return "HotValvePumpBoard"; break;
    }
    return "NA";
}

const char* systemInfo()
{
    static char buf[400] = { 0 };
    BoardInfo info = { 0 };

    if (HAL_otpRead(&info) != OTP_SUCCESS)
    {
        info.v1.boardType = 0;
        info.v1.pcbVersion.major = 0;
        info.v1.pcbVersion.minor = 0;
        info.v1.otpVersion = 0;
    }

    int len = 0;
    len  = snprintf(&buf[len], sizeof(buf) - len, "Serial Number: %lX%lX%lX\r\n", ID1, ID2, ID3);
    len += snprintf(&buf[len], sizeof(buf) - len, "Product Type: %s\r\n", productType(info.v1.boardType));
    len += snprintf(&buf[len], sizeof(buf) - len, "MCU Family: %s\r\n", mcuType());
    len += snprintf(&buf[len], sizeof(buf) - len, "Software Version: %s\r\n", GIT_VERSION);
    len += snprintf(&buf[len], sizeof(buf) - len, "Compile Date: %s\r\n", GIT_DATE);
    len += snprintf(&buf[len], sizeof(buf) - len, "Git SHA %s\r\n", GIT_SHA);
    len += snprintf(&buf[len], sizeof(buf) - len, "PCB Version: %d.%d", info.v1.pcbVersion.major, info.v1.pcbVersion.minor);

    return buf;
}
