#include "ADC2_CAPTURE_FML.h"

#include "adc.h"

uint16_t adc2_dma_buffer[ADC2_DMA_BUFFER_LENGTH]
    __attribute__((section(".dma_buffer"), aligned(32)));
volatile uint8_t adc2_deal_flag;
volatile uint8_t adc2_proc_flag;

static volatile uint8_t adc2_capture_active;

void MY_ADC2_Init(void)
{
    adc2_deal_flag = 0U;
    adc2_proc_flag = 0U;
    adc2_capture_active = 0U;

    /* H750 adaptation: RAM_D2 is cacheable, so prepare the full DMA range. */
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)adc2_dma_buffer,
                                      sizeof(adc2_dma_buffer));

    if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_dma_buffer,
                          ADC2_DMA_BUFFER_LENGTH) == HAL_OK)
    {
        adc2_capture_active = 1U;
    }
}

bool ADC2_CAPTURE_FML_IsActive(void)
{
    return (adc2_capture_active != 0U);
}

void ADC2_CAPTURE_FML_OnDmaComplete(ADC_HandleTypeDef *hadc)
{
    if ((hadc->Instance == ADC2) && (adc2_capture_active != 0U))
    {
        /* Same as the reference project: the ISR only publishes this flag. */
        adc2_deal_flag = 1U;
    }
}

void ADC2_CAPTURE_FML_OnAdcError(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC2)
    {
        adc2_capture_active = 0U;
    }
}
