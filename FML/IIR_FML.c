#include "IIR_FML.h"

#include <stddef.h>
#include <string.h>

#define IIR_LOWPASS_SIGNATURE 0x49495231UL
#define IIR_LOWPASS_S2_COEFFICIENT 1.0e-8
#define IIR_LOWPASS_S1_COEFFICIENT 3.0e-4
#define IIR_DAC12_MAX_CODE 4095.0f

bool IIR_Lowpass_Init(iir_lowpass_filter_t *filter, float32_t sample_rate_hz)
{
    double sample_rate;
    double denominator0;
    double denominator1;
    double denominator2;

    if ((filter == NULL) || (sample_rate_hz <= 0.0f))
    {
        return false;
    }

    sample_rate = (double)sample_rate_hz;
    denominator0 = (4.0 * IIR_LOWPASS_S2_COEFFICIENT * sample_rate * sample_rate) +
                   (2.0 * IIR_LOWPASS_S1_COEFFICIENT * sample_rate) + 1.0;
    denominator1 = 2.0 - (8.0 * IIR_LOWPASS_S2_COEFFICIENT * sample_rate * sample_rate);
    denominator2 = (4.0 * IIR_LOWPASS_S2_COEFFICIENT * sample_rate * sample_rate) -
                   (2.0 * IIR_LOWPASS_S1_COEFFICIENT * sample_rate) + 1.0;

    memset(filter, 0, sizeof(*filter));
    filter->coefficients[0] = (float32_t)(1.0 / denominator0);
    filter->coefficients[1] = (float32_t)(2.0 / denominator0);
    filter->coefficients[2] = filter->coefficients[0];

    /* CMSIS DF1 uses positive feedback terms, unlike the usual a1/a2 form. */
    filter->coefficients[3] = (float32_t)(-denominator1 / denominator0);
    filter->coefficients[4] = (float32_t)(-denominator2 / denominator0);
    filter->sample_rate_hz = sample_rate_hz;
    filter->signature = IIR_LOWPASS_SIGNATURE;

    arm_biquad_cascade_df1_init_f32(&filter->instance,
                                    1U,
                                    filter->coefficients,
                                    filter->state);
    return true;
}

void IIR_Lowpass_Reset(iir_lowpass_filter_t *filter)
{
    if ((filter == NULL) || (filter->signature != IIR_LOWPASS_SIGNATURE))
    {
        return;
    }

    memset(filter->state, 0, sizeof(filter->state));
}

bool IIR_Lowpass_Process(iir_lowpass_filter_t *filter,
                         const float32_t *input,
                         float32_t *output,
                         uint32_t sample_count)
{
    if ((filter == NULL) || (input == NULL) || (output == NULL) ||
        (sample_count == 0U) || (filter->signature != IIR_LOWPASS_SIGNATURE))
    {
        return false;
    }

    arm_biquad_cascade_df1_f32(&filter->instance, input, output, sample_count);
    return true;
}

bool IIR_Lowpass_ProcessAdc12ToDac12(iir_lowpass_filter_t *filter,
                                     const uint16_t *adc_codes,
                                     uint16_t *dac_codes,
                                     q15_t *q15_scratch,
                                     float32_t *scratch,
                                     uint32_t sample_count,
                                     uint16_t adc_midpoint_code,
                                     uint16_t dac_midpoint_code)
{
    uint32_t index;

    if ((filter == NULL) || (adc_codes == NULL) || (dac_codes == NULL) ||
        (q15_scratch == NULL) || (scratch == NULL) || (sample_count == 0U))
    {
        return false;
    }

    for (index = 0U; index < sample_count; index++)
    {
        q15_scratch[index] = (q15_t)adc_codes[index];
    }

    arm_offset_q15(q15_scratch,
                   -(q15_t)adc_midpoint_code,
                   q15_scratch,
                   sample_count);
    arm_q15_to_float(q15_scratch, scratch, sample_count);

    if (!IIR_Lowpass_Process(filter, scratch, scratch, sample_count))
    {
        return false;
    }

    arm_float_to_q15(scratch, q15_scratch, sample_count);
    arm_offset_q15(q15_scratch,
                   (q15_t)dac_midpoint_code,
                   q15_scratch,
                   sample_count);

    for (index = 0U; index < sample_count; index++)
    {
        if (q15_scratch[index] <= 0)
        {
            dac_codes[index] = 0U;
        }
        else if (q15_scratch[index] >= (q15_t)IIR_DAC12_MAX_CODE)
        {
            dac_codes[index] = (uint16_t)IIR_DAC12_MAX_CODE;
        }
        else
        {
            dac_codes[index] = (uint16_t)q15_scratch[index];
        }
    }

    return true;
}

const float32_t *IIR_Lowpass_GetCoefficients(const iir_lowpass_filter_t *filter)
{
    if ((filter == NULL) || (filter->signature != IIR_LOWPASS_SIGNATURE))
    {
        return NULL;
    }

    return filter->coefficients;
}

float32_t IIR_Lowpass_GetSampleRate(const iir_lowpass_filter_t *filter)
{
    if ((filter == NULL) || (filter->signature != IIR_LOWPASS_SIGNATURE))
    {
        return 0.0f;
    }

    return filter->sample_rate_hz;
}
