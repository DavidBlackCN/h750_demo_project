#include "ADC_VOFA_FML.h"

#include "adc.h"
#include "tim.h"
#include "usart.h"
#include <stdio.h>

#define ADC_VOFA_SAMPLE_RATE_HZ   1000000U
#define ADC_VOFA_DMA_SAMPLE_COUNT 1024U
#define ADC_VOFA_TX_BUFFER_SIZE   16384U

static uint16_t s_adc_buffer[ADC_VOFA_DMA_SAMPLE_COUNT]
    __attribute__((section(".dma_buffer"), aligned(32)));
static char s_tx_buffer[ADC_VOFA_TX_BUFFER_SIZE];
static volatile bool s_active;
static volatile bool s_capture_ready;
static volatile uint32_t s_error_count;
static bool s_transmitted;

static HAL_StatusTypeDef adc_vofa_configure_trigger_timer(void)
{
    RCC_ClkInitTypeDef clocks = {0};
    uint32_t flash_latency = 0U;
    uint32_t timer_clock_hz = HAL_RCC_GetPCLK2Freq();
    uint32_t divider;

    HAL_RCC_GetClockConfig(&clocks, &flash_latency);
    if (clocks.APB2CLKDivider != RCC_APB2_DIV1)
    {
        timer_clock_hz *= 2U;
    }

    divider = (timer_clock_hz + (ADC_VOFA_SAMPLE_RATE_HZ / 2U)) / ADC_VOFA_SAMPLE_RATE_HZ;
    if ((divider < 2U) || (divider > 65536U))
    {
        return HAL_ERROR;
    }

    (void)HAL_TIM_Base_Stop(&htim1);
    __HAL_TIM_SET_PRESCALER(&htim1, 0U);
    __HAL_TIM_SET_AUTORELOAD(&htim1, divider - 1U);
    __HAL_TIM_SET_COUNTER(&htim1, 0U);
    MODIFY_REG(htim1.Instance->CR2, TIM_CR2_MMS, TIM_TRGO_UPDATE);
    htim1.Instance->EGR = TIM_EGR_UG;
    return HAL_OK;
}

HAL_StatusTypeDef ADC_VOFA_FML_Start(void)
{
    s_active = false;
    s_capture_ready = false;
    s_error_count = 0U;
    s_transmitted = false;

    if (adc_vofa_configure_trigger_timer() != HAL_OK)
    {
        return HAL_ERROR;
    }

    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)s_adc_buffer, sizeof(s_adc_buffer));
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc_buffer, ADC_VOFA_DMA_SAMPLE_COUNT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    s_active = true;
    if (HAL_TIM_Base_Start(&htim1) != HAL_OK)
    {
        s_active = false;
        (void)HAL_ADC_Stop_DMA(&hadc1);
        return HAL_ERROR;
    }
    return HAL_OK;
}

bool ADC_VOFA_FML_IsActive(void)
{
    return s_active;
}

void ADC_VOFA_FML_OnAdcHalfComplete(void)
{
    /* The first half is retained; transmission starts only after the full frame. */
}

void ADC_VOFA_FML_OnAdcComplete(void)
{
    __HAL_TIM_DISABLE(&htim1);
    s_active = false;
    s_capture_ready = true;
}

void ADC_VOFA_FML_OnAdcError(void)
{
    s_error_count++;
}

void ADC_VOFA_FML_Process(void)
{
    size_t length = 0U;

    if ((!s_capture_ready) || s_transmitted)
    {
        return;
    }

    s_capture_ready = false;
    (void)HAL_ADC_Stop_DMA(&hadc1);
    SCB_InvalidateDCache_by_Addr((uint32_t *)s_adc_buffer, sizeof(s_adc_buffer));
    for (uint32_t index = 0U; index < ADC_VOFA_DMA_SAMPLE_COUNT; ++index)
    {
        const float voltage = (3.3f * (float)s_adc_buffer[index]) / 4095.0f;
        const int written = snprintf(&s_tx_buffer[length], ADC_VOFA_TX_BUFFER_SIZE - length,
                                     "adc_pa1:%.3f\r\n", (double)voltage);
        if ((written <= 0) || ((size_t)written >= (ADC_VOFA_TX_BUFFER_SIZE - length)))
        {
            return;
        }
        length += (size_t)written;
    }

    (void)HAL_UART_Transmit(&huart1, (uint8_t *)s_tx_buffer, (uint16_t)length, 250U);
    s_transmitted = true;
}
