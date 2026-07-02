#include "FFT_FML.h"

#include "arm_const_structs.h"
#include <math.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

float fft_input_buffer[FFT_LENGTH * 2U];
float fft_magnitude[FFT_LENGTH / 2U];
float window_buffer[FFT_LENGTH];
float fft_out[FFT_LENGTH / 2U];

void generate_hanning_window(void)
{
    for (uint32_t i = 0; i < FFT_LENGTH; i++)
    {
        window_buffer[i] = 0.5f * (1.0f - cosf(2.0f * PI * (float)i / (float)(FFT_LENGTH - 1U)));
    }
}

static void run_cfft_1024(float *fft_complex_buf)
{
    arm_cfft_f32(&arm_cfft_sR_f32_len1024, fft_complex_buf, 0, 1);
}

void calculate_fft(float *input_data, uint32_t data_len)
{
    calculate_fft_fml(input_data, fft_input_buffer, fft_magnitude, fft_out, FFT_LENGTH, data_len);
}

void calculate_fft_fml(float *input_data, float *fft_complex_buf, float *fft_mag_buf,
                       float *fft_out_buf, uint16_t fft_len, uint32_t data_len)
{
    uint32_t actual_length = data_len;

    if (fft_len != FFT_LENGTH)
    {
        return;
    }

    if (actual_length > fft_len)
    {
        actual_length = fft_len;
    }

    memset(fft_complex_buf, 0, (uint32_t)fft_len * 2U * sizeof(float));
    memset(fft_mag_buf, 0, (uint32_t)fft_len / 2U * sizeof(float));
    memset(fft_out_buf, 0, (uint32_t)fft_len / 2U * sizeof(float));

    for (uint32_t i = 0; i < actual_length; i++)
    {
        fft_complex_buf[2U * i] = input_data[i] * window_buffer[i];
        fft_complex_buf[2U * i + 1U] = 0.0f;
    }

    run_cfft_1024(fft_complex_buf);
    arm_cmplx_mag_f32(fft_complex_buf, fft_mag_buf, (uint32_t)fft_len / 2U);

    for (uint32_t i = 0; i < (uint32_t)fft_len / 2U; i++)
    {
        float scale = (i == 0U) ? (1.0f / (float)fft_len) : (2.0f / (float)fft_len);
        fft_mag_buf[i] = fft_mag_buf[i] * scale * 1.5f;
        fft_out_buf[i] = sqrtf(fft_mag_buf[i]);
    }
}

void calculate_fft_fml_no_window(float *input_data, float *fft_complex_buf, float *fft_mag_buf,
                                 float *fft_out_buf, uint16_t fft_len, uint32_t data_len)
{
    uint32_t actual_length = data_len;

    if (fft_len != FFT_LENGTH)
    {
        return;
    }

    if (actual_length > fft_len)
    {
        actual_length = fft_len;
    }

    memset(fft_complex_buf, 0, (uint32_t)fft_len * 2U * sizeof(float));
    memset(fft_mag_buf, 0, (uint32_t)fft_len / 2U * sizeof(float));
    memset(fft_out_buf, 0, (uint32_t)fft_len / 2U * sizeof(float));

    for (uint32_t i = 0; i < actual_length; i++)
    {
        fft_complex_buf[2U * i] = input_data[i];
        fft_complex_buf[2U * i + 1U] = 0.0f;
    }

    run_cfft_1024(fft_complex_buf);
    arm_cmplx_mag_f32(fft_complex_buf, fft_mag_buf, (uint32_t)fft_len / 2U);

    for (uint32_t i = 0; i < (uint32_t)fft_len / 2U; i++)
    {
        float scale = (i == 0U) ? (1.0f / (float)fft_len) : (2.0f / (float)fft_len);
        fft_mag_buf[i] = fft_mag_buf[i] * scale;
        fft_out_buf[i] = sqrtf(fft_mag_buf[i]);
    }
}

float get_fft_vpp(float *fft_out_buf, uint32_t fft_len)
{
    uint32_t len = fft_len / 2U;
    float max_value = 0.0f;
    uint32_t max_index = 0;

    if (len <= DC_INDEX)
    {
        return 0.0f;
    }

    arm_max_f32(&fft_out_buf[DC_INDEX], len - DC_INDEX, &max_value, &max_index);
    (void)max_index;

    return max_value * 2.0f;
}

uint32_t get_fft_max_index(float *fft_out_buf, uint32_t fft_len)
{
    uint32_t len = fft_len / 2U;
    float max_value = 0.0f;
    uint32_t max_index = 0;

    if (len <= DC_INDEX)
    {
        return 0U;
    }

    arm_max_f32(&fft_out_buf[DC_INDEX], len - DC_INDEX, &max_value, &max_index);
    (void)max_value;

    return max_index + DC_INDEX;
}

uint32_t get_fft_second_max_index(float *fft_out_buf, uint32_t fft_len, uint32_t guard_bins)
{
    uint32_t len = fft_len / 2U;
    uint32_t max_idx = get_fft_max_index(fft_out_buf, fft_len);
    uint32_t second_idx = 0U;
    float second_val = -1.0f;

    for (uint32_t i = DC_INDEX; i < len; i++)
    {
        uint32_t distance = (i > max_idx) ? (i - max_idx) : (max_idx - i);
        if (distance <= guard_bins)
        {
            continue;
        }

        if (fft_out_buf[i] > second_val)
        {
            second_val = fft_out_buf[i];
            second_idx = i;
        }
    }

    return second_idx;
}

float get_fft_max_phase(float *fft_input_buf, float *fft_out_buf, uint32_t fft_len)
{
    uint32_t max_index = get_fft_max_index(fft_out_buf, fft_len);
    float phase = atan2f(fft_input_buf[2U * max_index + 1U], fft_input_buf[2U * max_index]);

    while (phase > PI)
    {
        phase -= 2.0f * PI;
    }

    while (phase < -PI)
    {
        phase += 2.0f * PI;
    }

    return phase;
}
