#include "ADC2_FFT_FML.h"

#include <math.h>

HAL_StatusTypeDef ADC2_FFT_FML_TransformReal(float *real,
                                              float *imaginary,
                                              uint32_t sample_count,
                                              float *half_magnitude)
{
    uint32_t reversed = 0U;

    if ((real == NULL) || (imaginary == NULL) || (half_magnitude == NULL) ||
        (sample_count != ADC2_FFT_LENGTH))
    {
        return HAL_ERROR;
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
        float angle = -6.28318530717958647692f / (float)length;
        float step_real = cosf(angle);
        float step_imaginary = sinf(angle);
        uint32_t half_length = length >> 1U;

        for (uint32_t start = 0U; start < sample_count; start += length)
        {
            float twiddle_real = 1.0f;
            float twiddle_imaginary = 0.0f;

            for (uint32_t offset = 0U; offset < half_length; ++offset)
            {
                uint32_t even_index = start + offset;
                uint32_t odd_index = even_index + half_length;
                float odd_real = (twiddle_real * real[odd_index]) -
                                 (twiddle_imaginary * imaginary[odd_index]);
                float odd_imaginary = (twiddle_real * imaginary[odd_index]) +
                                      (twiddle_imaginary * real[odd_index]);
                float next_twiddle_real;

                real[odd_index] = real[even_index] - odd_real;
                imaginary[odd_index] = imaginary[even_index] - odd_imaginary;
                real[even_index] += odd_real;
                imaginary[even_index] += odd_imaginary;

                next_twiddle_real = (twiddle_real * step_real) -
                                    (twiddle_imaginary * step_imaginary);
                twiddle_imaginary = (twiddle_real * step_imaginary) +
                                    (twiddle_imaginary * step_real);
                twiddle_real = next_twiddle_real;
            }
        }
    }

    half_magnitude[0] = fabsf(real[0]) / (float)sample_count;
    for (uint32_t bin = 1U; bin < (ADC2_FFT_LENGTH / 2U); ++bin)
    {
        float re = real[bin];
        float im = imaginary[bin];

        half_magnitude[bin] = (2.0f * sqrtf((re * re) + (im * im))) /
                              (float)sample_count;
    }

    return HAL_OK;
}
