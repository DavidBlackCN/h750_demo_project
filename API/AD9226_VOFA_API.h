#ifndef AD9226_VOFA_API_H
#define AD9226_VOFA_API_H

#include "main.h"

#include <stdbool.h>

HAL_StatusTypeDef AD9226_VOFA_API_Init(void);
void AD9226_VOFA_API_Process(void);
bool AD9226_VOFA_API_HasError(void);

#endif
