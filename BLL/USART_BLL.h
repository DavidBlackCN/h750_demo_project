#ifndef __USART_BLL_H__
#define __USART_BLL_H__

#include "main.h"

HAL_StatusTypeDef Usart_Send_ADC_Data(float *ADC_Data, UART_HandleTypeDef *huart, uint32_t ADC_Data_Len);

#endif
