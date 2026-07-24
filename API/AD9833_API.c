#include "AD9833_API.h"

#include "AD9833.h"

static uint16_t ad9833_api_phase_degrees_to_word(float phase_deg)
{
    while (phase_deg < 0.0f)
    {
        phase_deg += 360.0f;
    }
    while (phase_deg >= 360.0f)
    {
        phase_deg -= 360.0f;
    }
    return (uint16_t)(phase_deg * 4096.0f / 360.0f + 0.5f) & 0x0FFFU;
}

HAL_StatusTypeDef AD9833_API_StartSine(float frequency_hz, float phase_deg)
{
    if (frequency_hz <= 0.0f)
    {
        return HAL_ERROR;
    }
    if (AD9833_Init() == 0U)
    {
        return HAL_ERROR;
    }

    AD9833_SetFrequency(AD9833_REG_FREQ0, frequency_hz, AD9833_FSEL0);
    AD9833_SetPhase(AD9833_REG_PHASE0,
                     ad9833_api_phase_degrees_to_word(phase_deg));
    AD9833_Setup(AD9833_FSEL0, AD9833_PSEL0, AD9833_OUT_SINUS);
    AD9833_ClearReset();
    return HAL_OK;
}

HAL_StatusTypeDef AD9833_API_SetPhaseDegrees(float phase_deg)
{
    AD9833_SetPhase(AD9833_REG_PHASE0,
                     ad9833_api_phase_degrees_to_word(phase_deg));
    return HAL_OK;
}

HAL_StatusTypeDef AD9833_API_StartSineDemo(void)
{
    return AD9833_API_StartSine(AD9833_API_DEMO_FREQUENCY_HZ, 0.0f);
}
