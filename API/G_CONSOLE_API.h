#ifndef G_CONSOLE_API_H
#define G_CONSOLE_API_H

#include "main.h"

#define G_CONSOLE_API_REQUIREMENT_ALL  0U

HAL_StatusTypeDef G_ConsoleAPI_Init(UART_HandleTypeDef *huart,
                                    uint8_t selected_requirement);
void G_ConsoleAPI_Process(void);

#endif
