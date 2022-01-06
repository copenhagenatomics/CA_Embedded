/*
 * inputValidation.h
 *
 *  Created on: Aug 26, 2021
 *      Author: matias
 */

#ifndef INC_INPUTVALIDATION_H_
#define INC_INPUTVALIDATION_H_

struct actuationInfo
{
    int pin; // pins 0-3 are interpreted as single ports - pin '-1' is interpreted as all
    int pwmDutyCycle;
    int timeOn; // time on is in seconds - timeOn '-1' is interpreted as indefinitely
    bool isInputValid;
};

struct actuationInfo parseAndValidateInput(const char * inputBuffer);

#endif /* INC_INPUTVALIDATION_H_ */
