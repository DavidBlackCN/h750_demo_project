#include "TJC_HMI_ECHO_API.h"

#include <stdio.h>

#define TJC_HMI_ECHO_FRAME_CAPACITY 64U
#define TJC_HMI_ECHO_FIREWATER_LINE_CAPACITY 320U
#define TJC_HMI_ECHO_FRAME_END_BYTE 0xFFU
#define TJC_HMI_ECHO_FRAME_END_COUNT 3U
#define TJC_HMI_ECHO_TX_TIMEOUT_MS 20U
#define TJC_HMI_ECHO_DIAG_PERIOD_MS 500U

static UART_HandleTypeDef *s_hmi_uart;
static UART_HandleTypeDef *s_debug_uart;
static uint8_t s_frame[TJC_HMI_ECHO_FRAME_CAPACITY];
static uint8_t s_frame_length;
static uint8_t s_end_count;
static uint8_t s_drop_until_frame_end;
static uint32_t s_rx_byte_count;
static uint32_t s_frame_count;
static uint32_t s_drop_count;
static uint32_t s_last_diag_tick;

static void TJC_HMI_ECHO_API_SendFireWaterDiagnostics(void)
{
    char line[64];
    int length;

    length = snprintf(line, sizeof(line), "hmi_diag:%lu,%lu,%lu\r\n",
                      (unsigned long)s_rx_byte_count,
                      (unsigned long)s_frame_count,
                      (unsigned long)s_drop_count);
    if ((length > 0) && (length < (int)sizeof(line)))
    {
        (void)HAL_UART_Transmit(s_debug_uart, (const uint8_t *)line,
                                (uint16_t)length, TJC_HMI_ECHO_TX_TIMEOUT_MS);
    }
}

static void TJC_HMI_ECHO_API_SendFireWaterFrame(void)
{
    char line[TJC_HMI_ECHO_FIREWATER_LINE_CAPACITY];
    int length;

    length = snprintf(line, sizeof(line), "hmi_frame:%lu,%u",
                      (unsigned long)s_frame_count,
                      (unsigned int)s_frame_length);
    if ((length <= 0) || (length >= (int)sizeof(line)))
    {
        return;
    }

    for (uint8_t index = 0U; index < s_frame_length; ++index)
    {
        const int written = snprintf(&line[length], sizeof(line) - (size_t)length,
                                     ",%u", (unsigned int)s_frame[index]);

        if ((written <= 0) || (written >= (int)(sizeof(line) - (size_t)length)))
        {
            return;
        }
        length += written;
    }

    if ((size_t)length > (sizeof(line) - 3U))
    {
        return;
    }
    line[length++] = '\r';
    line[length++] = '\n';
    (void)HAL_UART_Transmit(s_debug_uart, (const uint8_t *)line,
                            (uint16_t)length, TJC_HMI_ECHO_TX_TIMEOUT_MS);
}

HAL_StatusTypeDef TJC_HMI_ECHO_API_Init(UART_HandleTypeDef *hmi_uart,
                                        UART_HandleTypeDef *debug_uart)
{
    if ((hmi_uart == NULL) || (debug_uart == NULL))
    {
        return HAL_ERROR;
    }

    s_hmi_uart = hmi_uart;
    s_debug_uart = debug_uart;
    s_frame_length = 0U;
    s_end_count = 0U;
    s_drop_until_frame_end = 0U;
    s_rx_byte_count = 0U;
    s_frame_count = 0U;
    s_drop_count = 0U;
    s_last_diag_tick = HAL_GetTick();
    TJC_HMI_ECHO_API_SendFireWaterDiagnostics();
    return HAL_OK;
}

void TJC_HMI_ECHO_API_Process(void)
{
    uint8_t byte;

    if ((s_hmi_uart == NULL) || (s_debug_uart == NULL))
    {
        return;
    }

    while (HAL_UART_Receive(s_hmi_uart, &byte, 1U, 0U) == HAL_OK)
    {
        s_rx_byte_count++;

        if (s_drop_until_frame_end != 0U)
        {
            s_end_count = (byte == TJC_HMI_ECHO_FRAME_END_BYTE) ?
                          (uint8_t)(s_end_count + 1U) : 0U;
            if (s_end_count == TJC_HMI_ECHO_FRAME_END_COUNT)
            {
                s_end_count = 0U;
                s_drop_until_frame_end = 0U;
            }
            continue;
        }

        if (s_frame_length >= sizeof(s_frame))
        {
            s_frame_length = 0U;
            s_end_count = (byte == TJC_HMI_ECHO_FRAME_END_BYTE) ? 1U : 0U;
            s_drop_until_frame_end = 1U;
            s_drop_count++;
            continue;
        }

        s_frame[s_frame_length++] = byte;
        if (byte == TJC_HMI_ECHO_FRAME_END_BYTE)
        {
            s_end_count++;
            if (s_end_count == TJC_HMI_ECHO_FRAME_END_COUNT)
            {
                s_frame_count++;
                TJC_HMI_ECHO_API_SendFireWaterFrame();
                s_frame_length = 0U;
                s_end_count = 0U;
            }
        }
        else
        {
            s_end_count = 0U;
        }
    }

    if ((uint32_t)(HAL_GetTick() - s_last_diag_tick) >= TJC_HMI_ECHO_DIAG_PERIOD_MS)
    {
        s_last_diag_tick = HAL_GetTick();
        TJC_HMI_ECHO_API_SendFireWaterDiagnostics();
    }
}
