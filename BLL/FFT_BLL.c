#include "FFT_BLL.h"

#include "FFT_FML.h"
#include <math.h>
#include <string.h>

#define FFT_SECOND_PEAK_GUARD_BINS 3U
#define FFT_MAX_THD_HARMONIC 5U
#define FFT_MIN_SIGNAL_VPEAK 0.010f
#define FFT_FINE_SEARCH_ITERATIONS 14U

fft_result_t fft_result;
static float fft_sample_rate_hz = FFT_SAMPLE_RATE;

static float fft_interpolate_peak_bin(uint32_t index);

static float fft_bin_to_hz(float bin)
{
    return bin * fft_sample_rate_hz / (float)FFT_LENGTH;
}

static float fft_windowed_tone_power(const float *time_data, uint32_t data_len, float bin)
{
    float angle;
    float rotate_real;
    float rotate_imag;
    float oscillator_real = 1.0f;
    float oscillator_imag = 0.0f;
    float sum_real = 0.0f;
    float sum_imag = 0.0f;

    if (time_data == NULL || data_len < FFT_LENGTH || bin <= 0.0f ||
        bin >= (float)(FFT_LENGTH / 2U))
    {
        return 0.0f;
    }

    angle = 2.0f * PI * bin / (float)FFT_LENGTH;
    rotate_real = cosf(angle);
    rotate_imag = -sinf(angle);

    for (uint32_t i = 0U; i < FFT_LENGTH; i++)
    {
        float sample = time_data[i] * window_buffer[i];
        float next_real;

        sum_real += sample * oscillator_real;
        sum_imag += sample * oscillator_imag;
        next_real = oscillator_real * rotate_real - oscillator_imag * rotate_imag;
        oscillator_imag = oscillator_real * rotate_imag + oscillator_imag * rotate_real;
        oscillator_real = next_real;
    }

    return sum_real * sum_real + sum_imag * sum_imag;
}

static float fft_refine_peak_bin(const float *time_data, uint32_t data_len, uint32_t peak_index)
{
    float left;
    float right;

    if (time_data == NULL || data_len < FFT_LENGTH || peak_index <= DC_INDEX ||
        peak_index + 1U >= (FFT_LENGTH / 2U))
    {
        return fft_interpolate_peak_bin(peak_index);
    }

    left = (float)peak_index - 0.5f;
    right = (float)peak_index + 0.5f;

    /* Hann 窗主瓣在该区间内单峰，用三分搜索直接寻找连续频率峰值。 */
    for (uint32_t i = 0U; i < FFT_FINE_SEARCH_ITERATIONS; i++)
    {
        float third = (right - left) / 3.0f;
        float point_1 = left + third;
        float point_2 = right - third;

        if (fft_windowed_tone_power(time_data, data_len, point_1) <
            fft_windowed_tone_power(time_data, data_len, point_2))
        {
            left = point_1;
        }
        else
        {
            right = point_2;
        }
    }

    return 0.5f * (left + right);
}

static float fft_band_power(uint32_t center, uint32_t radius)
{
    uint32_t first = (center > radius) ? (center - radius) : DC_INDEX;
    uint32_t last = center + radius;
    float power = 0.0f;

    if (first < DC_INDEX)
    {
        first = DC_INDEX;
    }
    if (last >= (FFT_LENGTH / 2U))
    {
        last = (FFT_LENGTH / 2U) - 1U;
    }

    for (uint32_t i = first; i <= last; i++)
    {
        power += fft_out[i] * fft_out[i];
    }
    return power;
}

static uint32_t fft_find_harmonic_peak(float expected_bin)
{
    int32_t center = (int32_t)(expected_bin + 0.5f);
    int32_t first = center - 2;
    int32_t last = center + 2;
    uint32_t peak_index;
    float peak_value;

    if (first < (int32_t)DC_INDEX)
    {
        first = (int32_t)DC_INDEX;
    }
    if (last >= (int32_t)(FFT_LENGTH / 2U))
    {
        last = (int32_t)(FFT_LENGTH / 2U) - 1;
    }

    peak_index = (uint32_t)first;
    peak_value = fft_out[peak_index];
    for (int32_t i = first + 1; i <= last; i++)
    {
        if (fft_out[i] > peak_value)
        {
            peak_value = fft_out[i];
            peak_index = (uint32_t)i;
        }
    }
    return peak_index;
}

