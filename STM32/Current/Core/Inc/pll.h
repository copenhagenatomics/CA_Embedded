/*!
 * @file    pll.h
 * @brief   Header file of pll.c
 * @date    07/11/2025
 * @author  Timothé Dodin
*/

#ifndef INC_PLL_H_
#define INC_PLL_H_

#include "CurrentApp.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define OMEGA_TO_HZ       (ADC_F_S / (2.0 * M_PI))
#define ROCOF_TO_HZ_PER_S (ADC_F_S * ADC_F_S / (2.0 * M_PI))

typedef struct _PLL {
    float theta;        // [rad]                 - Phase estimation
    float omega;        // [rad/sample]          - Angular speed estimation
    float omegaFilt;    // [rad/sample]          - Filtered angular speed estimation
    float integrator;   // [1/sample]            - Integrator
    float amp;          // [-]                   - Fundamental amplitude estimation
    float inPhaseFilt;  // [-]                   - In phase component
    float quadFilt;     // [-]                   - Quadrature component
    float rocof;        // [rad/(sample·sample)] - Rate of change of frequency
} PLL_t; // Phase Lock Loop handler

/***************************************************************************************************
** PUBLIC FUNCTION DECLARATIONS
***************************************************************************************************/

void pllReset(PLL_t *pll);
void pllStep(PLL_t *pll, float newSample);

#endif /* PLL_H_ */
