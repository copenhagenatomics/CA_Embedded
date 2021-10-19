/*
 * Generic functions used the the STM system
 */

#include <stdio.h>

#include "stm32f4xx_hal.h"
#include "systemInfo.h"
#include "githash.h"

// F4xx UID
#define ID1 *((unsigned long *) (UID_BASE))
#define ID2 *((unsigned long *) (UID_BASE + 4U))
#define ID3 *((unsigned long *) (UID_BASE + 8U))

const char* systemInfo(const char* productType, const char* mcuFamily, const char* pcbVersion)
{
    static char buf[400] = { 0 };
    int len = 0;

    len  = snprintf(&buf[len], sizeof(buf) - len, "Serial Number: %lX%lX%lX\r\n", ID1, ID2, ID3);
    len += snprintf(&buf[len], sizeof(buf) - len, "Product Type: %s\r\n", productType);
    len += snprintf(&buf[len], sizeof(buf) - len, "MCU Family: %s\r\n", mcuFamily);
    len += snprintf(&buf[len], sizeof(buf) - len, "Software Version: %s\r\n", GIT_VERSION);
    len += snprintf(&buf[len], sizeof(buf) - len, "Compile Date: %s\r\n", GIT_DATE);
    len += snprintf(&buf[len], sizeof(buf) - len, "Git SHA %s\r\n", GIT_SHA);
    len += snprintf(&buf[len], sizeof(buf) - len, "PCB Version: %s", pcbVersion);

    return buf;
}
