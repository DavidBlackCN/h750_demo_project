#ifndef AD9226_BLL_H
#define AD9226_BLL_H

#include "main.h"

typedef struct
{
    float frequency_hz;
    float harmonic_rms_v[5];
    float thd_percent;
    uint16_t minimum_code;
    uint16_t maximum_code;
    float mean_code;
    uint32_t averaged_periods;
} AD9226_AnalysisResult;

HAL_StatusTypeDef AD9226_BLL_Analyze(const uint16_t *samples,
                                     uint32_t sample_count,
                                     AD9226_AnalysisResult *result);

#endif
