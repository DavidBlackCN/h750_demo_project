#include "G1_FIR_BLL.h"

/* Kaiser-windowed sinc low-pass, Fs=3.2 MHz, Fc=700 kHz, beta=7.86.
   Calculated response: about 0.001 dB at 500 kHz and -90.7 dB at 1 MHz. */
static const float s_coefficients[G1_FIR_BLL_TAP_COUNT] = {
    -0.000026846523f, -0.000024338114f,  0.000099352788f,
     0.000142179751f, -0.000175222081f, -0.000434756184f,
     0.000131860828f,  0.000941813034f,  0.000249725012f,
    -0.001573978370f, -0.001237346320f,  0.002027047950f,
     0.003026699690f, -0.001748045000f, -0.005563667190f,
     0.0f,             0.008360601360f,  0.003957085770f,
    -0.010373443600f, -0.010600832600f,  0.009982554350f,
     0.019877860000f, -0.005032000610f, -0.031038467200f,
    -0.007340381100f,  0.042664505800f,  0.032128748800f,
    -0.052921193800f, -0.085233939300f,  0.059981542800f,
     0.311002990000f,  0.437499780000f,  0.311002990000f,
     0.059981542800f, -0.085233939300f, -0.052921193800f,
     0.032128748800f,  0.042664505800f, -0.007340381100f,
    -0.031038467200f, -0.005032000610f,  0.019877860000f,
     0.009982554350f, -0.010600832600f, -0.010373443600f,
     0.003957085770f,  0.008360601360f,  0.0f,
    -0.005563667190f, -0.001748045000f,  0.003026699690f,
     0.002027047950f, -0.001237346320f, -0.001573978370f,
     0.000249725012f,  0.000941813034f,  0.000131860828f,
    -0.000434756184f, -0.000175222081f,  0.000142179751f,
     0.000099352788f, -0.000024338114f, -0.000026846523f,
};

static uint32_t g1_fir_mirror_index(int32_t index, uint32_t sample_count)
{
    if (index < 0)
    {
        return (uint32_t)(-index);
    }
    if ((uint32_t)index >= sample_count)
    {
        return (2U * sample_count) - 2U - (uint32_t)index;
    }
    return (uint32_t)index;
}

bool G1_FIR_BLL_FilterFrame(const float *input,
                            float *output,
                            uint32_t sample_count)
{
    const uint32_t half_taps = (G1_FIR_BLL_TAP_COUNT - 1U) / 2U;

    if ((input == NULL) || (output == NULL) ||
        (sample_count < G1_FIR_BLL_TAP_COUNT))
    {
        return false;
    }

    for (uint32_t sample = 0U; sample < sample_count; ++sample)
    {
        float filtered = s_coefficients[half_taps] * input[sample];

        for (uint32_t tap = 0U; tap < half_taps; ++tap)
        {
            const int32_t offset = (int32_t)(half_taps - tap);
            const uint32_t left = g1_fir_mirror_index((int32_t)sample - offset,
                                                       sample_count);
            const uint32_t right = g1_fir_mirror_index((int32_t)sample + offset,
                                                        sample_count);

            filtered += s_coefficients[tap] * (input[left] + input[right]);
        }

        output[sample] = filtered;
    }

    return true;
}
