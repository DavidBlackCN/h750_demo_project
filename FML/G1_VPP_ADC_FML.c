#include "G1_VPP_ADC_FML.h"

#include "adc.h"
#include "tim.h"

static uint16_t s_dma_buffer[G1_VPP_ADC_FRAME_SAMPLES]
    __attribute__((section(".dma_buffer"), aligned(32)));
static uint8_t s_calibrated;
static uint8_t s_active;
static uint8_t s_complete;
static uint8_t s_error;
static uint32_t s_error_code;
static float s_sample_rate_hz;

static uint32_t g1_vpp_get_tim1_clock_hz(void)
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

static HAL_StatusTypeDef g1_vpp_configure_timer(void)
{
    uint32_t timer_clock_hz = g1_vpp_get_tim1_clock_hz();
    uint32_t divider = (uint32_t)((((float)timer_clock_hz) /
                                  G1_VPP_ADC_SAMPLE_RATE_HZ) + 0.5f);

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
    s_sample_rate_hz = (float)timer_clock_hz / (float)divider;

    return HAL_OK;
}

HAL_StatusTypeDef G1_VPP_ADC_FML_Start(void)
{
    HAL_StatusTypeDef status;

    if ((s_active != 0U) || (s_complete != 0U))
    {
        return HAL_BUSY;
    }

    (void)HAL_TIM_Base_Stop(&htim1);
    s_error = 0U;
    s_error_code = HAL_ADC_ERROR_NONE;

    if (s_calibrated == 0U)
    {
        status = HAL_ADCEx_Calibration_Start(&hadc1,
                                             ADC_CALIB_OFFSET_LINEARITY,
                                             ADC_SINGLE_ENDED);
        if (status != HAL_OK)
        {
            s_error = 1U;
            s_error_code = HAL_ADC_GetError(&hadc1);
            return status;
        }
        s_calibrated = 1U;
    }

    status = g1_vpp_configure_timer();
    if (status != HAL_OK)
    {
        return status;
    }

    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)s_dma_buffer,
                                      sizeof(s_dma_buffer));
    status = HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_dma_buffer,
                               G1_VPP_ADC_FRAME_SAMPLES);
    if (status != HAL_OK)
    {
        s_error = 1U;
        s_error_code = HAL_ADC_GetError(&hadc1);
        return status;
    }

    s_active = 1U;
    status = HAL_TIM_Base_Start(&htim1);
    if (status != HAL_OK)
    {
        s_active = 0U;
        (void)HAL_ADC_Stop_DMA(&hadc1);
        s_error = 1U;
        s_error_code = HAL_ADC_GetError(&hadc1);
    }

    return status;
}

void G1_VPP_ADC_FML_Poll(void)
{
    if ((s_active == 0U) || (hadc1.DMA_Handle == NULL))
    {
        return;
    }

    if (HAL_ADC_GetError(&hadc1) != HAL_ADC_ERROR_NONE)
    {
        __HAL_TIM_DISABLE(&htim1);
        s_active = 0U;
        s_error = 1U;
        s_error_code = HAL_ADC_GetError(&hadc1);
        return;
    }

    if (__HAL_DMA_GET_COUNTER(hadc1.DMA_Handle) == 0U)
    {
        __HAL_TIM_DISABLE(&htim1);
        s_active = 0U;
        s_complete = 1U;
    }
}

bool G1_VPP_ADC_FML_TakeFrame(const uint16_t **samples,
                               uint32_t *sample_count)
{
    if ((s_complete == 0U) || (samples == NULL) || (sample_count == NULL))
    {
        return false;
    }

    s_complete = 0U;
    (void)HAL_TIM_Base_Stop(&htim1);
    (void)HAL_ADC_Stop_DMA(&hadc1);
    SCB_InvalidateDCache_by_Addr((uint32_t *)s_dma_buffer,
                                 sizeof(s_dma_buffer));
    *samples = s_dma_buffer;
    *sample_count = G1_VPP_ADC_FRAME_SAMPLES;

    return true;
}

bool G1_VPP_ADC_FML_HasError(void)
{
    return (s_error != 0U);
}

uint32_t G1_VPP_ADC_FML_GetErrorCode(void)
{
    return s_error_code;
}

float G1_VPP_ADC_FML_GetSampleRateHz(void)
{
    return s_sample_rate_hz;
}

bool G1_VPP_ADC_FML_OnAdcComplete(ADC_HandleTypeDef *hadc)
{
    if ((s_active == 0U) || (hadc != &hadc1))
    {
        return false;
    }

    __HAL_TIM_DISABLE(&htim1);
    s_active = 0U;
    s_complete = 1U;
    return true;
}

bool G1_VPP_ADC_FML_OnAdcError(ADC_HandleTypeDef *hadc)
{
    if ((s_active == 0U) || (hadc != &hadc1))
    {
        return false;
    }

    __HAL_TIM_DISABLE(&htim1);
    s_active = 0U;
    s_error = 1U;
    s_error_code = HAL_ADC_GetError(&hadc1);
    return true;
}
