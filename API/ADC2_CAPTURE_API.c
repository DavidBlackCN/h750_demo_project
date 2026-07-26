#include "ADC2_CAPTURE_API.h"

#include "ADC2_CAPTURE_BLL.h"
#include "ADC2_CAPTURE_FML.h"

HAL_StatusTypeDef ADC2_CAPTURE_API_Start(float requested_sample_rate_hz,
                                         uint32_t sample_count)
{
    return ADC2_CAPTURE_FML_Start(requested_sample_rate_hz, sample_count);
}

bool ADC2_CAPTURE_API_HasFrame(void)
{
    return (adc2_proc_flag != 0U);
}

void ADC2_CAPTURE_API_ReleaseFrame(void)
{
    adc2_proc_flag = 0U;
}

const uint16_t *ADC2_CAPTURE_API_GetRawFrame(void)
{
    return ADC2_CAPTURE_FML_GetRawBuffer();
}

const float *ADC2_CAPTURE_API_GetVoltageFrame(void)
{
    return ADC2_CAPTURE_BLL_GetVoltageFrame();
}

uint32_t ADC2_CAPTURE_API_GetSampleCount(void)
{
    return ADC2_CAPTURE_BLL_GetSampleCount();
}

float ADC2_CAPTURE_API_GetSampleRateHz(void)
{
    return ADC2_CAPTURE_BLL_GetSampleRateHz();
}

void adc2_proc(void)
{
    ADC2_CAPTURE_FML_Poll();
    adc2_deal();

}
