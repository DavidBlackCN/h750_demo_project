#ifndef ADC2_CAPTURE_API_H
#define ADC2_CAPTURE_API_H

#include "main.h"

#include <stdbool.h>

HAL_StatusTypeDef ADC2_CAPTURE_API_Start(float requested_sample_rate_hz,
                                         uint32_t sample_count);
bool ADC2_CAPTURE_API_HasFrame(void);
bool ADC2_CAPTURE_API_HasError(void);
uint32_t ADC2_CAPTURE_API_GetErrorCode(void);
void ADC2_CAPTURE_API_ReleaseFrame(void);
const uint16_t *ADC2_CAPTURE_API_GetRawFrame(void);
const float *ADC2_CAPTURE_API_GetVoltageFrame(void);
uint32_t ADC2_CAPTURE_API_GetSampleCount(void);
float ADC2_CAPTURE_API_GetSampleRateHz(void);

/* Same application-stage name as the reference project. */
void adc2_proc(void);

#endif
