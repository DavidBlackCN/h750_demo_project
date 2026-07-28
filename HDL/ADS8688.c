#include "ADS8688.h"

#define ADS8688_COMMAND_RESET                0x8500U
#define ADS8688_COMMAND_AUTO_RESET           0xA000U
#define ADS8688_COMMAND_MANUAL_BASE          0xC000U
#define ADS8688_PROGRAM_AUTO_SEQUENCE        0x01U
#define ADS8688_PROGRAM_CHANNEL_POWER_DOWN   0x02U
#define ADS8688_PROGRAM_RANGE_CH0            0x05U
#define ADS8688_CHANNEL_COUNT                8U

static void ADS8688_SoftSpi_WriteByte(uint8_t data)
{
    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15,
                          ((data & 0x80U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
        data <<= 1U;
    }
}

static uint8_t ADS8688_SoftSpi_ReadByte(void)
{
    uint8_t data = 0U;

    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
        data <<= 1U;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_SET)
        {
            data |= 0x01U;
        }
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
    }

    return data;
}

static void ADS8688_Select(void)
{
    HAL_GPIO_WritePin(ADS8688_CS_GPIO_Port, ADS8688_CS_Pin, GPIO_PIN_RESET);
}

static void ADS8688_Deselect(void)
{
    HAL_GPIO_WritePin(ADS8688_CS_GPIO_Port, ADS8688_CS_Pin, GPIO_PIN_SET);
}

HAL_StatusTypeDef ADS8688_WriteCommand(uint16_t command)
{
    ADS8688_Select();
    ADS8688_SoftSpi_WriteByte((uint8_t)(command >> 8U));
    ADS8688_SoftSpi_WriteByte((uint8_t)command);
    ADS8688_Deselect();
    return HAL_OK;
}

HAL_StatusTypeDef ADS8688_WriteProgramRegister(uint8_t address, uint8_t data)
{
    ADS8688_Select();
    ADS8688_SoftSpi_WriteByte((uint8_t)((address << 1U) | 0x01U));
    ADS8688_SoftSpi_WriteByte(data);
    ADS8688_Deselect();
    return HAL_OK;
}

HAL_StatusTypeDef ADS8688_ReadProgramRegister(uint8_t address, uint8_t *data)
{
    if (data == NULL)
    {
        return HAL_ERROR;
    }

    ADS8688_Select();
    ADS8688_SoftSpi_WriteByte((uint8_t)(address << 1U));
    (void)ADS8688_SoftSpi_ReadByte();
    *data = ADS8688_SoftSpi_ReadByte();
    ADS8688_Deselect();
    return HAL_OK;
}

HAL_StatusTypeDef ADS8688_SetChannelRange(uint8_t channel, ADS8688_Range range)
{
    if ((channel >= ADS8688_CHANNEL_COUNT) ||
        ((uint8_t)range > (uint8_t)ADS8688_RANGE_UNIPOLAR_5V12))
    {
        return HAL_ERROR;
    }

    return ADS8688_WriteProgramRegister(
        (uint8_t)(ADS8688_PROGRAM_RANGE_CH0 + channel), (uint8_t)range);
}

HAL_StatusTypeDef ADS8688_SetAutoScanMask(uint8_t channel_mask)
{
    return ADS8688_WriteProgramRegister(ADS8688_PROGRAM_AUTO_SEQUENCE,
                                        channel_mask);
}

HAL_StatusTypeDef ADS8688_SetChannelPowerDownMask(uint8_t channel_mask)
{
    return ADS8688_WriteProgramRegister(ADS8688_PROGRAM_CHANNEL_POWER_DOWN,
                                        channel_mask);
}

HAL_StatusTypeDef ADS8688_StartAutoScan(void)
{
    return ADS8688_WriteCommand(ADS8688_COMMAND_AUTO_RESET);
}

HAL_StatusTypeDef ADS8688_SelectManualChannel(uint8_t channel)
{
    if (channel >= ADS8688_CHANNEL_COUNT)
    {
        return HAL_ERROR;
    }

    return ADS8688_WriteCommand(
        (uint16_t)(ADS8688_COMMAND_MANUAL_BASE | ((uint16_t)channel << 10U)));
}

HAL_StatusTypeDef ADS8688_ReadConversion(uint16_t *code)
{
    uint8_t data_high;
    uint8_t data_low;

    if (code == NULL)
    {
        return HAL_ERROR;
    }

    ADS8688_Select();
    ADS8688_SoftSpi_WriteByte(0U);
    ADS8688_SoftSpi_WriteByte(0U);
    data_high = ADS8688_SoftSpi_ReadByte();
    data_low = ADS8688_SoftSpi_ReadByte();
    ADS8688_Deselect();

    *code = ((uint16_t)data_high << 8U) | data_low;
    return HAL_OK;
}

HAL_StatusTypeDef ADS8688_ReadAutoScan(uint16_t *codes,
                                       uint8_t channel_count)
{
    if ((codes == NULL) || (channel_count == 0U) ||
        (channel_count > ADS8688_CHANNEL_COUNT))
    {
        return HAL_ERROR;
    }

    /* This follows the reference driver's AUTO_RST read sequence: each
       enabled channel is fetched by a complete four-byte zero transfer. */
    for (uint8_t channel = 0U; channel < channel_count; ++channel)
    {
        if (ADS8688_ReadConversion(&codes[channel]) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef ADS8688_ReadManualChannels(uint16_t *codes,
                                             uint8_t channel_count)
{
    if ((codes == NULL) || (channel_count == 0U) ||
        (channel_count > ADS8688_CHANNEL_COUNT))
    {
        return HAL_ERROR;
    }

    for (uint8_t channel = 0U; channel < channel_count; ++channel)
    {
        if ((ADS8688_SelectManualChannel(channel) != HAL_OK) ||
            (ADS8688_ReadConversion(&codes[channel]) != HAL_OK))
        {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef ADS8688_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = ADS8688_CS_Pin | GPIO_PIN_13 | GPIO_PIN_15;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_14;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &gpio);

    ADS8688_Deselect();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13 | GPIO_PIN_15, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ADS8688_RST_PD_GPIO_Port, ADS8688_RST_PD_Pin,
                      GPIO_PIN_SET);
    HAL_Delay(1U);

    if (ADS8688_WriteCommand(ADS8688_COMMAND_RESET) != HAL_OK)
    {
        return HAL_ERROR;
    }
    HAL_Delay(1U);
    return HAL_OK;
}

uint32_t ADS8688_GetErrorCount(void)
{
    return 0U;
}
