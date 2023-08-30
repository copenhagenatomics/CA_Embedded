/*
 * transmitterIR.h
 *
 *  Created on: Jun 28, 2022
 *      Author: matias
 */

#ifndef INC_TRANSMITTERIR_H_
#define INC_TRANSMITTERIR_H_

/***************************************************************************************************
** INCLUDES
***************************************************************************************************/

#include <stdbool.h>

#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

// IR Codes AC1
#define IR_ADDRESS 0xc4d36480

#define TEMP_30		0x00240070
#define TEMP_25		0x00240090
#define TEMP_24		0x00240010
#define TEMP_23		0x002400e0
#define TEMP_22		0x00240060
#define TEMP_21		0x002400a0
#define TEMP_20		0x00240020
#define TEMP_19		0x002400c0
#define TEMP_18		0x00240040
#define TEMP_5		0x00240200
#define AC_OFF 		0x40090

#define CRC30 	0x320000
#define CRC25	0xe20000
#define CRC24	0x620000
#define CRC23	0xa20000
#define CRC22	0x220000
#define CRC21	0xc20000
#define CRC20	0x420000
#define CRC19	0x820000
#define CRC18	0x20000
#define CRC5	0xc90000
#define CRC_OFF 0xc20000

#define FAN_HIGH	0xa0000000
#define FAN_HIGH_5	0xa000a800

#define MSG_LEN_BITS 113

/* IR codes AC2 */
#define AC2_TEMP_COMMAND 0xB24D

#define AC2_FAN_MODE_0 0x9F60
#define AC2_FAN_MODE_1 0x5FA0
#define AC2_FAN_MODE_2 0x3FC0
#define AC2_FAN_MODE_A 0xBF40

#define AC2_TEMP_17    0x00FF
#define AC2_TEMP_18    0x10EF
#define AC2_TEMP_19    0x30CF
#define AC2_TEMP_20    0x20DF
#define AC2_TEMP_21    0x609F
#define AC2_TEMP_22    0x708F
#define AC2_TEMP_23    0x50AF
#define AC2_TEMP_24    0x40BF
#define AC2_TEMP_25    0xC03F
#define AC2_TEMP_26    0xD02F
#define AC2_TEMP_27    0x906F
#define AC2_TEMP_28    0x807F
#define AC2_TEMP_29    0xA05F
#define AC2_TEMP_30    0xB04F
#define AC2_TEMP_OFF   0xE41B

#define AC2_OFF             0x7B84
#define AC2_OFF_SWING_END   0xE01F

#define AC2_MSG_LEN_BITS 48

/* Sending PWMs
** 
** The timings below are based on a timer clock speed of 16 MHz, with a prescaler of 1 (e.g. final 
** clock speed is 8 MHz)
*/
/* First aircon remote timings */
#define START_BIT_ARR	37601	// Period of 4700us (~213Hz) --- NOTE prescaler for this setting
#define START_BIT_CCR	26400	// On time (70%)

#define HIGH_BIT_ARR	13920	// Period of 1740us (~575Hz)
#define HIGH_BIT_CCR	4315 	// On time (31%)

#define LOW_BIT_ARR		7042 	// Period of 880us (~1136Hz)
#define LOW_BIT_CCR		4296    // On time (61%)

/* Second aircon remote timings */
#define START_BIT_ARR_AC2   70400   // Period = 8800 us
#define START_BIT_CCR_AC2   35200   // On time (50%)
#define START_BIT_REST_AC2  35200   // Period = 4400 us

#define HIGH_BIT_ARR_AC2    18400   // Period of 2300us (~435Hz)
#define HIGH_BIT_CCR_AC2    4800    // On time (26%)

#define LOW_BIT_ARR_AC2     9600    // Period of 1200us (~833Hz)
#define LOW_BIT_CCR_AC2     4800    // On time (50%)

#define NUM_COMMAND_REPEATS 5

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void getACStates(int * tempState);
bool updateTemperatureIR(int temp);
void turnOnLED();
void turnOffLED();
void turnOffAC();
void pwmGPIO();
void initTransmitterIR(TIM_HandleTypeDef *timFreqCarrier_, TIM_HandleTypeDef *timSignal_);

#endif /* INC_TRANSMITTERIR_H_ */
