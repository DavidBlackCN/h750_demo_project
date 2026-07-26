#include "ADC2_CAPTURE_FML.h"

#include "adc.h"
#include "tim.h"

uint16_t adc2_dma_buffer[ADC2_DMA_BUFFER_LENGTH]
    __attribute__((section(".dma_buffer"), aligned(32)));
volatile uint8_t adc2_deal_flag;
volatile uint8_t adc2_proc_flag;

static volatile uint8_t adc2_capture_active;
static volatile uint8_t adc2_capture_complete;
static uint32_t adc2_capture_sample_count;
static float adc2_capture_sample_rate_hz;

static uint32_t adc2_capture_get_tim1_clock_hz(void)
{
    RCC_ClkInitTypeDef clocks = {0};
    uint32_t flash_latency = 0U;
    uint32_t timer_clock_hz = HAL_RCC_GetPCLK2Freq();

    HAL_RCC_GetClockConfig(&clocks, &flash_latency);
    if (clocks.APB2CLKDivider != RCC_APB2_DIV1)
    {
        timer_clock_hz *= 2U;
    }

    return timer_clock_hz;
}

static HAL_StatusTypeDef adc2_capture_configure_timer(float requested_sample_rate_hz)
{
    uint32_t timer_clock_hz;
    uint32_t divider;

    if (requested_sample_rate_hz <= 0.0f)
    {
        return HAL_ERROR;
    }

    timer_clock_hz = adc2_capture_get_tim1_clock_hz();
    divider = (uint32_t)(((float)timer_clock_hz / requested_sample_rate_hz) + 0.5f);
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
    adc2_capture_sample_rate_hz = (float)timer_clock_hz / (float)divider;

    return HAL_OK;
}

void MY_ADC2_Init(void)
{
    (void)ADC2_CAPTURE_FML_Start(1000000.0f, 1024U);
}

HAL_StatusTypeDef ADC2_CAPTURE_FML_Start(float requested_sample_rate_hz,
                                         uint32_t sample_count)
{
    HAL_StatusTypeDef status;

    if ((sample_count == 0U) || (sample_count > ADC2_DMA_BUFFER_LENGTH) ||
        (adc2_capture_active != 0U) || (adc2_capture_complete != 0U))
    {
        return HAL_BUSY;
    }

    adc2_deal_flag = 0U;
    adc2_proc_flag = 0U;
    adc2_capture_complete = 0U;
    adc2_capture_sample_count = sample_count;

    /* H750 adaptation: RAM_D2 is cacheable, so prepare the full DMA range. */
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)adc2_dma_buffer,
                                      sample_count * sizeof(adc2_dma_buffer[0]));

    status = adc2_capture_configure_timer(requested_sample_rate_hz);
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_dma_buffer, sample_count);
    if (status != HAL_OK)
    {
        return status;
    }

    adc2_capture_active = 1U;
    status = HAL_TIM_Base_Start(&htim1);
    if (status != HAL_OK)
    {
        adc2_capture_active = 0U;
        (void)HAL_ADC_Stop_DMA(&hadc2);
    }

    return status;
}

bool ADC2_CAPTURE_FML_IsActive(void)
{
    return (adc2_capture_active != 0U);
}

void ADC2_CAPTURE_FML_Poll(void)
{
    if ((adc2_capture_active != 0U) && (hadc2.DMA_Handle != NULL) &&
        (__HAL_DMA_GET_COUNTER(hadc2.DMA_Handle) == 0U))
    {
        __HAL_TIM_DISABLE(&htim1);
        adc2_capture_active = 0U;
        adc2_deal_flag = 1U;
        adc2_capture_complete = 1U;
    }
}

bool ADC2_CAPTURE_FML_TakeCompletedFrame(void)
{
    if (adc2_capture_complete == 0U)
    {
        return false;
    }

    adc2_capture_complete = 0U;
    (void)HAL_TIM_Base_Stop(&htim1);
    (void)HAL_ADC_Stop_DMA(&hadc2);
    SCB_InvalidateDCache_by_Addr((uint32_t *)adc2_dma_buffer,
                                 adc2_capture_sample_count * sizeof(adc2_dma_buffer[0]));

    return true;
}

const uint16_t *ADC2_CAPTURE_FML_GetRawBuffer(void)
{
    return adc2_dma_buffer;
}

uint32_t ADC2_CAPTURE_FML_GetSampleCount(void)
{
    return adc2_capture_sample_count;
}

float ADC2_CAPTURE_FML_GetSampleRateHz(void)
{
    return adc2_capture_sample_rate_hz;
}

void ADC2_CAPTURE_FML_OnDmaComplete(ADC_HandleTypeDef *hadc)
{
    if ((hadc->Instance == ADC2) && (adc2_capture_active != 0U))
    {
        /* The ISR only freezes the trigger and publishes frame completion. */
        __HAL_TIM_DISABLE(&htim1);
        adc2_capture_active = 0U;
        adc2_deal_flag = 1U;
        adc2_capture_complete = 1U;
    }
}

void ADC2_CAPTURE_FML_OnAdcError(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC2)
    {
        adc2_capture_active = 0U;
        adc2_capture_complete = 0U;
    }
}
