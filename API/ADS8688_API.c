#include "ADS8688_API.h"

#include "ADS8688.h"
#include "ADS8688_BLL.h"
#include "usart.h"
#include "USART_FML.h"
#include <stdio.h>

#define ADS8688_CHANNEL_COUNT                  4U
#define ADS8688_FIRST_CHANNEL                   0U
#define ADS8688_POWER_DOWN_MASK                 0xF0U
#define ADS8688_CHANNEL_RANGE                   ADS8688_RANGE_BIPOLAR_10V24
#define ADS8688_OUTPUT_DECIMATION                  25U
#define ADS8688_FIREWATER_LINE_CAPACITY            80U

static uint32_t s_frame_index;
static uint16_t s_channel_codes[ADS8688_CHANNEL_COUNT];
static uint32_t s_ads8688_output_error_count;
static uint32_t s_ads8688_capture_error_count;

static void ADS8688_API_SendFireWaterChannels(void)
{
    char line[ADS8688_FIREWATER_LINE_CAPACITY];
    float voltages[ADS8688_CHANNEL_COUNT];
    int length;

    for (uint8_t channel = 0U; channel < ADS8688_CHANNEL_COUNT; ++channel)
    {
        voltages[channel] = ADS8688_BLL_CodeToVolts(s_channel_codes[channel],
                                                     ADS8688_CHANNEL_RANGE);
    }

    length = snprintf(line, sizeof(line),
                      "samples:%.6f,%.6f,%.6f,%.6f\r\n",
                      (double)voltages[0U], (double)voltages[1U],
                      (double)voltages[2U], (double)voltages[3U]);

    if ((length <= 0) || (length >= (int)sizeof(line)) ||
        (Usart_Send_ComputerAsync(&huart1, line) != HAL_OK))
    {
        s_ads8688_output_error_count++;
    }
}

HAL_StatusTypeDef ADS8688_API_Init(void)
{
    s_ads8688_output_error_count = 0U;
    s_ads8688_capture_error_count = 0U;
    s_frame_index = 0U;

    if ((ADS8688_Init() != HAL_OK) ||
        (ADS8688_SetChannelPowerDownMask(ADS8688_POWER_DOWN_MASK) != HAL_OK))
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

    return HAL_OK;
}

void ADS8688_API_Process(void)
{
    if (ADS8688_ReadManualChannels(s_channel_codes,
                                   ADS8688_CHANNEL_COUNT) != HAL_OK)
    {
        s_ads8688_capture_error_count++;
        return;
    }

    s_frame_index++;
    if ((s_frame_index % ADS8688_OUTPUT_DECIMATION) == 0U)
    {
        ADS8688_API_SendFireWaterChannels();
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
