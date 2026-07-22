#include "AD9226_API.h"

#include "AD9226.h"
#include "AD9226_BLL.h"
#include "usart.h"

#include <stdio.h>
#include <string.h>

#define AD9226_UART_TIMEOUT_MS (1000UL)
#define AD9226_REPORT_PERIOD_MS (100UL)

static uint8_t s_tx_buffer[512]
    __attribute__((section(".dma_buffer"), aligned(32)));

static HAL_StatusTypeDef AD9226_API_Send(const char *text)
{
    HAL_StatusTypeDef status;
    uint32_t start_tick;
    size_t length;

    if (text == NULL)
    {
        return HAL_ERROR;
    }

    length = strlen(text);
    if ((length == 0U) || (length >= sizeof(s_tx_buffer)))
    {
        return HAL_ERROR;
    }

    memcpy(s_tx_buffer, text, length);
    SCB_CleanDCache_by_Addr((uint32_t *)s_tx_buffer,
                            (int32_t)sizeof(s_tx_buffer));

    status = HAL_UART_Transmit_DMA(&huart3, s_tx_buffer, (uint16_t)length);
    if (status != HAL_OK)
    {
        return status;
    }

    start_tick = HAL_GetTick();
    while (HAL_UART_GetState(&huart3) != HAL_UART_STATE_READY)
    {
        if ((HAL_GetTick() - start_tick) > AD9226_UART_TIMEOUT_MS)
        {
            (void)HAL_UART_AbortTransmit(&huart3);
            return HAL_TIMEOUT;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef AD9226_API_Init(void)
{
    HAL_StatusTypeDef status = AD9226_Init();

    if (status == HAL_OK)
    {
        status = AD9226_API_Send("ad9226_ready:1,sample_rate:1000000,samples:4096\r\n");
    }

    return status;
}

void AD9226_API_Process(void)
{
    static char report[384];
    AD9226_AnalysisResult result;
    HAL_StatusTypeDef status;
    int length;

    status = AD9226_CaptureFrame();
    if (status != HAL_OK)
    {
        (void)AD9226_API_Send("dcmi_capture_error:1\r\n");
        HAL_Delay(200U);
        return;
    }

    status = AD9226_BLL_Analyze(AD9226_GetCaptureBuffer(),
                                AD9226_GetCaptureLength(),
                                &result);
    if (status != HAL_OK)
    {
        length = snprintf(report, sizeof(report),
                          "thd_valid:0,min:%u,max:%u,mean:%.2f\r\n",
                          result.minimum_code,
                          result.maximum_code,
                          (double)result.mean_code);
    }
    else
    {
        length = snprintf(report, sizeof(report),
                          "frequency:%.3f,thd:%.3f,u1:%.6f,u2:%.6f,"
                          "u3:%.6f,u4:%.6f,u5:%.6f,min:%u,max:%u,"
                          "mean:%.2f,periods:%lu\r\n",
                          (double)result.frequency_hz,
                          (double)result.thd_percent,
                          (double)result.harmonic_rms_v[0],
                          (double)result.harmonic_rms_v[1],
                          (double)result.harmonic_rms_v[2],
                          (double)result.harmonic_rms_v[3],
                          (double)result.harmonic_rms_v[4],
                          result.minimum_code,
                          result.maximum_code,
                          (double)result.mean_code,
                          (unsigned long)result.averaged_periods);
    }

    if ((length > 0) && ((size_t)length < sizeof(report)))
    {
        (void)AD9226_API_Send(report);
    }

    HAL_Delay(AD9226_REPORT_PERIOD_MS);
}
