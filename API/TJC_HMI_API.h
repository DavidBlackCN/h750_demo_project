#ifndef TJC_HMI_API_H
#define TJC_HMI_API_H

#include "main.h"

HAL_StatusTypeDef TJC_HMI_API_Init(UART_HandleTypeDef *huart);
void TJC_HMI_API_Process(void);

#endif
