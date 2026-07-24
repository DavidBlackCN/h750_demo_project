#include "DLIA_API.h"

#include "ADC_FML.h"
#include "DLIA_BLL.h"
#include "USART_FML.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"

#include <stdio.h>

#define DLIA_API_SAMPLE_RATE_HZ       1000000.0f
#define DLIA_API_ADC_FULL_SCALE_VOLTAGE 3.3f
#define DLIA_API_ADC_CODE_RANGE       1024.0f
#define DLIA_API_AMPLITUDE_LPF_BETA   0.125f
#define DLIA_API_PHASE_DIFFERENCE_LPF_BETA 0.0625f
#define DLIA_API_MIN_PEAK_VOLTAGE     0.020f
#define DLIA_API_REPORT_PERIOD_MS     100U
#define DLIA_API_CAPTURE_TIMEOUT_MS   20U
#define DLIA_API_ENABLE_PERIODIC_REPORT 0U

static dlia_bll_state_t s_dlia_state;
static dlia_bll_result_t s_dlia_result;
static uint16_t s_channel1_codes[DLIA_BLL_BLOCK_SIZE];
static uint16_t s_channel2_codes[DLIA_BLL_BLOCK_SIZE];
static uint16_t s_channel1_min_code;
static uint16_t s_channel1_max_code;
static uint16_t s_channel2_min_code;
static uint16_t s_channel2_max_code;
static uint8_t s_capture_active = 0U;
static uint8_t s_phase_ready = 0U;
static uint32_t s_capture_start_tick = 0U;
static uint32_t s_last_report_tick = 0U;

static HAL_StatusTypeDef dlia_start_capture(void)
{
    HAL_StatusTypeDef status;

    ADC_Dual_SetDiscoverySampling();
    status = ADC_Dual_StartCaptureLength(DLIA_BLL_BLOCK_SIZE);
    if (status == HAL_OK)
    {
        s_capture_start_tick = HAL_GetTick();
        s_capture_active = 1U;
    }
    return status;
}

static void dlia_stop_capture(void)
{
    (void)HAL_TIM_Base_Stop(&htim1);
    (void)HAL_ADCEx_MultiModeStop_DMA(&hadc1);
    s_capture_active = 0U;
}

static void dlia_unpack_capture(void)
{
    uint32_t index;

    SCB_InvalidateDCache_by_Addr(adc1_dma_buffer,
                                 (int32_t)sizeof(adc1_dma_buffer));
    s_channel1_min_code = UINT16_MAX;
    s_channel1_max_code = 0U;
    s_channel2_min_code = UINT16_MAX;
    s_channel2_max_code = 0U;
    for (index = 0U; index < DLIA_BLL_BLOCK_SIZE; index++)
    {
        uint32_t packed_codes = adc1_dma_buffer[index];

        s_channel1_codes[index] = (uint16_t)(packed_codes & 0x03FFU);
        s_channel2_codes[index] = (uint16_t)((packed_codes >> 16) & 0x03FFU);
        if (s_channel1_codes[index] < s_channel1_min_code)
        {
            s_channel1_min_code = s_channel1_codes[index];
        }
        if (s_channel1_codes[index] > s_channel1_max_code)
        {
            s_channel1_max_code = s_channel1_codes[index];
        }
        if (s_channel2_codes[index] < s_channel2_min_code)
        {
            s_channel2_min_code = s_channel2_codes[index];
        }
        if (s_channel2_codes[index] > s_channel2_max_code)
        {
            s_channel2_max_code = s_channel2_codes[index];
        }
    }
}

