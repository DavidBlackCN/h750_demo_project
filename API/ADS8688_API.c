#include "ADS8688_API.h"

#include "ADS8688.h"
#include "ADS8688_BLL.h"
#include "spi.h"
#include "usart.h"
#include "USART_FML.h"
#include <stdio.h>

#define ADS8688_CHANNEL_COUNT                  4U
#define ADS8688_FIRST_CHANNEL                   0U
#define ADS8688_CHANNEL_MASK                    0x0FU
#define ADS8688_POWER_DOWN_MASK                 0xF0U
#define ADS8688_CHANNEL_RANGE                   ADS8688_RANGE_BIPOLAR_10V24
#define ADS8688_TOTAL_SAMPLE_RATE_HZ            400000U
#define ADS8688_PER_CHANNEL_SAMPLE_RATE_HZ      100000U
#define ADS8688_VOFA_FRAME_RATE_HZ                4000U
#define ADS8688_VOFA_DECIMATION                 (ADS8688_PER_CHANNEL_SAMPLE_RATE_HZ / \
                                                 ADS8688_VOFA_FRAME_RATE_HZ)
#define ADS8688_FIREWATER_LINE_CAPACITY            32U

static uint32_t s_next_sample_cycle;
static uint32_t s_channel_index;
static uint32_t s_frame_index;
static uint16_t s_channel_codes[ADS8688_CHANNEL_COUNT];
static uint32_t s_ads8688_output_error_count;
static uint32_t s_ads8688_capture_error_count;

static void ADS8688_API_SendFireWaterCh1(void)
{
    char line[ADS8688_FIREWATER_LINE_CAPACITY];
    float voltage;
    int length;

    voltage = ADS8688_BLL_CodeToVolts(s_channel_codes[0U],
                                       ADS8688_CHANNEL_RANGE);
    length = snprintf(line, sizeof(line), "CH1:%.6f\r\n", (double)voltage);

    if ((length <= 0) || (length >= (int)sizeof(line)) ||
        (Usart_Send_ComputerAsync(&huart1, line) != HAL_OK))
    {
        s_ads8688_output_error_count++;
    }
}

HAL_StatusTypeDef ADS8688_API_Init(void)
{
    uint8_t readback;
    uint16_t discarded_code;

    s_ads8688_output_error_count = 0U;
    s_ads8688_capture_error_count = 0U;
    s_channel_index = 0U;
    s_frame_index = 0U;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#if defined(DWT_LAR)
    DWT->LAR = 0xC5ACCE55U;
#endif
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    if ((ADS8688_Init(&hspi2) != HAL_OK) ||
        (ADS8688_SetChannelPowerDownMask(ADS8688_POWER_DOWN_MASK) != HAL_OK) ||
        (ADS8688_SetAutoScanMask(ADS8688_CHANNEL_MASK) != HAL_OK) ||
        (ADS8688_ReadProgramRegister(0x01U, &readback) != HAL_OK) ||
        (readback != ADS8688_CHANNEL_MASK))
    {
        return HAL_ERROR;
    }

    for (uint8_t channel = ADS8688_FIRST_CHANNEL;
         channel < (ADS8688_FIRST_CHANNEL + ADS8688_CHANNEL_COUNT); ++channel)
    {
        if (ADS8688_SetChannelRange(channel, ADS8688_CHANNEL_RANGE) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }

    if ((ADS8688_StartAutoScan() != HAL_OK) ||
        (ADS8688_ReadConversionFast(&discarded_code) != HAL_OK))
    {
        return HAL_ERROR;
    }

    s_next_sample_cycle = DWT->CYCCNT;
    return HAL_OK;
}

void ADS8688_API_Process(void)
{
    const uint32_t sample_period_cycles =
        SystemCoreClock / ADS8688_TOTAL_SAMPLE_RATE_HZ;
    uint16_t code;

    if ((sample_period_cycles == 0U) ||
        ((int32_t)(DWT->CYCCNT - s_next_sample_cycle) < 0))
    {
        return;
    }

    s_next_sample_cycle += sample_period_cycles;
    if ((int32_t)(DWT->CYCCNT - s_next_sample_cycle) >= 0)
    {
        /* A late foreground pass drops stale schedule slots instead of catching
           up in a burst and distorting the channel sampling interval. */
        s_next_sample_cycle = DWT->CYCCNT + sample_period_cycles;
    }

    if (ADS8688_ReadConversionFast(&code) != HAL_OK)
    {
        s_ads8688_capture_error_count++;
        return;
    }

    s_channel_codes[s_channel_index++] = code;
    if (s_channel_index < ADS8688_CHANNEL_COUNT)
    {
        return;
    }

    s_channel_index = 0U;
    s_frame_index++;
    if ((s_frame_index % ADS8688_VOFA_DECIMATION) == 0U)
    {
        ADS8688_API_SendFireWaterCh1();
    }
}

uint32_t ADS8688_API_GetOutputErrorCount(void)
{
    return s_ads8688_output_error_count;
}

uint32_t ADS8688_API_GetCaptureErrorCount(void)
{
    return s_ads8688_capture_error_count;
}
