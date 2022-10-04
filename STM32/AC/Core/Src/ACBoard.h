/*
 * ACBoard.h
 *
 *  Created on: 6 Jan 2022
 *      Author: agp
 */

#ifndef SRC_ACBOARD_H_
#define SRC_ACBOARD_H_

void handleUserInputs(const char* startMsg);
void ACBoardInit(ADC_HandleTypeDef* hadc, WWDG_HandleTypeDef* hwwdg);
void ACBoardLoop(const char* startMsg);

#endif /* SRC_ACBOARD_H_ */