static void dlia_report_result(void)
{
    char message[192];
    uint32_t now = HAL_GetTick();

    if ((now - s_last_report_tick) < DLIA_API_REPORT_PERIOD_MS)
    {
        return;
    }
    s_last_report_tick = now;

    if ((s_dlia_result.channel1.amplitude_peak_voltage < DLIA_API_MIN_PEAK_VOLTAGE) ||
        (s_dlia_result.channel2.amplitude_peak_voltage < DLIA_API_MIN_PEAK_VOLTAGE))
    {
        snprintf(message, sizeof(message),
                 "phase=invalid raw=%.3fdeg adc1=[%u,%u] amp=%.3fV adc2=[%u,%u] amp=%.3fV\r\n",
                 (double)s_dlia_result.raw_phase_difference_deg,
                 (unsigned int)s_channel1_min_code,
                 (unsigned int)s_channel1_max_code,
                 (double)s_dlia_result.channel1.amplitude_peak_voltage,
                 (unsigned int)s_channel2_min_code,
                 (unsigned int)s_channel2_max_code,
                 (double)s_dlia_result.channel2.amplitude_peak_voltage);
        (void)Usart_Send_Computer(&huart1, message);
        return;
    }

    snprintf(message, sizeof(message),
             "phase=%.3fdeg raw=%.3fdeg adc1=[%u,%u] amp=%.3fV adc2=[%u,%u] amp=%.3fV\r\n",
             (double)s_dlia_result.phase_difference_deg,
             (double)s_dlia_result.raw_phase_difference_deg,
             (unsigned int)s_channel1_min_code,
             (unsigned int)s_channel1_max_code,
             (double)s_dlia_result.channel1.amplitude_peak_voltage,
             (unsigned int)s_channel2_min_code,
             (unsigned int)s_channel2_max_code,
             (double)s_dlia_result.channel2.amplitude_peak_voltage);
    (void)Usart_Send_Computer(&huart1, message);
}

HAL_StatusTypeDef DLIA_API_Init(void)
{
    const dlia_bll_config_t config =
    {
        .reference_frequency_hz = DLIA_API_REFERENCE_FREQUENCY_HZ,
        .sample_rate_hz = DLIA_API_SAMPLE_RATE_HZ,
        .adc_full_scale_voltage = DLIA_API_ADC_FULL_SCALE_VOLTAGE,
        .adc_code_range = DLIA_API_ADC_CODE_RANGE,
        .amplitude_lpf_beta = DLIA_API_AMPLITUDE_LPF_BETA,
        .phase_difference_lpf_beta = DLIA_API_PHASE_DIFFERENCE_LPF_BETA
    };

    if (!DLIA_BLL_Init(&config, &s_dlia_state))
    {
        return HAL_ERROR;
    }

    s_last_report_tick = HAL_GetTick() - DLIA_API_REPORT_PERIOD_MS;
    s_phase_ready = 0U;
    return dlia_start_capture();
}

bool DLIA_API_TakeValidPhase(float *phase_difference_deg)
{
    if ((phase_difference_deg == NULL) || (s_phase_ready == 0U))
    {
        return false;
    }

    *phase_difference_deg = s_dlia_result.phase_difference_deg;
    s_phase_ready = 0U;
    return true;
}

void DLIA_API_Process(void)
{
    const dlia_bll_config_t config =
    {
        .reference_frequency_hz = DLIA_API_REFERENCE_FREQUENCY_HZ,
        .sample_rate_hz = DLIA_API_SAMPLE_RATE_HZ,
        .adc_full_scale_voltage = DLIA_API_ADC_FULL_SCALE_VOLTAGE,
        .adc_code_range = DLIA_API_ADC_CODE_RANGE,
        .amplitude_lpf_beta = DLIA_API_AMPLITUDE_LPF_BETA,
        .phase_difference_lpf_beta = DLIA_API_PHASE_DIFFERENCE_LPF_BETA
    };
    uint32_t ndtr;

    if (s_capture_active == 0U)
    {
        if (dlia_start_capture() != HAL_OK)
        {
            Error_Handler();
        }
        return;
    }

    if (hadc1.DMA_Handle == NULL)
    {
        Error_Handler();
        return;
    }

    ndtr = __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle);
    if (ndtr == 0U)
    {
        dlia_stop_capture();
        dlia_unpack_capture();
        if (DLIA_BLL_ProcessPair(&config, &s_dlia_state,
                                 s_channel1_codes, s_channel2_codes,
                                 DLIA_BLL_BLOCK_SIZE, &s_dlia_result))
        {
            s_phase_ready = ((s_dlia_result.channel1.amplitude_peak_voltage >=
                              DLIA_API_MIN_PEAK_VOLTAGE) &&
                             (s_dlia_result.channel2.amplitude_peak_voltage >=
                              DLIA_API_MIN_PEAK_VOLTAGE)) ? 1U : 0U;
            if (DLIA_API_ENABLE_PERIODIC_REPORT != 0U)
            {
                dlia_report_result();
            }
        }
        return;
    }

    if ((HAL_GetTick() - s_capture_start_tick) >= DLIA_API_CAPTURE_TIMEOUT_MS)
    {
        char message[64];

        snprintf(message, sizeof(message), "dlia timeout ndtr=%lu\r\n",
                 (unsigned long)ndtr);
        dlia_stop_capture();
        (void)Usart_Send_Computer(&huart1, message);
    }
}
