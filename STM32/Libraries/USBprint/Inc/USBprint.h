/*
 * USBprint.h
 *
 *  Created on: Apr 26, 2021
 *      Author: alema
 */

#ifndef INC_USBPRINT_H_
#define INC_USBPRINT_H_

#include <unistd.h>
#if defined(STM32F401xC)
#include "stm32f4xx_hal.h"
#elif defined(STM32H753xx)
#include "stm32h7xx_hal.h"
#endif
#include "stdarg.h"

// Wrap vsnprintf(char *str, size_t size, const char *format, va_list ap)
// and send data to USB port. The n indicates that buffer overflow is handled.
// String/n is max 256 bytes.
int USBnprintf(const char * format, ... );

// Same interface ansi C write, same return values.
ssize_t writeUSB(const void *buf, size_t count);

// Return the number of bytes possible to write to buffer.
size_t txAvailable();

#endif /* INC_USBPRINT_H_ */
