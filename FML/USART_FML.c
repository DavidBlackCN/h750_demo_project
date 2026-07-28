#include "USART_FML.h"

#include <string.h>

#define USART_ASYNC_LINE_CAPACITY 384U

volatile HAL_StatusTypeDef usart_last_status = HAL_OK;
volatile uint32_t usart_send_fail_count = 0;

static uint8_t s_async_active_line[USART_ASYNC_LINE_CAPACITY]
    __attribute__((section(".dma_buffer"), aligned(32)));
static uint8_t s_async_pending_line[USART_ASYNC_LINE_CAPACITY]
    __attribute__((section(".dma_buffer"), aligned(32)));
static volatile uint16_t s_async_active_length;
static volatile uint16_t s_async_pending_length;
static volatile uint8_t s_async_tx_active;

static void usart_async_start(UART_HandleTypeDef *huart, uint16_t length)
{
    SCB_CleanDCache_by_Addr((uint32_t *)s_async_active_line,
                            USART_ASYNC_LINE_CAPACITY);
    usart_last_status = HAL_UART_Transmit_DMA(huart, s_async_active_line, length);
    if (usart_last_status != HAL_OK)
    {
        usart_send_fail_count++;
        __disable_irq();
        s_async_tx_active = 0U;
        s_async_active_length = 0U;
        s_async_pending_length = 0U;
        __enable_irq();
    }
}

HAL_StatusTypeDef Usart_Send_Computer(UART_HandleTypeDef *huart, char *msg)
{
    usart_last_status = HAL_UART_Transmit(huart, (uint8_t *)msg, strlen(msg), 20);

    if (usart_last_status != HAL_OK)
    {
        usart_send_fail_count++;
    }

    return usart_last_status;
}

HAL_StatusTypeDef Usart_Send_ComputerAsync(UART_HandleTypeDef *huart,
                                           const char *msg)
{
    size_t length;

    if ((huart == NULL) || (msg == NULL) || (huart->Instance != USART1))
    {
        return HAL_ERROR;
    }

    length = strlen(msg);
    if (length >= USART_ASYNC_LINE_CAPACITY)
    {
        length = USART_ASYNC_LINE_CAPACITY - 1U;
    }
    if (length == 0U)
    {
        return HAL_OK;
    }

    return Usart_Send_ComputerAsyncData(huart, (const uint8_t *)msg,
                                        (uint16_t)length);
}

HAL_StatusTypeDef Usart_Send_ComputerAsyncData(UART_HandleTypeDef *huart,
                                               const uint8_t *data,
                                               uint16_t length)
{
    uint8_t start_transmit = 0U;

    if ((huart == NULL) || (data == NULL) || (huart->Instance != USART1) ||
        (length == 0U) || (length > USART_ASYNC_LINE_CAPACITY))
    {
        return HAL_ERROR;
    }

    __disable_irq();
    if (s_async_tx_active == 0U)
    {
        memcpy(s_async_active_line, data, length);
        s_async_active_length = length;
        s_async_tx_active = 1U;
        start_transmit = 1U;
    }
    else
    {
        /* Keep the newest result while a previous UART DMA transfer is active. */
        memcpy(s_async_pending_line, data, length);
        s_async_pending_length = length;
    }
    __enable_irq();

    if (start_transmit != 0U)
    {
        usart_async_start(huart, length);
    }

    return usart_last_status;
}

uint8_t USART_FML_OnTxComplete(UART_HandleTypeDef *huart)
{
    uint16_t next_length = 0U;

    if ((huart == NULL) || (huart->Instance != USART1) ||
        (s_async_tx_active == 0U))
    {
        return 0U;
    }

    __disable_irq();
    if (s_async_pending_length != 0U)
    {
        next_length = s_async_pending_length;
        memcpy(s_async_active_line, s_async_pending_line, next_length);
        s_async_active_length = next_length;
        s_async_pending_length = 0U;
    }
    else
    {
        s_async_tx_active = 0U;
    }
    __enable_irq();

    if (next_length != 0U)
    {
        usart_async_start(huart, next_length);
    }

    return 1U;
}

uint8_t USART_FML_OnError(UART_HandleTypeDef *huart)
{
    if ((huart == NULL) || (huart->Instance != USART1) ||
        (s_async_tx_active == 0U))
    {
        return 0U;
    }

    __disable_irq();
    s_async_tx_active = 0U;
    s_async_active_length = 0U;
    s_async_pending_length = 0U;
    __enable_irq();
    usart_send_fail_count++;
    return 1U;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    (void)USART_FML_OnError(huart);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    (void)USART_FML_OnTxComplete(huart);
}
