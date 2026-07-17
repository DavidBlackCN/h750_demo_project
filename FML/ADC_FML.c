#include "ADC_FML.h"

#include "USART_FML.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include <stdio.h>

/*
 * 1 kHz 固定输入、1024 点 FFT：75 MHz / 2417 = 31030.2027 Hz，
 * 单帧包含 33.000107 个周期，接近相干采样且五次谐波远低于 Nyquist。
 */
#define ADC1_TARGET_SAMPLE_RATE_HZ 31030U

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
static uint8_t adc1_first_capture = 1U;
static uint32_t adc1_get_tim1_clock_hz(void)
{
    uint32_t timer_clock_hz = HAL_RCC_GetPCLK2Freq();

    if ((RCC->D2CFGR & RCC_D2CFGR_D2PPRE2) != RCC_D2CFGR_D2PPRE2_DIV1)
    {
        timer_clock_hz *= 2U;
    }

    return timer_clock_hz;
}

static HAL_StatusTypeDef adc1_configure_tim1_trigger(void)
{
    TIM_MasterConfigTypeDef master_config = {0};
    uint32_t timer_clock_hz = adc1_get_tim1_clock_hz();
    uint32_t period_counts;

    if (timer_clock_hz < ADC1_TARGET_SAMPLE_RATE_HZ)
    {
        return HAL_ERROR;
    }

    period_counts = timer_clock_hz / ADC1_TARGET_SAMPLE_RATE_HZ;
    htim1.Init.Prescaler = 0U;
    htim1.Init.Period = period_counts - 1U;
    __HAL_TIM_SET_PRESCALER(&htim1, htim1.Init.Prescaler);
    __HAL_TIM_SET_AUTORELOAD(&htim1, htim1.Init.Period);
    __HAL_TIM_SET_COUNTER(&htim1, 0U);

    master_config.MasterOutputTrigger = TIM_TRGO_UPDATE;
    master_config.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    return HAL_TIMEx_MasterConfigSynchronization(&htim1, &master_config);
}

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
        /* DMA 为 one-shot；满帧后立即停触发，避免下一次转换产生 overrun。 */
        __HAL_TIM_DISABLE(&htim1);
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

float MY_ADC1_GetSampleRateHz(void)
{
    uint32_t timer_clock_hz = adc1_get_tim1_clock_hz();

    return (float)timer_clock_hz /
           ((float)(htim1.Init.Prescaler + 1U) * (float)(htim1.Init.Period + 1U));
}

void MY_ADC1_StopCapture(void)
{
    (void)HAL_TIM_Base_Stop(&htim1);
    (void)HAL_ADC_Stop_DMA(&hadc1);
}

HAL_StatusTypeDef MY_ADC1_StartCapture(void)
{
    HAL_StatusTypeDef status;

    adc1_half_flag = 0U;
    adc1_deal_flag = 0U;
    adc1_proc_flag = 0U;

    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
    {
        SCB_CleanInvalidateDCache_by_Addr(adc1_dma_buffer, sizeof(adc1_dma_buffer));
    }

    if (adc1_first_capture != 0U)
    {
        (void)Usart_Send_Computer(&huart1, "adc dma start call\r\n");
    }
    status = HAL_ADC_Start_DMA(&hadc1, adc1_dma_buffer, ADC1_DMA_BUFFER_LENGTH);
    if (status != HAL_OK)
    {
        return status;
    }

    if (adc1_first_capture != 0U)
    {
        (void)Usart_Send_Computer(&huart1, "adc dma started, timer start call\r\n");
    }
    status = HAL_TIM_Base_Start(&htim1);
    if (status != HAL_OK)
    {
        HAL_ADC_Stop_DMA(&hadc1);
        return status;
    }

    if (adc1_first_capture != 0U)
    {
        (void)Usart_Send_Computer(&huart1, "adc timer started\r\n");
        adc1_first_capture = 0U;
    }

    return HAL_OK;
}

HAL_StatusTypeDef MY_ADC1_Init(void)
{
    HAL_StatusTypeDef status;

    status = adc1_configure_tim1_trigger();
    if (status != HAL_OK)
    {
        return status;
    }

    /* 当前板卡启动校准会卡在 ADCAL 等待；FFT 前会去均值，先跳过 offset 校准。 */
    (void)Usart_Send_Computer(&huart1, "adc timer configured, starting dma\r\n");
    return MY_ADC1_StartCapture();
}
