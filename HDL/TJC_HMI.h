#ifndef TJC_HMI_H
#define TJC_HMI_H

#include "main.h"

#include <stdint.h>

typedef enum
{
    TJC_HMI_EVENT_NONE = 0,
    TJC_HMI_EVENT_READY,
    TJC_HMI_EVENT_TOUCH,
    TJC_HMI_EVENT_COMMAND,
    TJC_HMI_EVENT_NUMBER,
    TJC_HMI_EVENT_TEXT
} TJC_HMI_EventType;

#define TJC_HMI_EVENT_TEXT_MAX_LENGTH  32U

typedef struct
{
    TJC_HMI_EventType type;
    uint8_t page_id;
    uint8_t component_id;
    uint8_t touch_event;
    uint8_t command;
    uint32_t value;
    char text[TJC_HMI_EVENT_TEXT_MAX_LENGTH];
} TJC_HMI_Event;

void TJC_HMI_Init(UART_HandleTypeDef *huart);
HAL_StatusTypeDef TJC_HMI_SendCommand(const char *command);
HAL_StatusTypeDef TJC_HMI_SetText(const char *object, const char *text);
HAL_StatusTypeDef TJC_HMI_SetValue(const char *object, int32_t value);
TJC_HMI_Event TJC_HMI_Poll(void);

#endif
