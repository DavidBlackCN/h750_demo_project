#include "ADS8688.h"

#define ADS8688_SPI_TIMEOUT_MS              10U
#define ADS8688_COMMAND_RESET                0x8500U
#define ADS8688_COMMAND_MANUAL_BASE          0xC000U
#define ADS8688_PROGRAM_RANGE_CH0            0x05U
#define ADS8688_CHANNEL_COUNT                8U
#define ADS8688_FAST_TIMEOUT_MS               1U

static SPI_HandleTypeDef *s_ads8688_spi;
static uint32_t s_ads8688_error_count;

static HAL_StatusTypeDef ADS8688_WaitSpiFlag(uint32_t flag)
{
    uint32_t start_cycles = DWT->CYCCNT;
    uint32_t timeout_cycles =
        (SystemCoreClock / 1000U) * ADS8688_FAST_TIMEOUT_MS;

    while ((SPI2->SR & flag) == 0U)
    {
        if ((DWT->CYCCNT - start_cycles) >= timeout_cycles)
        {
            return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
}

static void ADS8688_CloseFastTransfer(void)
{
    SPI2->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC;
    CLEAR_BIT(SPI2->CR1, SPI_CR1_SPE);
    ADS8688_CS_GPIO_Port->BSRR = ADS8688_CS_Pin;
}

static HAL_StatusTypeDef ADS8688_Transfer(const uint8_t *tx_data,
                                          uint8_t *rx_data,
                                          uint16_t length)
{
    HAL_StatusTypeDef status;

    if ((s_ads8688_spi == NULL) || (tx_data == NULL) ||
        (rx_data == NULL) || (length == 0U))
    {
        return HAL_ERROR;
    }

    ADS8688_CS_GPIO_Port->BSRR = ((uint32_t)ADS8688_CS_Pin << 16U);
    status = HAL_SPI_TransmitReceive(s_ads8688_spi, (uint8_t *)tx_data,
                                    rx_data, length, ADS8688_SPI_TIMEOUT_MS);
    HAL_GPIO_WritePin(ADS8688_CS_GPIO_Port, ADS8688_CS_Pin, GPIO_PIN_SET);

    if (status != HAL_OK)
    {
        s_ads8688_error_count++;
    }
    return status;
}

HAL_StatusTypeDef ADS8688_WriteCommand(uint16_t command)
{
    uint8_t tx_data[2] = {(uint8_t)(command >> 8), (uint8_t)command};
    uint8_t rx_data[2] = {0U};

    return ADS8688_Transfer(tx_data, rx_data, sizeof(tx_data));
}

HAL_StatusTypeDef ADS8688_WriteProgramRegister(uint8_t address, uint8_t data)
{
    uint8_t tx_data[2] = {(uint8_t)((address << 1U) | 0x01U), data};
    uint8_t rx_data[2] = {0U};

    return ADS8688_Transfer(tx_data, rx_data, sizeof(tx_data));
}

HAL_StatusTypeDef ADS8688_ReadProgramRegister(uint8_t address, uint8_t *data)
{
    uint8_t tx_data[3] = {(uint8_t)(address << 1U), 0U, 0U};
    uint8_t rx_data[3] = {0U};
    HAL_StatusTypeDef status;

    if (data == NULL)
    {
        return HAL_ERROR;
    }

    status = ADS8688_Transfer(tx_data, rx_data, sizeof(tx_data));
    if (status == HAL_OK)
    {
        *data = rx_data[2];
    }
    return status;
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
    uint8_t tx_data[4] = {0U};
    uint8_t rx_data[4] = {0U};
    HAL_StatusTypeDef status;

    if (code == NULL)
    {
        return HAL_ERROR;
    }

    status = ADS8688_Transfer(tx_data, rx_data, sizeof(tx_data));
    if (status == HAL_OK)
    {
        *code = ((uint16_t)rx_data[2] << 8U) | rx_data[3];
    }
    return status;
}

HAL_StatusTypeDef ADS8688_ReadConversionFast(uint16_t *code)
{
    uint8_t rx_data[4];
    HAL_StatusTypeDef status = HAL_OK;

    if ((code == NULL) || (s_ads8688_spi == NULL) ||
        (s_ads8688_spi->Instance != SPI2))
    {
        return HAL_ERROR;
    }

    ADS8688_CS_GPIO_Port->BSRR = ((uint32_t)ADS8688_CS_Pin << 16U);
    MODIFY_REG(SPI2->CR2, SPI_CR2_TSIZE, 4U);
    SET_BIT(SPI2->CR1, SPI_CR1_SPE);
    SET_BIT(SPI2->CR1, SPI_CR1_CSTART);

    for (uint32_t i = 0U; i < 4U; i++)
    {
        status = ADS8688_WaitSpiFlag(SPI_SR_TXP);
        if (status != HAL_OK)
        {
            break;
        }
        *(__IO uint8_t *)&SPI2->TXDR = 0U;
    }

    if (status == HAL_OK)
    {
        status = ADS8688_WaitSpiFlag(SPI_SR_RXWNE);
    }
    if (status == HAL_OK)
    {
        for (uint32_t i = 0U; i < 4U; i++)
        {
            rx_data[i] = *(__IO uint8_t *)&SPI2->RXDR;
        }
        status = ADS8688_WaitSpiFlag(SPI_SR_EOT);
    }

    ADS8688_CloseFastTransfer();
    if (status != HAL_OK)
    {
        s_ads8688_error_count++;
        return status;
    }

    *code = ((uint16_t)rx_data[2] << 8U) | rx_data[3];
    return HAL_OK;
}

HAL_StatusTypeDef ADS8688_Init(SPI_HandleTypeDef *hspi)
{
    if ((hspi == NULL) || (hspi->Instance != SPI2))
    {
        return HAL_ERROR;
    }

    s_ads8688_spi = hspi;
    s_ads8688_error_count = 0U;
    HAL_GPIO_WritePin(ADS8688_CS_GPIO_Port, ADS8688_CS_Pin, GPIO_PIN_SET);
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
    return s_ads8688_error_count;
}
