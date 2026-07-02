#ifndef __FFT_BLL_H__
#define __FFT_BLL_H__

#include "main.h"

typedef struct
{
    uint32_t peak_index;
    uint32_t second_peak_index;
    float peak_bin;
    float second_peak_bin;
    float peak_value;
    float second_peak_value;
    float freq_hz;
    float second_freq_hz;
    float vpp;
    float phase_rad;
} fft_result_t;

extern fft_result_t fft_result;

void FFT_BLL_UpdateResult(void);
uint32_t FFT_BLL_GetPeakIndex(void);
float FFT_BLL_GetPeakValue(void);
float FFT_BLL_GetPeakFreqHz(void);
float FFT_BLL_GetVpp(void);

#endif
