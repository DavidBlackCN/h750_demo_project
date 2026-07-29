#include "G1_FFT_FML.h"

#include <math.h>

static bool g1_fft_is_power_of_two(uint32_t value)
{
    return (value != 0U) && ((value & (value - 1U)) == 0U);
}

bool G1_FFT_FML_TransformReal(const float *samples,
                              uint32_t sample_count,
                              float *real,
                              float *imaginary)
{
    uint32_t reversed = 0U;

    if ((samples == NULL) || (real == NULL) || (imaginary == NULL) ||
        (sample_count > G1_FFT_FML_MAX_SAMPLES) ||
        !g1_fft_is_power_of_two(sample_count))
    {
        return false;
    }

    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        real[index] = samples[index];
        imaginary[index] = 0.0f;
    }

    for (uint32_t index = 1U; index < sample_count; ++index)
    {
        uint32_t bit = sample_count >> 1U;

        while ((reversed & bit) != 0U)
        {
            reversed &= ~bit;
            bit >>= 1U;
        }
        reversed |= bit;

        if (index < reversed)
        {
            float value = real[index];
            real[index] = real[reversed];
            real[reversed] = value;
            value = imaginary[index];
            imaginary[index] = imaginary[reversed];
            imaginary[reversed] = value;
        }
    }

    for (uint32_t length = 2U; length <= sample_count; length <<= 1U)
    {
        const float angle = -6.28318530717958647692f / (float)length;
        const float step_real = cosf(angle);
        const float step_imaginary = sinf(angle);
        const uint32_t half_length = length >> 1U;

        for (uint32_t start = 0U; start < sample_count; start += length)
        {
            float twiddle_real = 1.0f;
            float twiddle_imaginary = 0.0f;

            for (uint32_t offset = 0U; offset < half_length; ++offset)
            {
                const uint32_t even_index = start + offset;
                const uint32_t odd_index = even_index + half_length;
                const float odd_real = (twiddle_real * real[odd_index]) -
                                       (twiddle_imaginary * imaginary[odd_index]);
                const float odd_imaginary = (twiddle_real * imaginary[odd_index]) +
                                            (twiddle_imaginary * real[odd_index]);
                const float next_twiddle_real = (twiddle_real * step_real) -
                                                (twiddle_imaginary * step_imaginary);

                real[odd_index] = real[even_index] - odd_real;
                imaginary[odd_index] = imaginary[even_index] - odd_imaginary;
                real[even_index] += odd_real;
                imaginary[even_index] += odd_imaginary;
                twiddle_imaginary = (twiddle_real * step_imaginary) +
                                    (twiddle_imaginary * step_real);
                twiddle_real = next_twiddle_real;
            }
        }
    }

    return true;
}
