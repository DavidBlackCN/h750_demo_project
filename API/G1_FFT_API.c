#include "G1_FFT_API.h"

bool G1_FFT_API_Analyze(const float *voltage_samples,
                        uint32_t sample_count,
                        const g1_fft_input_t *input,
                        g1_fft_result_t *result)
{
    return G1_FFT_BLL_Analyze(voltage_samples, sample_count, input, result);
}
