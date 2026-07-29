#include "G1_VPP_API.h"

#include "G1_VPP_ADC_FML.h"
#include "USART_FML.h"

#include "usart.h"

#include <stdio.h>

static g1_vpp_calibration_t s_calibration = {
    .adc_reference_volts = 3.3f,
    .front_end_gain = 1.0f,
    .calibration_scale = 1.0f,
};
static g1_vpp_result_t s_result;
static g1_fft_result_t s_fft_result;
static float s_fft_input[G1_VPP_ADC_FRAME_SAMPLES]
    __attribute__((section(".dma_buffer"), aligned(32)));
static uint8_t s_error;
static uint32_t s_error_code;
static uint8_t s_done;

#define G1_VPP_UART_WAVE_SAMPLES G1_VPP_ADC_FRAME_SAMPLES
#define G1_VPP_HARMONIC_SEARCH_MAX_HZ 205000.0f

static HAL_StatusTypeDef g1_vpp_send_raw_waveform(const uint16_t *samples,
                                                   uint32_t sample_count)
{
    char message[32];
    int length;

    uint32_t output_count = sample_count;

    if (output_count > G1_VPP_UART_WAVE_SAMPLES)
    {
        output_count = G1_VPP_UART_WAVE_SAMPLES;
    }

    for (uint32_t index = 0U; index < output_count; ++index)
    {
        float input_millivolts = 1000.0f *
            G1_VPP_BLL_CodeToInputVolts(samples[index], &s_calibration);
        length = snprintf(message, sizeof(message), "wave:%.3f\r\n",
                          (double)input_millivolts);

        if ((length <= 0) || (length >= (int)sizeof(message)) ||
            (Usart_Send_Computer(&huart1, message) != HAL_OK))
        {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

static HAL_StatusTypeDef g1_vpp_analyze_and_send_fft(const uint16_t *samples,
                                                     uint32_t sample_count)
{
    const g1_fft_input_t fft_input = {
        .sample_rate_hz = G1_VPP_ADC_FML_GetSampleRateHz(),
        .search_min_frequency_hz = 10000.0f,
        /* Keep two-plus FFT bins above the 200 kHz task edge so a peak near
           200 kHz is not lost when its nearest bin lies slightly above it. */
        .search_max_frequency_hz = G1_VPP_HARMONIC_SEARCH_MAX_HZ,
        .min_peak_amplitude_volts = 0.001f,
    };
    char message[96];
    int length;
    uint32_t harmonic_count = 0U;

    if (sample_count > G1_VPP_ADC_FRAME_SAMPLES)
    {
        return HAL_ERROR;
    }

    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        s_fft_input[index] = G1_VPP_BLL_CodeToInputVolts(samples[index],
                                                          &s_calibration);
    }

    if (!G1_FFT_API_Analyze(s_fft_input, sample_count, &fft_input, &s_fft_result))
    {
        return HAL_ERROR;
    }

    for (uint32_t index = 0U; index < s_fft_result.component_count; ++index)
    {
        if (s_fft_result.components[index].harmonic_order != 0U)
        {
            ++harmonic_count;
        }
    }

    for (uint32_t index = 0U; index < s_fft_result.spectrum_bin_count; ++index)
    {
        length = snprintf(message, sizeof(message),
                          "spectrum:%.3f\r\n",
                          (double)(s_fft_result.spectrum_peak_volts[index] * 1000.0f));
        if ((length <= 0) || (length >= (int)sizeof(message)) ||
            (Usart_Send_Computer(&huart1, message) != HAL_OK))
        {
            return HAL_ERROR;
        }
    }

    length = snprintf(message, sizeof(message),
                      "harmonics begin count=%lu\r\nfundamental_Hz=%.3f\r\n",
                      (unsigned long)harmonic_count,
                      (double)s_fft_result.fundamental_hz);
    if ((length <= 0) || (length >= (int)sizeof(message)) ||
        (Usart_Send_Computer(&huart1, message) != HAL_OK))
    {
        return HAL_ERROR;
    }

    for (uint32_t index = 0U; index < s_fft_result.component_count; ++index)
    {
        const g1_fft_component_t *component = &s_fft_result.components[index];

        if (component->harmonic_order == 0U)
        {
            continue;
        }

        length = snprintf(message, sizeof(message),
                          "harmonic n=%lu f_Hz=%.3f fit_Hz=%.3f amp_mVpp=%.3f phase_rad=%.4f\r\n",
                          (unsigned long)component->harmonic_order,
                          (double)component->frequency_hz,
                          (double)component->fitted_frequency_hz,
                          (double)(component->peak_amplitude_volts * 2000.0f),
                          (double)component->phase_radians);
        if ((length <= 0) || (length >= (int)sizeof(message)) ||
            (Usart_Send_Computer(&huart1, message) != HAL_OK))
        {
            return HAL_ERROR;
        }
    }

    if (Usart_Send_Computer(&huart1, "harmonics end\r\n") != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef G1_VPP_API_Init(void)
{
    s_result.valid = false;
    s_error = 0U;
    s_error_code = HAL_ADC_ERROR_NONE;
    s_done = 0U;
    s_fft_result.valid = false;

    return G1_VPP_ADC_FML_Start();
}

void G1_VPP_API_Process(void)
{
    const uint16_t *samples;
    uint32_t sample_count;
    if (s_done != 0U)
    {
        return;
    }

    G1_VPP_ADC_FML_Poll();
    if (G1_VPP_ADC_FML_HasError())
    {
        s_error = 1U;
        s_error_code = G1_VPP_ADC_FML_GetErrorCode();
        return;
    }

    if (!G1_VPP_ADC_FML_TakeFrame(&samples, &sample_count))
    {
        return;
    }

    if (!G1_VPP_BLL_Analyze(samples, sample_count, &s_calibration, &s_result))
    {
        s_error = 1U;
        s_error_code = HAL_ERROR;
        return;
    }

    if (g1_vpp_send_raw_waveform(samples, sample_count) != HAL_OK)
    {
        s_error = 1U;
        s_error_code = HAL_ERROR;
        return;
    }

    if (g1_vpp_analyze_and_send_fft(samples, sample_count) != HAL_OK)
    {
        s_error = 1U;
        s_error_code = HAL_ERROR;
        return;
    }

    /* The ADC1 one-shot report is complete; keep ADC/TIM stopped. */
    s_done = 1U;
}

void G1_VPP_API_SetCalibration(const g1_vpp_calibration_t *calibration)
{
    if ((calibration != NULL) && (calibration->adc_reference_volts > 0.0f) &&
        (calibration->front_end_gain > 0.0f) &&
        (calibration->calibration_scale > 0.0f))
    {
        s_calibration = *calibration;
        s_result.valid = false;
    }
}

const g1_vpp_result_t *G1_VPP_API_GetResult(void)
{
    return &s_result;
}

const g1_fft_result_t *G1_VPP_API_GetFftResult(void)
{
    return &s_fft_result;
}

bool G1_VPP_API_HasError(void)
{
    return (s_error != 0U);
}

uint32_t G1_VPP_API_GetErrorCode(void)
{
    return s_error_code;
}
