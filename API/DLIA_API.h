#ifndef DLIA_API_H
#define DLIA_API_H

#include "main.h"

#include <stdbool.h>

/* Change this together with the AD9833/output reference frequency. */
#define DLIA_API_REFERENCE_FREQUENCY_HZ 10000.0f

HAL_StatusTypeDef DLIA_API_Init(void);
void DLIA_API_Process(void);
bool DLIA_API_TakeValidPhase(float *phase_difference_deg);

#endif
