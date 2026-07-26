#include "ADC2_FFT_API.h"

#include "ADC2_CAPTURE_API.h"
#include "ADC2_FFT_BLL.h"
#include "ADC2_FFT_FML.h"
#include "tim.h"
#include "usart.h"

#include <stdio.h>

#define ADC2_FFT_COARSE_SAMPLE_RATE_HZ  409600.0f
#define ADC2_FFT_ADC2_VOLTS_PER_LSB      (3.3f / 1023.0f)

typedef enum
{
    ADC2_FFT_API_IDLE = 0U,
    ADC2_FFT_API_WAIT_COARSE,
    ADC2_FFT_API_WAIT_FINAL,
    ADC2_FFT_API_DONE,
    ADC2_FFT_API_ERROR
} adc2_fft_api_state_t;

static adc2_fft_api_state_t s_state = ADC2_FFT_API_IDLE;
static adc2_fft_capture_request_t s_coherent_request;
static adc2_fft_result_t s_final_result;
static adc2_fft_input_config_t s_adc2_input_config =
{
    .sample_rate_hz = ADC2_FFT_COARSE_SAMPLE_RATE_HZ,
    .volts_per_lsb = ADC2_FFT_ADC2_VOLTS_PER_LSB,
    .search_min_frequency_hz = 10000.0f,
    .search_max_frequency_hz = 100000.0f,
    .sample_count = ADC2_FFT_LENGTH,
    .sample_bit_width = 10U,
    .sample_encoding = ADC2_FFT_SAMPLE_UNSIGNED
};

static uint32_t adc2_fft_get_tim1_clock_hz(void)
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

static HAL_StatusTypeDef adc2_fft_send_final_report(void)
{
    char line[160];
    const uint16_t *raw = ADC2_CAPTURE_API_GetRawFrame();
    const float *half_magnitude = ADC2_FFT_BLL_GetHalfMagnitude();
    uint32_t sample_count = ADC2_CAPTURE_API_GetSampleCount();
    uint32_t half_magnitude_count = ADC2_FFT_BLL_GetHalfMagnitudeCount();
    int length;

    length = snprintf(line, sizeof(line), "raw begin n=%lu fs=%.3f\r\n",
                      (unsigned long)sample_count,
                      (double)ADC2_CAPTURE_API_GetSampleRateHz());
    if ((length <= 0) ||
        (HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 20U) != HAL_OK))
    {
        return HAL_ERROR;
    }

    for (uint32_t i = 0U; i < sample_count; ++i)
    {
        length = snprintf(line, sizeof(line), "raw:%u\r\n",
                          (unsigned int)(raw[i] & 0x03FFU));
        if ((length <= 0) ||
            (HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 20U) != HAL_OK))
        {
            return HAL_ERROR;
        }
    }

    length = snprintf(line, sizeof(line), "spectrum begin n=%lu df=%.6f\r\n",
                      (unsigned long)half_magnitude_count,
                      (double)s_final_result.frequency_resolution_hz);
    if ((length <= 0) ||
        (HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 20U) != HAL_OK))
    {
        return HAL_ERROR;
    }

    for (uint32_t bin = 0U; bin < half_magnitude_count; ++bin)
    {
        length = snprintf(line, sizeof(line), "fft:%lu,%.6f\r\n",
                          (unsigned long)bin,
                          (double)half_magnitude[bin]);
        if ((length <= 0) ||
            (HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 20U) != HAL_OK))
        {
            return HAL_ERROR;
        }
    }

    length = snprintf(line, sizeof(line),
                      "raw end\r\nspectrum end\r\nresult freq=%.3fHz bin=%.3f fs=%.3fHz df=%.6fHz closure=%.6f\r\n",
                      (double)s_final_result.frequency_hz,
                      (double)s_final_result.peak_bin,
                      (double)s_final_result.sample_rate_hz,
                      (double)s_final_result.frequency_resolution_hz,
                      (double)s_coherent_request.closure_error_cycles);
    if ((length <= 0) ||
        (HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 20U) != HAL_OK))
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef ADC2_FFT_API_Start(void)
{
    if ((s_state != ADC2_FFT_API_IDLE) && (s_state != ADC2_FFT_API_DONE) &&
        (s_state != ADC2_FFT_API_ERROR))
    {
        return HAL_BUSY;
    }

    if (ADC2_CAPTURE_API_Start(ADC2_FFT_COARSE_SAMPLE_RATE_HZ,
                               ADC2_FFT_LENGTH) != HAL_OK)
    {
        s_state = ADC2_FFT_API_ERROR;
        return HAL_ERROR;
    }

    s_state = ADC2_FFT_API_WAIT_COARSE;
    return HAL_OK;
}

void ADC2_FFT_API_Process(void)
{
    adc2_fft_result_t coarse_result;

    if (!ADC2_CAPTURE_API_HasFrame())
    {
        return;
    }

    if (s_state == ADC2_FFT_API_WAIT_COARSE)
    {
        s_adc2_input_config.sample_count = ADC2_CAPTURE_API_GetSampleCount();
        s_adc2_input_config.sample_rate_hz = ADC2_CAPTURE_API_GetSampleRateHz();
        if ((ADC2_FFT_BLL_Analyze(ADC2_CAPTURE_API_GetRawFrame(),
                                  &s_adc2_input_config,
                                  &coarse_result) != HAL_OK) ||
            (ADC2_FFT_BLL_ChooseCoherentCapture(&coarse_result,
                                                 adc2_fft_get_tim1_clock_hz(),
                                                 &s_coherent_request) != HAL_OK))
        {
            ADC2_CAPTURE_API_ReleaseFrame();
            s_state = ADC2_FFT_API_ERROR;
            return;
        }

        ADC2_CAPTURE_API_ReleaseFrame();
        if (ADC2_CAPTURE_API_Start(s_coherent_request.requested_sample_rate_hz,
                                   s_coherent_request.sample_count) != HAL_OK)
        {
            s_state = ADC2_FFT_API_ERROR;
            return;
        }

        s_state = ADC2_FFT_API_WAIT_FINAL;
        return;
    }

    if (s_state == ADC2_FFT_API_WAIT_FINAL)
    {
        s_adc2_input_config.sample_count = ADC2_CAPTURE_API_GetSampleCount();
        s_adc2_input_config.sample_rate_hz = ADC2_CAPTURE_API_GetSampleRateHz();
        if (ADC2_FFT_BLL_Analyze(ADC2_CAPTURE_API_GetRawFrame(),
                                 &s_adc2_input_config,
                                 &s_final_result) != HAL_OK)
        {
            ADC2_CAPTURE_API_ReleaseFrame();
            s_state = ADC2_FFT_API_ERROR;
            return;
        }

        if (adc2_fft_send_final_report() != HAL_OK)
        {
            ADC2_CAPTURE_API_ReleaseFrame();
            s_state = ADC2_FFT_API_ERROR;
            return;
        }

        ADC2_CAPTURE_API_ReleaseFrame();
        s_state = ADC2_FFT_API_DONE;
    }
}
