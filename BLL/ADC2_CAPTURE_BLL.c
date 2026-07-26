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

        if (!ADC2_CAPTURE_FML_TakeCompletedFrame())
        {
            return;
        }

        for (uint32_t i = 0U; i < ADC2_CAPTURE_FML_GetSampleCount(); ++i)
        {
            /* ADC2 is currently configured for 10-bit right-aligned data. */
            adc2_capture_data[i] = (float)(adc2_dma_buffer[i] & 0x03FFU) *
                                   3.3f / ADC2_CAPTURE_FULL_SCALE_CODE;
        }

        adc2_proc_flag = 1U;
    }
}

const float *ADC2_CAPTURE_BLL_GetVoltageFrame(void)
{
    return adc2_capture_data;
}

uint32_t ADC2_CAPTURE_BLL_GetSampleCount(void)
{
    return ADC2_CAPTURE_FML_GetSampleCount();
}

float ADC2_CAPTURE_BLL_GetSampleRateHz(void)
{
    return ADC2_CAPTURE_FML_GetSampleRateHz();
}
