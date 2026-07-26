#include "ADC2_FFT_BLL.h"

#include "ADC2_FFT_FML.h"

#include <math.h>

#define ADC2_FFT_MAX_SAMPLE_RATE_HZ       409600.0f
#define ADC2_FFT_MIN_SAMPLE_RATE_HZ       360000.0f
/* Keep classification above the 1 MS/s measurement rate, while retaining
   margin for ADC trigger, DMA, and analog-front-end timing on the board. */
#define ADC2_FFT_CLASSIFY_MAX_SAMPLE_RATE_HZ 2000000.0f
#define ADC2_FFT_WAVEFORM_MIN_AMPLITUDE_V   0.020f
#define ADC2_FFT_CLASSIFY_MIN_CORRELATION    0.90f
#define ADC2_FFT_CLASSIFY_MIN_MARGIN         0.002f
#define ADC2_FFT_CLASSIFY_MIN_ODD_DISTORTION 0.012f
#define ADC2_FFT_SINE_DISTORTION_WEIGHT      250.0f

/* These are CPU-only FFT work buffers, not DMA memory. */
static float s_fft_real[ADC2_FFT_LENGTH];
static float s_fft_imaginary[ADC2_FFT_LENGTH];
static float s_half_magnitude[ADC2_FFT_LENGTH / 2U];

static const uint16_t s_classify_samples_per_period[] =
{
    8U, 16U, 32U, 64U, 128U, 256U, 512U, 1024U
};

static float adc2_fft_harmonic_magnitude(float harmonic_bin)
{
    uint32_t nearest_bin;

    if (harmonic_bin <= 0.0f)
    {
        return -1.0f;
    }

    nearest_bin = (uint32_t)(harmonic_bin + 0.5f);
    if ((nearest_bin == 0U) ||
        (nearest_bin >= (ADC2_FFT_LENGTH / 2U)))
    {
        return -1.0f;
    }

    return s_half_magnitude[nearest_bin];
}

static float adc2_fft_harmonic_band_magnitude(float harmonic_bin)
{
    uint32_t center_bin;
    uint32_t first_bin;
    uint32_t last_bin;
    float peak_magnitude;

    if (harmonic_bin <= 0.0f)
    {
        return -1.0f;
    }

    center_bin = (uint32_t)(harmonic_bin + 0.5f);
    if ((center_bin == 0U) ||
        (center_bin >= ((ADC2_FFT_LENGTH / 2U) - 1U)))
    {
        return -1.0f;
    }

    first_bin = (center_bin > 0U) ? (center_bin - 1U) : center_bin;
    last_bin = center_bin + 1U;
    peak_magnitude = s_half_magnitude[first_bin];
    for (uint32_t bin = first_bin + 1U; bin <= last_bin; ++bin)
    {
        if (s_half_magnitude[bin] > peak_magnitude)
        {
            peak_magnitude = s_half_magnitude[bin];
        }
    }

    return peak_magnitude;
}

static float adc2_fft_spectral_profile_score(const float *actual,
                                              const float *expected,
                                              uint32_t count)
{
    float actual_energy = 0.0f;
    float expected_energy = 0.0f;
    float cross_energy = 0.0f;

    for (uint32_t index = 0U; index < count; ++index)
    {
        actual_energy += actual[index] * actual[index];
        expected_energy += expected[index] * expected[index];
        cross_energy += actual[index] * expected[index];
    }

    if ((actual_energy <= 1.0e-12f) || (expected_energy <= 1.0e-12f))
    {
        return -1.0f;
    }

    return cross_energy / sqrtf(actual_energy * expected_energy);
}

