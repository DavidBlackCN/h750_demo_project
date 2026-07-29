#ifndef AD9226_H
#define AD9226_H

#include "main.h"

/* Reference AD9226 DCMI wiring: TIM1_CH1/PA8 -> ADC CLK and PA6/PIXCLK. */
#define AD9226_SAMPLE_RATE_HZ  5000000UL
#define AD9226_CAPTURE_SAMPLES 8192UL

HAL_StatusTypeDef AD9226_Init(void);
HAL_StatusTypeDef AD9226_CaptureFrame(void);
const uint16_t *AD9226_GetCaptureBuffer(void);
uint32_t AD9226_GetCaptureLength(void);

#endif
