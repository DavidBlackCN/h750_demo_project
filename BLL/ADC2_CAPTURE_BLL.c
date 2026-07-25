#include "ADC2_CAPTURE_BLL.h"

#include "ADC2_CAPTURE_FML.h"
#include "adc.h"

#define ADC2_CAPTURE_FULL_SCALE_CODE 1023.0f

float adc2_capture_data[ADC2_DMA_BUFFER_LENGTH];

void adc2_deal(void)
{
    if (adc2_deal_flag != 0U)
    {
        adc2_deal_flag = 0U;

        /* DMA has filled RAM_D2; make its writes visible to the M7 core. */
        SCB_InvalidateDCache_by_Addr((uint32_t *)adc2_dma_buffer,
                                     sizeof(adc2_dma_buffer));

        for (uint32_t i = 0U; i < ADC2_DMA_BUFFER_LENGTH; ++i)
        {
            /* ADC2 is currently configured for 10-bit right-aligned data. */
            adc2_capture_data[i] = (float)(adc2_dma_buffer[i] & 0x03FFU) *
                                   3.3f / ADC2_CAPTURE_FULL_SCALE_CODE;
        }

        (void)HAL_ADC_Stop_DMA(&hadc2);
        adc2_proc_flag = 1U;
    }
}
