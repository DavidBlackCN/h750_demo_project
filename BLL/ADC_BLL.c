#include "ADC_BLL.h"

#include "ADC_FML.h"
#include "adc.h"

void adc1_deal(void)
{
    if (adc1_deal_flag)
    {
        adc1_deal_flag = 0;

        MY_ADC1_StopCapture();
        if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
        {
            SCB_InvalidateDCache_by_Addr(adc1_dma_buffer, sizeof(adc1_dma_buffer));
        }

        for (uint32_t i = 0; i < ADC1_FREQ_BLOCK_LENGTH; i++)
        {
            adc1_data[i] = (float)(adc1_dma_buffer[i] & 0x0000FFFFU) * 3.3f / 65535.0f;
        }

        adc1_proc_flag = 1;
    }
}
