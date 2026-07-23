#include "DAC_RATE_TEST_FML.h"

#include "arm_math.h"
#include "dac.h"
#include "tim.h"

#define DAC_RATE_TEST_TWO_PI 6.28318530717958647692f
#define DAC_RATE_TEST_REFERENCE_VOLTAGE 3.3f

__attribute__((section(".dma_buffer"), aligned(32)))
static uint32_t s_dac_buffer[DAC_RATE_TEST_BUFFER_SAMPLES];

static uint32_t dac_rate_test_get_tim4_clock_hz(void)
{
    RCC_ClkInitTypeDef clocks = {0};
    uint32_t flash_latency = 0U;
    uint32_t timer_clock_hz = HAL_RCC_GetPCLK1Freq();

    HAL_RCC_GetClockConfig(&clocks, &flash_latency);
    if (clocks.APB1CLKDivider != RCC_HCLK_DIV1)
    {
        timer_clock_hz *= 2U;
    }

    return timer_clock_hz;
}

static HAL_StatusTypeDef dac_rate_test_configure_timer(void)
{
    uint32_t timer_clock_hz = dac_rate_test_get_tim4_clock_hz();
    uint32_t divider;

    if (timer_clock_hz == 0U)
    {
        return HAL_ERROR;
    }

    divider = (timer_clock_hz + (DAC_RATE_TEST_SAMPLE_RATE_HZ / 2U)) /
              DAC_RATE_TEST_SAMPLE_RATE_HZ;
    if ((divider < 2U) || (divider > 65536U))
    {
        return HAL_ERROR;
    }

    (void)HAL_TIM_Base_Stop(&htim4);
    __HAL_TIM_SET_PRESCALER(&htim4, 0U);
    __HAL_TIM_SET_AUTORELOAD(&htim4, divider - 1U);
    __HAL_TIM_SET_COUNTER(&htim4, 0U);
    MODIFY_REG(htim4.Instance->CR2, TIM_CR2_MMS, TIM_TRGO_UPDATE);
    htim4.Instance->EGR = TIM_EGR_UG;

    return HAL_OK;
}

static void dac_rate_test_fill_sine(void)
{
    const float32_t amplitude_v = DAC_RATE_TEST_SINE_VPP_V * 0.5f;
    uint32_t index;

    for (index = 0U; index < DAC_RATE_TEST_BUFFER_SAMPLES; index++)
    {
        /* One complete cycle per DMA buffer: the circular boundary is continuous. */
        float32_t phase = DAC_RATE_TEST_TWO_PI *
                          ((float32_t)index / (float32_t)DAC_RATE_TEST_BUFFER_SAMPLES);
        float32_t voltage = DAC_RATE_TEST_SINE_OFFSET_V + (amplitude_v * arm_sin_f32(phase));
        float32_t code = (voltage * 4095.0f / DAC_RATE_TEST_REFERENCE_VOLTAGE) + 0.5f;

        if (code < 0.0f)
        {
            code = 0.0f;
        }
        else if (code > 4095.0f)
        {
            code = 4095.0f;
        }
        s_dac_buffer[index] = (uint32_t)code;
    }

    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
    {
        SCB_CleanDCache_by_Addr((uint32_t *)s_dac_buffer, sizeof(s_dac_buffer));
    }
}

HAL_StatusTypeDef DAC_RateTest_FML_Start(void)
{
    HAL_StatusTypeDef status;

    dac_rate_test_fill_sine();
    status = dac_rate_test_configure_timer();
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_DAC_Start_DMA(&hdac1,
                               DAC_CHANNEL_1,
                               s_dac_buffer,
                               DAC_RATE_TEST_BUFFER_SAMPLES,
                               DAC_ALIGN_12B_R);
    if (status != HAL_OK)
    {
        return status;
    }

    return HAL_TIM_Base_Start(&htim4);
}
