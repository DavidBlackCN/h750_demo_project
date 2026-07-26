#ifndef ADC2_FFT_FML_H
#define ADC2_FFT_FML_H

#include "main.h"

#define ADC2_FFT_LENGTH 4096U

HAL_StatusTypeDef ADC2_FFT_FML_TransformReal(float *real,
                                              float *imaginary,
                                              uint32_t sample_count,
                                              float *half_magnitude);

#endif
