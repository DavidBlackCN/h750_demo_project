#include "ADC_API.h"

#include "ADC_BLL.h"
#include "ADC_FML.h"
#include "FFT_BLL.h"
#include "FFT_FML.h"
#include "USART_BLL.h"
#include "USART_FML.h"
#include "usart.h"
#include <stdio.h>

static float fft_centered_data[FFT_LENGTH];

static void adc_make_centered_fft_input(void)
{
    float mean = 0.0f;
    uint32_t len = ADC1_FREQ_BLOCK_LENGTH;

    if (len > FFT_LENGTH)
    {
        len = FFT_LENGTH;
    }

    for (uint32_t i = 0; i < len; i++)
    {
        mean += adc1_data[i];
    }
    mean /= (float)len;

    for (uint32_t i = 0; i < len; i++)
    {
        fft_centered_data[i] = adc1_data[i] - mean;
    }

    for (uint32_t i = len; i < FFT_LENGTH; i++)
    {
        fft_centered_data[i] = 0.0f;
    }
}

static void adc_print_fft_summary(void)
{
    char msg[160];

    snprintf(msg, sizeof(msg),
             "FFT peak=%lu bin=%.3f freq=%.2fHz amp=%.5f vpp=%.5f phase=%.5frad\r\n",
             (unsigned long)fft_result.peak_index,
             fft_result.peak_bin,
             fft_result.freq_hz,
             fft_result.peak_value,
             fft_result.vpp,
             fft_result.phase_rad);
    Usart_Send_Computer(&huart1, msg);

    snprintf(msg, sizeof(msg),
             "FFT second=%lu bin=%.3f freq=%.2fHz amp=%.5f\r\n",
             (unsigned long)fft_result.second_peak_index,
             fft_result.second_peak_bin,
             fft_result.second_freq_hz,
             fft_result.second_peak_value);
    Usart_Send_Computer(&huart1, msg);
}

void adc_proc(void)
{
    adc1_deal();

    if (adc1_proc_flag)
    {
        adc1_proc_flag = 0;
        Usart_Send_Computer(&huart1, "raw begin\r\n");
        (void)Usart_Send_ADC_Data(adc1_data, &huart1, ADC1_FREQ_BLOCK_LENGTH);
        Usart_Send_Computer(&huart1, "raw end\r\n");

        generate_hanning_window();
        adc_make_centered_fft_input();
        calculate_fft(fft_centered_data, ADC1_FREQ_BLOCK_LENGTH);
        FFT_BLL_UpdateResult();

        Usart_Send_Computer(&huart1, "spectrum begin\r\n");
        (void)Usart_Send_ADC_Data(fft_out, &huart1, FFT_LENGTH / 2U);
        Usart_Send_Computer(&huart1, "spectrum end\r\n");
        adc_print_fft_summary();
    }
}
