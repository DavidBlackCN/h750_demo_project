#ifndef ADC_VOFA_FML_H
#define ADC_VOFA_FML_H

#include "main.h"
#include <stdbool.h>

HAL_StatusTypeDef ADC_VOFA_FML_Start(void);
void ADC_VOFA_FML_Process(void);
void ADC_VOFA_FML_OnAdcHalfComplete(ADC_HandleTypeDef *hadc);
void ADC_VOFA_FML_OnAdcComplete(ADC_HandleTypeDef *hadc);
void ADC_VOFA_FML_OnAdcError(ADC_HandleTypeDef *hadc);
bool ADC_VOFA_FML_IsActive(void);

#endif
