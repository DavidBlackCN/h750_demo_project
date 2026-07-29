#ifndef G1_FFT_BLL_H
#define G1_FFT_BLL_H

#include <stdbool.h>
#include <stdint.h>

#define G1_FFT_MAX_COMPONENTS 6U
#define G1_FFT_MAX_SPECTRUM_BINS 4096U

typedef struct
{
    float bin_frequency_hz;
    float frequency_hz;
    float fitted_frequency_hz;
    float peak_amplitude_volts;
    float phase_radians;
    float bin_offset;
    uint32_t bin_index;
    uint32_t harmonic_order;
} g1_fft_component_t;

typedef struct
{
    float sample_rate_hz;
    float search_min_frequency_hz;
    float search_max_frequency_hz;
    float min_peak_amplitude_volts;
} g1_fft_input_t;

typedef struct
{
    float dc_volts;
    float bin_width_hz;
    float fundamental_hz;
    uint32_t first_spectrum_bin;
    uint32_t spectrum_bin_count;
    uint32_t component_count;
    float spectrum_peak_volts[G1_FFT_MAX_SPECTRUM_BINS];
    g1_fft_component_t components[G1_FFT_MAX_COMPONENTS];
    float fitted_vpp_volts;
    float fitted_ac_rms_volts;
    float fitted_dc_volts;
    bool fitted_waveform_valid;
    bool valid;
} g1_fft_result_t;

bool G1_FFT_BLL_Analyze(const float *voltage_samples,
                        uint32_t sample_count,
                        const g1_fft_input_t *input,
                        g1_fft_result_t *result);
bool G1_FFT_BLL_FitHarmonics(const float *voltage_samples,
                             uint32_t sample_count,
                             g1_fft_result_t *result);

#endif
