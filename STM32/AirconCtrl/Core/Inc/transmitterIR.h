/*
 * transmitterIR.h
 *
 *  Created on: Jun 28, 2022
 *      Author: matias
 */

#ifndef INC_TRANSMITTERIR_H_
#define INC_TRANSMITTERIR_H_

#include "stm32f4xx_hal.h"

#define IR_ADDRESS 0xc4d36480

#define TEMP_30		0x00240070
#define TEMP_29		0x002400b0
#define TEMP_28		0x00240030
#define TEMP_27		0x002400d0
#define TEMP_26		0x00240050
#define TEMP_25		0x00240090
#define TEMP_24		0x00240010
#define TEMP_23		0x002400e0
#define TEMP_22		0x00240060
#define TEMP_21		0x002400a0
#define TEMP_20		0x00240020
#define TEMP_19		0x002400c0
#define TEMP_18		0x00240040
#define TEMP_17		0x00240080
#define TEMP_16		0x00240000

#define CRC25	0xe20000
#define CRC24	0x620000
#define CRC23	0xa20000
#define CRC22	0x220000
#define CRC21	0xc20000
#define CRC20	0x420000
#define CRC19	0x820000
#define CRC18	0x20000

#define FAN_LOW		0x40000000
#define FAN_MID		0xc0000000
#define FAN_HIGH	0xa0000000
#define FAN_MASK 	0xf0000000

#define FAN_C		0x00000010
#define OUT_C		0x00000020
#define BLOWER		0x00000040
#define BLOWER_MASK 0x000000f0

#define START_BIT_ARR	37601	// Period of 4700us (~213Hz) --- NOTE prescaler for this setting
#define START_BIT_CCR	26400	// On time (70%)

#define HIGH_BIT_ARR	13920	// Period of 1740us (~575Hz)
#define HIGH_BIT_CCR	4315 	// On time (31%)

#define LOW_BIT_ARR		7042 	// Period of 880us (~1136Hz)
#define LOW_BIT_CCR		4296    // On time (61%)


void updateTemperatureIR(int temp);
void turnOnLED();
void turnOffLED();
void pwmGPIO();
void initTransmitterIR(TIM_HandleTypeDef *timFreqCarrier_, TIM_HandleTypeDef *timSignal_);

#endif /* INC_TRANSMITTERIR_H_ */
