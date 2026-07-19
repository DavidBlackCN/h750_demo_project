#ifndef ADS8688_API_H
#define ADS8688_API_H

#include "main.h"

HAL_StatusTypeDef ADS8688_API_Init(void);
void ADS8688_API_Process(void);
uint32_t ADS8688_API_GetOutputErrorCount(void);

#endif
