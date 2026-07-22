#include "AD9226_BLL.h"

#include "AD9226.h"
#include "arm_const_structs.h"
#include "arm_math.h"

#include <string.h>

#define AD9226_FFT_LENGTH       1024UL
#define AD9226_MAX_CROSSINGS    6UL
#define AD9226_MAX_PERIODS      3UL
#define AD9226_MIN_PERIOD       800.0f
#define AD9226_MAX_PERIOD       1200.0f
#define AD9226_MIN_SPAN_CODES   40U
#define AD9226_LSB_VOLTS        (10.0f / 4096.0f)
#define AD9226_INV_SQRT_TWO     0.70710678118f

static float32_t s_fft_buffer[AD9226_FFT_LENGTH * 2UL]
    __attribute__((aligned(32)));

static float32_t AD9226_Interpolate(const uint16_t *samples,
                                    uint32_t sample_count,
                                    float32_t position)
{
    uint32_t index = (uint32_t)position;
    float32_t fraction;
    float32_t first;
    float32_t second;

    if (index >= (sample_count - 1UL))
    {
        return (float32_t)samples[sample_count - 1UL];
    }

    fraction = position - (float32_t)index;
    first = (float32_t)samples[index];
    second = (float32_t)samples[index + 1UL];
    return first + ((second - first) * fraction);
}

HAL_StatusTypeDef AD9226_BLL_Analyze(const uint16_t *samples,
                                     uint32_t sample_count,
                                     AD9226_AnalysisResult *result)
{
    float32_t crossings[AD9226_MAX_CROSSINGS];
    float32_t center;
    float32_t hysteresis;
    float32_t low_threshold;
    float32_t high_threshold;
    float32_t period_sum = 0.0f;
    float32_t cycle_mean = 0.0f;
    float32_t magnitude[5];
    float32_t harmonic_power = 0.0f;
    uint64_t code_sum = 0ULL;
    uint16_t minimum = 0x0FFFU;
    uint16_t maximum = 0U;
    uint32_t crossing_count = 0UL;
    uint32_t period_count = 0UL;
    uint8_t was_below = 0U;

    if ((samples == NULL) || (result == NULL) ||
        (sample_count < AD9226_CAPTURE_SAMPLES))
    {
        return HAL_ERROR;
    }

    memset(result, 0, sizeof(*result));
    memset(s_fft_buffer, 0, sizeof(s_fft_buffer));

    for (uint32_t i = 0UL; i < AD9226_CAPTURE_SAMPLES; i++)
    {
        uint16_t sample = samples[i];

        code_sum += sample;
        if (sample < minimum)
        {
            minimum = sample;
        }
        if (sample > maximum)
        {
            maximum = sample;
        }
    }

    result->minimum_code = minimum;
    result->maximum_code = maximum;
    result->mean_code = (float32_t)code_sum /
                        (float32_t)AD9226_CAPTURE_SAMPLES;

    if (((uint32_t)maximum - (uint32_t)minimum) < AD9226_MIN_SPAN_CODES)
    {
        return HAL_ERROR;
    }

    center = result->mean_code;
    hysteresis = (float32_t)(maximum - minimum) * 0.05f;
    if (hysteresis < 4.0f)
    {
        hysteresis = 4.0f;
    }
    low_threshold = center - hysteresis;
    high_threshold = center + hysteresis;

    for (uint32_t i = 0UL;
         (i < AD9226_CAPTURE_SAMPLES) &&
         (crossing_count < AD9226_MAX_CROSSINGS);
         i++)
    {
        float32_t sample = (float32_t)samples[i];

        if (sample < low_threshold)
        {
            was_below = 1U;
        }
        else if ((was_below != 0U) && (sample >= high_threshold) && (i != 0UL))
        {
            uint32_t lower_index = i - 1UL;
            float32_t lower;
            float32_t upper;
            float32_t crossing;

            while ((lower_index != 0UL) &&
                   ((float32_t)samples[lower_index] > center))
            {
                lower_index--;
            }

            lower = (float32_t)samples[lower_index];
            upper = (float32_t)samples[lower_index + 1UL];
            if (upper > lower)
            {
                crossing = (float32_t)lower_index +
                           ((center - lower) / (upper - lower));
                if ((crossing_count == 0UL) ||
                    ((crossing - crossings[crossing_count - 1UL]) >
                     (AD9226_MIN_PERIOD * 0.5f)))
                {
                    crossings[crossing_count++] = crossing;
                }
            }
            was_below = 0U;
        }
    }

    for (uint32_t i = 0UL;
         ((i + 1UL) < crossing_count) && (period_count < AD9226_MAX_PERIODS);
         i++)
    {
        float32_t period = crossings[i + 1UL] - crossings[i];

        if ((period < AD9226_MIN_PERIOD) ||
            (period > AD9226_MAX_PERIOD) ||
            ((crossings[i] + period) >=
             (float32_t)(AD9226_CAPTURE_SAMPLES - 1UL)))
        {
            continue;
        }

        for (uint32_t point = 0UL; point < AD9226_FFT_LENGTH; point++)
        {
            float32_t position = crossings[i] +
                (((float32_t)point * period) / (float32_t)AD9226_FFT_LENGTH);

            s_fft_buffer[2UL * point] +=
                AD9226_Interpolate(samples, sample_count, position);
        }
        period_sum += period;
        period_count++;
    }

    if (period_count == 0UL)
    {
        return HAL_ERROR;
    }

    for (uint32_t point = 0UL; point < AD9226_FFT_LENGTH; point++)
    {
        s_fft_buffer[2UL * point] /= (float32_t)period_count;
        cycle_mean += s_fft_buffer[2UL * point];
    }
    cycle_mean /= (float32_t)AD9226_FFT_LENGTH;

    for (uint32_t point = 0UL; point < AD9226_FFT_LENGTH; point++)
    {
        s_fft_buffer[2UL * point] -= cycle_mean;
        s_fft_buffer[(2UL * point) + 1UL] = 0.0f;
    }

    arm_cfft_f32(&arm_cfft_sR_f32_len1024, s_fft_buffer, 0U, 1U);

    for (uint32_t harmonic = 0UL; harmonic < 5UL; harmonic++)
    {
        uint32_t bin = harmonic + 1UL;
        float32_t real = s_fft_buffer[2UL * bin];
        float32_t imaginary = s_fft_buffer[(2UL * bin) + 1UL];
        float32_t magnitude_squared = (real * real) + (imaginary * imaginary);

        if (arm_sqrt_f32(magnitude_squared, &magnitude[harmonic]) !=
            ARM_MATH_SUCCESS)
        {
            return HAL_ERROR;
        }

        result->harmonic_rms_v[harmonic] =
            ((2.0f * magnitude[harmonic]) / (float32_t)AD9226_FFT_LENGTH) *
            AD9226_LSB_VOLTS * AD9226_INV_SQRT_TWO;
    }

    if (magnitude[0] < 1.0f)
    {
        return HAL_ERROR;
    }

    for (uint32_t harmonic = 1UL; harmonic < 5UL; harmonic++)
    {
        harmonic_power += magnitude[harmonic] * magnitude[harmonic];
    }

    if (arm_sqrt_f32(harmonic_power, &result->thd_percent) !=
        ARM_MATH_SUCCESS)
    {
        return HAL_ERROR;
    }

    result->thd_percent = (result->thd_percent / magnitude[0]) * 100.0f;
    result->frequency_hz = (float32_t)AD9226_SAMPLE_RATE_HZ /
        (period_sum / (float32_t)period_count);
    result->averaged_periods = period_count;
    return HAL_OK;
}
