#include "ADC_FML.h"

#include "USART_FML.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include <stdio.h>

uint32_t adc1_dma_buffer[ADC1_DMA_BUFFER_LENGTH] __attribute__((section(".dma_buffer"), aligned(32)));
float adc1_data[ADC1_FREQ_BLOCK_LENGTH];

volatile uint8_t adc1_half_flag = 0;
volatile uint8_t adc1_deal_flag = 0;
volatile uint8_t adc1_proc_flag = 0;

volatile uint32_t adc1_half_callback_count = 0;
volatile uint32_t adc1_full_callback_count = 0;
volatile uint32_t adc1_error_callback_count = 0;
volatile uint32_t adc1_last_dma_ndtr = 0;
volatile uint32_t adc1_last_adc_error = 0;
volatile uint32_t adc1_last_dma_error = 0;
volatile uint32_t adc1_last_callback_tick = 0;
volatile uint8_t adc1_last_callback_type = 0;

static volatile uint8_t adc1_debug_print_pending = 0;

static void adc1_debug_capture(ADC_HandleTypeDef *hadc, uint8_t callback_type)
{
    adc1_last_callback_type = callback_type;
    adc1_last_callback_tick = HAL_GetTick();
    adc1_last_adc_error = HAL_ADC_GetError(hadc);

    if (hadc->DMA_Handle != NULL)
    {
        adc1_last_dma_ndtr = __HAL_DMA_GET_COUNTER(hadc->DMA_Handle);
        adc1_last_dma_error = HAL_DMA_GetError(hadc->DMA_Handle);
    }
    else
    {
        adc1_last_dma_ndtr = 0;
        adc1_last_dma_error = 0xFFFFFFFFU;
    }

    adc1_debug_print_pending = 1;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        adc1_half_callback_count++;
        adc1_half_flag = 1;
        adc1_debug_capture(hadc, 1U);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        adc1_full_callback_count++;
        adc1_deal_flag = 1;
        adc1_debug_capture(hadc, 2U);
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        adc1_error_callback_count++;
        adc1_debug_capture(hadc, 3U);
    }
}

void ADC_Debug_Print(void)
{
    char msg[160];
    uint32_t half_count;
    uint32_t full_count;
    uint32_t error_count;
    uint32_t ndtr;
    uint32_t adc_error;
    uint32_t dma_error;
    uint32_t tick;
    uint8_t callback_type;

    if (!adc1_debug_print_pending)
    {
        return;
    }

    adc1_debug_print_pending = 0;

    half_count = adc1_half_callback_count;
    full_count = adc1_full_callback_count;
    error_count = adc1_error_callback_count;
    ndtr = adc1_last_dma_ndtr;
    adc_error = adc1_last_adc_error;
    dma_error = adc1_last_dma_error;
    tick = adc1_last_callback_tick;
    callback_type = adc1_last_callback_type;

    snprintf(msg, sizeof(msg),
             "adc cb type=%u half=%lu full=%lu err=%lu ndtr=%lu adc_err=0x%08lX dma_err=0x%08lX tick=%lu\r\n",
             callback_type,
             (unsigned long)half_count,
             (unsigned long)full_count,
             (unsigned long)error_count,
             (unsigned long)ndtr,
             (unsigned long)adc_error,
             (unsigned long)dma_error,
             (unsigned long)tick);

    (void)Usart_Send_Computer(&huart1, msg);
}

HAL_StatusTypeDef MY_ADC1_Init(void)
{
    HAL_StatusTypeDef status;

    status = HAL_ADC_Start_DMA(&hadc1, adc1_dma_buffer, ADC1_DMA_BUFFER_LENGTH);
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_TIM_Base_Start(&htim1);
    if (status != HAL_OK)
    {
        HAL_ADC_Stop_DMA(&hadc1);
        return status;
    }

    return HAL_OK;
}
