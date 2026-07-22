#include "G_BASIC_API.h"

#include "AD9910_API.h"

#include <string.h>

static G_BasicAPI_Result g_result;

#define G_BASIC_API_R3_FREQUENCY_HZ      1000U
#define G_BASIC_API_R3_TARGET_TENTHS_VPP   20U

static HAL_StatusTypeDef G_BasicAPI_StartModelControl(uint8_t requirement,
                                                      uint32_t frequency_hz,
                                                      uint8_t target_tenths_vpp)
{
    G_ModelBLL_Result model_result;
    uint32_t start_tick;

    memset(&g_result, 0, sizeof(g_result));
    g_result.requirement = requirement;
    g_result.frequency_hz = frequency_hz;
    g_result.target_output_mvpp = (uint16_t)target_tenths_vpp * 100U;

    if (G_ModelBLL_Calculate(frequency_hz, target_tenths_vpp,
                             &model_result) != G_MODEL_BLL_OK)
    {
        g_result.state = G_BASIC_API_PARAMETER_ERROR;
        return HAL_ERROR;
    }

    g_result.source_output_mvpp = model_result.source_output_mvpp;
    g_result.model_gain = model_result.model_gain;
    g_result.gain_index = model_result.gain_index;
    g_result.amplitude_code = model_result.amplitude_code;

    start_tick = HAL_GetTick();
    if (AD9910_API_StartSineByAmplitudeCode(frequency_hz,
                                             model_result.amplitude_code) != HAL_OK)
    {
        g_result.state = G_BASIC_API_HARDWARE_ERROR;
        return HAL_ERROR;
    }

    g_result.start_elapsed_ms = HAL_GetTick() - start_tick;
    g_result.state = G_BASIC_API_RUNNING;
    return HAL_OK;
}

HAL_StatusTypeDef G_BasicAPI_StartRequirement2(uint32_t frequency_hz)
{
    uint16_t amplitude_code;
    uint32_t start_tick;

    memset(&g_result, 0, sizeof(g_result));
    g_result.requirement = 2U;
    g_result.frequency_hz = frequency_hz;
    g_result.target_output_mvpp = G_BASIC_API_SIGNAL_OUTPUT_MVPP;
    g_result.source_output_mvpp = (float)G_BASIC_API_SIGNAL_OUTPUT_MVPP;

    if ((frequency_hz < G_BASIC_API_SIGNAL_FREQUENCY_MIN_HZ) ||
        (frequency_hz > G_BASIC_API_SIGNAL_FREQUENCY_MAX_HZ) ||
        ((frequency_hz % G_BASIC_API_SIGNAL_FREQUENCY_STEP_HZ) != 0U) ||
        (G_ModelBLL_OutputMvppToAmplitudeCode(
             (float)G_BASIC_API_SIGNAL_OUTPUT_MVPP,
             &amplitude_code) != G_MODEL_BLL_OK))
    {
        g_result.state = G_BASIC_API_PARAMETER_ERROR;
        return HAL_ERROR;
    }

    g_result.amplitude_code = amplitude_code;
    start_tick = HAL_GetTick();
    AD9910_output_sine(frequency_hz, G_BASIC_API_SIGNAL_OUTPUT_MVPP);

    g_result.start_elapsed_ms = HAL_GetTick() - start_tick;
    g_result.state = G_BASIC_API_RUNNING;
    return HAL_OK;
}

HAL_StatusTypeDef G_BasicAPI_StartRequirement3(void)
{
    return G_BasicAPI_StartModelControl(3U, G_BASIC_API_R3_FREQUENCY_HZ,
                                        G_BASIC_API_R3_TARGET_TENTHS_VPP);
}

HAL_StatusTypeDef G_BasicAPI_StartRequirement4(uint32_t frequency_hz,
                                               uint8_t target_tenths_vpp)
{
    return G_BasicAPI_StartModelControl(4U, frequency_hz,
                                        target_tenths_vpp);
}

const G_BasicAPI_Result *G_BasicAPI_GetResult(void)
{
    return &g_result;
}
