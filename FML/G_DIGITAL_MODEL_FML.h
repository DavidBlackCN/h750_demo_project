#ifndef __G_DIGITAL_MODEL_FML_H__
#define __G_DIGITAL_MODEL_FML_H__

#include "main.h"
#include <stdbool.h>

#define G_DIGITAL_MODEL_BUFFER_LENGTH  256U

HAL_StatusTypeDef G_DigitalModelFML_Start(void);
HAL_StatusTypeDef G_DigitalModelFML_Stop(void);
bool G_DigitalModelFML_IsRunning(void);
void G_DigitalModelFML_ADCProcess(bool second_half);
void G_DigitalModelFML_ADCError(void);
uint32_t G_DigitalModelFML_GetProcessedBlockCount(void);
uint32_t G_DigitalModelFML_GetErrorCount(void);

#endif
