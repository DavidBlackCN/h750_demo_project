#ifndef AD9833_API_H
#define AD9833_API_H

#include "main.h"
#include "AD9833.h"

#define AD9833_API_DEMO_FREQUENCY_HZ 10000.0f

/* Initialize the device and leave it held in reset without enabling output. */
HAL_StatusTypeDef AD9833_API_Init(void);

/*
 * Configure FREQ0, PHASE0 and the requested output mode, then release reset.
 * Valid waveform values are AD9833_OUT_SINUS, AD9833_OUT_TRIANGLE,
 * AD9833_OUT_MSB and AD9833_OUT_MSB2.
 */
void AD9833_API_OutputWaveform(float frequency_hz, uint16_t waveform);

HAL_StatusTypeDef AD9833_API_StartSine(float frequency_hz, float phase_deg);
HAL_StatusTypeDef AD9833_API_SetPhaseDegrees(float phase_deg);

/* Start the standalone 10 kHz, phase-0 sine demonstration on AD9833 FREQ0. */
HAL_StatusTypeDef AD9833_API_StartSineDemo(void);

#endif
