#ifndef DPLL_API_H
#define DPLL_API_H

#include "main.h"

/* Scope-standard phase: CH2 (reference / ADC1) - CH1 (AD9833 / ADC2). */
#define DPLL_API_TARGET_PHASE_DEG          0.0f
#define DPLL_API_PHASE_CALIBRATION_DEG     0.0f
#define DPLL_API_INITIAL_PHASE_COMMAND_DEG 180.0f
#define DPLL_API_PROPORTIONAL_GAIN         0.15f
#define DPLL_API_INTEGRAL_GAIN             0.0f
#define DPLL_API_UPDATE_PERIOD_MS          10U

HAL_StatusTypeDef DPLL_API_Init(void);
HAL_StatusTypeDef DPLL_API_ProcessPhase(float phase_difference_deg);
void DPLL_API_ProcessUart(void);

#endif
