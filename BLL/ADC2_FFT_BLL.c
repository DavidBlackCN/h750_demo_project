#include "ADC2_FFT_BLL.h"

#include "ADC2_FFT_FML.h"

#include <math.h>

#define ADC2_FFT_MAX_SAMPLE_RATE_HZ       409600.0f
#define ADC2_FFT_MIN_SAMPLE_RATE_HZ       360000.0f

/* These are CPU-only FFT work buffers, not DMA memory. */
static float s_fft_real[ADC2_FFT_LENGTH];
static float s_fft_imaginary[ADC2_FFT_LENGTH];
static float s_half_magnitude[ADC2_FFT_LENGTH / 2U];

static HAL_StatusTypeDef adc2_fft_decode_sample(uint16_t raw_sample,
                                                const adc2_fft_input_config_t *input_config,
                                                float *sample_voltage)
{
    uint32_t code_mask;
    uint32_t code;
    int32_t signed_code;

    if ((input_config->sample_bit_width == 0U) ||
        (input_config->sample_bit_width > 16U))
    {
        return HAL_ERROR;
    }

    code_mask = (input_config->sample_bit_width == 16U) ?
                0xFFFFU : ((1UL << input_config->sample_bit_width) - 1UL);
    code = (uint32_t)raw_sample & code_mask;

    switch (input_config->sample_encoding)
    {
    case ADC2_FFT_SAMPLE_UNSIGNED:
        signed_code = (int32_t)code;
        break;

    case ADC2_FFT_SAMPLE_OFFSET_BINARY:
        signed_code = (int32_t)code - (int32_t)(1UL <<
                      (input_config->sample_bit_width - 1U));
        break;

    case ADC2_FFT_SAMPLE_TWOS_COMPLEMENT:
        if ((code & (1UL << (input_config->sample_bit_width - 1U))) != 0U)
        {
            signed_code = (int32_t)(code - (1UL << input_config->sample_bit_width));
        }
        else
        {
            signed_code = (int32_t)code;
        }
        break;

    default:
        return HAL_ERROR;
    }

    *sample_voltage = (float)signed_code * input_config->volts_per_lsb;
    return HAL_OK;
}

