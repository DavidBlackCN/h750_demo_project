#include "DAC2_SELFTEST_API.h"

#include "DAC_FML.h"

#define DAC2_SELFTEST_FREQUENCY_HZ 1000.0f
#define DAC2_SELFTEST_VPP_V         1.0f
#define DAC2_SELFTEST_OFFSET_V      1.65f

HAL_StatusTypeDef DAC2_SelfTest_API_Init(void)
{
    return DAC_Waveform_StartChannel(DAC_CHANNEL_2,
                                     DAC_USER_WAVE_SINE,
                                     DAC2_SELFTEST_FREQUENCY_HZ,
                                     DAC2_SELFTEST_VPP_V,
                                     DAC2_SELFTEST_OFFSET_V);
}
