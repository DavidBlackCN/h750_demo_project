#ifndef AD9910_API_H
#define AD9910_API_H

#include "main.h"

#define AD9910_API_MAX_DIRECT_MVPP  724U
#define AD9910_API_MAX_TRIANGLE_MVPP  AD9910_API_MAX_DIRECT_MVPP

typedef enum
{
    AD9910_API_WAVE_SINE = 0,
    AD9910_API_WAVE_TRIANGLE,
    AD9910_API_WAVE_SQUARE
} AD9910_API_Waveform;

/**
 * @brief Initialize AD9910 and start the selected waveform on Profile 0.
 * @param waveform Sine, triangle, or square wave.
 * @param frequency_hz Output frequency in hertz. Sine supports 1 Hz to
 *        400 MHz; RAM triangle/square support 77 Hz to 5 MHz.
 * @param amplitude_mvpp Desired direct-output amplitude in millivolts peak-to-peak.
 * @retval HAL_OK when configured, HAL_ERROR when a parameter is out of range.
 */
HAL_StatusTypeDef AD9910_API_StartWaveform(AD9910_API_Waveform waveform,
                                          uint32_t frequency_hz,
                                          uint32_t amplitude_mvpp);

#endif
