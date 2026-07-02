#include "FFT_BLL.h"

#include "FFT_FML.h"

#define FFT_SECOND_PEAK_GUARD_BINS 3U

fft_result_t fft_result;

static float fft_bin_to_hz(float bin)
{
    return bin * FFT_SAMPLE_RATE / (float)FFT_LENGTH;
}

static float fft_interpolate_peak_bin(uint32_t index)
{
    float y0;
    float y1;
    float y2;
    float denom;
    float delta;

    if (index <= DC_INDEX || index + 1U >= (FFT_LENGTH / 2U))
    {
        return (float)index;
    }

    y0 = fft_out[index - 1U];
    y1 = fft_out[index];
    y2 = fft_out[index + 1U];
    denom = y0 - (2.0f * y1) + y2;

    if (denom > -1.0e-12f && denom < 1.0e-12f)
    {
        return (float)index;
    }

    delta = 0.5f * (y0 - y2) / denom;
    if (delta > 0.5f)
    {
        delta = 0.5f;
    }
    else if (delta < -0.5f)
    {
        delta = -0.5f;
    }

    return (float)index + delta;
}

void FFT_BLL_UpdateResult(void)
{
    fft_result.peak_index = get_fft_max_index(fft_out, FFT_LENGTH);
    fft_result.second_peak_index = get_fft_second_max_index(fft_out, FFT_LENGTH, FFT_SECOND_PEAK_GUARD_BINS);
    fft_result.peak_bin = fft_interpolate_peak_bin(fft_result.peak_index);
    fft_result.second_peak_bin = fft_interpolate_peak_bin(fft_result.second_peak_index);

    fft_result.peak_value = (fft_result.peak_index < (FFT_LENGTH / 2U)) ? fft_out[fft_result.peak_index] : 0.0f;
    fft_result.second_peak_value = (fft_result.second_peak_index < (FFT_LENGTH / 2U)) ? fft_out[fft_result.second_peak_index] : 0.0f;

    fft_result.freq_hz = fft_bin_to_hz(fft_result.peak_bin);
    fft_result.second_freq_hz = fft_bin_to_hz(fft_result.second_peak_bin);
    fft_result.vpp = get_fft_vpp(fft_out, FFT_LENGTH);
    fft_result.phase_rad = get_fft_max_phase(fft_input_buffer, fft_out, FFT_LENGTH);
}

uint32_t FFT_BLL_GetPeakIndex(void)
{
    return fft_result.peak_index;
}

float FFT_BLL_GetPeakValue(void)
{
    return fft_result.peak_value;
}

float FFT_BLL_GetPeakFreqHz(void)
{
    return fft_result.freq_hz;
}

float FFT_BLL_GetVpp(void)
{
    return fft_result.vpp;
}
