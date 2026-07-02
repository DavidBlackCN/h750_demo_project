#include "USART_FML.h"

#include <string.h>

volatile HAL_StatusTypeDef usart_last_status = HAL_OK;
volatile uint32_t usart_send_fail_count = 0;

HAL_StatusTypeDef Usart_Send_Computer(UART_HandleTypeDef *huart, char *msg)
{
    usart_last_status = HAL_UART_Transmit(huart, (uint8_t *)msg, strlen(msg), 20);

    if (usart_last_status != HAL_OK)
    {
        usart_send_fail_count++;
    }

    return usart_last_status;
}
