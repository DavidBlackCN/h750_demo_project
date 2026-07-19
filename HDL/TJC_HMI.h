#ifndef TJC_HMI_H
#define TJC_HMI_H

#include "main.h"

#include <stdint.h>

typedef enum
{
    TJC_HMI_EVENT_NONE = 0,
    TJC_HMI_EVENT_READY,
    TJC_HMI_EVENT_TOUCH,
    TJC_HMI_EVENT_DEMO_TOGGLE,
    TJC_HMI_EVENT_DEMO_RESET
} TJC_HMI_EventType;

typedef struct
{
    TJC_HMI_EventType type;
    uint8_t page_id;
    uint8_t component_id;
    uint8_t touch_event;
} TJC_HMI_Event;

void TJC_HMI_Init(UART_HandleTypeDef *huart);
HAL_StatusTypeDef TJC_HMI_SendCommand(const char *command);
HAL_StatusTypeDef TJC_HMI_SetText(const char *object, const char *text);
HAL_StatusTypeDef TJC_HMI_SetValue(const char *object, int32_t value);
TJC_HMI_Event TJC_HMI_Poll(void);

#endif
