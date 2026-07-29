#include "G1_FFT_BLL.h"

#include "G1_FFT_FML.h"

#include <math.h>
#include <string.h>

static float s_windowed_samples[G1_FFT_FML_MAX_SAMPLES];
static float s_real[G1_FFT_FML_MAX_SAMPLES];
static float s_imaginary[G1_FFT_FML_MAX_SAMPLES];

/* The contest source contains the fundamental plus at most two components
   whose orders are no greater than four.  Do not manufacture a subharmonic
   from unrelated higher-frequency distortion. */
#define G1_FFT_MAX_TASK_HARMONIC_ORDER 4U
#define G1_FFT_MIN_FUNDAMENTAL_PEAK_V  0.0005f

static float g1_fft_refine_peak_bin(const g1_fft_result_t *result,
                                    uint32_t bin_index)
{
    const uint32_t spectrum_index = bin_index - result->first_spectrum_bin;
    const float floor_amplitude = 1.0e-12f;
    float left;
    float centre;
    float right;
    float denominator;
    float offset;

    if ((spectrum_index == 0U) ||
        ((spectrum_index + 1U) >= result->spectrum_bin_count))
    {
        return 0.0f;
    }

    left = logf(fmaxf(result->spectrum_peak_volts[spectrum_index - 1U],
                       floor_amplitude));
    centre = logf(fmaxf(result->spectrum_peak_volts[spectrum_index],
                         floor_amplitude));
    right = logf(fmaxf(result->spectrum_peak_volts[spectrum_index + 1U],
                        floor_amplitude));
    denominator = left - (2.0f * centre) + right;
    if (fabsf(denominator) < 1.0e-12f)
    {
        return 0.0f;
    }

    offset = 0.5f * (left - right) / denominator;
    if (offset > 0.5f)
    {
        offset = 0.5f;
    }
    else if (offset < -0.5f)
    {
        offset = -0.5f;
    }

    return offset;
}

static void g1_fft_insert_component(g1_fft_result_t *result,
                                    const g1_fft_component_t *component)
{
    uint32_t insert_at = result->component_count;

    if (insert_at < G1_FFT_MAX_COMPONENTS)
    {
        ++result->component_count;
    }
    else if (component->peak_amplitude_volts <=
             result->components[G1_FFT_MAX_COMPONENTS - 1U].peak_amplitude_volts)
    {
        return;
    }
    else
    {
        insert_at = G1_FFT_MAX_COMPONENTS - 1U;
    }

    while ((insert_at > 0U) &&
           (component->peak_amplitude_volts >
            result->components[insert_at - 1U].peak_amplitude_volts))
    {
        result->components[insert_at] = result->components[insert_at - 1U];
        --insert_at;
    }
    result->components[insert_at] = *component;
}

static float g1_fft_measure_tone_peak(const float *samples,
                                      uint32_t sample_count,
                                      float sample_rate_hz,
                                      float frequency_hz,
                                      float dc_volts,
                                      float *phase_radians)
{
    float cosine_sum = 0.0f;
    float sine_sum = 0.0f;

    for (uint32_t sample = 0U; sample < sample_count; ++sample)
    {
        const float phase = 6.28318530717958647692f * frequency_hz *
            (float)sample / sample_rate_hz;
        const float value = samples[sample] - dc_volts;

        cosine_sum += value * cosf(phase);
        sine_sum += value * sinf(phase);
    }

    if (phase_radians != NULL)
    {
        *phase_radians = atan2f(sine_sum, cosine_sum);
    }
    return (2.0f * sqrtf((cosine_sum * cosine_sum) +
                         (sine_sum * sine_sum))) / (float)sample_count;
}

