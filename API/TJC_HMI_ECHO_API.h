#ifndef TJC_HMI_ECHO_API_H
#define TJC_HMI_ECHO_API_H

#include "main.h"

HAL_StatusTypeDef TJC_HMI_ECHO_API_Init(UART_HandleTypeDef *hmi_uart,
                                        UART_HandleTypeDef *debug_uart);
void TJC_HMI_ECHO_API_Process(void);

#endif
