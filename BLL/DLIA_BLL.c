#include "DLIA_BLL.h"

#include <math.h>

#define DLIA_BLL_PI                3.14159265358979323846f
#define DLIA_BLL_LUT_SIZE          1024U
#define DLIA_BLL_LUT_MASK          (DLIA_BLL_LUT_SIZE - 1U)
#define DLIA_BLL_LUT_INDEX_SHIFT   22U
#define DLIA_BLL_LUT_FRACTION_MASK 0xFFU
#define DLIA_BLL_LUT_FRACTION_SHIFT 14U
#define DLIA_BLL_INV_256           0.00390625f

static float s_sine_lut[DLIA_BLL_LUT_SIZE];
static uint8_t s_lut_initialized = 0U;

typedef struct
{
    float sum_i_channel1;
    float sum_q_channel1;
    float sum_i_channel2;
    float sum_q_channel2;
    float sum_channel1;
    float sum_channel2;
    float sum_ref_sine;
    float sum_ref_cosine;
} dlia_bll_accumulator_t;

static float dlia_bll_interpolate_sine(uint32_t phase_accumulator)
{
    uint32_t index0 = (phase_accumulator >> DLIA_BLL_LUT_INDEX_SHIFT) &
                      DLIA_BLL_LUT_MASK;
    uint32_t index1 = (index0 + 1U) & DLIA_BLL_LUT_MASK;
    float fraction = (float)((phase_accumulator >> DLIA_BLL_LUT_FRACTION_SHIFT) &
                             DLIA_BLL_LUT_FRACTION_MASK) * DLIA_BLL_INV_256;

    return s_sine_lut[index0] + fraction * (s_sine_lut[index1] - s_sine_lut[index0]);
}

static void dlia_bll_update_channel(float sum_i, float sum_q, float sum_codes,
                                    float sum_ref_sine, float sum_ref_cosine,
                                    uint32_t length, const dlia_bll_config_t *config,
                                    float *lpf_amplitude,
                                    dlia_bll_channel_result_t *result)
{
    float average_codes = sum_codes / (float)length;
    float scale = config->adc_full_scale_voltage / config->adc_code_range *
                  (2.0f / (float)length);
    float i_voltage = (sum_i - average_codes * sum_ref_sine) * scale;
    float q_voltage = (sum_q - average_codes * sum_ref_cosine) * scale;
    float amplitude = sqrtf(i_voltage * i_voltage + q_voltage * q_voltage);

    /* Each normal-DMA frame starts after a variable processing gap. Its
       absolute I/Q angle can therefore change even for a stable input.
       Averaging I/Q would cancel valid vectors; smooth the scalar magnitude
       only, and derive phase from the current frame. */
    *lpf_amplitude += config->amplitude_lpf_beta *
                      (amplitude - *lpf_amplitude);

    result->i_voltage = i_voltage;
    result->q_voltage = q_voltage;
    result->amplitude_peak_voltage = *lpf_amplitude;
    result->phase_deg = atan2f(q_voltage, i_voltage) * 180.0f / DLIA_BLL_PI;
}

float DLIA_BLL_WrapPhaseDeg(float phase_deg)
{
    while (phase_deg > 180.0f)
    {
        phase_deg -= 360.0f;
    }
    while (phase_deg <= -180.0f)
    {
        phase_deg += 360.0f;
    }
    return phase_deg;
}

void DLIA_BLL_InitLut(void)
{
    uint32_t index;

    for (index = 0U; index < DLIA_BLL_LUT_SIZE; index++)
    {
        s_sine_lut[index] = sinf(2.0f * DLIA_BLL_PI * (float)index /
                                 (float)DLIA_BLL_LUT_SIZE);
    }

    s_lut_initialized = 1U;
}

