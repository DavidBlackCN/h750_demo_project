#include "ADS8688_API.h"

#include "ADS8688.h"
#include "ADS8688_BLL.h"
#include "spi.h"
#include "usart.h"
#include <stdio.h>

#define ADS8688_DEMO_CHANNEL                 0U
#define ADS8688_DEMO_RANGE                   ADS8688_RANGE_BIPOLAR_10V24
#define ADS8688_PROGRAM_CHANNEL_POWER_DOWN   0x02U
#define ADS8688_PROGRAM_RANGE_CH0            0x05U
#define ADS8688_CAPTURE_SAMPLE_RATE_HZ        500000U
#define ADS8688_CAPTURE_SAMPLE_COUNT          1000U

static uint32_t s_ads8688_output_error_count;
static uint16_t s_ads8688_capture_codes[ADS8688_CAPTURE_SAMPLE_COUNT];

static HAL_StatusTypeDef ADS8688_API_InitCycleCounter(void)
{
    uint32_t start_cycles;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#if defined(DWT_LAR)
    DWT->LAR = 0xC5ACCE55U;
#endif
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    start_cycles = DWT->CYCCNT;
    __NOP();
    __NOP();
    return (DWT->CYCCNT != start_cycles) ? HAL_OK : HAL_ERROR;
}

static HAL_StatusTypeDef ADS8688_API_CaptureFrame(void)
{
    uint32_t sample_period_cycles;
    uint32_t next_sample_cycles;
    uint16_t discarded_code;

    sample_period_cycles = SystemCoreClock / ADS8688_CAPTURE_SAMPLE_RATE_HZ;
    if ((sample_period_cycles == 0U) ||
        (ADS8688_ReadConversionFast(&discarded_code) != HAL_OK))
    {
        return HAL_ERROR;
    }

    next_sample_cycles = DWT->CYCCNT;
    for (uint32_t i = 0U; i < ADS8688_CAPTURE_SAMPLE_COUNT; i++)
    {
        next_sample_cycles += sample_period_cycles;
        while ((int32_t)(DWT->CYCCNT - next_sample_cycles) < 0)
        {
        }

        if (ADS8688_ReadConversionFast(&s_ads8688_capture_codes[i]) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

static HAL_StatusTypeDef ADS8688_API_SendFireWater(float voltage)
{
    char frame[32];
    int length;
    HAL_StatusTypeDef status;

    length = snprintf(frame, sizeof(frame), "voltage:%.6f\r\n",
                      (double)voltage);
    if ((length <= 0) || (length >= (int)sizeof(frame)))
    {
        s_ads8688_output_error_count++;
        return HAL_ERROR;
    }

    status = HAL_UART_Transmit(&huart1, (uint8_t *)frame,
                               (uint16_t)length, 10U);
    if (status != HAL_OK)
    {
        s_ads8688_output_error_count++;
    }
    return status;
}

HAL_StatusTypeDef ADS8688_API_Init(void)
{
    uint8_t range_readback;
    uint16_t discarded_code;

    s_ads8688_output_error_count = 0U;
    if ((ADS8688_API_InitCycleCounter() != HAL_OK) ||
        (ADS8688_Init(&hspi2) != HAL_OK) ||
        (ADS8688_WriteProgramRegister(ADS8688_PROGRAM_CHANNEL_POWER_DOWN,
                                      0x00U) != HAL_OK) ||
        (ADS8688_SetChannelRange(ADS8688_DEMO_CHANNEL,
                                 ADS8688_DEMO_RANGE) != HAL_OK) ||
        (ADS8688_ReadProgramRegister(ADS8688_PROGRAM_RANGE_CH0,
                                     &range_readback) != HAL_OK) ||
        (range_readback != (uint8_t)ADS8688_DEMO_RANGE) ||
        (ADS8688_SelectManualChannel(ADS8688_DEMO_CHANNEL) != HAL_OK) ||
        (ADS8688_ReadConversion(&discarded_code) != HAL_OK))
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

void ADS8688_API_Process(void)
{
    if (ADS8688_API_CaptureFrame() != HAL_OK)
    {
        return;
    }

    for (uint32_t i = 0U; i < ADS8688_CAPTURE_SAMPLE_COUNT; i++)
    {
        float voltage = ADS8688_BLL_CodeToVolts(
            s_ads8688_capture_codes[i], ADS8688_DEMO_RANGE);

        if (ADS8688_API_SendFireWater(voltage) != HAL_OK)
        {
            break;
        }
    }
}

uint32_t ADS8688_API_GetOutputErrorCount(void)
{
    return s_ads8688_output_error_count;
}
