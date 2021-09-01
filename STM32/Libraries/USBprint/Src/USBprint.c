/*
 * USBprint.c
 * provides a function similar to the standard printf with capability to print data over USB
 *  Created on: Apr 26, 2021
 *      Author: Alexander.mizrahi@copenhagenatomics.com
 */


#include "USBprint.h"
#include "stdarg.h"
#include "string.h"
#include "stdio.h"
#include "usbd_cdc_if.h"

void USBprintf(const char* data, ...){
	va_list args;
	va_start(args, data);

	while(*data != '\0'){
		//if data index is a int
		if(*data == 'd'){
			int i = va_arg(args,uint32_t);
			char dataPrint[32] = {'\0'};
			snprintf(dataPrint, sizeof(dataPrint), "%d", i);
			CDC_Transmit_FS((uint8_t*)dataPrint, strlen(dataPrint));
			HAL_Delay(2); //working min delay is 2ms 
		}
		//if char
		else if(*data == 'c'){
			char s = va_arg(args,uint32_t);
			char dataPrint[sizeof(s)+1] = {'\0'}; //2+1 -> 2 for return and newline character 1 for null character added by snprintf
			snprintf(dataPrint, sizeof(dataPrint), "%c", s);
			CDC_Transmit_FS((uint8_t*)dataPrint, strlen(dataPrint));
			HAL_Delay(2);
		}
		//if float
		else if(*data == 'f'){
			double d = va_arg(args,double);
			char dataPrint[32] = {'\0'};
			snprintf(dataPrint, sizeof(dataPrint), "%.2f", d);
			CDC_Transmit_FS((uint8_t*)dataPrint, strlen(dataPrint));
			HAL_Delay(2);
		}
		//if string
		else if(*data == 's'){
			char* s = va_arg(args,char*);
			//limits the length of string to 100 characters
			char dataPrint[100+1] = {'\0'};
			snprintf(dataPrint, 100+2+1, "%s", s);
			CDC_Transmit_FS((uint8_t*)dataPrint, strlen(dataPrint));
			HAL_Delay(2);
		}
		else if(*data == 'X'){
			unsigned int X = va_arg(args,uint32_t);
			//limits the length of string to 100 characters
			char dataPrint[100+1] = {'\0'};
			snprintf(dataPrint, 100+1, "%X", X);
			CDC_Transmit_FS((uint8_t*)dataPrint, strlen(dataPrint));
			HAL_Delay(2);
		}
		data++;
	}

	char dataPrint[3] = {'\0'};
	snprintf(dataPrint, 3, "%c%c", '\r','\n');
	CDC_Transmit_FS((uint8_t*)dataPrint, strlen(dataPrint));
	HAL_Delay(2);
	va_end(args);
}

int USBnprintf(const char * format, ... )
{
    static char buffer[256];
    int len = snprintf(buffer, 3, "\r\n");

    va_list args;
    va_start (args, format);
    len += vsnprintf(&buffer[len], sizeof(buffer) - len, format, args);
    va_end (args);

    CDC_Transmit_FS((uint8_t*)buffer, len);
    HAL_Delay(2);

    return len;
}
