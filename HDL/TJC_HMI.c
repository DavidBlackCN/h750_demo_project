#include "TJC_HMI.h"

#include <stdio.h>
#include <string.h>

#define TJC_HMI_COMMAND_MAX_LENGTH  96U
#define TJC_HMI_RX_FRAME_MAX_LENGTH 16U
#define TJC_HMI_TX_TIMEOUT_MS       20U

static UART_HandleTypeDef *tjc_huart;
static uint8_t tjc_rx_frame[TJC_HMI_RX_FRAME_MAX_LENGTH];
static uint8_t tjc_rx_length;
static uint8_t tjc_rx_ff_count;

static TJC_HMI_Event TJC_HMI_ParseFrame(void)
{
    TJC_HMI_Event event = {TJC_HMI_EVENT_NONE, 0U, 0U, 0U};

    if ((tjc_rx_length == 1U) && (tjc_rx_frame[0] == 0x88U))
    {
        event.type = TJC_HMI_EVENT_READY;
    }
    else if ((tjc_rx_length == 4U) && (tjc_rx_frame[0] == 0x65U))
    {
        event.type = TJC_HMI_EVENT_TOUCH;
        event.page_id = tjc_rx_frame[1];
        event.component_id = tjc_rx_frame[2];
        event.touch_event = tjc_rx_frame[3];
    }
    else if ((tjc_rx_length == 3U) &&
             (tjc_rx_frame[0] == 0x5AU) &&
             (tjc_rx_frame[1] == 0xA5U))
    {
        if (tjc_rx_frame[2] == 0x01U)
        {
            event.type = TJC_HMI_EVENT_DEMO_TOGGLE;
        }
        else if (tjc_rx_frame[2] == 0x02U)
        {
            event.type = TJC_HMI_EVENT_DEMO_RESET;
        }
    }

    tjc_rx_length = 0U;
    tjc_rx_ff_count = 0U;
    return event;
}

void TJC_HMI_Init(UART_HandleTypeDef *huart)
{
    tjc_huart = huart;
    tjc_rx_length = 0U;
    tjc_rx_ff_count = 0U;
}

HAL_StatusTypeDef TJC_HMI_SendCommand(const char *command)
{
    static const uint8_t terminator[3] = {0xFFU, 0xFFU, 0xFFU};
    HAL_StatusTypeDef status;

    if ((tjc_huart == NULL) || (command == NULL))
    {
        return HAL_ERROR;
    }

    status = HAL_UART_Transmit(tjc_huart,
                               (const uint8_t *)command,
                               (uint16_t)strlen(command),
                               TJC_HMI_TX_TIMEOUT_MS);
    if (status != HAL_OK)
    {
        return status;
    }

    return HAL_UART_Transmit(tjc_huart,
                             (uint8_t *)terminator,
                             sizeof(terminator),
                             TJC_HMI_TX_TIMEOUT_MS);
}

HAL_StatusTypeDef TJC_HMI_SetText(const char *object, const char *text)
{
    char command[TJC_HMI_COMMAND_MAX_LENGTH];
    int length;

    if ((object == NULL) || (text == NULL))
    {
        return HAL_ERROR;
    }

    length = snprintf(command, sizeof(command), "%s.txt=\"%s\"", object, text);
    if ((length < 0) || ((size_t)length >= sizeof(command)))
    {
        return HAL_ERROR;
    }

    return TJC_HMI_SendCommand(command);
}

HAL_StatusTypeDef TJC_HMI_SetValue(const char *object, int32_t value)
{
    char command[TJC_HMI_COMMAND_MAX_LENGTH];
    int length;

    if (object == NULL)
    {
        return HAL_ERROR;
    }

    length = snprintf(command, sizeof(command), "%s.val=%ld", object, (long)value);
    if ((length < 0) || ((size_t)length >= sizeof(command)))
    {
        return HAL_ERROR;
    }

    return TJC_HMI_SendCommand(command);
}

TJC_HMI_Event TJC_HMI_Poll(void)
{
    TJC_HMI_Event no_event = {TJC_HMI_EVENT_NONE, 0U, 0U, 0U};
    uint8_t byte;

    if (tjc_huart == NULL)
    {
        return no_event;
    }

    while (HAL_UART_Receive(tjc_huart, &byte, 1U, 0U) == HAL_OK)
    {
        if (byte == 0xFFU)
        {
            tjc_rx_ff_count++;
            if (tjc_rx_ff_count == 3U)
            {
                return TJC_HMI_ParseFrame();
            }
            continue;
        }

        while (tjc_rx_ff_count > 0U)
        {
            if (tjc_rx_length < sizeof(tjc_rx_frame))
            {
                tjc_rx_frame[tjc_rx_length++] = 0xFFU;
            }
            tjc_rx_ff_count--;
        }

        if (tjc_rx_length < sizeof(tjc_rx_frame))
        {
            tjc_rx_frame[tjc_rx_length++] = byte;
        }
        else
        {
            tjc_rx_length = 0U;
            tjc_rx_ff_count = 0U;
        }
    }

    return no_event;
}
