#include "SUPER_FFT_FML.h"

#include <math.h>

#define SUPER_FFT_FML_TWO_PI          6.28318530717958647692f
#define SUPER_FFT_FML_PHASE_RESET_STEP 64U

static float s_hanning_window[SUPER_FFT_FML_LENGTH];
static uint8_t s_window_initialized;

void SUPER_FFT_FML_InitWindow(void)
{
    if (s_window_initialized != 0U)
    {
        return;
    }

    for (uint32_t index = 0U; index < SUPER_FFT_FML_LENGTH; ++index)
    {
        s_hanning_window[index] = 0.5f *
                                  (1.0f - cosf(SUPER_FFT_FML_TWO_PI *
                                                (float)index /
                                                (float)(SUPER_FFT_FML_LENGTH - 1U)));
    }

    s_window_initialized = 1U;
}

void SUPER_FFT_FML_AccumulatePower(const float *samples,
                                   uint32_t first_bin,
                                   uint32_t last_bin,
                                   float *power_spectrum)
{
    if ((samples == NULL) || (power_spectrum == NULL) ||
        (first_bin == 0U) || (last_bin < first_bin) ||
        (last_bin >= (SUPER_FFT_FML_LENGTH / 2U)))
    {
        return;
    }

    SUPER_FFT_FML_InitWindow();

    for (uint32_t bin = first_bin; bin <= last_bin; ++bin)
    {
        const float phase_step = -SUPER_FFT_FML_TWO_PI * (float)bin /
                                 (float)SUPER_FFT_FML_LENGTH;
        const float step_real = cosf(phase_step);
        const float step_imaginary = sinf(phase_step);
        float phase_real = 1.0f;
        float phase_imaginary = 0.0f;
        float accum_real = 0.0f;
        float accum_imaginary = 0.0f;

        for (uint32_t index = 0U; index < SUPER_FFT_FML_LENGTH; ++index)
        {
            float next_phase_real;

            if ((index % SUPER_FFT_FML_PHASE_RESET_STEP) == 0U)
            {
                const float exact_phase = phase_step * (float)index;

                phase_real = cosf(exact_phase);
                phase_imaginary = sinf(exact_phase);
            }

            accum_real += samples[index] * s_hanning_window[index] * phase_real;
            accum_imaginary += samples[index] * s_hanning_window[index] *
                               phase_imaginary;

            next_phase_real = (phase_real * step_real) -
                              (phase_imaginary * step_imaginary);
            phase_imaginary = (phase_real * step_imaginary) +
                              (phase_imaginary * step_real);
            phase_real = next_phase_real;
        }

        power_spectrum[bin] += (accum_real * accum_real) +
                               (accum_imaginary * accum_imaginary);
    }
}
