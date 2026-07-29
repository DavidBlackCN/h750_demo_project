#include "G1_VPP_BLL.h"

#include <float.h>
#include <math.h>

#define G1_VPP_ADC_FULL_SCALE_CODE  16383.0f
#define G1_VPP_INTERP_PHASES         16U
#define G1_VPP_INTERP_TAPS           8U
#define G1_VPP_INTERP_FIRST_OFFSET   (-3)

static float s_interpolation_coefficients[G1_VPP_INTERP_PHASES]
                                         [G1_VPP_INTERP_TAPS];
static uint8_t s_interpolation_ready;

static void g1_vpp_prepare_interpolator(void)
{
    if (s_interpolation_ready != 0U)
    {
        return;
    }

    for (uint32_t phase = 0U; phase < G1_VPP_INTERP_PHASES; ++phase)
    {
        float position = (float)phase / (float)G1_VPP_INTERP_PHASES;

        for (uint32_t tap = 0U; tap < G1_VPP_INTERP_TAPS; ++tap)
        {
            float numerator = 1.0f;
            float denominator = 1.0f;
            int32_t node = G1_VPP_INTERP_FIRST_OFFSET + (int32_t)tap;

            for (uint32_t other = 0U; other < G1_VPP_INTERP_TAPS; ++other)
            {
                int32_t other_node = G1_VPP_INTERP_FIRST_OFFSET +
                                     (int32_t)other;

                if (other != tap)
                {
                    numerator *= position - (float)other_node;
                    denominator *= (float)(node - other_node);
                }
            }

            s_interpolation_coefficients[phase][tap] = numerator / denominator;
        }
    }

    s_interpolation_ready = 1U;
}

float G1_VPP_BLL_CodeToInputVolts(uint16_t code,
                                  const g1_vpp_calibration_t *calibration)
{
    float adc_pin_volts = ((float)(code & 0x3FFFU) *
                           calibration->adc_reference_volts) /
                          G1_VPP_ADC_FULL_SCALE_CODE;

    return (adc_pin_volts / calibration->front_end_gain) *
           calibration->calibration_scale;
}

bool G1_VPP_BLL_Analyze(const uint16_t *samples, uint32_t sample_count,
                        const g1_vpp_calibration_t *calibration,
                        g1_vpp_result_t *result)
{
    float minimum = FLT_MAX;
    float maximum = -FLT_MAX;
    float mean = 0.0f;
    float ac_power = 0.0f;

    if ((samples == NULL) || (calibration == NULL) || (result == NULL) ||
        (sample_count < G1_VPP_INTERP_TAPS) ||
        (calibration->adc_reference_volts <= 0.0f) ||
        (calibration->front_end_gain <= 0.0f) ||
        (calibration->calibration_scale <= 0.0f))
    {
        return false;
    }

    g1_vpp_prepare_interpolator();

    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        float sample = G1_VPP_BLL_CodeToInputVolts(samples[index], calibration);

        mean += sample;

        if (sample < minimum)
        {
            minimum = sample;
        }
        if (sample > maximum)
        {
            maximum = sample;
        }
    }

    mean /= (float)sample_count;
    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        float ac_sample = G1_VPP_BLL_CodeToInputVolts(samples[index], calibration) -
                          mean;
        ac_power += ac_sample * ac_sample;
    }

    /*
     * The highest component in task 1 is 200 kHz, while the frame is sampled
     * at 1.875 MS/s.  The eight-point interpolation removes the phase error
     * caused by using only the raw sample maximum/minimum; no frequency result
     * is produced by this module.
     */
    for (uint32_t center = 3U; (center + 4U) < sample_count; ++center)
    {
        for (uint32_t phase = 1U; phase < G1_VPP_INTERP_PHASES; ++phase)
        {
            float interpolated = 0.0f;

            for (uint32_t tap = 0U; tap < G1_VPP_INTERP_TAPS; ++tap)
            {
                int32_t offset = G1_VPP_INTERP_FIRST_OFFSET + (int32_t)tap;
                uint32_t index = (uint32_t)((int32_t)center + offset);
                interpolated += G1_VPP_BLL_CodeToInputVolts(samples[index],
                                                             calibration) *
                    s_interpolation_coefficients[phase][tap];
            }

            if (interpolated < minimum)
            {
                minimum = interpolated;
            }
            if (interpolated > maximum)
            {
                maximum = interpolated;
            }
        }
    }

    result->input_min_volts = minimum;
    result->input_max_volts = maximum;
    result->input_vpp_volts = maximum - minimum;
    result->input_mean_volts = mean;
    result->input_rms_volts = sqrtf(ac_power / (float)sample_count);
    result->adc_pin_vpp_volts = result->input_vpp_volts *
                                 calibration->front_end_gain /
                                 calibration->calibration_scale;
    result->sample_count = sample_count;
    result->valid = true;

    return true;
}