HAL_StatusTypeDef ADC2_FFT_BLL_Analyze(const uint16_t *raw_samples,
                                       const adc2_fft_input_config_t *input_config,
                                       adc2_fft_result_t *result)
{
    float mean = 0.0f;
    float peak_value;
    float peak_bin;
    uint32_t first_bin;
    uint32_t last_bin;
    uint32_t peak_index;

    if ((raw_samples == NULL) || (input_config == NULL) || (result == NULL) ||
        (input_config->sample_count != ADC2_FFT_LENGTH) ||
        (input_config->sample_rate_hz <= 0.0f) ||
        (input_config->volts_per_lsb <= 0.0f) ||
        (input_config->search_min_frequency_hz < 0.0f) ||
        (input_config->search_max_frequency_hz <=
         input_config->search_min_frequency_hz))
    {
        return HAL_ERROR;
    }

    for (uint32_t i = 0U; i < input_config->sample_count; ++i)
    {
        if (adc2_fft_decode_sample(raw_samples[i], input_config,
                                   &s_fft_real[i]) != HAL_OK)
        {
            return HAL_ERROR;
        }
        mean += s_fft_real[i];
    }
    mean /= (float)input_config->sample_count;

    for (uint32_t i = 0U; i < input_config->sample_count; ++i)
    {
        s_fft_real[i] -= mean;
        s_fft_imaginary[i] = 0.0f;
    }

    if (ADC2_FFT_FML_TransformReal(s_fft_real, s_fft_imaginary,
                                   input_config->sample_count,
                                   s_half_magnitude) != HAL_OK)
    {
        return HAL_ERROR;
    }

    first_bin = (uint32_t)ceilf(input_config->search_min_frequency_hz *
                                (float)input_config->sample_count /
                                input_config->sample_rate_hz);
    last_bin = (uint32_t)floorf(input_config->search_max_frequency_hz *
                                (float)input_config->sample_count /
                                input_config->sample_rate_hz);
    if (first_bin < 1U)
    {
        first_bin = 1U;
    }
    if (last_bin >= (input_config->sample_count / 2U))
    {
        last_bin = (input_config->sample_count / 2U) - 1U;
    }
    if (first_bin > last_bin)
    {
        return HAL_ERROR;
    }

    peak_index = first_bin;
    peak_value = s_half_magnitude[peak_index];
    for (uint32_t bin = first_bin + 1U; bin <= last_bin; ++bin)
    {
        if (s_half_magnitude[bin] > peak_value)
        {
            peak_index = bin;
            peak_value = s_half_magnitude[bin];
        }
    }

    peak_bin = (float)peak_index;
    if ((peak_index > first_bin) && (peak_index < last_bin))
    {
        float y0 = s_half_magnitude[peak_index - 1U];
        float y1 = s_half_magnitude[peak_index];
        float y2 = s_half_magnitude[peak_index + 1U];
        float denominator = y0 - (2.0f * y1) + y2;

        if (fabsf(denominator) > 1.0e-12f)
        {
            float offset = 0.5f * (y0 - y2) / denominator;

            if (offset > 0.5f)
            {
                offset = 0.5f;
            }
            else if (offset < -0.5f)
            {
                offset = -0.5f;
            }
            peak_bin += offset;
        }
    }

    result->sample_rate_hz = input_config->sample_rate_hz;
    result->frequency_resolution_hz = input_config->sample_rate_hz /
                                      (float)input_config->sample_count;
    result->peak_bin = peak_bin;
    result->frequency_hz = peak_bin * result->frequency_resolution_hz;
    result->peak_amplitude_v = peak_value;

    return HAL_OK;
}

HAL_StatusTypeDef ADC2_FFT_BLL_ChooseCoherentCapture(
    const adc2_fft_result_t *coarse_result,
    uint32_t timer_clock_hz,
    adc2_fft_capture_request_t *request)
{
    uint32_t minimum_divider;
    uint32_t maximum_divider;
    uint32_t best_divider = 0U;
    float best_error = 1.0e30f;

    if ((coarse_result == NULL) || (request == NULL) ||
        (coarse_result->frequency_hz <= 0.0f) || (timer_clock_hz == 0U))
    {
        return HAL_ERROR;
    }

    minimum_divider = (uint32_t)(((float)timer_clock_hz /
                                  ADC2_FFT_MAX_SAMPLE_RATE_HZ) + 0.999999f);
    maximum_divider = (uint32_t)((float)timer_clock_hz /
                                 ADC2_FFT_MIN_SAMPLE_RATE_HZ);
    if ((minimum_divider < 2U) || (maximum_divider < minimum_divider))
    {
        return HAL_ERROR;
    }

    for (uint32_t divider = minimum_divider; divider <= maximum_divider; ++divider)
    {
        float actual_sample_rate_hz;
        float cycles;
        float error;

        actual_sample_rate_hz = (float)timer_clock_hz / (float)divider;
        cycles = coarse_result->frequency_hz * (float)ADC2_FFT_LENGTH /
                 actual_sample_rate_hz;
        error = fabsf(cycles - roundf(cycles));

        if (error < best_error)
        {
            best_error = error;
            best_divider = divider;
        }
    }

    if (best_divider == 0U)
    {
        return HAL_ERROR;
    }

    request->requested_sample_rate_hz = (float)timer_clock_hz / (float)best_divider;
    request->sample_count = ADC2_FFT_LENGTH;
    request->closure_error_cycles = best_error;

    return HAL_OK;
}

const float *ADC2_FFT_BLL_GetHalfMagnitude(void)
{
    return s_half_magnitude;
}

uint32_t ADC2_FFT_BLL_GetHalfMagnitudeCount(void)
{
    return ADC2_FFT_LENGTH / 2U;
}
