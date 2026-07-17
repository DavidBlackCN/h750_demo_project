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
    float thd_percent;
    float harmonic_2_ratio;
    float harmonic_3_ratio;
    float harmonic_5_ratio;
    uint8_t signal_valid;
    uint8_t thd_valid;
    uint8_t waveform;
} fft_result_t;

typedef enum
{
    FFT_WAVE_NONE = 0,
    FFT_WAVE_SINE,
    FFT_WAVE_SQUARE,
    FFT_WAVE_TRIANGLE,
    FFT_WAVE_UNKNOWN
} fft_waveform_t;

extern fft_result_t fft_result;

void FFT_BLL_UpdateResult(void);
void FFT_BLL_UpdateResultFromSamples(const float *time_data, uint32_t data_len);
void FFT_BLL_SetSampleRateHz(float sample_rate_hz);
const char *FFT_BLL_GetWaveformName(void);
uint32_t FFT_BLL_GetPeakIndex(void);
float FFT_BLL_GetPeakValue(void);
float FFT_BLL_GetPeakFreqHz(void);
float FFT_BLL_GetVpp(void);

#endif
