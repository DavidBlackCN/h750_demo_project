#include "USART_BLL.h"

#include "USART_FML.h"
#include <stdio.h>

HAL_StatusTypeDef Usart_Send_ADC_Data(float *ADC_Data, UART_HandleTypeDef *huart, uint32_t ADC_Data_Len)
{
    char msg[32];

    for (uint32_t i = 0; i < ADC_Data_Len; i++)
    {
        snprintf(msg, sizeof(msg), "%.5f\r\n", ADC_Data[i]);
        if (Usart_Send_Computer(huart, msg) != HAL_OK)
        {
            return usart_last_status;
        }
    }

    return HAL_OK;
}