static void fft_update_thd_and_waveform(const float *time_data, uint32_t data_len)
{
    float harmonic_power[FFT_MAX_THD_HARMONIC + 1U] = {0.0f};
    float fundamental_power;
    float thd_power = 0.0f;
    uint32_t harmonic_count = 0U;
    uint32_t max_harmonic;

    fft_result.thd_percent = 0.0f;
    fft_result.harmonic_2_ratio = 0.0f;
    fft_result.harmonic_3_ratio = 0.0f;
    fft_result.harmonic_5_ratio = 0.0f;
    fft_result.thd_valid = 0U;
    fft_result.waveform = FFT_WAVE_NONE;

    if (fft_result.peak_value < FFT_MIN_SIGNAL_VPEAK)
    {
        return;
    }

    fft_result.signal_valid = 1U;
    fft_result.waveform = FFT_WAVE_UNKNOWN;

    /* Hann 主瓣按中心 bin 左右各 1 点积分；过低频时谐波带会重叠，不给出 THD。 */
    if (fft_result.peak_index < 4U || fft_result.peak_bin <= 0.0f)
    {
        return;
    }

    if (time_data != NULL && data_len >= FFT_LENGTH)
    {
        fundamental_power = fft_windowed_tone_power(time_data, data_len, fft_result.peak_bin);
    }
    else
    {
        fundamental_power = fft_band_power(fft_result.peak_index, 1U);
    }
    if (fundamental_power <= 1.0e-12f)
    {
        return;
    }

    max_harmonic = (uint32_t)(((float)(FFT_LENGTH / 2U - 2U)) / fft_result.peak_bin);
    if (max_harmonic > FFT_MAX_THD_HARMONIC)
    {
        max_harmonic = FFT_MAX_THD_HARMONIC;
    }

    for (uint32_t harmonic = 2U; harmonic <= max_harmonic; harmonic++)
    {
        float harmonic_bin = fft_result.peak_bin * (float)harmonic;

        if (time_data != NULL && data_len >= FFT_LENGTH)
        {
            harmonic_power[harmonic] = fft_windowed_tone_power(time_data, data_len, harmonic_bin);
        }
        else
        {
            uint32_t index = fft_find_harmonic_peak(harmonic_bin);
            harmonic_power[harmonic] = fft_band_power(index, 1U);
        }
        thd_power += harmonic_power[harmonic];
        harmonic_count++;
    }

    if (harmonic_count == 0U)
    {
        return;
    }

    fft_result.thd_percent = sqrtf(thd_power / fundamental_power) * 100.0f;
    fft_result.thd_valid = 1U;

    if (max_harmonic >= 2U)
    {
        fft_result.harmonic_2_ratio = sqrtf(harmonic_power[2U] / fundamental_power);
    }
    if (max_harmonic >= 3U)
    {
        fft_result.harmonic_3_ratio = sqrtf(harmonic_power[3U] / fundamental_power);
    }
    if (max_harmonic >= 5U)
    {
        fft_result.harmonic_5_ratio = sqrtf(harmonic_power[5U] / fundamental_power);
    }

    if (fft_result.thd_percent < 5.0f)
    {
        fft_result.waveform = FFT_WAVE_SINE;
    }
    else if (max_harmonic >= 5U && fft_result.harmonic_2_ratio < 0.15f)
    {
        if (fft_result.harmonic_3_ratio > 0.20f && fft_result.harmonic_5_ratio > 0.08f)
        {
            fft_result.waveform = FFT_WAVE_SQUARE;
        }
        else if (fft_result.harmonic_3_ratio >= 0.055f &&
                 fft_result.harmonic_3_ratio <= 0.18f &&
                 fft_result.harmonic_5_ratio < 0.08f)
        {
            fft_result.waveform = FFT_WAVE_TRIANGLE;
        }
    }
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

static void fft_update_result(const float *time_data, uint32_t data_len)
{
    memset(&fft_result, 0, sizeof(fft_result));
    fft_result.peak_index = get_fft_max_index(fft_out, FFT_LENGTH);
    fft_result.second_peak_index = get_fft_second_max_index(fft_out, FFT_LENGTH, FFT_SECOND_PEAK_GUARD_BINS);
    fft_result.peak_bin = fft_refine_peak_bin(time_data, data_len, fft_result.peak_index);
    fft_result.second_peak_bin = fft_interpolate_peak_bin(fft_result.second_peak_index);

    fft_result.peak_value = (fft_result.peak_index < (FFT_LENGTH / 2U)) ? fft_out[fft_result.peak_index] : 0.0f;
    fft_result.second_peak_value = (fft_result.second_peak_index < (FFT_LENGTH / 2U)) ? fft_out[fft_result.second_peak_index] : 0.0f;

    fft_result.freq_hz = fft_bin_to_hz(fft_result.peak_bin);
    fft_result.second_freq_hz = fft_bin_to_hz(fft_result.second_peak_bin);
    fft_result.vpp = get_fft_vpp(fft_out, FFT_LENGTH);
    fft_result.phase_rad = get_fft_max_phase(fft_input_buffer, fft_out, FFT_LENGTH);
    fft_update_thd_and_waveform(time_data, data_len);

    if (fft_result.signal_valid == 0U)
    {
        fft_result.freq_hz = 0.0f;
        fft_result.vpp = 0.0f;
    }
}

void FFT_BLL_UpdateResult(void)
{
    fft_update_result(NULL, 0U);
}

void FFT_BLL_UpdateResultFromSamples(const float *time_data, uint32_t data_len)
{
    fft_update_result(time_data, data_len);
}

void FFT_BLL_SetSampleRateHz(float sample_rate_hz)
{
    if (sample_rate_hz > 0.0f)
    {
        fft_sample_rate_hz = sample_rate_hz;
    }
}

const char *FFT_BLL_GetWaveformName(void)
{
    switch ((fft_waveform_t)fft_result.waveform)
    {
    case FFT_WAVE_NONE:
        return "none";
    case FFT_WAVE_SINE:
        return "sine";
    case FFT_WAVE_SQUARE:
        return "square";
    case FFT_WAVE_TRIANGLE:
        return "triangle";
    default:
        return "unknown";
    }
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
