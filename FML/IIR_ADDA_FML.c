#include "IIR_ADDA_FML.h"

#include "IIR_FML.h"
#include "adc.h"
#include "dac.h"
#include "tim.h"

#include <string.h>

__attribute__((section(".dma_buffer"), aligned(32)))
static uint16_t s_adc_dma_buffer[IIR_ADDA_DMA_SAMPLE_COUNT];

__attribute__((section(".dma_buffer"), aligned(32)))
static uint16_t s_dac_dma_buffer[IIR_ADDA_DMA_SAMPLE_COUNT];

#if !IIR_ADDA_BYPASS_TEST_ENABLED
static q15_t s_filter_q15_scratch[IIR_ADDA_DMA_HALF_SAMPLE_COUNT];
static float32_t s_filter_scratch[IIR_ADDA_DMA_HALF_SAMPLE_COUNT];
static iir_lowpass_filter_t s_filter;
#endif
static volatile uint8_t s_active = 0U;
static volatile uint32_t s_half_callback_count = 0U;
static volatile uint32_t s_full_callback_count = 0U;
static volatile uint32_t s_error_count = 0U;
static uint32_t s_actual_sample_rate_hz = 0U;

static uint32_t iir_adda_get_timer_clock_hz(uint32_t pclk_hz,
                                             uint32_t apb_divider)
{
    return (apb_divider == RCC_HCLK_DIV1) ? pclk_hz : (pclk_hz * 2U);
}

static HAL_StatusTypeDef iir_adda_configure_timers(uint32_t requested_sample_rate_hz)
{
    RCC_ClkInitTypeDef clocks = {0};
    uint32_t flash_latency = 0U;
    uint32_t tim1_clock_hz;
    uint32_t tim4_clock_hz;
    uint32_t divider;

    if (requested_sample_rate_hz == 0U)
    {
        return HAL_ERROR;
    }

    HAL_RCC_GetClockConfig(&clocks, &flash_latency);
    tim1_clock_hz = iir_adda_get_timer_clock_hz(HAL_RCC_GetPCLK2Freq(),
                                                 clocks.APB2CLKDivider);
    tim4_clock_hz = iir_adda_get_timer_clock_hz(HAL_RCC_GetPCLK1Freq(),
                                                 clocks.APB1CLKDivider);
    if ((tim1_clock_hz == 0U) || (tim1_clock_hz != tim4_clock_hz))
    {
        return HAL_ERROR;
    }

    divider = (tim1_clock_hz + (requested_sample_rate_hz / 2U)) /
              requested_sample_rate_hz;
    if ((divider < 2U) || (divider > 65536U))
    {
        return HAL_ERROR;
    }

    (void)HAL_TIM_Base_Stop(&htim1);
    (void)HAL_TIM_Base_Stop(&htim4);
    __HAL_TIM_SET_PRESCALER(&htim1, 0U);
    __HAL_TIM_SET_AUTORELOAD(&htim1, divider - 1U);
    __HAL_TIM_SET_COUNTER(&htim1, 0U);
    MODIFY_REG(htim1.Instance->CR2, TIM_CR2_MMS, TIM_TRGO_UPDATE);
    htim1.Instance->EGR = TIM_EGR_UG;

    __HAL_TIM_SET_PRESCALER(&htim4, 0U);
    __HAL_TIM_SET_AUTORELOAD(&htim4, divider - 1U);
    __HAL_TIM_SET_COUNTER(&htim4, 0U);
    htim4.Instance->EGR = TIM_EGR_UG;

    s_actual_sample_rate_hz = tim1_clock_hz / divider;
    return HAL_OK;
}

static void iir_adda_prepare_dma_buffers(void)
{
    uint32_t index;

    for (index = 0U; index < IIR_ADDA_DMA_SAMPLE_COUNT; index++)
    {
        s_dac_dma_buffer[index] = 2048U;
    }

    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
    {
        SCB_CleanInvalidateDCache_by_Addr((uint32_t *)s_adc_dma_buffer,
                                          sizeof(s_adc_dma_buffer));
        SCB_CleanDCache_by_Addr((uint32_t *)s_dac_dma_buffer,
                                sizeof(s_dac_dma_buffer));
    }
}

#if IIR_ADDA_BYPASS_TEST_ENABLED
static void iir_adda_copy_adc12_to_dac12(const uint16_t *adc_codes,
                                         uint16_t *dac_codes,
                                         uint32_t sample_count)
{
    uint32_t index;

    for (index = 0U; index < sample_count; index++)
    {
        dac_codes[index] = (adc_codes[index] > 4095U) ? 4095U : adc_codes[index];
    }
}
#endif

