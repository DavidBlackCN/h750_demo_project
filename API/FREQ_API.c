#include "FREQ_API.h"

#include "USART_FML.h"
#include "usart.h"
#include <stdio.h>

#define FREQ_PRINT_PERIOD_MS (1000U)

static uint32_t s_last_print_ms = 0U;

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
        (void)Usart_Send_Computer(&huart1,
            "freq demo PA0/TIM2_CH1 range=1Hz..1MHz resolution=0.5Hz/10Hz\r\n");
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
    (void)Usart_Send_Computer(&huart1, message);
}

const freq_result_t *FREQ_API_GetResult(void)
{
    return FREQ_FML_GetResult();
}
