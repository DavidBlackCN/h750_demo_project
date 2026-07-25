#ifndef SUPER_FFT_H
#define SUPER_FFT_H

#include "main.h"
#include <stdbool.h>

/*
 * ADC3 FFT frequency measurement.
 *
 * Before calling SUPER_FFT_Start(), the application must have called
 * MX_DMA_Init() and MX_ADC3_Init().  Call SUPER_FFT_Process() repeatedly in
 * the foreground until SUPER_FFT_IsReady() becomes true.
 */
HAL_StatusTypeDef SUPER_FFT_Start(void);
void SUPER_FFT_Process(void);
void SUPER_FFT_Stop(void);

bool SUPER_FFT_IsActive(void);
bool SUPER_FFT_IsReady(void);
float SUPER_FFT_GetFrequencyHz(void);
float SUPER_FFT_GetSampleRateHz(void);

/* These two functions are called by the central ADC HAL callback router. */
void SUPER_FFT_OnAdcComplete(ADC_HandleTypeDef *hadc);
void SUPER_FFT_OnAdcError(ADC_HandleTypeDef *hadc);

#endif
