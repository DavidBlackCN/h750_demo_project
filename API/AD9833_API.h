#ifndef AD9833_API_H
#define AD9833_API_H

#include "main.h"

#define AD9833_API_DEMO_FREQUENCY_HZ 10000.0f

HAL_StatusTypeDef AD9833_API_StartSine(float frequency_hz, float phase_deg);
HAL_StatusTypeDef AD9833_API_SetPhaseDegrees(float phase_deg);

/* Start the standalone 10 kHz, phase-0 sine demonstration on AD9833 FREQ0. */
HAL_StatusTypeDef AD9833_API_StartSineDemo(void);

#endif
