#ifndef __FREQ_FML_H__
#define __FREQ_FML_H__

#include "main.h"

/* Calibrated with a 1 MHz reference: 1002814 Hz measured -> 1000000 Hz actual.
   Override this macro at build time or update it after calibrating another board. */
#ifndef FREQ_CALIBRATION_FACTOR
#define FREQ_CALIBRATION_FACTOR (0.997193896f)
#endif

typedef enum
{
    FREQ_STATUS_NO_SIGNAL = 0,
    FREQ_STATUS_MEASURING,
    FREQ_STATUS_VALID,
    FREQ_STATUS_BELOW_RANGE,
    FREQ_STATUS_ABOVE_RANGE,
    FREQ_STATUS_ERROR
} freq_status_t;

typedef enum
{
    FREQ_MODE_PROBE = 0,
    FREQ_MODE_INPUT_CAPTURE,
    FREQ_MODE_DMA_CAPTURE
} freq_mode_t;

typedef struct
{
    float raw_hz;
    float display_hz;
    uint32_t period_ticks;
    uint32_t measured_periods;
    uint32_t update_count;
    freq_status_t status;
    freq_mode_t mode;
} freq_result_t;

HAL_StatusTypeDef FREQ_FML_Init(void);
void FREQ_FML_Process(void);
const freq_result_t *FREQ_FML_GetResult(void);

#endif
