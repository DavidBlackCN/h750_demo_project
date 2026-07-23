#ifndef __IIR_ADDA_FML_H__
#define __IIR_ADDA_FML_H__

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define IIR_ADDA_SAMPLE_RATE_HZ 1000000U
#define IIR_ADDA_DMA_SAMPLE_COUNT 1024U
#define IIR_ADDA_DMA_HALF_SAMPLE_COUNT (IIR_ADDA_DMA_SAMPLE_COUNT / 2U)
#define IIR_ADDA_BYPASS_TEST_ENABLED 0U

HAL_StatusTypeDef IIR_ADDA_FML_Start(void);
HAL_StatusTypeDef IIR_ADDA_FML_Stop(void);

bool IIR_ADDA_FML_IsActive(void);
void IIR_ADDA_FML_OnAdcHalfComplete(void);
void IIR_ADDA_FML_OnAdcComplete(void);
void IIR_ADDA_FML_OnAdcError(void);

uint32_t IIR_ADDA_FML_GetActualSampleRateHz(void);
uint32_t IIR_ADDA_FML_GetHalfCallbackCount(void);
uint32_t IIR_ADDA_FML_GetFullCallbackCount(void);
uint32_t IIR_ADDA_FML_GetErrorCount(void);

#endif