static float g1_fft_infer_fundamental(const float *samples,
                                      uint32_t sample_count,
                                      const g1_fft_input_t *input,
                                      g1_fft_result_t *result)
{
    float best_frequency = 0.0f;

    for (uint32_t candidate = 0U; candidate < result->component_count; ++candidate)
    {
        for (uint32_t divisor = 1U; divisor <= 4U; ++divisor)
        {
            const float base_frequency = result->components[candidate].frequency_hz /
                (float)divisor;
            float phase_radians;
            uint32_t matches = 0U;

            if ((base_frequency < input->search_min_frequency_hz) ||
                (base_frequency > input->search_max_frequency_hz) ||
                (g1_fft_measure_tone_peak(samples, sample_count,
                                          input->sample_rate_hz, base_frequency,
                                          result->dc_volts, &phase_radians) <
                 G1_FFT_MIN_FUNDAMENTAL_PEAK_V))
            {
                continue;
            }

            for (uint32_t component = 0U; component < result->component_count;
                 ++component)
            {
                const float harmonic = floorf((result->components[component].frequency_hz /
                                                base_frequency) + 0.5f);

                if ((harmonic >= 1.0f) &&
                    (harmonic <= (float)G1_FFT_MAX_TASK_HARMONIC_ORDER) &&
                    (fabsf(result->components[component].frequency_hz -
                           (harmonic * base_frequency)) <=
                     (1.5f * result->bin_width_hz)))
                {
                    ++matches;
                }
            }

            if ((matches >= 2U) &&
                ((best_frequency == 0.0f) || (base_frequency < best_frequency)))
            {
                best_frequency = base_frequency;
            }
        }
    }

    return best_frequency;
}

static float g1_fft_refine_fundamental_least_squares(g1_fft_result_t *result)
{
    const float initial_frequency = result->fundamental_hz;
    float numerator = 0.0f;
    float denominator = 0.0f;

    if (initial_frequency <= 0.0f)
    {
        return 0.0f;
    }

    for (uint32_t index = 0U; index < result->component_count; ++index)
    {
        g1_fft_component_t *component = &result->components[index];
        const float harmonic = floorf((component->frequency_hz /
                                       initial_frequency) + 0.5f);
        const float weight = component->peak_amplitude_volts *
                             component->peak_amplitude_volts;

        if ((harmonic < 1.0f) ||
            (harmonic > (float)G1_FFT_MAX_TASK_HARMONIC_ORDER) ||
            (fabsf(component->frequency_hz - (harmonic * initial_frequency)) >
             (1.5f * result->bin_width_hz)))
        {
            component->harmonic_order = 0U;
            continue;
        }

        component->harmonic_order = (uint32_t)harmonic;
        numerator += weight * harmonic * component->frequency_hz;
        denominator += weight * harmonic * harmonic;
    }

    if (denominator <= 0.0f)
    {
        return initial_frequency;
    }

    return numerator / denominator;
}

