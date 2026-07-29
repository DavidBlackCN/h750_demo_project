#ifndef G1_VPP_ADC_FML_H
#define G1_VPP_ADC_FML_H

#include "main.h"

#include <stdbool.h>

#define G1_VPP_ADC_FRAME_SAMPLES       4096U
#define G1_VPP_ADC_SAMPLE_RATE_HZ      1875000.0f

HAL_StatusTypeDef G1_VPP_ADC_FML_Start(void);
void G1_VPP_ADC_FML_Poll(void);
bool G1_VPP_ADC_FML_TakeFrame(const uint16_t **samples,
                               uint32_t *sample_count);
bool G1_VPP_ADC_FML_HasError(void);
uint32_t G1_VPP_ADC_FML_GetErrorCode(void);
float G1_VPP_ADC_FML_GetSampleRateHz(void);
bool G1_VPP_ADC_FML_OnAdcComplete(ADC_HandleTypeDef *hadc);
bool G1_VPP_ADC_FML_OnAdcError(ADC_HandleTypeDef *hadc);

#endif
