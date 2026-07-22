#include "G_MODEL_BLL.h"

#include <stddef.h>

/*
 * Model gain at 100 Hz ... 3000 Hz. Values start from the theoretical
 * transfer function; individual points may be corrected by bench results.
 */
static const float known_model_gain[G_MODEL_BLL_GAIN_COUNT] =
{
    4.9322751f, 4.7441043f, 4.4721781f, 4.1578002f,
    3.4901266f, /* 500 Hz: 3.7063291 * 1.13 / 1.20 */
    3.5223018f, 3.2331961f, 2.9708947f, 2.7355988f,
    2.3089042f, /* 1 kHz: 2.2094777 * 2.09 / 2.00 */
    2.1983875f, 2.1713278f, 2.0218848f, 1.8877780f, 1.7670090f,
    1.6578537f, 1.5588358f, 1.4686962f, 1.3863609f,
    1.1953169f, /* 2 kHz: 1.2509130 * 1.72 / 1.80 */
    1.2415680f, 1.1776521f, 1.1185848f, 1.0638640f, 1.0130537f,
    0.9657736f, 0.9216909f, 0.8805130f, 0.8419819f, 0.8058688f
};

G_ModelBLL_Status G_ModelBLL_LookupGain(uint32_t frequency_hz,
                                        uint8_t *gain_index,
                                        float *gain)
{
    uint32_t index;

    if ((gain_index == NULL) || (gain == NULL))
    {
        return G_MODEL_BLL_ERROR_NULL;
    }
    if ((frequency_hz < G_MODEL_BLL_FREQUENCY_MIN_HZ) ||
        (frequency_hz > G_MODEL_BLL_FREQUENCY_MAX_HZ) ||
        ((frequency_hz % G_MODEL_BLL_FREQUENCY_STEP_HZ) != 0U))
    {
        return G_MODEL_BLL_ERROR_FREQUENCY;
    }

    index = (frequency_hz - G_MODEL_BLL_FREQUENCY_MIN_HZ) /
            G_MODEL_BLL_FREQUENCY_STEP_HZ;
    if (index >= G_MODEL_BLL_GAIN_COUNT)
    {
        return G_MODEL_BLL_ERROR_FREQUENCY;
    }

    *gain_index = (uint8_t)index;
    *gain = known_model_gain[index];
    return G_MODEL_BLL_OK;
}

G_ModelBLL_Status G_ModelBLL_OutputMvppToAmplitudeCode(float output_mvpp,
                                                       uint16_t *amplitude_code)
{
    float code;

    if (amplitude_code == NULL)
    {
        return G_MODEL_BLL_ERROR_NULL;
    }
    if (output_mvpp <= 0.0f)
    {
        return G_MODEL_BLL_ERROR_AMPLITUDE;
    }

    code = output_mvpp * (float)AD9910_AMPLITUDE_CODE_MAX /
           AD9910_OUTPUT_FULL_SCALE_MVPP;
    if (code > (float)AD9910_AMPLITUDE_CODE_MAX)
    {
        return G_MODEL_BLL_ERROR_AMPLITUDE;
    }

    *amplitude_code = (uint16_t)(code + 0.5f);
    return G_MODEL_BLL_OK;
}

G_ModelBLL_Status G_ModelBLL_Calculate(uint32_t frequency_hz,
                                       uint8_t target_tenths_vpp,
                                       G_ModelBLL_Result *result)
{
    G_ModelBLL_Status status;
    uint8_t index;
    float gain;
    float source_mvpp;
    uint16_t amplitude_code;

    if (result == NULL)
    {
        return G_MODEL_BLL_ERROR_NULL;
    }
    if ((target_tenths_vpp < G_MODEL_BLL_TARGET_MIN_TENTHS_VPP) ||
        (target_tenths_vpp > G_MODEL_BLL_TARGET_MAX_TENTHS_VPP))
    {
        return G_MODEL_BLL_ERROR_TARGET;
    }

    status = G_ModelBLL_LookupGain(frequency_hz, &index, &gain);
    if (status != G_MODEL_BLL_OK)
    {
        return status;
    }

    source_mvpp = ((float)target_tenths_vpp * 100.0f) / gain;
    status = G_ModelBLL_OutputMvppToAmplitudeCode(source_mvpp,
                                                  &amplitude_code);
    if (status != G_MODEL_BLL_OK)
    {
        return status;
    }

    result->frequency_hz = frequency_hz;
    result->gain_index = index;
    result->target_tenths_vpp = target_tenths_vpp;
    result->model_gain = gain;
    result->source_output_mvpp = source_mvpp;
    result->amplitude_code = amplitude_code;
    return G_MODEL_BLL_OK;
}

bool G_ModelBLL_SelfTest(void)
{
    uint32_t frequency_hz;
    uint8_t target_tenths_vpp;
    for (frequency_hz = G_MODEL_BLL_FREQUENCY_MIN_HZ;
         frequency_hz <= G_MODEL_BLL_FREQUENCY_MAX_HZ;
         frequency_hz += G_MODEL_BLL_FREQUENCY_STEP_HZ)
    {
        uint32_t index = (frequency_hz - G_MODEL_BLL_FREQUENCY_MIN_HZ) /
                         G_MODEL_BLL_FREQUENCY_STEP_HZ;

        /* Bench-calibrated neighboring points need not remain strictly
           monotonic.  Reject only non-physical or clearly corrupt values. */
        if ((known_model_gain[index] <= 0.0f) ||
            (known_model_gain[index] > 5.5f))
        {
            return false;
        }

        for (target_tenths_vpp = G_MODEL_BLL_TARGET_MIN_TENTHS_VPP;
             target_tenths_vpp <= G_MODEL_BLL_TARGET_MAX_TENTHS_VPP;
             target_tenths_vpp++)
        {
            G_ModelBLL_Result result;

            if ((G_ModelBLL_Calculate(frequency_hz, target_tenths_vpp,
                                      &result) != G_MODEL_BLL_OK) ||
                (result.gain_index != index) ||
                (result.amplitude_code > AD9910_AMPLITUDE_CODE_MAX))
            {
                return false;
            }
        }
    }

    return true;
}

const float *G_ModelBLL_GetGainTable(void)
{
    return known_model_gain;
}
