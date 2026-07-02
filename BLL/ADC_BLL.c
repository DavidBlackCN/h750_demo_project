#include "ADC_BLL.h"

#include "ADC_FML.h"
#include "adc.h"
#include "tim.h"

void adc1_deal(void)
{
    if (adc1_deal_flag)
    {
        adc1_deal_flag = 0;

        for (uint32_t i = 0; i < ADC1_FREQ_BLOCK_LENGTH; i++)
        {
            adc1_data[i] = (float)(adc1_dma_buffer[i] & 0x0000FFFFU) * 3.3f / 65535.0f;
        }

        HAL_ADC_Stop_DMA(&hadc1);
        HAL_TIM_Base_Stop(&htim1);
        adc1_proc_flag = 1;
    }
}
