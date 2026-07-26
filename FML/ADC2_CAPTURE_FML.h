#ifndef ADC2_CAPTURE_FML_H
#define ADC2_CAPTURE_FML_H

#include "main.h"
#include <stdbool.h>

#define ADC2_CAPTURE_MAX_LENGTH 4096U
#define ADC2_DMA_BUFFER_LENGTH  ADC2_CAPTURE_MAX_LENGTH

extern uint16_t adc2_dma_buffer[ADC2_DMA_BUFFER_LENGTH];
extern volatile uint8_t adc2_deal_flag;
extern volatile uint8_t adc2_proc_flag;

/* Legacy reference entry point: starts a 1024-point, 1 MS/s frame. */
void MY_ADC2_Init(void);
HAL_StatusTypeDef ADC2_CAPTURE_FML_Start(float requested_sample_rate_hz,
                                         uint32_t sample_count);
bool ADC2_CAPTURE_FML_IsActive(void);
bool ADC2_CAPTURE_FML_HasError(void);
uint32_t ADC2_CAPTURE_FML_GetErrorCode(void);
void ADC2_CAPTURE_FML_Poll(void);
bool ADC2_CAPTURE_FML_TakeCompletedFrame(void);
const uint16_t *ADC2_CAPTURE_FML_GetRawBuffer(void);
uint32_t ADC2_CAPTURE_FML_GetSampleCount(void);
float ADC2_CAPTURE_FML_GetSampleRateHz(void);
void ADC2_CAPTURE_FML_OnDmaComplete(ADC_HandleTypeDef *hadc);
void ADC2_CAPTURE_FML_OnAdcError(ADC_HandleTypeDef *hadc);

#endif
