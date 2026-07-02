#include "FFT_API.h"

#include "ADC_FML.h"
#include "FFT_BLL.h"
#include "FFT_FML.h"
#include "USART_BLL.h"
#include "USART_FML.h"
#include "usart.h"
#include <stdio.h>

volatile uint8_t fft_proc_flag = 0;

static void fft_print_summary(void)
{
    char msg[160];

    snprintf(msg, sizeof(msg),
             "FFT peak=%lu freq=%.2fHz amp=%.5f vpp=%.5f phase=%.5frad\r\n",
             (unsigned long)fft_result.peak_index,
             fft_result.freq_hz,
             fft_result.peak_value,
             fft_result.vpp,
             fft_result.phase_rad);
    Usart_Send_Computer(&huart1, msg);

    snprintf(msg, sizeof(msg),
             "FFT second=%lu freq=%.2fHz amp=%.5f\r\n",
             (unsigned long)fft_result.second_peak_index,
             fft_result.second_freq_hz,
             fft_result.second_peak_value);
    Usart_Send_Computer(&huart1, msg);
}

static HAL_StatusTypeDef fft_send_array_with_progress(const char *name, float *data, uint32_t len)
{
    char msg[48];
    HAL_StatusTypeDef status;

    snprintf(msg, sizeof(msg), "%s begin len=%lu\r\n", name, (unsigned long)len);
    if (Usart_Send_Computer(&huart1, msg) != HAL_OK)
    {
        return usart_last_status;
    }

    for (uint32_t offset = 0; offset < len; offset += 128U)
    {
        uint32_t chunk_len = len - offset;
        if (chunk_len > 128U)
        {
            chunk_len = 128U;
        }

        snprintf(msg, sizeof(msg), "%s offset=%lu\r\n", name, (unsigned long)offset);
        if (Usart_Send_Computer(&huart1, msg) != HAL_OK)
        {
            return usart_last_status;
        }

        status = Usart_Send_ADC_Data(&data[offset], &huart1, chunk_len);
        if (status != HAL_OK)
        {
            snprintf(msg, sizeof(msg), "%s tx fail status=%lu\r\n", name, (unsigned long)status);
            (void)Usart_Send_Computer(&huart1, msg);
            return status;
        }
    }

    snprintf(msg, sizeof(msg), "%s end\r\n", name);
    return Usart_Send_Computer(&huart1, msg);
}

void fft_proc(void)
{
    if (!fft_proc_flag)
    {
        return;
    }

    fft_proc_flag = 0;

    FFT_BLL_UpdateResult();
    if (fft_send_array_with_progress("raw", adc1_data, ADC1_FREQ_BLOCK_LENGTH) != HAL_OK)
    {
        return;
    }
    if (fft_send_array_with_progress("spectrum", fft_out, FFT_LENGTH / 2U) != HAL_OK)
    {
        return;
    }
    fft_print_summary();
}

void FFT_PROC(void)
{
    fft_proc();
}

void FFT_API(void)
{
    fft_proc();
}
