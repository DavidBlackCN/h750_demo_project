#include "ADC2_FFT_API.h"

#include "ADC2_CAPTURE_API.h"
#include "ADC2_FFT_BLL.h"
#include "ADC2_FFT_FML.h"
#include "tim.h"
#include "usart.h"

#include <stdio.h>

#define ADC2_FFT_COARSE_SAMPLE_RATE_HZ  409600.0f
#define ADC2_FFT_ADC2_VOLTS_PER_LSB      (3.3f / 1023.0f)
#define ADC2_FFT_CAPTURE_TIMEOUT_MS      100U

typedef enum
{
    ADC2_FFT_API_IDLE = 0U,
    ADC2_FFT_API_WAIT_COARSE,
    ADC2_FFT_API_WAIT_FINAL,
    ADC2_FFT_API_WAIT_CLASSIFY,
    ADC2_FFT_API_DONE,
    ADC2_FFT_API_ERROR
} adc2_fft_api_state_t;

static adc2_fft_api_state_t s_state = ADC2_FFT_API_IDLE;
static adc2_fft_capture_request_t s_coherent_request;
static adc2_fft_waveform_capture_request_t s_classification_request;
static adc2_fft_result_t s_final_result;
static adc2_fft_result_t s_classification_spectrum_result;
static uint32_t s_capture_started_ms;
static adc2_fft_input_config_t s_adc2_input_config =
{
    .sample_rate_hz = ADC2_FFT_COARSE_SAMPLE_RATE_HZ,
    .volts_per_lsb = ADC2_FFT_ADC2_VOLTS_PER_LSB,
    .search_min_frequency_hz = 1000.0f,
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

static void adc2_fft_send_error(const char *stage)
{
    char line[96];
    int length = snprintf(line, sizeof(line),
                          "adc2fft error stage=%s adc=0x%08lx\r\n",
                          stage,
                          (unsigned long)ADC2_CAPTURE_API_GetErrorCode());

    if ((length > 0) && (length < (int)sizeof(line)))
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)line,
                                (uint16_t)length, 20U);
    }
}

static void adc2_fft_send_stage(const char *stage)
{
    char line[48];
    int length = snprintf(line, sizeof(line), "adc2fft stage=%s\r\n", stage);

    if ((length > 0) && (length < (int)sizeof(line)))
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)line,
                                (uint16_t)length, 20U);
    }
}

static void adc2_fft_send_classification_report(void)
{
    char line[256];
    int length = snprintf(
        line, sizeof(line),
        "classification wave=%s spec_sine=%.4f spec_triangle=%.4f spec_square=%.4f h3=%.4f h5=%.4f class_fs=%.3fHz class_spp=%lu class_closure=%.6f\r\n",
        ADC2_FFT_BLL_WaveformText(s_final_result.waveform),
        (double)s_final_result.sine_spectral_score,
        (double)s_final_result.triangle_spectral_score,
        (double)s_final_result.square_spectral_score,
        (double)s_final_result.harmonic3_ratio,
        (double)s_final_result.harmonic5_ratio,
        (double)s_classification_spectrum_result.sample_rate_hz,
        (unsigned long)s_classification_request.samples_per_period,
        (double)s_classification_request.closure_error_cycles);

    if ((length > 0) && (length < (int)sizeof(line)))
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)line,
                                (uint16_t)length, 20U);
    }
}

static void adc2_fft_send_classification_unavailable(void)
{
    static const char line[] = "classification wave=unknown status=unavailable\r\n";

    (void)HAL_UART_Transmit(&huart1, (uint8_t *)line,
                            (uint16_t)(sizeof(line) - 1U), 20U);
}

static HAL_StatusTypeDef adc2_fft_start_capture(float sample_rate_hz,
                                                 uint32_t sample_count,
                                                 const char *stage)
{
    HAL_StatusTypeDef status = ADC2_CAPTURE_API_Start(sample_rate_hz,
                                                       sample_count);

    if (status == HAL_OK)
    {
        s_capture_started_ms = HAL_GetTick();
        adc2_fft_send_stage(stage);
    }

    return status;
}

static const char *adc2_fft_current_stage(void)
{
    if (s_state == ADC2_FFT_API_WAIT_COARSE)
    {
        return "coarse";
    }
    if (s_state == ADC2_FFT_API_WAIT_FINAL)
    {
        return "final";
    }
    if (s_state == ADC2_FFT_API_WAIT_CLASSIFY)
    {
        return "classify";
    }

    return "idle";
}