bool G1_FFT_BLL_Analyze(const float *voltage_samples,
                        uint32_t sample_count,
                        const g1_fft_input_t *input,
                        g1_fft_result_t *result)
{
    float sum = 0.0f;
    float window_sum = 0.0f;
    uint32_t first_bin;
    uint32_t last_bin;

    if ((voltage_samples == NULL) || (input == NULL) || (result == NULL) ||
        (sample_count < 8U) || (sample_count > G1_FFT_FML_MAX_SAMPLES) ||
        (input->sample_rate_hz <= 0.0f) ||
        (input->search_max_frequency_hz < input->search_min_frequency_hz))
    {
        return false;
    }

    memset(result, 0, sizeof(*result));
    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        sum += voltage_samples[index];
    }
    result->dc_volts = sum / (float)sample_count;
    result->bin_width_hz = input->sample_rate_hz / (float)sample_count;

    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        const float window = 0.5f -
            (0.5f * cosf((6.28318530717958647692f * (float)index) /
                         (float)(sample_count - 1U)));
        s_windowed_samples[index] = (voltage_samples[index] - result->dc_volts) * window;
        window_sum += window;
    }

    if ((window_sum <= 0.0f) ||
        !G1_FFT_FML_TransformReal(s_windowed_samples, sample_count, s_real, s_imaginary))
    {
        return false;
    }

    first_bin = (uint32_t)ceilf(input->search_min_frequency_hz / result->bin_width_hz);
    last_bin = (uint32_t)floorf(input->search_max_frequency_hz / result->bin_width_hz);
    if (first_bin < 1U)
    {
        first_bin = 1U;
    }
    if (last_bin >= (sample_count / 2U))
    {
        last_bin = (sample_count / 2U) - 1U;
    }
    if (first_bin > last_bin)
    {
        return false;
    }

    result->first_spectrum_bin = 1U;
    result->spectrum_bin_count = (sample_count / 2U) - 1U;
    for (uint32_t bin = result->first_spectrum_bin;
         bin < (sample_count / 2U);
         ++bin)
    {
        const float re = s_real[bin];
        const float im = s_imaginary[bin];

        result->spectrum_peak_volts[bin - result->first_spectrum_bin] =
            (2.0f * sqrtf((re * re) + (im * im))) / window_sum;
    }

    for (uint32_t bin = first_bin; bin <= last_bin; ++bin)
    {
        const float re = s_real[bin];
        const float im = s_imaginary[bin];
        const float amplitude = (2.0f * sqrtf((re * re) + (im * im))) / window_sum;
        g1_fft_component_t component;

        if ((amplitude < input->min_peak_amplitude_volts) ||
            ((bin > first_bin) &&
             (amplitude < (2.0f * sqrtf((s_real[bin - 1U] * s_real[bin - 1U]) +
                                        (s_imaginary[bin - 1U] * s_imaginary[bin - 1U]))) / window_sum)) ||
            ((bin < last_bin) &&
             (amplitude < (2.0f * sqrtf((s_real[bin + 1U] * s_real[bin + 1U]) +
                                        (s_imaginary[bin + 1U] * s_imaginary[bin + 1U]))) / window_sum)) )
        {
            continue;
        }

        component.bin_frequency_hz = (float)bin * result->bin_width_hz;
        component.bin_offset = g1_fft_refine_peak_bin(result, bin);
        component.frequency_hz = ((float)bin + component.bin_offset) *
                                 result->bin_width_hz;
        component.fitted_frequency_hz = 0.0f;
        component.peak_amplitude_volts = amplitude;
        component.phase_radians = atan2f(im, re);
        component.bin_index = bin;
        component.harmonic_order = 0U;
        g1_fft_insert_component(result, &component);
    }

    result->fundamental_hz = g1_fft_infer_fundamental(voltage_samples,
                                                       sample_count, input,
                                                       result);
    if ((result->fundamental_hz <= 0.0f) && (result->component_count != 0U))
    {
        /* A single clear tone is still a valid periodic signal.  Keep the
           measurement path alive and report that lowest detected component
           instead of failing the entire frame because no harmonic pair was
           available for the integer-ratio test. */
        result->fundamental_hz = result->components[0].frequency_hz;
        for (uint32_t index = 1U; index < result->component_count; ++index)
        {
            if (result->components[index].frequency_hz < result->fundamental_hz)
            {
                result->fundamental_hz = result->components[index].frequency_hz;
            }
        }
    }
    if (result->fundamental_hz > 0.0f)
    {
        uint8_t fundamental_present = 0U;

        for (uint32_t index = 0U; index < result->component_count; ++index)
        {
            if (fabsf(result->components[index].frequency_hz -
                      result->fundamental_hz) <= (1.5f * result->bin_width_hz))
            {
                fundamental_present = 1U;
                break;
            }
        }
        if (fundamental_present == 0U)
        {
            g1_fft_component_t component = {0};

            component.frequency_hz = result->fundamental_hz;
            component.bin_frequency_hz = result->fundamental_hz;
            component.peak_amplitude_volts = g1_fft_measure_tone_peak(
                voltage_samples, sample_count, input->sample_rate_hz,
                result->fundamental_hz, result->dc_volts,
                &component.phase_radians);
            component.fitted_frequency_hz = result->fundamental_hz;
            component.harmonic_order = 1U;
            g1_fft_insert_component(result, &component);
        }
    }
    result->fundamental_hz = g1_fft_refine_fundamental_least_squares(result);
    if (result->fundamental_hz > 0.0f)
    {
        for (uint32_t index = 0U; index < result->component_count; ++index)
        {
            g1_fft_component_t *component = &result->components[index];

            if (component->harmonic_order != 0U)
            {
                component->fitted_frequency_hz =
                    (float)component->harmonic_order * result->fundamental_hz;
            }
        }
    }
    result->valid = (result->component_count != 0U);
    return result->valid;
}

