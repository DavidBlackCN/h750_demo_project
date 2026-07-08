#include "DAC8830_SPI.h"

static void dac8830_spi_delay(void)
{
    __NOP();
    __NOP();
}

static void dac8830_write_pin(GPIO_TypeDef *port, uint16_t pin,
                              GPIO_PinState state)
{
    HAL_GPIO_WritePin(port, pin, state);
}

static void dac8830_cs1_high(void)
{
    dac8830_write_pin(DAC8830_CS1_GPIO_Port, DAC8830_CS1_Pin, GPIO_PIN_SET);
}

static void dac8830_cs1_low(void)
{
    dac8830_write_pin(DAC8830_CS1_GPIO_Port, DAC8830_CS1_Pin, GPIO_PIN_RESET);
}

static void dac8830_cs2_high(void)
{
    dac8830_write_pin(DAC8830_CS2_GPIO_Port, DAC8830_CS2_Pin, GPIO_PIN_SET);
}

static void dac8830_cs2_low(void)
{
    dac8830_write_pin(DAC8830_CS2_GPIO_Port, DAC8830_CS2_Pin, GPIO_PIN_RESET);
}

static void dac8830_sdi_high(void)
{
    dac8830_write_pin(DAC8830_SDI_GPIO_Port, DAC8830_SDI_Pin, GPIO_PIN_SET);
}

static void dac8830_sdi_low(void)
{
    dac8830_write_pin(DAC8830_SDI_GPIO_Port, DAC8830_SDI_Pin, GPIO_PIN_RESET);
}

static void dac8830_sclk_high(void)
{
    dac8830_write_pin(DAC8830_SCLK_GPIO_Port, DAC8830_SCLK_Pin, GPIO_PIN_SET);
}

static void dac8830_sclk_low(void)
{
    dac8830_write_pin(DAC8830_SCLK_GPIO_Port, DAC8830_SCLK_Pin, GPIO_PIN_RESET);
}

static void dac8830_spi_write16(uint16_t data)
{
    for (uint8_t i = 0U; i < 16U; i++)
    {
        if ((data & 0x8000U) != 0U)
        {
            dac8830_sdi_high();
        }
        else
        {
            dac8830_sdi_low();
        }

        dac8830_spi_delay();
        dac8830_sclk_high();
        dac8830_spi_delay();
        dac8830_sclk_low();
        dac8830_spi_delay();
        data <<= 1U;
    }
}

void DAC8830_SPI_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    dac8830_cs1_high();
    dac8830_cs2_high();
    dac8830_sdi_low();
    dac8830_sclk_low();

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.Pin = DAC8830_CS1_Pin;
    HAL_GPIO_Init(DAC8830_CS1_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = DAC8830_CS2_Pin;
    HAL_GPIO_Init(DAC8830_CS2_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = DAC8830_SDI_Pin;
    HAL_GPIO_Init(DAC8830_SDI_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = DAC8830_SCLK_Pin;
    HAL_GPIO_Init(DAC8830_SCLK_GPIO_Port, &GPIO_InitStruct);
}

void DAC8830_SPI_WriteChannel1(uint16_t data)
{
    dac8830_sclk_low();
    dac8830_cs1_low();
    dac8830_spi_delay();
    dac8830_spi_write16(data);
    dac8830_spi_delay();
    dac8830_cs1_high();
}

void DAC8830_SPI_WriteChannel2(uint16_t data)
{
    dac8830_sclk_low();
    dac8830_cs2_low();
    dac8830_spi_delay();
    dac8830_spi_write16(data);
    dac8830_spi_delay();
    dac8830_cs2_high();
}

void DAC8830_SPI_WriteBoth(uint16_t data)
{
    dac8830_sclk_low();
    dac8830_cs1_low();
    dac8830_cs2_low();
    dac8830_spi_delay();
    dac8830_spi_write16(data);
    dac8830_spi_delay();
    dac8830_cs1_high();
    dac8830_cs2_high();
}
