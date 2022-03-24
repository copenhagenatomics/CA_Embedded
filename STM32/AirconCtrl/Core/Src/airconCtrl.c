/*
 * airconCtrl.c
 *
 *  Created on: Mar 24, 2022
 *      Author: agp
 */
#include "airconCtrl.h"
#include "time32.h"
#include "USBprint.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "usb_cdc_fops.h"

static CAProtocolCtx caProto =
{
        .undefined = NULL,
        .printHeader = CAPrintHeader,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL, // TODO: change method for calibration?
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL
};

void airconCtrlInit()
{
	initCAProtocol(&caProto, usb_cdc_rx);
}

void airconCtrlLoop()
{
	static uint32_t tStamp = 0;

	inputCAProtocol(&caProto);

	if (tdiff_u32(HAL_GetTick(), tStamp) > 1000)
	{
		tStamp = HAL_GetTick();
		USBnprintf("Hello world\r\n");
	}
}

