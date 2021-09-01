/*
 * F401_UID.c
 *
 *  Created on: May 3, 2021
 *      Author: alema
 * 
 * Declare SoftwareVersion, ProductType, mcuFamily, pcbVersion as #define(s) at top of main.c (before #include F4xx_UID.h).
 */

//-------------F401--------------------
#define ID1 (*(unsigned long *)0x1FFF7A10)
#define ID2 (*(unsigned long *)0x1FFF7A14)
#define ID3 (*(unsigned long *)0x1FFF7A18)

#include "..\Inc\F4xx_UID.h"

//char softwareVersion[];
//char productType[];
//char mcuFamily[];
//char pcbVersion[];
char compileDate[] = __DATE__  " " __TIME__;

void printHeader() {

	USBprintf("sXXX", "Serial Number: ", ID1, ID2, ID3);

	USBprintf("ss", "Product Type: ", productType);

	USBprintf("ss", "Software Version: ", softwareVersion);

	USBprintf("ss", "Compile Date: ", compileDate);

	USBprintf("ss", "MCU Family: ", mcuFamily);

	USBprintf("ss", "PCB Version: ", pcbVersion);
}
