#include "ADC_BLL.h"

#include "ADC_FML.h"
#include "adc.h"
#include "tim.h"

void adc1_deal(void)
{
    if (adc1_deal_flag)
    {
        adc1_deal_flag = 0;

        (void)HAL_TIM_Base_Stop(&htim1);
        (void)HAL_ADCEx_MultiModeStop_DMA(&hadc1);
        SCB_InvalidateDCache_by_Addr(adc1_dma_buffer,
                                    (int32_t)sizeof(adc1_dma_buffer));

        for (uint32_t i = 0; i < adc1_capture_length; i++)
        {
            adc1_data[i] = (float)(adc1_dma_buffer[i] & 0x0000FFFFU) * 3.3f / 65535.0f;
            adc2_data[i] = (float)((adc1_dma_buffer[i] >> 16) & 0x0000FFFFU) * 3.3f / 65535.0f;
        }

        adc1_proc_flag = 1;
    }
}
