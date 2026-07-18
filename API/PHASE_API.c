#include "PHASE_API.h"

#include "ADC_BLL.h"
#include "ADC_FML.h"
#include "FFT_FML.h"
#include "PHASE_BLL.h"
#include "USART_FML.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include <stdio.h>

typedef enum
{
    PHASE_API_MEASURE_FREQUENCY = 0,
    PHASE_API_COHERENT_PHASE
} phase_api_state_t;

static phase_api_state_t s_phase_state = PHASE_API_MEASURE_FREQUENCY;
static float s_measured_frequency_hz = 0.0f;
static adc_coherent_config_t s_coherent_config;
static uint32_t s_frequency_calculation_us = 0U;
static uint32_t s_capture_start_tick = 0U;
static uint8_t s_capture_active = 0U;

static uint32_t phase_cycles_to_us(uint32_t cycles)
{
    uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    return (cycles_per_us > 0U) ? (cycles / cycles_per_us) : 0U;
}

static HAL_StatusTypeDef phase_start_frequency_capture(void)
{
    HAL_StatusTypeDef status;

    ADC_Dual_SetDiscoverySampling();
    s_phase_state = PHASE_API_MEASURE_FREQUENCY;
    status = ADC_Dual_StartCaptureLength(FFT_LENGTH);
    if (status == HAL_OK)
    {
        s_capture_start_tick = HAL_GetTick();
        s_capture_active = 1U;
    }
    return status;
}

HAL_StatusTypeDef Phase_API_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    generate_hanning_window();
    return phase_start_frequency_capture();
}

static void phase_poll_capture(void)
{
    uint32_t ndtr;

    if (!s_capture_active || adc1_deal_flag || adc1_proc_flag ||
        (hadc1.DMA_Handle == NULL))
    {
        return;
    }

    ndtr = __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle);
    if (ndtr == 0U)
    {
        /* Normal-mode DMA frame complete; completion is polled deliberately. */
        __HAL_TIM_DISABLE(&htim1);
        adc1_deal_flag = 1U;
        s_capture_active = 0U;
        return;
    }

    if ((HAL_GetTick() - s_capture_start_tick) >= 100U)
    {
        char message[320];

        snprintf(message, sizeof(message),
                 "phase capture timeout state=%u ndtr=%lu/%lu tim_cr1=0x%08lX tim_cnt=%lu psc=%lu arr=%lu adc_cr=0x%08lX adc_cfgr=0x%08lX adc_isr=0x%08lX adc_ccr=0x%08lX dma_err=0x%08lX\r\n",
                 (unsigned int)s_phase_state,
                 (unsigned long)ndtr,
                 (unsigned long)adc1_capture_length,
                 (unsigned long)htim1.Instance->CR1,
                 (unsigned long)htim1.Instance->CNT,
                 (unsigned long)htim1.Instance->PSC,
                 (unsigned long)htim1.Instance->ARR,
                 (unsigned long)hadc1.Instance->CR,
                 (unsigned long)hadc1.Instance->CFGR,
                 (unsigned long)hadc1.Instance->ISR,
                 (unsigned long)ADC12_COMMON->CCR,
                 (unsigned long)HAL_DMA_GetError(hadc1.DMA_Handle));
        (void)Usart_Send_Computer(&huart1, message);

        (void)HAL_TIM_Base_Stop(&htim1);
        (void)HAL_ADCEx_MultiModeStop_DMA(&hadc1);
        s_capture_active = 0U;
        if (phase_start_frequency_capture() != HAL_OK)
        {
            (void)Usart_Send_Computer(&huart1,
                                      "phase timeout restart failed\r\n");
        }
    }
}

void Phase_API_Process(void)
{
    phase_poll_capture();
    adc1_deal();

    if (adc1_proc_flag)
    {
        char message[320];
        HAL_StatusTypeDef restart_status;
        uint32_t captured_length = adc1_capture_length;
        uint32_t calculation_start_cycles = DWT->CYCCNT;

        adc1_proc_flag = 0U;
        s_capture_active = 0U;

        if (s_phase_state == PHASE_API_MEASURE_FREQUENCY)
        {
            float nominal_frequency_hz;

            s_measured_frequency_hz = Phase_BLL_EstimateFrequency(
                adc1_data, adc2_data, captured_length, ADC_DUAL_SAMPLE_RATE_HZ);
            nominal_frequency_hz = Phase_BLL_NominalFrequency(
                s_measured_frequency_hz);

            if ((nominal_frequency_hz < 1000.0f) ||
                (nominal_frequency_hz > 100000.0f) ||
                (ADC_Dual_ConfigureCoherentSampling(s_measured_frequency_hz,
                                                    &s_coherent_config) != HAL_OK))
            {
                snprintf(message, sizeof(message),
                         "phase frequency invalid freq=%.2fHz\r\n",
                         s_measured_frequency_hz);
                (void)Usart_Send_Computer(&huart1, message);
                restart_status = phase_start_frequency_capture();
            }
            else
            {
                s_frequency_calculation_us = phase_cycles_to_us(
                    DWT->CYCCNT - calculation_start_cycles);
                s_phase_state = PHASE_API_COHERENT_PHASE;
                restart_status = ADC_Dual_StartCaptureLength(
                    s_coherent_config.sample_count);
                if (restart_status == HAL_OK)
                {
                    s_capture_start_tick = HAL_GetTick();
                    s_capture_active = 1U;
                }
            }
        }
        else
        {
            Phase_BLL_ProcessKnownFrequency(adc1_data, adc2_data,
                                            captured_length,
                                            s_coherent_config.sample_rate_hz,
                                            s_measured_frequency_hz);
            if ((s_coherent_config.closure_error_deg < -0.1f) ||
                (s_coherent_config.closure_error_deg > 0.1f))
            {
                phase_result.valid = 0U;
            }

            if (phase_result.valid)
            {
                snprintf(message, sizeof(message),
                         "freq=%.2fHz phase=%.3fdeg\r\n",
                         phase_result.frequency_hz,
                         phase_result.phase_difference_deg);
                (void)Usart_Send_Computer(&huart1, message);
            }

            restart_status = phase_start_frequency_capture();
        }

        if (restart_status != HAL_OK)
        {
            snprintf(message, sizeof(message),
                     "phase adc restart error=%u\r\n", (unsigned int)restart_status);
            (void)Usart_Send_Computer(&huart1, message);
        }
    }
}
