#include "ADC_FML.h"

#include "USART_FML.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include <math.h>
#include <stdio.h>

uint32_t adc1_dma_buffer[ADC1_DMA_BUFFER_LENGTH] __attribute__((section(".dma_buffer"), aligned(32)));
float adc1_data[ADC1_FREQ_BLOCK_LENGTH];
float adc2_data[ADC1_FREQ_BLOCK_LENGTH];

volatile uint8_t adc1_half_flag = 0;
volatile uint8_t adc1_deal_flag = 0;
volatile uint8_t adc1_proc_flag = 0;
volatile uint32_t adc1_capture_length = ADC1_FREQ_BLOCK_LENGTH;

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
        /* Freeze the trigger immediately; the normal-mode DMA frame is full. */
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

static uint32_t adc_dual_timer_clock_hz(void)
{
    RCC_ClkInitTypeDef clocks = {0};
    uint32_t flash_latency;
    uint32_t timer_clock_hz = HAL_RCC_GetPCLK2Freq();

    HAL_RCC_GetClockConfig(&clocks, &flash_latency);
    if (clocks.APB2CLKDivider != RCC_APB2_DIV1)
    {
        timer_clock_hz *= 2U;
    }
    return timer_clock_hz;
}

static void adc_dual_apply_timer_divider(uint32_t divider)
{
    (void)HAL_TIM_Base_Stop(&htim1);
    __HAL_TIM_SET_PRESCALER(&htim1, 0U);
    __HAL_TIM_SET_AUTORELOAD(&htim1, divider - 1U);
    __HAL_TIM_SET_COUNTER(&htim1, 0U);
    htim1.Instance->EGR = TIM_EGR_UG;
}

void ADC_Dual_SetDiscoverySampling(void)
{
    uint32_t timer_clock_hz = adc_dual_timer_clock_hz();
    uint32_t divider = (timer_clock_hz + 500000U) / 1000000U;

    if (divider == 0U)
    {
        divider = 1U;
    }
    adc_dual_apply_timer_divider(divider);
}

HAL_StatusTypeDef ADC_Dual_ConfigureCoherentSampling(float frequency_hz,
                                                     adc_coherent_config_t *config)
{
    const uint32_t minimum_samples = 3072U;
    uint32_t timer_clock_hz = adc_dual_timer_clock_hz();
    const uint32_t minimum_rate_hz = 1000000U;
    const uint32_t maximum_rate_hz = 1100000U;
    uint32_t minimum_divider;
    uint32_t maximum_divider;
    float best_error = 1.0e30f;
    float best_rate_distance = 1.0e30f;
    adc_coherent_config_t best = {0};

    if ((config == NULL) || (frequency_hz <= 0.0f) || (timer_clock_hz == 0U))
    {
        return HAL_ERROR;
    }

    /* Keep coherent capture close to 1 MS/s.  The narrow upper margin avoids
       the unstable frames observed when this range extended to 1.5 MS/s. */
    minimum_divider = (timer_clock_hz + maximum_rate_hz - 1U) /
                      maximum_rate_hz;
    maximum_divider = timer_clock_hz / minimum_rate_hz;
    if (minimum_divider == 0U)
    {
        minimum_divider = 1U;
    }
    if (maximum_divider < minimum_divider)
    {
        return HAL_ERROR;
    }

    for (uint32_t divider = minimum_divider; divider <= maximum_divider; divider++)
    {
        float sample_rate_hz = (float)timer_clock_hz / (float)divider;
        float rate_distance = fabsf(sample_rate_hz - 1000000.0f);

        for (uint32_t length = minimum_samples;
             length <= ADC1_FREQ_BLOCK_LENGTH;
             length++)
        {
            float exact_cycles = frequency_hz * (float)length / sample_rate_hz;
            uint32_t cycles = (uint32_t)(exact_cycles + 0.5f);
            float error;

            if (cycles < 3U)
            {
                continue;
            }

            error = fabsf(exact_cycles - (float)cycles);
            if ((error < best_error) ||
                ((fabsf(error - best_error) < 1.0e-9f) &&
                 ((length > best.sample_count) ||
                  ((length == best.sample_count) &&
                   (rate_distance < best_rate_distance)))))
            {
                best_error = error;
                best_rate_distance = rate_distance;
                best.timer_divider = divider;
                best.sample_count = length;
                best.coherent_cycles = cycles;
                best.sample_rate_hz = sample_rate_hz;
                best.closure_error_deg = (exact_cycles - (float)cycles) * 360.0f;
            }
        }
    }

    if (best.sample_count == 0U)
    {
        return HAL_ERROR;
    }

    adc_dual_apply_timer_divider(best.timer_divider);
    *config = best;
    return HAL_OK;
}

HAL_StatusTypeDef ADC_Dual_StartCaptureLength(uint32_t sample_count)
{
    HAL_StatusTypeDef status;
    static uint8_t calibrated = 0U;

    (void)HAL_TIM_Base_Stop(&htim1);

    if (!calibrated)
    {
        ADC_MultiModeTypeDef multimode = {0};

        /* Calibrate both ADCs while independent, then make ADC2 the slave. */
        multimode.Mode = ADC_MODE_INDEPENDENT;
        status = HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode);
        if (status != HAL_OK)
        {
            return status;
        }

        status = HAL_ADCEx_Calibration_Start(&hadc1,
                                             ADC_CALIB_OFFSET_LINEARITY,
                                             ADC_SINGLE_ENDED);
        if (status != HAL_OK)
        {
            return status;
        }

        status = HAL_ADCEx_Calibration_Start(&hadc2,
                                             ADC_CALIB_OFFSET_LINEARITY,
                                             ADC_SINGLE_ENDED);
        if (status != HAL_OK)
        {
            return status;
        }

        multimode.Mode = ADC_DUALMODE_REGSIMULT;
        multimode.DualModeData = ADC_DUALMODEDATAFORMAT_32_10_BITS;
        multimode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_1CYCLE;
        status = HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode);
        if (status != HAL_OK)
        {
            return status;
        }
        calibrated = 1U;
    }

    if ((sample_count == 0U) || (sample_count > ADC1_DMA_BUFFER_LENGTH))
    {
        return HAL_ERROR;
    }

    adc1_half_flag = 0U;
    adc1_deal_flag = 0U;
    adc1_capture_length = sample_count;
    __HAL_TIM_SET_COUNTER(&htim1, 0U);

    SCB_CleanInvalidateDCache_by_Addr(adc1_dma_buffer,
                                      (int32_t)sizeof(adc1_dma_buffer));

    status = HAL_ADCEx_MultiModeStart_DMA(&hadc1,
                                          adc1_dma_buffer,
                                          sample_count);
    if (status != HAL_OK)
    {
        return status;
    }

    /* This one-shot frame is consumed by polling NDTR in PHASE_API.  The
       polling path is deterministic on this project, while the DMA transfer
       complete callback is not delivered by the current interrupt setup. */
    __HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_TC | DMA_IT_HT);

    status = HAL_TIM_Base_Start(&htim1);
    if (status != HAL_OK)
    {
        (void)HAL_ADCEx_MultiModeStop_DMA(&hadc1);
        return status;
    }

    return HAL_OK;
}

HAL_StatusTypeDef ADC_Dual_StartCapture(void)
{
    return ADC_Dual_StartCaptureLength(ADC1_FREQ_BLOCK_LENGTH);
}

HAL_StatusTypeDef MY_ADC1_Init(void)
{
    return ADC_Dual_StartCapture();
}
