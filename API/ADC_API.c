#include "ADC_API.h"

#include "ADC_BLL.h"
#include "ADC_FML.h"
#include "FFT_BLL.h"
#include "FFT_FML.h"
#include "USART_FML.h"
#include "usart.h"
#include <stdio.h>

#define ADC_API_CAPTURE_TIMEOUT_MS 100U

static float fft_centered_data[FFT_LENGTH];
static uint32_t adc_capture_start_tick = 0U;
static uint8_t adc_capture_active = 0U;

static HAL_StatusTypeDef adc_send_text(char *text)
{
    return Usart_Send_Computer(&huart1, text);
}

static void adc_make_centered_fft_input(void)
{
    float mean = 0.0f;

    for (uint32_t i = 0; i < FFT_LENGTH; i++)
    {
        mean += adc1_data[i];
    }
    mean /= (float)FFT_LENGTH;

    for (uint32_t i = 0; i < FFT_LENGTH; i++)
    {
        fft_centered_data[i] = adc1_data[i] - mean;
    }
}

static HAL_StatusTypeDef adc_print_raw(void)
{
    char msg[48];

    if (adc_send_text("raw begin\r\n") != HAL_OK)
    {
        return usart_last_status;
    }

    for (uint32_t i = 0; i < ADC1_FREQ_BLOCK_LENGTH; i++)
    {
        snprintf(msg, sizeof(msg), "%lu,%.6f\r\n", (unsigned long)i, adc1_data[i]);
        if (adc_send_text(msg) != HAL_OK)
        {
            return usart_last_status;
        }
    }

    return adc_send_text("raw end\r\n");
}

static HAL_StatusTypeDef adc_print_spectrum(void)
{
    char msg[48];

    if (adc_send_text("spectrum begin\r\n") != HAL_OK)
    {
        return usart_last_status;
    }

    for (uint32_t i = 0; i < (FFT_LENGTH / 2U); i++)
    {
        snprintf(msg, sizeof(msg), "%lu,%.6f\r\n",
                 (unsigned long)i, fft_out[i]);
        if (adc_send_text(msg) != HAL_OK)
        {
            return usart_last_status;
        }
    }

    return adc_send_text("spectrum end\r\n");
}

static HAL_StatusTypeDef adc_print_result(float sample_rate_hz)
{
    char msg[224];

    if (fft_result.signal_valid == 0U)
    {
        snprintf(msg, sizeof(msg),
                 "result status=no_signal freq=0.00Hz thd=invalid wave=none fs=%.2fHz\r\n",
                 sample_rate_hz);
    }
    else if (fft_result.thd_valid == 0U)
    {
        snprintf(msg, sizeof(msg),
                 "result status=limited freq=%.2fHz thd=invalid wave=%s fs=%.2fHz\r\n",
                 fft_result.freq_hz, FFT_BLL_GetWaveformName(), sample_rate_hz);
    }
    else
    {
        snprintf(msg, sizeof(msg),
                 "result status=valid freq=%.2fHz thd=%.3f%% wave=%s h2=%.4f h3=%.4f h5=%.4f fs=%.2fHz\r\n",
                 fft_result.freq_hz,
                 fft_result.thd_percent,
                 FFT_BLL_GetWaveformName(),
                 fft_result.harmonic_2_ratio,
                 fft_result.harmonic_3_ratio,
                 fft_result.harmonic_5_ratio,
                 sample_rate_hz);
    }

    return adc_send_text(msg);
}

static void adc_process_frame(void)
{
    char msg[80];
    float sample_rate_hz = MY_ADC1_GetSampleRateHz();

    adc_make_centered_fft_input();
    calculate_fft(fft_centered_data, FFT_LENGTH);
    FFT_BLL_SetSampleRateHz(sample_rate_hz);
    FFT_BLL_UpdateResultFromSamples(fft_centered_data, FFT_LENGTH);

    snprintf(msg, sizeof(msg), "frame begin fs=%.2f n=%u\r\n",
             sample_rate_hz, (unsigned int)FFT_LENGTH);
    if (adc_send_text(msg) == HAL_OK &&
        adc_print_raw() == HAL_OK &&
        adc_print_spectrum() == HAL_OK)
    {
        (void)adc_print_result(sample_rate_hz);
        (void)adc_send_text("frame end\r\n");
    }
}

HAL_StatusTypeDef ADC_API_Init(void)
{
    char msg[64];
    float sample_rate_hz;

    generate_hanning_window();
    (void)adc_send_text("adc init begin\r\n");

    if (MY_ADC1_Init() != HAL_OK)
    {
        (void)adc_send_text("adc init failed\r\n");
        return HAL_ERROR;
    }

    sample_rate_hz = MY_ADC1_GetSampleRateHz();
    FFT_BLL_SetSampleRateHz(sample_rate_hz);
    adc_capture_active = 1U;
    adc_capture_start_tick = HAL_GetTick();
    snprintf(msg, sizeof(msg), "adc init ok fs=%.2fHz, capture started\r\n", sample_rate_hz);
    (void)adc_send_text(msg);
    return HAL_OK;
}

void ADC_API_Process(void)
{
    adc1_deal();

    if (adc1_proc_flag != 0U)
    {
        adc1_proc_flag = 0U;
        adc_capture_active = 0U;
        adc_process_frame();
    }

    if (adc_capture_active != 0U &&
        (HAL_GetTick() - adc_capture_start_tick) > ADC_API_CAPTURE_TIMEOUT_MS)
    {
        MY_ADC1_StopCapture();
        adc_capture_active = 0U;
        (void)adc_send_text("adc capture timeout\r\n");
    }
}

void adc_proc(void)
{
    ADC_API_Process();
}