static adc2_fft_waveform_t adc2_fft_classify_spectrum(
    const adc2_fft_result_t *result)
{
    adc2_fft_waveform_t best_waveform = ADC2_FFT_WAVEFORM_SINE;
    float best_correlation = result->sine_spectral_score;
    float runner_up = -1.0f;

    if (result->triangle_spectral_score > best_correlation)
    {
        runner_up = best_correlation;
        best_correlation = result->triangle_spectral_score;
        best_waveform = ADC2_FFT_WAVEFORM_TRIANGLE;
    }
    else
    {
        runner_up = result->triangle_spectral_score;
    }

    if (result->square_spectral_score > best_correlation)
    {
        runner_up = best_correlation;
        best_correlation = result->square_spectral_score;
        best_waveform = ADC2_FFT_WAVEFORM_SQUARE;
    }
    else if (result->square_spectral_score > runner_up)
    {
        runner_up = result->square_spectral_score;
    }

    if ((best_correlation < ADC2_FFT_CLASSIFY_MIN_CORRELATION) ||
        ((best_correlation - runner_up) < ADC2_FFT_CLASSIFY_MIN_MARGIN))
    {
        return ADC2_FFT_WAVEFORM_UNKNOWN;
    }

    return best_waveform;
}

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

    /* Include both band-edge bins. The hardware timer divider makes the actual
       sample rate slightly different from the nominal request, so rounding the
       lower edge upward can exclude a valid signal exactly at search_min. */
    first_bin = (uint32_t)floorf(input_config->search_min_frequency_hz *
                                 (float)input_config->sample_count /
                                 input_config->sample_rate_hz);
    last_bin = (uint32_t)ceilf(input_config->search_max_frequency_hz *
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
    result->harmonic3_ratio = -1.0f;
    result->harmonic5_ratio = -1.0f;
    if (peak_value >= ADC2_FFT_WAVEFORM_MIN_AMPLITUDE_V)
    {
        result->harmonic3_ratio = adc2_fft_harmonic_magnitude(3.0f * peak_bin);
        result->harmonic5_ratio = adc2_fft_harmonic_magnitude(5.0f * peak_bin);
        if (result->harmonic3_ratio >= 0.0f)
        {
            result->harmonic3_ratio /= peak_value;
        }
        if (result->harmonic5_ratio >= 0.0f)
        {
            result->harmonic5_ratio /= peak_value;
        }
    }
    result->sine_spectral_score = -1.0f;
    result->triangle_spectral_score = -1.0f;
    result->square_spectral_score = -1.0f;
    result->waveform = ADC2_FFT_WAVEFORM_UNKNOWN;

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

HAL_StatusTypeDef ADC2_FFT_BLL_ChooseWaveformCapture(
    float measured_frequency_hz,
    uint32_t timer_clock_hz,
    adc2_fft_waveform_capture_request_t *request)
{
    uint32_t best_samples_per_period = 0U;
    uint32_t best_divider = 0U;
    float best_error = 1.0e30f;

    if ((request == NULL) || (measured_frequency_hz <= 0.0f) ||
        (timer_clock_hz == 0U))
    {
        return HAL_ERROR;
    }

    for (uint32_t index = 0U;
         index < (sizeof(s_classify_samples_per_period) /
                  sizeof(s_classify_samples_per_period[0]));
         ++index)
    {
        uint32_t samples_per_period = s_classify_samples_per_period[index];
        float requested_sample_rate_hz = measured_frequency_hz *
                                         (float)samples_per_period;
        uint32_t divider;
        float actual_sample_rate_hz;
        float cycles;
        float error;

        if (requested_sample_rate_hz > ADC2_FFT_CLASSIFY_MAX_SAMPLE_RATE_HZ)
        {
            continue;
        }

        divider = (uint32_t)(((float)timer_clock_hz /
                              requested_sample_rate_hz) + 0.5f);
        if ((divider < 2U) || (divider > 65536U))
        {
            continue;
        }

        actual_sample_rate_hz = (float)timer_clock_hz / (float)divider;
        cycles = measured_frequency_hz * (float)ADC2_FFT_LENGTH /
                 actual_sample_rate_hz;
        error = fabsf(cycles - roundf(cycles));

        if ((error < best_error) ||
            ((fabsf(error - best_error) < 1.0e-6f) &&
             (samples_per_period > best_samples_per_period)))
        {
            best_error = error;
            best_divider = divider;
            best_samples_per_period = samples_per_period;
        }
    }

    if (best_divider == 0U)
    {
        return HAL_ERROR;
    }

    request->requested_sample_rate_hz = (float)timer_clock_hz /
                                         (float)best_divider;
    request->sample_count = ADC2_FFT_LENGTH;
    request->samples_per_period = best_samples_per_period;
    request->closure_error_cycles = best_error;
    return HAL_OK;
}

HAL_StatusTypeDef ADC2_FFT_BLL_ClassifySpectrum(
    const adc2_fft_input_config_t *input_config,
    float known_frequency_hz,
    adc2_fft_result_t *result)
{
    adc2_fft_result_t classification_result = {0};
    float actual_harmonics[6];
    float triangle_profile[6];
    float square_profile[6];
    float odd_distortion_energy = 0.0f;
    float odd_distortion_ratio;
    uint32_t harmonic_count = 0U;

    if ((input_config == NULL) || (result == NULL) ||
        (input_config->sample_count != ADC2_FFT_LENGTH) ||
        (input_config->sample_rate_hz <= 0.0f) ||
        (known_frequency_hz <= 0.0f))
    {
        return HAL_ERROR;
    }

    for (uint32_t harmonic = 1U; harmonic <= 11U; harmonic += 2U)
    {
        float harmonic_bin = (float)harmonic * known_frequency_hz /
                             input_config->sample_rate_hz *
                             (float)input_config->sample_count;
        float magnitude = adc2_fft_harmonic_band_magnitude(harmonic_bin);

        if (magnitude < 0.0f)
        {
            break;
        }
        actual_harmonics[harmonic_count] = magnitude;
        triangle_profile[harmonic_count] = 1.0f / ((float)harmonic *
                                                   (float)harmonic);
        square_profile[harmonic_count] = 1.0f / (float)harmonic;
        ++harmonic_count;
    }

    if ((harmonic_count < 2U) ||
        (actual_harmonics[0] < ADC2_FFT_WAVEFORM_MIN_AMPLITUDE_V))
    {
        return HAL_ERROR;
    }

    classification_result.peak_amplitude_v = actual_harmonics[0];
    classification_result.harmonic3_ratio = actual_harmonics[1] /
                                               actual_harmonics[0];
    classification_result.harmonic5_ratio = (harmonic_count >= 3U) ?
                                               (actual_harmonics[2] /
                                                actual_harmonics[0]) : -1.0f;
    for (uint32_t index = 1U; index < harmonic_count; ++index)
    {
        odd_distortion_energy += actual_harmonics[index] *
                                  actual_harmonics[index];
    }
    odd_distortion_ratio = sqrtf(odd_distortion_energy) /
                           actual_harmonics[0];

    /* Fundamental energy must not dominate the waveform decision. The sine
       score decays smoothly with aggregate odd-harmonic distortion, while
       triangle and square scores compare only the residual harmonic shape. */
    classification_result.sine_spectral_score = 1.0f / sqrtf(
        1.0f + (ADC2_FFT_SINE_DISTORTION_WEIGHT * odd_distortion_ratio *
                 odd_distortion_ratio));
    if (odd_distortion_ratio >= ADC2_FFT_CLASSIFY_MIN_ODD_DISTORTION)
    {
        classification_result.triangle_spectral_score =
            adc2_fft_spectral_profile_score(&actual_harmonics[1],
                                             &triangle_profile[1],
                                             harmonic_count - 1U);
        classification_result.square_spectral_score =
            adc2_fft_spectral_profile_score(&actual_harmonics[1],
                                             &square_profile[1],
                                             harmonic_count - 1U);
    }
    else
    {
        classification_result.triangle_spectral_score = -1.0f;
        classification_result.square_spectral_score = -1.0f;
    }
    classification_result.waveform = adc2_fft_classify_spectrum(
        &classification_result);

    result->harmonic3_ratio = classification_result.harmonic3_ratio;
    result->harmonic5_ratio = classification_result.harmonic5_ratio;
    result->sine_spectral_score = classification_result.sine_spectral_score;
    result->triangle_spectral_score = classification_result.triangle_spectral_score;
    result->square_spectral_score = classification_result.square_spectral_score;
    result->waveform = classification_result.waveform;
    return HAL_OK;
}

const char *ADC2_FFT_BLL_WaveformText(adc2_fft_waveform_t waveform)
{
    switch (waveform)
    {
    case ADC2_FFT_WAVEFORM_SINE:
        return "sine";

    case ADC2_FFT_WAVEFORM_TRIANGLE:
        return "triangle";

    case ADC2_FFT_WAVEFORM_SQUARE:
        return "square";

    default:
        return "unknown";
    }
}

const float *ADC2_FFT_BLL_GetHalfMagnitude(void)
{
    return s_half_magnitude;
}

uint32_t ADC2_FFT_BLL_GetHalfMagnitudeCount(void)
{
    return ADC2_FFT_LENGTH / 2U;
}
