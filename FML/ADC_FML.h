#ifndef __ADC_FML_H__
#define __ADC_FML_H__

#include "main.h"

#define ADC_DUAL_SAMPLE_RATE_HZ 1000000.0f
#define ADC1_FREQ_BLOCK_LENGTH 4096U
#define ADC1_DMA_BUFFER_LENGTH ADC1_FREQ_BLOCK_LENGTH

typedef struct
{
    uint32_t timer_divider;
    uint32_t sample_count;
    uint32_t coherent_cycles;
    float sample_rate_hz;
    float closure_error_deg;
} adc_coherent_config_t;

extern uint32_t adc1_dma_buffer[ADC1_DMA_BUFFER_LENGTH];
extern float adc1_data[ADC1_FREQ_BLOCK_LENGTH];
extern float adc2_data[ADC1_FREQ_BLOCK_LENGTH];

extern volatile uint8_t adc1_half_flag;
extern volatile uint8_t adc1_deal_flag;
extern volatile uint8_t adc1_proc_flag;
extern volatile uint32_t adc1_capture_length;

extern volatile uint32_t adc1_half_callback_count;
extern volatile uint32_t adc1_full_callback_count;
extern volatile uint32_t adc1_error_callback_count;
extern volatile uint32_t adc1_last_dma_ndtr;
extern volatile uint32_t adc1_last_adc_error;
extern volatile uint32_t adc1_last_dma_error;
extern volatile uint32_t adc1_last_callback_tick;
extern volatile uint8_t adc1_last_callback_type;

HAL_StatusTypeDef MY_ADC1_Init(void);
HAL_StatusTypeDef ADC_Dual_StartCapture(void);
HAL_StatusTypeDef ADC_Dual_StartCaptureLength(uint32_t sample_count);
void ADC_Dual_SetDiscoverySampling(void);
HAL_StatusTypeDef ADC_Dual_ConfigureCoherentSampling(float frequency_hz,
                                                     adc_coherent_config_t *config);
void ADC_Debug_Print(void);

#endif
