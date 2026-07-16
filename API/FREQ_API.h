#ifndef __FREQ_API_H__
#define __FREQ_API_H__

#include "FREQ_FML.h"

HAL_StatusTypeDef FREQ_API_Init(void);
void FREQ_API_Process(void);
const freq_result_t *FREQ_API_GetResult(void);

#endif
