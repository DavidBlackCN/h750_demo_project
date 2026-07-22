#ifndef AD9226_H
#define AD9226_H

#include "main.h"

#define AD9226_SAMPLE_RATE_HZ  1000000UL
#define AD9226_CAPTURE_SAMPLES 4096UL

HAL_StatusTypeDef AD9226_Init(void);
HAL_StatusTypeDef AD9226_CaptureFrame(void);
const uint16_t *AD9226_GetCaptureBuffer(void);
uint32_t AD9226_GetCaptureLength(void);

#endif
