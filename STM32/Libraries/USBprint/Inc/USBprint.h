/*
 * USBprint.h
 *
 *  Created on: Apr 26, 2021
 *      Author: alema
 */

#ifndef INC_USBPRINT_H_
#define INC_USBPRINT_H_

#include "stm32f4xx_hal.h"
#include "stdarg.h"

void USBprintf(const char* data, ...);

// Wrap vsnprintf(char *str, size_t size, const char *format, va_list ap)
// and send data to SB port. The n indicates that buffer overflow is handled.
// String/n is max 256 bytes.
int USBnprintf(const char * format, ... );

#endif /* INC_USBPRINT_H_ */
