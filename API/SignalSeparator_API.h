#ifndef __SIGNAL_SEPARATOR_API_H__
#define __SIGNAL_SEPARATOR_API_H__

#include "main.h"

typedef enum
{
    SIGNAL_SEPARATOR_WAVE_SINE = 0,
    SIGNAL_SEPARATOR_WAVE_TRIANGLE
} signal_separator_wave_t;

typedef struct
{
    float freq_hz;
    float vpp;
    signal_separator_wave_t wave;
} signal_separator_result_t;

void SignalSeparator_Init(void);
void SignalSeparator_Process(void);

const signal_separator_result_t *SignalSeparator_GetSignalA(void);
const signal_separator_result_t *SignalSeparator_GetSignalB(void);

#endif
