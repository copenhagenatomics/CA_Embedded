/*
 * handleGenericMessages.h
 *
 *  Created on: May 3, 2021
 *      Author: alema
 */

#ifndef INC_HANDLEGENERICMESSAGES_H_
#define INC_HANDLEGENERICMESSAGES_H_



#endif /* INC_HANDLEGENERICMESSAGES_H_ */

#include "stm32f4xx_hal.h"
#include "string.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "jumpToBootloader.h"
#include "USBprint.h"

extern void printHeader();
void handleGenericMessages(char * inputBuffer);