bool G1_FFT_BLL_FitHarmonics(const float *voltage_samples,
                             uint32_t sample_count,
                             g1_fft_result_t *result)
{
    enum { G1_FIT_MAX_TERMS = 1 + (2 * G1_FFT_MAX_COMPONENTS) };
    float normal[G1_FIT_MAX_TERMS][G1_FIT_MAX_TERMS + 1U] = {{0.0f}};
    float basis[G1_FIT_MAX_TERMS];
    uint32_t component_indices[G1_FFT_MAX_COMPONENTS];
    uint32_t fit_component_count = 0U;
    uint32_t term_count;

    if ((voltage_samples == NULL) || (result == NULL) ||
        (sample_count == 0U) || (result->fundamental_hz <= 0.0f))
    {
        return false;
    }

    for (uint32_t index = 0U; index < result->component_count; ++index)
    {
        const g1_fft_component_t *component = &result->components[index];

        if ((component->harmonic_order != 0U) &&
            ((component->fitted_frequency_hz > 0.0f) ||
             (component->frequency_hz > 0.0f)))
        {
            component_indices[fit_component_count++] = index;
        }
    }

    if (fit_component_count == 0U)
    {
        return false;
    }

    term_count = 1U + (2U * fit_component_count);
    for (uint32_t sample = 0U; sample < sample_count; ++sample)
    {
        basis[0] = 1.0f;
        for (uint32_t component = 0U; component < fit_component_count; ++component)
        {
            const g1_fft_component_t *peak =
                &result->components[component_indices[component]];
            const float frequency = (peak->fitted_frequency_hz > 0.0f) ?
                peak->fitted_frequency_hz : peak->frequency_hz;
            const float phase = 6.28318530717958647692f * frequency *
                (float)sample / (result->bin_width_hz * (float)sample_count);

            basis[1U + (2U * component)] = cosf(phase);
            basis[2U + (2U * component)] = sinf(phase);
        }

        for (uint32_t row = 0U; row < term_count; ++row)
        {
            normal[row][term_count] += basis[row] * voltage_samples[sample];
            for (uint32_t column = 0U; column < term_count; ++column)
            {
                normal[row][column] += basis[row] * basis[column];
            }
        }
    }

    for (uint32_t pivot = 0U; pivot < term_count; ++pivot)
    {
        uint32_t best_row = pivot;
        float best_value = fabsf(normal[pivot][pivot]);

        for (uint32_t row = pivot + 1U; row < term_count; ++row)
        {
            const float value = fabsf(normal[row][pivot]);
            if (value > best_value)
            {
                best_value = value;
                best_row = row;
            }
        }
        if (best_value < 1.0e-10f)
        {
            return false;
        }
        if (best_row != pivot)
        {
            for (uint32_t column = pivot; column <= term_count; ++column)
            {
                const float value = normal[pivot][column];
                normal[pivot][column] = normal[best_row][column];
                normal[best_row][column] = value;
            }
        }

        {
            const float divisor = normal[pivot][pivot];
            for (uint32_t column = pivot; column <= term_count; ++column)
            {
                normal[pivot][column] /= divisor;
            }
        }
        for (uint32_t row = 0U; row < term_count; ++row)
        {
            float factor;
            if (row == pivot)
            {
                continue;
            }
            factor = normal[row][pivot];
            for (uint32_t column = pivot; column <= term_count; ++column)
            {
                normal[row][column] -= factor * normal[pivot][column];
            }
        }
    }

    result->fitted_dc_volts = normal[0][term_count];
    result->fitted_ac_rms_volts = 0.0f;
    for (uint32_t component = 0U; component < fit_component_count; ++component)
    {
        g1_fft_component_t *peak =
            &result->components[component_indices[component]];
        const float cosine = normal[1U + (2U * component)][term_count];
        const float sine = normal[2U + (2U * component)][term_count];
        const float amplitude = sqrtf((cosine * cosine) + (sine * sine));

        peak->peak_amplitude_volts = amplitude;
        peak->phase_radians = atan2f(-sine, cosine);
        result->fitted_ac_rms_volts += 0.5f * amplitude * amplitude;
    }
    result->fitted_ac_rms_volts = sqrtf(result->fitted_ac_rms_volts);

    {
        const uint32_t reconstruction_points = 4096U;
        float minimum = 0.0f;
        float maximum = 0.0f;

        for (uint32_t point = 0U; point < reconstruction_points; ++point)
        {
            float value = result->fitted_dc_volts;
            const float time = (float)point /
                (result->fundamental_hz * (float)reconstruction_points);

            for (uint32_t component = 0U; component < fit_component_count; ++component)
            {
                const float cosine = normal[1U + (2U * component)][term_count];
                const float sine = normal[2U + (2U * component)][term_count];
                const g1_fft_component_t *peak =
                    &result->components[component_indices[component]];
                const float frequency = (peak->fitted_frequency_hz > 0.0f) ?
                    peak->fitted_frequency_hz : peak->frequency_hz;
                const float phase = 6.28318530717958647692f * frequency * time;

                value += (cosine * cosf(phase)) + (sine * sinf(phase));
            }
            if ((point == 0U) || (value < minimum))
            {
                minimum = value;
            }
            if ((point == 0U) || (value > maximum))
            {
                maximum = value;
            }
        }
        result->fitted_vpp_volts = maximum - minimum;
    }

    result->fitted_waveform_valid = true;
    return true;
}