static HAL_StatusTypeDef adc2_fft_send_final_report(void)
{
    char line[320];
    const uint16_t *raw = ADC2_CAPTURE_API_GetRawFrame();
    const float *half_magnitude = ADC2_FFT_BLL_GetHalfMagnitude();
    uint32_t sample_count = ADC2_CAPTURE_API_GetSampleCount();
    uint32_t half_magnitude_count = ADC2_FFT_BLL_GetHalfMagnitudeCount();
    float spectrum_resolution_hz = ADC2_CAPTURE_API_GetSampleRateHz() /
                                   (float)sample_count;
    int length;

    length = snprintf(line, sizeof(line), "raw begin n=%lu fs=%.3f\r\n",
                      (unsigned long)sample_count,
                      (double)ADC2_CAPTURE_API_GetSampleRateHz());
    if ((length <= 0) || (length >= (int)sizeof(line)) ||
        (HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 20U) != HAL_OK))
    {
        return HAL_ERROR;
    }

    for (uint32_t i = 0U; i < sample_count; ++i)
    {
        length = snprintf(line, sizeof(line), "raw:%u\r\n",
                          (unsigned int)(raw[i] & 0x03FFU));
        if ((length <= 0) || (length >= (int)sizeof(line)) ||
            (HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 20U) != HAL_OK))
        {
            return HAL_ERROR;
        }
    }

    length = snprintf(line, sizeof(line), "spectrum begin n=%lu df=%.6f\r\n",
                      (unsigned long)half_magnitude_count,
                      (double)spectrum_resolution_hz);
    if ((length <= 0) || (length >= (int)sizeof(line)) ||
        (HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 20U) != HAL_OK))
    {
        return HAL_ERROR;
    }

    for (uint32_t bin = 0U; bin < half_magnitude_count; ++bin)
    {
        length = snprintf(line, sizeof(line), "fft:%lu,%.6f\r\n",
                          (unsigned long)bin,
                          (double)half_magnitude[bin]);
        if ((length <= 0) || (length >= (int)sizeof(line)) ||
            (HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 20U) != HAL_OK))
        {
            return HAL_ERROR;
        }
    }

    length = snprintf(line, sizeof(line),
                      "raw end\r\nspectrum end\r\nresult freq=%.3fHz bin=%.3f fs=%.3fHz df=%.6fHz closure=%.6f classify=pending\r\n",
                      (double)s_final_result.frequency_hz,
                      (double)s_final_result.peak_bin,
                      (double)s_final_result.sample_rate_hz,
                      (double)s_final_result.frequency_resolution_hz,
                      (double)s_coherent_request.closure_error_cycles);
    if ((length <= 0) || (length >= (int)sizeof(line)) ||
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

    if (adc2_fft_start_capture(ADC2_FFT_COARSE_SAMPLE_RATE_HZ,
                                ADC2_FFT_LENGTH, "coarse") != HAL_OK)
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

    if (ADC2_CAPTURE_API_HasError())
    {
        adc2_fft_send_error(adc2_fft_current_stage());
        if (s_state == ADC2_FFT_API_WAIT_CLASSIFY)
        {
            adc2_fft_send_classification_unavailable();
            s_state = ADC2_FFT_API_DONE;
        }
        else
        {
            s_state = ADC2_FFT_API_ERROR;
        }
        return;
    }

    if (!ADC2_CAPTURE_API_HasFrame())
    {
        if (((s_state == ADC2_FFT_API_WAIT_COARSE) ||
             (s_state == ADC2_FFT_API_WAIT_FINAL) ||
             (s_state == ADC2_FFT_API_WAIT_CLASSIFY)) &&
            ((HAL_GetTick() - s_capture_started_ms) >=
             ADC2_FFT_CAPTURE_TIMEOUT_MS))
        {
            adc2_fft_send_error(adc2_fft_current_stage());
            if (s_state == ADC2_FFT_API_WAIT_CLASSIFY)
            {
                adc2_fft_send_classification_unavailable();
                s_state = ADC2_FFT_API_DONE;
            }
            else
            {
                s_state = ADC2_FFT_API_ERROR;
            }
        }
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
        if (adc2_fft_start_capture(s_coherent_request.requested_sample_rate_hz,
                                   s_coherent_request.sample_count,
                                   "final") != HAL_OK)
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

        /* Publish measurement data before optional high-rate classification.
           A classifier failure must never suppress a valid frequency result. */
        if (adc2_fft_send_final_report() != HAL_OK)
        {
            ADC2_CAPTURE_API_ReleaseFrame();
            s_state = ADC2_FFT_API_ERROR;
            return;
        }

        if (ADC2_FFT_BLL_ChooseWaveformCapture(s_final_result.frequency_hz,
                                               adc2_fft_get_tim1_clock_hz(),
                                               &s_classification_request) != HAL_OK)
        {
            ADC2_CAPTURE_API_ReleaseFrame();
            adc2_fft_send_classification_unavailable();
            s_state = ADC2_FFT_API_DONE;
            return;
        }

        ADC2_CAPTURE_API_ReleaseFrame();
        if (adc2_fft_start_capture(
                s_classification_request.requested_sample_rate_hz,
                s_classification_request.sample_count,
                "classify") != HAL_OK)
        {
            adc2_fft_send_classification_unavailable();
            s_state = ADC2_FFT_API_DONE;
            return;
        }

        s_state = ADC2_FFT_API_WAIT_CLASSIFY;
        return;
    }

    if (s_state == ADC2_FFT_API_WAIT_CLASSIFY)
    {
        s_adc2_input_config.sample_count = ADC2_CAPTURE_API_GetSampleCount();
        s_adc2_input_config.sample_rate_hz = ADC2_CAPTURE_API_GetSampleRateHz();
        if ((ADC2_FFT_BLL_Analyze(ADC2_CAPTURE_API_GetRawFrame(),
                                  &s_adc2_input_config,
                                  &s_classification_spectrum_result) != HAL_OK) ||
            (ADC2_FFT_BLL_ClassifySpectrum(&s_adc2_input_config,
                                            s_final_result.frequency_hz,
                                            &s_final_result) != HAL_OK))
        {
            adc2_fft_send_classification_unavailable();
        }
        else
        {
            adc2_fft_send_classification_report();
        }

        ADC2_CAPTURE_API_ReleaseFrame();
        s_state = ADC2_FFT_API_DONE;
    }
}
