#ifndef DLIA_BLL_H
#define DLIA_BLL_H

#include <stdbool.h>
#include <stdint.h>

#define DLIA_BLL_BLOCK_SIZE 512U

typedef struct
{
    float reference_frequency_hz;
    float sample_rate_hz;
    float adc_full_scale_voltage;
    float adc_code_range;
    /* Smooth only the per-frame magnitude. Do not average I/Q across
       discontinuous normal-DMA frames. */
    float amplitude_lpf_beta;
    float phase_difference_lpf_beta;
} dlia_bll_config_t;

typedef struct
{
    uint32_t phase_step;
    uint32_t phase_accumulator;
    float lpf_amplitude_channel1;
    float lpf_amplitude_channel2;
    float lpf_phase_difference_sine;
    float lpf_phase_difference_cosine;
} dlia_bll_state_t;

typedef struct
{
    float i_voltage;
    float q_voltage;
    float amplitude_peak_voltage;
    float phase_deg;
} dlia_bll_channel_result_t;

typedef struct
{
    dlia_bll_channel_result_t channel1;
    dlia_bll_channel_result_t channel2;
    float raw_phase_difference_deg;
    float phase_difference_deg;
} dlia_bll_result_t;

/*
 * Build the 1024-point sine table once before calling DLIA_BLL_Init().
 * The pair processor deliberately advances one common NCO once per sample,
 * so both channels are demodulated against exactly the same reference phase.
 */
void DLIA_BLL_InitLut(void);
bool DLIA_BLL_Init(const dlia_bll_config_t *config, dlia_bll_state_t *state);
bool DLIA_BLL_ProcessPair(const dlia_bll_config_t *config,
                          dlia_bll_state_t *state,
                          const uint16_t *channel1_codes,
                          const uint16_t *channel2_codes,
                          uint32_t length,
                          dlia_bll_result_t *result);

float DLIA_BLL_WrapPhaseDeg(float phase_deg);

#endif
