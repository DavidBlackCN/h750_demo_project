#ifndef G1_FIR_BLL_H
#define G1_FIR_BLL_H

#include <stdbool.h>
#include <stdint.h>

/* 63-tap, zero-phase FIR for 3.2 MS/s frames.  It passes 0 to 500 kHz and
   rejects the task's >= 1 MHz interference band. */
#define G1_FIR_BLL_TAP_COUNT 63U

bool G1_FIR_BLL_FilterFrame(const float *input,
                            float *output,
                            uint32_t sample_count);

#endif