static void iir_adda_process_half(uint32_t input_offset, uint32_t output_offset)
{
    if (s_active == 0U)
    {
        return;
    }

    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
    {
        SCB_InvalidateDCache_by_Addr((uint32_t *)&s_adc_dma_buffer[input_offset],
                                     IIR_ADDA_DMA_HALF_SAMPLE_COUNT * sizeof(uint16_t));
    }

#if IIR_ADDA_BYPASS_TEST_ENABLED
    iir_adda_copy_adc12_to_dac12(&s_adc_dma_buffer[input_offset],
                                 &s_dac_dma_buffer[output_offset],
                                 IIR_ADDA_DMA_HALF_SAMPLE_COUNT);
#else
    if (!IIR_Lowpass_ProcessAdc12ToDac12(&s_filter,
                                         &s_adc_dma_buffer[input_offset],
                                         &s_dac_dma_buffer[output_offset],
                                         s_filter_q15_scratch,
                                         s_filter_scratch,
                                         IIR_ADDA_DMA_HALF_SAMPLE_COUNT,
                                         2048U,
                                         2048U))
    {
        s_error_count++;
        return;
    }
#endif

    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
    {
        SCB_CleanDCache_by_Addr((uint32_t *)&s_dac_dma_buffer[output_offset],
                                IIR_ADDA_DMA_HALF_SAMPLE_COUNT * sizeof(uint16_t));
    }
}

HAL_StatusTypeDef IIR_ADDA_FML_Start(void)
{
    HAL_StatusTypeDef status;

    s_active = 0U;
    s_half_callback_count = 0U;
    s_full_callback_count = 0U;
    s_error_count = 0U;

    status = iir_adda_configure_timers(IIR_ADDA_SAMPLE_RATE_HZ);
    if (status != HAL_OK)
    {
        return status;
    }

#if !IIR_ADDA_BYPASS_TEST_ENABLED
    if (!IIR_Lowpass_Init(&s_filter, (float32_t)s_actual_sample_rate_hz))
    {
        return HAL_ERROR;
    }
#endif

    iir_adda_prepare_dma_buffers();

    status = HAL_DAC_Start_DMA(&hdac1,
                               DAC_CHANNEL_1,
                               (const uint32_t *)s_dac_dma_buffer,
                               IIR_ADDA_DMA_SAMPLE_COUNT,
                               DAC_ALIGN_12B_R);
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_ADC_Start_DMA(&hadc1,
                               (uint32_t *)s_adc_dma_buffer,
                               IIR_ADDA_DMA_SAMPLE_COUNT);
    if (status != HAL_OK)
    {
        (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
        return status;
    }

    s_active = 1U;

    /*
     * Keep the DAC DMA phase ahead of the ADC DMA phase.  At an ADC half/full
     * callback, the same-side DAC half has then already been consumed in the
     * current DAC cycle, and is not read again until the next cycle.  Starting
     * ADC first makes DAC lag by the software start interval and can overwrite
     * the last samples currently being output at every half-buffer boundary.
     */
    status = HAL_TIM_Base_Start(&htim4);
    if (status != HAL_OK)
    {
        (void)IIR_ADDA_FML_Stop();
        return status;
    }

    status = HAL_TIM_Base_Start(&htim1);
    if (status != HAL_OK)
    {
        (void)IIR_ADDA_FML_Stop();
        return status;
    }

    return HAL_OK;
}

HAL_StatusTypeDef IIR_ADDA_FML_Stop(void)
{
    HAL_StatusTypeDef adc_status;
    HAL_StatusTypeDef dac_status;
    HAL_StatusTypeDef tim1_status;
    HAL_StatusTypeDef tim4_status;

    s_active = 0U;
    tim1_status = HAL_TIM_Base_Stop(&htim1);
    tim4_status = HAL_TIM_Base_Stop(&htim4);
    adc_status = HAL_ADC_Stop_DMA(&hadc1);
    dac_status = HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);

    if (adc_status != HAL_OK)
    {
        return adc_status;
    }
    if (dac_status != HAL_OK)
    {
        return dac_status;
    }
    if (tim1_status != HAL_OK)
    {
        return tim1_status;
    }
    return tim4_status;
}

bool IIR_ADDA_FML_IsActive(void)
{
    return s_active != 0U;
}

void IIR_ADDA_FML_OnAdcHalfComplete(void)
{
    s_half_callback_count++;
    iir_adda_process_half(0U, 0U);
}

void IIR_ADDA_FML_OnAdcComplete(void)
{
    s_full_callback_count++;
    iir_adda_process_half(IIR_ADDA_DMA_HALF_SAMPLE_COUNT,
                          IIR_ADDA_DMA_HALF_SAMPLE_COUNT);
}

void IIR_ADDA_FML_OnAdcError(void)
{
    s_error_count++;
}

uint32_t IIR_ADDA_FML_GetActualSampleRateHz(void)
{
    return s_actual_sample_rate_hz;
}

uint32_t IIR_ADDA_FML_GetHalfCallbackCount(void)
{
    return s_half_callback_count;
}

uint32_t IIR_ADDA_FML_GetFullCallbackCount(void)
{
    return s_full_callback_count;
}

uint32_t IIR_ADDA_FML_GetErrorCount(void)
{
    return s_error_count;
}
