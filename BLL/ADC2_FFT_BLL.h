#ifndef ADC2_FFT_BLL_H
#define ADC2_FFT_BLL_H

#include "main.h"

typedef enum
{
    ADC2_FFT_WAVEFORM_UNKNOWN = 0U,
    ADC2_FFT_WAVEFORM_SINE,
    ADC2_FFT_WAVEFORM_TRIANGLE,
    ADC2_FFT_WAVEFORM_SQUARE
} adc2_fft_waveform_t;

typedef struct
{
    float frequency_hz;
    float peak_bin;
    float peak_amplitude_v;
    float sample_rate_hz;
    float frequency_resolution_hz;
    float harmonic3_ratio;
    float harmonic5_ratio;
    float sine_spectral_score;
    float triangle_spectral_score;
    float square_spectral_score;
    adc2_fft_waveform_t waveform;
} adc2_fft_result_t;

typedef struct
{
    float requested_sample_rate_hz;
    uint32_t sample_count;
    float closure_error_cycles;
} adc2_fft_capture_request_t;

typedef struct
{
    float requested_sample_rate_hz;
    uint32_t sample_count;
    uint32_t samples_per_period;
    float closure_error_cycles;
} adc2_fft_waveform_capture_request_t;

typedef enum
{
    ADC2_FFT_SAMPLE_UNSIGNED = 0U,
    ADC2_FFT_SAMPLE_OFFSET_BINARY,
    ADC2_FFT_SAMPLE_TWOS_COMPLEMENT
} adc2_fft_sample_encoding_t;

typedef struct
{
    float sample_rate_hz;
    float volts_per_lsb;
    float search_min_frequency_hz;
    float search_max_frequency_hz;
    uint32_t sample_count;
    uint8_t sample_bit_width;
    adc2_fft_sample_encoding_t sample_encoding;
} adc2_fft_input_config_t;

/* Example: 12-bit unipolar ADC uses {fs, 3.3f / 4095.0f, fmin, fmax,
   4096U, 12U, ADC2_FFT_SAMPLE_UNSIGNED}. */
HAL_StatusTypeDef ADC2_FFT_BLL_Analyze(const uint16_t *raw_samples,
                                       const adc2_fft_input_config_t *input_config,
                                       adc2_fft_result_t *result);
HAL_StatusTypeDef ADC2_FFT_BLL_ChooseCoherentCapture(
    const adc2_fft_result_t *coarse_result,
    uint32_t timer_clock_hz,
    adc2_fft_capture_request_t *request);
HAL_StatusTypeDef ADC2_FFT_BLL_ChooseWaveformCapture(
    float measured_frequency_hz,
    uint32_t timer_clock_hz,
    adc2_fft_waveform_capture_request_t *request);
HAL_StatusTypeDef ADC2_FFT_BLL_ClassifySpectrum(
    const adc2_fft_input_config_t *input_config,
    float known_frequency_hz,
    adc2_fft_result_t *result);
const char *ADC2_FFT_BLL_WaveformText(adc2_fft_waveform_t waveform);
const float *ADC2_FFT_BLL_GetHalfMagnitude(void);
uint32_t ADC2_FFT_BLL_GetHalfMagnitudeCount(void);

#endif
