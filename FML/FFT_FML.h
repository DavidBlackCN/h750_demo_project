#ifndef __FFT_FML_H__
#define __FFT_FML_H__

#include "main.h"
#include "arm_math.h"

#define FFT_LENGTH 4096U
#define FFT_SAMPLE_RATE 1000000.0f
#define DC_INDEX 1U

extern float fft_input_buffer[FFT_LENGTH * 2U];
extern float fft_magnitude[FFT_LENGTH / 2U];
extern float window_buffer[FFT_LENGTH];
extern float fft_out[FFT_LENGTH / 2U];

void generate_hanning_window(void);
void calculate_fft(float *input_data, uint32_t data_len);
void calculate_fft_fml(float *input_data, float *fft_complex_buf, float *fft_mag_buf,
                       float *fft_out_buf, uint16_t fft_len, uint32_t data_len);
void calculate_fft_fml_no_window(float *input_data, float *fft_complex_buf, float *fft_mag_buf,
                                 float *fft_out_buf, uint16_t fft_len, uint32_t data_len);

float get_fft_vpp(float *fft_out_buf, uint32_t fft_len);
uint32_t get_fft_max_index(float *fft_out_buf, uint32_t fft_len);
uint32_t get_fft_second_max_index(float *fft_out_buf, uint32_t fft_len, uint32_t guard_bins);
float get_fft_max_phase(float *fft_input_buf, float *fft_out_buf, uint32_t fft_len);

#endif
