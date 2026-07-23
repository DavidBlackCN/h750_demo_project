#ifndef __DAC_FML_H__
#define __DAC_FML_H__

#include "main.h"
#include <stdint.h>

#define DAC_WAVE_BUFFER_LENGTH 256U
#define DAC_WAVE_REF_VOLTAGE   3.3f
#define DAC_WAVE_ZERO_AXIS_V    1.65f

typedef enum
{
    DAC_USER_WAVE_SINE = 0,
    DAC_USER_WAVE_SQUARE,
    DAC_USER_WAVE_TRIANGLE,
    DAC_USER_WAVE_SAWTOOTH,
    DAC_USER_WAVE_DC
} dac_wave_type_t;

typedef struct
{
    dac_wave_type_t type;
    float frequency_hz;
    float vpp;
    float offset_v;
    float sample_rate_hz;
} dac_wave_config_t;

HAL_StatusTypeDef DAC_Waveform_Apply(dac_wave_type_t type, float frequency_hz, float vpp, float offset_v);
HAL_StatusTypeDef DAC_Waveform_Start(dac_wave_type_t type, float frequency_hz, float vpp, float offset_v);
HAL_StatusTypeDef DAC_Waveform_StartChannel(uint32_t channel,
                                             dac_wave_type_t type,
                                             float frequency_hz,
                                             float vpp,
                                             float offset_v);
HAL_StatusTypeDef DAC_Waveform_Stop(void);

const dac_wave_config_t *DAC_Waveform_GetConfig(void);
const uint32_t *DAC_Waveform_GetBuffer(void);
uint32_t DAC_Waveform_GetBufferLength(void);

#endif
