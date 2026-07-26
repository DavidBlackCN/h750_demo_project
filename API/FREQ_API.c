#include "FREQ_API.h"

#include "usart.h"
#include <stdio.h>
#include <string.h>

#define FREQ_PRINT_PERIOD_MS (1000U)
#define FREQ_UART_LINE_CAPACITY (176U)

static uint32_t s_last_print_ms = 0U;
static uint8_t s_uart_active_line[FREQ_UART_LINE_CAPACITY];
static uint8_t s_uart_pending_line[FREQ_UART_LINE_CAPACITY];
static volatile uint16_t s_uart_active_length = 0U;
static volatile uint16_t s_uart_pending_length = 0U;
static volatile uint8_t s_uart_tx_active = 0U;

static void freq_uart_submit(const char *message)
{
    size_t length;
    uint8_t start_transmit = 0U;

    if (message == NULL)
    {
        return;
    }

    length = strlen(message);
    if (length >= FREQ_UART_LINE_CAPACITY)
    {
        length = FREQ_UART_LINE_CAPACITY - 1U;
    }
    if (length == 0U)
    {
        return;
    }

    __disable_irq();
    if (s_uart_tx_active == 0U)
    {
        memcpy(s_uart_active_line, message, length);
        s_uart_active_length = (uint16_t)length;
        s_uart_tx_active = 1U;
        start_transmit = 1U;
    }
    else
    {
        memcpy(s_uart_pending_line, message, length);
        s_uart_pending_length = (uint16_t)length;
    }
    __enable_irq();

    if ((start_transmit != 0U) &&
        (HAL_UART_Transmit_IT(&huart1, s_uart_active_line,
                              s_uart_active_length) != HAL_OK))
    {
        __disable_irq();
        s_uart_tx_active = 0U;
        __enable_irq();
    }
}

static const char *freq_status_text(freq_status_t status)
{
    switch (status)
    {
        case FREQ_STATUS_NO_SIGNAL:   return "no_signal";
        case FREQ_STATUS_MEASURING:   return "measuring";
        case FREQ_STATUS_VALID:       return "valid";
        case FREQ_STATUS_BELOW_RANGE: return "below_range";
        case FREQ_STATUS_ABOVE_RANGE: return "above_range";
        default:                      return "error";
    }
}

static const char *freq_mode_text(freq_mode_t mode)
{
    switch (mode)
    {
        case FREQ_MODE_INPUT_CAPTURE: return "ic";
        case FREQ_MODE_DMA_CAPTURE:   return "dma";
        default:                      return "probe";
    }
}

HAL_StatusTypeDef FREQ_API_Init(void)
{
    HAL_StatusTypeDef status = FREQ_FML_Init();

    if (status == HAL_OK)
    {
        freq_uart_submit("freq demo PA0/TIM2_CH1 range=1Hz..1MHz resolution=0.5Hz/10Hz\r\n");
    }

    return status;
}

void FREQ_API_Process(void)
{
    char message[176];
    uint32_t now = HAL_GetTick();
    const freq_result_t *result;

    FREQ_FML_Process();
    result = FREQ_FML_GetResult();

    if ((now - s_last_print_ms) < FREQ_PRINT_PERIOD_MS)
    {
        return;
    }

    s_last_print_ms = now;

    (void)snprintf(message, sizeof(message),
                   "freq=%.1fHz raw=%.3fHz mode=%s status=%s ticks=%lu periods=%lu\r\n",
                   result->display_hz,
                   result->raw_hz,
                   freq_mode_text(result->mode),
                   freq_status_text(result->status),
                   (unsigned long)result->period_ticks,
                   (unsigned long)result->measured_periods);
    freq_uart_submit(message);
}

const freq_result_t *FREQ_API_GetResult(void)
{
    return FREQ_FML_GetResult();
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    if (s_uart_pending_length != 0U)
    {
        memcpy(s_uart_active_line, s_uart_pending_line, s_uart_pending_length);
        s_uart_active_length = s_uart_pending_length;
        s_uart_pending_length = 0U;
        if (HAL_UART_Transmit_IT(&huart1, s_uart_active_line,
                                 s_uart_active_length) != HAL_OK)
        {
            s_uart_tx_active = 0U;
        }
    }
    else
    {
        s_uart_tx_active = 0U;
    }
}
