#ifndef ADC2_CAPTURE_FML_H
#define ADC2_CAPTURE_FML_H

#include "main.h"
#include <stdbool.h>

#define ADC2_DMA_BUFFER_LENGTH 1024U

extern uint16_t adc2_dma_buffer[ADC2_DMA_BUFFER_LENGTH];
extern volatile uint8_t adc2_deal_flag;
extern volatile uint8_t adc2_proc_flag;

/* Same entry-point name and responsibility as the reference ADC2 FML. */
void MY_ADC2_Init(void);
bool ADC2_CAPTURE_FML_IsActive(void);
void ADC2_CAPTURE_FML_OnDmaComplete(ADC_HandleTypeDef *hadc);
void ADC2_CAPTURE_FML_OnAdcError(ADC_HandleTypeDef *hadc);

#endif
