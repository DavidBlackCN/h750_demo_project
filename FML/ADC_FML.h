#ifndef __ADC_FML_H__
#define __ADC_FML_H__

#include "main.h"

#define ADC1_FREQ_BLOCK_LENGTH 1024U
#define ADC1_DMA_BUFFER_LENGTH ADC1_FREQ_BLOCK_LENGTH

extern uint32_t adc1_dma_buffer[ADC1_DMA_BUFFER_LENGTH];
extern float adc1_data[ADC1_FREQ_BLOCK_LENGTH];

extern volatile uint8_t adc1_half_flag;
extern volatile uint8_t adc1_deal_flag;
extern volatile uint8_t adc1_proc_flag;

extern volatile uint32_t adc1_half_callback_count;
extern volatile uint32_t adc1_full_callback_count;
extern volatile uint32_t adc1_error_callback_count;
extern volatile uint32_t adc1_last_dma_ndtr;
extern volatile uint32_t adc1_last_adc_error;
extern volatile uint32_t adc1_last_dma_error;
extern volatile uint32_t adc1_last_callback_tick;
extern volatile uint8_t adc1_last_callback_type;

HAL_StatusTypeDef MY_ADC1_Init(void);
HAL_StatusTypeDef MY_ADC1_StartCapture(void);
void MY_ADC1_StopCapture(void);
float MY_ADC1_GetSampleRateHz(void);
void ADC_Debug_Print(void);

#endif
