#ifndef __USART_FML_H__
#define __USART_FML_H__

#include "main.h"

extern volatile HAL_StatusTypeDef usart_last_status;
extern volatile uint32_t usart_send_fail_count;

HAL_StatusTypeDef Usart_Send_Computer(UART_HandleTypeDef *huart, char *msg);
HAL_StatusTypeDef Usart_Send_ComputerAsync(UART_HandleTypeDef *huart,
                                           const char *msg);
HAL_StatusTypeDef Usart_Send_ComputerAsyncData(UART_HandleTypeDef *huart,
                                               const uint8_t *data,
                                               uint16_t length);
uint8_t USART_FML_OnTxComplete(UART_HandleTypeDef *huart);
uint8_t USART_FML_OnError(UART_HandleTypeDef *huart);

#endif
