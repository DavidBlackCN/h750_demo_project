#include "DAC8830_SPI.h"

#include "spi.h"

#define DAC8830_SPI_POST_TX_DELAY_MS (0U)
#define DAC8830_SPI_WAIT_LIMIT       (1000U)
#define DAC8830_SPI_CLEAR_FLAGS      (SPI_IFCR_EOTC | SPI_IFCR_TXTFC | \
                                      SPI_IFCR_UDRC | SPI_IFCR_OVRC | \
                                      SPI_IFCR_TIFREC | SPI_IFCR_MODFC | \
                                      SPI_IFCR_TSERFC | SPI_IFCR_SUSPC)

static HAL_StatusTypeDef gDac8830SpiLastStatus = HAL_OK;
static uint32_t gDac8830SpiErrorCount = 0U;

static void dac8830_pin_high(GPIO_TypeDef *port, uint16_t pin)
{
    port->BSRR = (uint32_t) pin;
}

static void dac8830_pin_low(GPIO_TypeDef *port, uint16_t pin)
{
    port->BSRR = (uint32_t) pin << 16U;
}

static void dac8830_cs1_high(void)
{
    dac8830_pin_high(DAC8830_CS1_GPIO_Port, DAC8830_CS1_Pin);
}

static void dac8830_cs1_low(void)
{
    dac8830_pin_low(DAC8830_CS1_GPIO_Port, DAC8830_CS1_Pin);
}

static void dac8830_cs2_high(void)
{
    dac8830_pin_high(DAC8830_CS2_GPIO_Port, DAC8830_CS2_Pin);
}

static void dac8830_cs2_low(void)
{
    dac8830_pin_low(DAC8830_CS2_GPIO_Port, DAC8830_CS2_Pin);
}

static uint8_t dac8830_spi_wait_flag(uint32_t flag)
{
    uint32_t wait = DAC8830_SPI_WAIT_LIMIT;

    while ((hspi1.Instance->SR & flag) == 0U)
    {
        if (wait == 0U)
        {
            return 0U;
        }
        wait--;
    }

    return 1U;
}

static void dac8830_spi_error(void)
{
    hspi1.Instance->CR1 &= ~SPI_CR1_SPE;
    hspi1.Instance->IFCR = DAC8830_SPI_CLEAR_FLAGS;
    gDac8830SpiLastStatus = HAL_TIMEOUT;
    gDac8830SpiErrorCount++;
}

static void dac8830_spi_write16(uint16_t data)
{
    SPI_TypeDef *spi = hspi1.Instance;

    spi->CR1 &= ~SPI_CR1_SPE;
    spi->IFCR = DAC8830_SPI_CLEAR_FLAGS;
    MODIFY_REG(spi->CR2, SPI_CR2_TSIZE, 1U);
    spi->CR1 |= SPI_CR1_SPE;
    spi->CR1 |= SPI_CR1_CSTART;

    if (dac8830_spi_wait_flag(SPI_SR_TXP) == 0U)
    {
        dac8830_spi_error();
        return;
    }
    *((__IO uint16_t *) &spi->TXDR) = data;

    if (dac8830_spi_wait_flag(SPI_SR_EOT) == 0U)
    {
        dac8830_spi_error();
        return;
    }

    spi->IFCR = DAC8830_SPI_CLEAR_FLAGS;
    spi->CR1 &= ~SPI_CR1_SPE;
    gDac8830SpiLastStatus = HAL_OK;
#if (DAC8830_SPI_POST_TX_DELAY_MS > 0U)
    HAL_Delay(DAC8830_SPI_POST_TX_DELAY_MS);
#endif
}

void DAC8830_SPI_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();

    dac8830_cs1_high();
    dac8830_cs2_high();

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.Pin = DAC8830_CS1_Pin;
    HAL_GPIO_Init(DAC8830_CS1_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = DAC8830_CS2_Pin;
    HAL_GPIO_Init(DAC8830_CS2_GPIO_Port, &GPIO_InitStruct);

    /* SPI1 SCK/MOSI are configured by MX_SPI1_Init(). */
}

void DAC8830_SPI_WriteChannel1(uint16_t data)
{
    dac8830_cs1_low();
    dac8830_spi_write16(data);
    dac8830_cs1_high();
}

void DAC8830_SPI_WriteChannel2(uint16_t data)
{
    dac8830_cs2_low();
    dac8830_spi_write16(data);
    dac8830_cs2_high();
}

void DAC8830_SPI_WriteBoth(uint16_t data)
{
    dac8830_cs1_low();
    dac8830_cs2_low();
    dac8830_spi_write16(data);
    dac8830_cs1_high();
    dac8830_cs2_high();
}

HAL_StatusTypeDef DAC8830_SPI_GetLastStatus(void)
{
    return gDac8830SpiLastStatus;
}

uint32_t DAC8830_SPI_GetErrorCount(void)
{
    return gDac8830SpiErrorCount;
}
