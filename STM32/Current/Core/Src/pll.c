/*!
 * @file    pll.c
 * @brief   This file contains the implementation of a Phase Lock Loop
 * @date    07/11/2025
 * @author  Timothé Dodin
*/

#include <math.h>

#include "pll.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

// The following constants have been tuned during testing
#define PLL_ALPHA   0.002f   // [-]          - PLL magnitude smoothing factor
#define ROCOF_ALPHA 0.001f   // [-]          - Rocof smoothing factor
#define PLL_KP      5.0e-3f  // [rad/sample] - PLL proportional gain
#define PLL_KI      1.5e-3f  // [rad/sample] - PLL integral gain

#define PLL_MAX_HZ    150.0f                      // [Hz]         - Max PLL frequency
#define PLL_MAX_OMEGA (PLL_MAX_HZ / OMEGA_TO_HZ)  // [rad/sample] - Max PLL angular speed

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
 * @brief Resets PLL
 * @param pll PLL handler
*/
void pllReset(PLL_t *pll) {
    pll->amp         = 0.0;
    pll->inPhaseFilt = 0.0;
    pll->quadFilt    = 0.0;
    pll->integrator  = 0.0;
    pll->omega       = 0.0;
    pll->omegaFilt   = 0.0;
    pll->rocof       = 0.0;
    pll->theta       = 1.0;  // Different from 0 so it can start again
}

/*
PLL (Phase Lock Loop) principle
-
The goal is to make the internal oscillator match with the input signal
-
Input signal:
where A is the signal amplitude and ϕ its phase
x(t) = A cos(wt + ϕ)
-
In phase and quadrature components (comparison between signal and internal oscillator):
where θ is the internal oscillator phase (not constant)
inPhase = x(t) cos(θ) = A cos(wt + ϕ) cos(θ) = 0.5 A [ cos(θ − ωt − ϕ) + cos(θ + ωt + ϕ) ]
quad    = x(t) sin(θ) = A cos(wt + ϕ) sin(θ) = 0.5 A [ sin(θ − ωt − ϕ) + sin(θ + ωt + ϕ) ]
-
Low pass filtering
The second term (with θ + ωt + ϕ) is at twice the signal frequency so it is filtered away
(θ changes at each step to follow the input signal)
It remains:
where Δ is the phase error
inPhase = 0.5 A cos(θ − ωt − ϕ) = 0.5 A cos(Δ)
quad    = 0.5 A sin(θ − ωt − ϕ) = 0.5 A sin(Δ)
-
If the phase error Δ is brought to 0, it means:
inPhase -> 0.5 A
quad    -> 0
-
So quad is given as input to a PI controller which outputs the estimated input frequency
θ is then obtained by integration
-
The signal amplitude A can be computed:
amp = 2 √[inPhase² + quad²]
    = 2 √[(0.5 A cos(Δ))² + (0.5 A sin(Δ))²]
    = 2 ⋅ 0.5 ⋅ A √[(cos(Δ))² + (sin(Δ))²]
    = A
*/

/*!
 * @brief Updates PLL
 * @note  Works by fitting an internal oscillator to the input signal
 * @param pll PLL handler
 * @param newSample New ADC value
*/
void pllStep(PLL_t *pll, float newSample) {
    static float previousOmegaFilt = 0.0; // Used for derivating the frequency

    // In phase and quadrature signal computation
    float inPhase = newSample * cosf(pll->theta);
    float quad    = newSample * sinf(pll->theta);

    // Filtering
    pll->inPhaseFilt = (1.0 - PLL_ALPHA) * pll->inPhaseFilt + PLL_ALPHA * inPhase;
    pll->quadFilt   = (1.0 - PLL_ALPHA) * pll->quadFilt + PLL_ALPHA * quad;

    // Amplitude estimation
    pll->amp = 2.0 * sqrtf(pll->inPhaseFilt * pll->inPhaseFilt + pll->quadFilt * pll->quadFilt);

    // PI controller that brings quad to 0, so the estimation corresponds to reality
    if (pll->amp > 1.0e-2f) {
        quad /= pll->amp;
    }
    float P = PLL_KP * quad;
    float I = PLL_KI * quad;
    float output = P + pll->integrator + I;

    // Anti wind-up
    if (output > PLL_MAX_OMEGA) {
        output = PLL_MAX_OMEGA;
        if (I < 0.0) {
            pll->integrator += I;
        }
    }
    else if (output < -PLL_MAX_OMEGA) {
        output = -PLL_MAX_OMEGA;
        if (I > 0.0) {
            pll->integrator += I;
        }
    }
    else {
        pll->integrator += I;
    }
    pll->omega = output;

    // Filtering
    // Absolute value as PLL can lock on opposite side
    previousOmegaFilt = pll->omegaFilt;
    pll->omegaFilt = (1.0 - PLL_ALPHA) * pll->omegaFilt + PLL_ALPHA * fabsf(pll->omega);
    float newTheta =  pll->theta + pll->omega;

    // Wrapping
    pll->theta = fmodf(newTheta, 2.0 * M_PI);

    // Rate of change of frequency (filtered derivative)
    float raw_rocof = pll->omegaFilt - previousOmegaFilt;
    pll->rocof = (1.0 - ROCOF_ALPHA) * pll->rocof + ROCOF_ALPHA * raw_rocof;
}
