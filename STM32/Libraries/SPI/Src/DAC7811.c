/*
 * DAC7811.c
 *
 *  Created on: Feb 23, 2023
 *      Author: matias
 */

#include "DAC7811.h"
#include "USBprint.h"

#define DAC_TIMEOUT	   10

// Ctrl bit configurations
#define LoadUpdate    0x1
#define ReadBack      0x2
#define DisableDC	  0x9	// Disable Daisy-Chain setup
#define ResetZero     0xB
#define ResetMidScale 0xC

#define MID_SCALE	  0x07FF

// Helper union class for communicating with DAC.
typedef union DAC7811
{
    struct {
    	struct {
			uint8_t dataLsb    : 8;
		};
    	struct {
			uint8_t dataMsb    : 4;
			uint8_t ctrlbits   : 4;
		};
    }; // anonymous struct.
    uint8_t data[2];   // Data to be sent to DAC
    uint16_t fullmessage;
} DAC7811Cmd;

HAL_StatusTypeDef receiveData(DAC7811Device *dev, uint8_t* rxBuf)
{
	if (HAL_SPI_Receive(dev->hspi, rxBuf, 1, DAC_TIMEOUT) != HAL_OK)
		return HAL_ERROR;
	return HAL_OK;
}

HAL_StatusTypeDef transmitData(DAC7811Device *dev, uint8_t* txBuf)
{
	if (HAL_SPI_Transmit(dev->hspi, txBuf, 1, DAC_TIMEOUT) != HAL_OK)
		return HAL_ERROR;
	return HAL_OK;
}

HAL_StatusTypeDef loadAndUpdate(DAC7811Device *dev, uint16_t data)
{
	DAC7811Cmd cmd = { .ctrlbits = LoadUpdate, .dataMsb = (uint8_t) ((data >> 8) & 0x0f), .dataLsb = (data & 0xFF)};

	if (transmitData(dev, cmd.data) != HAL_OK)
		return HAL_ERROR;
	return HAL_OK;
}

HAL_StatusTypeDef readOutputValue(DAC7811Device *dev)
{
	DAC7811Cmd rxData = { .ctrlbits = ReadBack, .dataMsb = 0, .dataLsb = 0 };

	if (receiveData(dev, rxData.data) != HAL_OK)
		return HAL_ERROR;

	dev->outputValue = ( (uint16_t) (rxData.dataMsb << 8) | rxData.dataLsb) & 0x0FFF;
	return HAL_OK;
}


HAL_StatusTypeDef resetMidScale(DAC7811Device *dev)
{
	DAC7811Cmd cmd = { .ctrlbits = ResetMidScale, .dataMsb = 0, .dataLsb = 0 };

	if (transmitData(dev, cmd.data) != HAL_OK)
		return HAL_ERROR;
	return HAL_OK;
}

HAL_StatusTypeDef disableDaisyChain(DAC7811Device *dev)
{
	DAC7811Cmd cmd = { .ctrlbits = DisableDC, .dataMsb = 0, .dataLsb = 0 };

	if (transmitData(dev, cmd.data) != HAL_OK)
		return HAL_ERROR;
	return HAL_OK;
}

int DAC7811Init(DAC7811Device *dev)
{
	int ret = 0;

	stmSetGpio(dev->cs, false);
	if (disableDaisyChain(dev) != HAL_OK)
		ret = 1;
	stmSetGpio(dev->cs, true);

    // Low CS pin activates DAC
	stmSetGpio(dev->cs, false);
	if (loadAndUpdate(dev, 2047) != HAL_OK)
		ret = 1;
	stmSetGpio(dev->cs, true);

	return ret;
}
