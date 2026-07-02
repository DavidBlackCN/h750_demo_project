#ifndef __USART_FML_H__
#define __USART_FML_H__

#include "main.h"

extern volatile HAL_StatusTypeDef usart_last_status;
extern volatile uint32_t usart_send_fail_count;

HAL_StatusTypeDef Usart_Send_Computer(UART_HandleTypeDef *huart, char *msg);

#endif
