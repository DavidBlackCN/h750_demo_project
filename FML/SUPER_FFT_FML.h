#ifndef SUPER_FFT_FML_H
#define SUPER_FFT_FML_H

#include "main.h"

#define SUPER_FFT_FML_LENGTH 4096U

void SUPER_FFT_FML_InitWindow(void);
void SUPER_FFT_FML_AccumulatePower(const float *samples,
                                   uint32_t first_bin,
                                   uint32_t last_bin,
                                   float *power_spectrum);

#endif
