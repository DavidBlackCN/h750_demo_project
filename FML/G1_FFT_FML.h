#ifndef G1_FFT_FML_H
#define G1_FFT_FML_H

#include <stdbool.h>
#include <stdint.h>

#define G1_FFT_FML_MAX_SAMPLES 4096U

/* This interface accepts only a completed real-valued voltage frame. */
bool G1_FFT_FML_TransformReal(const float *samples,
                              uint32_t sample_count,
                              float *real,
                              float *imaginary);

#endif
