#ifndef PHASE_BLL_H
#define PHASE_BLL_H

#include "main.h"

typedef struct
{
    float frequency_hz;
    float phase_difference_deg;
    float channel1_vpp;
    float channel2_vpp;
    float channel1_dc;
    float channel2_dc;
    float channel1_raw_vpp;
    float channel2_raw_vpp;
    float channel1_fit_quality;
    float channel2_fit_quality;
    uint8_t valid;
} phase_result_t;

extern phase_result_t phase_result;

float Phase_BLL_NominalFrequency(float measured_frequency_hz);
float Phase_BLL_EstimateFrequency(float *channel1, float *channel2,
                                  uint32_t length, float sample_rate_hz);
void Phase_BLL_ProcessKnownFrequency(float *channel1, float *channel2,
                                     uint32_t length, float sample_rate_hz,
                                     float frequency_hz);
void Phase_BLL_Process(float *channel1, float *channel2, uint32_t length);

#endif