bool DLIA_BLL_Init(const dlia_bll_config_t *config, dlia_bll_state_t *state)
{
    if ((config == NULL) || (state == NULL) ||
        (config->reference_frequency_hz <= 0.0f) ||
        (config->sample_rate_hz <= 0.0f) ||
        (config->reference_frequency_hz >= config->sample_rate_hz * 0.5f) ||
        (config->adc_full_scale_voltage <= 0.0f) ||
        (config->adc_code_range < 2.0f) ||
        (config->amplitude_lpf_beta <= 0.0f) ||
        (config->amplitude_lpf_beta > 1.0f) ||
        (config->phase_difference_lpf_beta <= 0.0f) ||
        (config->phase_difference_lpf_beta > 1.0f))
    {
        return false;
    }

    if (s_lut_initialized == 0U)
    {
        DLIA_BLL_InitLut();
    }

    state->phase_step = (uint32_t)(config->reference_frequency_hz /
                                   config->sample_rate_hz * 4294967296.0f);
    state->phase_accumulator = 0U;
    state->lpf_amplitude_channel1 = 0.0f;
    state->lpf_amplitude_channel2 = 0.0f;
    state->lpf_phase_difference_sine = 0.0f;
    state->lpf_phase_difference_cosine = 0.0f;
    return true;
}

bool DLIA_BLL_ProcessPair(const dlia_bll_config_t *config,
                          dlia_bll_state_t *state,
                          const uint16_t *channel1_codes,
                          const uint16_t *channel2_codes,
                          uint32_t length,
                          dlia_bll_result_t *result)
{
    dlia_bll_accumulator_t sums = {0};
    uint32_t index;
    uint32_t phase_accumulator;

    if ((config == NULL) || (state == NULL) || (channel1_codes == NULL) ||
        (channel2_codes == NULL) || (result == NULL) ||
        (length != DLIA_BLL_BLOCK_SIZE) || (s_lut_initialized == 0U))
    {
        return false;
    }

    phase_accumulator = state->phase_accumulator;
    for (index = 0U; index < length; index++)
    {
        float reference_sine = dlia_bll_interpolate_sine(phase_accumulator);
        float reference_cosine = dlia_bll_interpolate_sine(
            phase_accumulator + (UINT32_MAX / 4U + 1U));
        float channel1 = (float)channel1_codes[index];
        float channel2 = (float)channel2_codes[index];

        sums.sum_i_channel1 += channel1 * reference_sine;
        sums.sum_q_channel1 += channel1 * reference_cosine;
        sums.sum_i_channel2 += channel2 * reference_sine;
        sums.sum_q_channel2 += channel2 * reference_cosine;
        sums.sum_channel1 += channel1;
        sums.sum_channel2 += channel2;
        sums.sum_ref_sine += reference_sine;
        sums.sum_ref_cosine += reference_cosine;
        phase_accumulator += state->phase_step;
    }

    state->phase_accumulator = phase_accumulator;
    dlia_bll_update_channel(sums.sum_i_channel1, sums.sum_q_channel1,
                            sums.sum_channel1, sums.sum_ref_sine,
                            sums.sum_ref_cosine, length, config,
                            &state->lpf_amplitude_channel1,
                            &result->channel1);
    dlia_bll_update_channel(sums.sum_i_channel2, sums.sum_q_channel2,
                            sums.sum_channel2, sums.sum_ref_sine,
                            sums.sum_ref_cosine, length, config,
                            &state->lpf_amplitude_channel2,
                            &result->channel2);
    result->raw_phase_difference_deg = DLIA_BLL_WrapPhaseDeg(
        result->channel2.phase_deg - result->channel1.phase_deg);
    {
        float raw_phase_radians = result->raw_phase_difference_deg *
                                  DLIA_BLL_PI / 180.0f;

        state->lpf_phase_difference_sine +=
            config->phase_difference_lpf_beta *
            (sinf(raw_phase_radians) - state->lpf_phase_difference_sine);
        state->lpf_phase_difference_cosine +=
            config->phase_difference_lpf_beta *
            (cosf(raw_phase_radians) - state->lpf_phase_difference_cosine);
        result->phase_difference_deg = atan2f(
            state->lpf_phase_difference_sine,
            state->lpf_phase_difference_cosine) * 180.0f / DLIA_BLL_PI;
    }
    return true;
}
