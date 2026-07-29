#ifndef G1_FFT_API_H
#define G1_FFT_API_H

#include "G1_FFT_BLL.h"

/* ADC, DMA and calibration are intentionally owned by the caller. */
bool G1_FFT_API_Analyze(const float *voltage_samples,
                        uint32_t sample_count,
                        const g1_fft_input_t *input,
                        g1_fft_result_t *result);

#endif
