#ifndef G1_VPP_BLL_H
#define G1_VPP_BLL_H

#include "main.h"

#include <stdbool.h>

typedef struct
{
    float adc_reference_volts;
    float front_end_gain;
    float calibration_scale;
} g1_vpp_calibration_t;

typedef struct
{
    float input_vpp_volts;
    float input_rms_volts;
    float input_mean_volts;
    float adc_pin_vpp_volts;
    float input_min_volts;
    float input_max_volts;
    uint32_t sample_count;
    bool valid;
} g1_vpp_result_t;

float G1_VPP_BLL_CodeToInputVolts(uint16_t code,
                                  const g1_vpp_calibration_t *calibration);
bool G1_VPP_BLL_Analyze(const uint16_t *samples, uint32_t sample_count,
                        const g1_vpp_calibration_t *calibration,
                        g1_vpp_result_t *result);

#endif
