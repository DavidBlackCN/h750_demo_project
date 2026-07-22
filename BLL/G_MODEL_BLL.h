#ifndef G_MODEL_BLL_H
#define G_MODEL_BLL_H

#include <stdbool.h>
#include <stdint.h>

#include "AD9910_Constants.h"

#define G_MODEL_BLL_FREQUENCY_MIN_HZ          100U
#define G_MODEL_BLL_FREQUENCY_MAX_HZ         3000U
#define G_MODEL_BLL_FREQUENCY_STEP_HZ         100U
#define G_MODEL_BLL_GAIN_COUNT                 30U
#define G_MODEL_BLL_TARGET_MIN_TENTHS_VPP      10U
#define G_MODEL_BLL_TARGET_MAX_TENTHS_VPP      20U
typedef enum
{
    G_MODEL_BLL_OK = 0,
    G_MODEL_BLL_ERROR_NULL,
    G_MODEL_BLL_ERROR_FREQUENCY,
    G_MODEL_BLL_ERROR_TARGET,
    G_MODEL_BLL_ERROR_AMPLITUDE
} G_ModelBLL_Status;

typedef struct
{
    uint32_t frequency_hz;
    uint8_t gain_index;
    uint8_t target_tenths_vpp;
    float model_gain;
    float source_output_mvpp;
    uint16_t amplitude_code;
} G_ModelBLL_Result;

G_ModelBLL_Status G_ModelBLL_LookupGain(uint32_t frequency_hz,
                                        uint8_t *gain_index,
                                        float *gain);
G_ModelBLL_Status G_ModelBLL_OutputMvppToAmplitudeCode(float output_mvpp,
                                                       uint16_t *amplitude_code);
G_ModelBLL_Status G_ModelBLL_Calculate(uint32_t frequency_hz,
                                       uint8_t target_tenths_vpp,
                                       G_ModelBLL_Result *result);
bool G_ModelBLL_SelfTest(void);
const float *G_ModelBLL_GetGainTable(void);

#endif
