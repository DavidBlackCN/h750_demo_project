#include "AD9959.h"

#define AD9959_REG_CSR       0x00U
#define AD9959_REG_FR1       0x01U
#define AD9959_REG_FR2       0x02U
#define AD9959_REG_CFR       0x03U
#define AD9959_REG_CFTW0     0x04U
#define AD9959_REG_CPOW0     0x05U
#define AD9959_REG_ACR       0x06U

static AD9959_Config ad9959_config;
static uint8_t ad9959_initialized;

static void AD9959_EnableGpioClock(GPIO_TypeDef *port)
{
    if (port == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
    else if (port == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }
    else if (port == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    }
    else if (port == GPIOD)
    {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    }
    else if (port == GPIOE)
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    }
    else if (port == GPIOF)
    {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    }
    else if (port == GPIOG)
    {
        __HAL_RCC_GPIOG_CLK_ENABLE();
    }
    else if (port == GPIOH)
    {
        __HAL_RCC_GPIOH_CLK_ENABLE();
    }
#if defined(GPIOI)
    else if (port == GPIOI)
    {
        __HAL_RCC_GPIOI_CLK_ENABLE();
    }
#endif
#if defined(GPIOJ)
    else if (port == GPIOJ)
    {
        __HAL_RCC_GPIOJ_CLK_ENABLE();
    }
#endif
#if defined(GPIOK)
    else if (port == GPIOK)
    {
        __HAL_RCC_GPIOK_CLK_ENABLE();
    }
#endif
}

static void AD9959_DelayHalfPeriod(void)
{
    volatile uint32_t cycles = ad9959_config.sclk_half_period_nops;

    while (cycles-- != 0U)
    {
        __NOP();
    }
}

static void AD9959_WritePin(GPIO_TypeDef *port, uint16_t pin,
                             GPIO_PinState state)
{
    HAL_GPIO_WritePin(port, pin, state);
}

static void AD9959_WriteByte(uint8_t data)
{
    uint8_t bit;

    AD9959_WritePin(ad9959_config.sclk_port, ad9959_config.sclk_pin,
                    GPIO_PIN_RESET);
    for (bit = 0U; bit < 8U; ++bit)
    {
        AD9959_WritePin(ad9959_config.sdio0_port, ad9959_config.sdio0_pin,
                        ((data & 0x80U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        AD9959_DelayHalfPeriod();
        AD9959_WritePin(ad9959_config.sclk_port, ad9959_config.sclk_pin,
                        GPIO_PIN_SET);
        AD9959_DelayHalfPeriod();
        AD9959_WritePin(ad9959_config.sclk_port, ad9959_config.sclk_pin,
                        GPIO_PIN_RESET);
        data <<= 1U;
    }
}

static HAL_StatusTypeDef AD9959_WriteRegisterRaw(uint8_t address,
                                                  const uint8_t *data,
                                                  uint8_t length)
{
    uint8_t index;

    if ((data == NULL) || (length == 0U))
    {
        return HAL_ERROR;
    }

    AD9959_WritePin(ad9959_config.cs_port, ad9959_config.cs_pin,
                    GPIO_PIN_RESET);
    AD9959_WriteByte(address);
    for (index = 0U; index < length; ++index)
    {
        AD9959_WriteByte(data[index]);
    }
    AD9959_WritePin(ad9959_config.cs_port, ad9959_config.cs_pin,
                    GPIO_PIN_SET);
    return HAL_OK;
}

static HAL_StatusTypeDef AD9959_ValidateChannels(AD9959_ChannelMask channels)
{
    return ((((uint8_t)channels & 0x0FU) == 0U) &&
            (((uint8_t)channels & 0xF0U) != 0U)) ? HAL_OK : HAL_ERROR;
}

static HAL_StatusTypeDef AD9959_SelectChannelsRaw(AD9959_ChannelMask channels)
{
    uint8_t data = (uint8_t)channels;

    return AD9959_WriteRegisterRaw(AD9959_REG_CSR, &data, 1U);
}

HAL_StatusTypeDef AD9959_Reset(void)
{
    if (ad9959_initialized == 0U)
    {
        return HAL_ERROR;
    }

    AD9959_WritePin(ad9959_config.reset_port, ad9959_config.reset_pin,
                    GPIO_PIN_SET);
    HAL_Delay(1U);
    AD9959_WritePin(ad9959_config.reset_port, ad9959_config.reset_pin,
                    GPIO_PIN_RESET);
    HAL_Delay(1U);
    return HAL_OK;
}

HAL_StatusTypeDef AD9959_IOUpdate(void)
{
    if (ad9959_initialized == 0U)
    {
        return HAL_ERROR;
    }

    AD9959_WritePin(ad9959_config.update_port, ad9959_config.update_pin,
                    GPIO_PIN_RESET);
    AD9959_DelayHalfPeriod();
    AD9959_WritePin(ad9959_config.update_port, ad9959_config.update_pin,
                    GPIO_PIN_SET);
    AD9959_DelayHalfPeriod();
    AD9959_WritePin(ad9959_config.update_port, ad9959_config.update_pin,
                    GPIO_PIN_RESET);
    return HAL_OK;
}

HAL_StatusTypeDef AD9959_Init(const AD9959_Config *config)
{
    GPIO_InitTypeDef gpio = {0};
    const uint8_t fr1[] = {0xD0U, 0x00U, 0x00U};
    const uint8_t fr2[] = {0x00U, 0x00U};
    const uint8_t cfr[] = {0x00U, 0x03U, 0x02U};

    if ((config == NULL) || (config->sclk_port == NULL) ||
        (config->cs_port == NULL) || (config->update_port == NULL) ||
        (config->sdio0_port == NULL) || (config->reset_port == NULL) ||
        (config->sclk_pin == 0U) || (config->cs_pin == 0U) ||
        (config->update_pin == 0U) || (config->sdio0_pin == 0U) ||
        (config->reset_pin == 0U))
    {
        return HAL_ERROR;
    }

    ad9959_config = *config;
    if (ad9959_config.system_clock_hz == 0U)
    {
        ad9959_config.system_clock_hz = AD9959_DEFAULT_SYSTEM_CLOCK_HZ;
    }
    if (ad9959_config.sclk_half_period_nops == 0U)
    {
        ad9959_config.sclk_half_period_nops = 8U;
    }

    AD9959_EnableGpioClock(ad9959_config.sclk_port);
    AD9959_EnableGpioClock(ad9959_config.cs_port);
    AD9959_EnableGpioClock(ad9959_config.update_port);
    AD9959_EnableGpioClock(ad9959_config.sdio0_port);
    AD9959_EnableGpioClock(ad9959_config.reset_port);

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    gpio.Pin = ad9959_config.sclk_pin;
    HAL_GPIO_Init(ad9959_config.sclk_port, &gpio);
    gpio.Pin = ad9959_config.cs_pin;
    HAL_GPIO_Init(ad9959_config.cs_port, &gpio);
    gpio.Pin = ad9959_config.update_pin;
    HAL_GPIO_Init(ad9959_config.update_port, &gpio);
    gpio.Pin = ad9959_config.sdio0_pin;
    HAL_GPIO_Init(ad9959_config.sdio0_port, &gpio);
    gpio.Pin = ad9959_config.reset_pin;
    HAL_GPIO_Init(ad9959_config.reset_port, &gpio);

    AD9959_WritePin(ad9959_config.sclk_port, ad9959_config.sclk_pin,
                    GPIO_PIN_RESET);
    AD9959_WritePin(ad9959_config.cs_port, ad9959_config.cs_pin,
                    GPIO_PIN_SET);
    AD9959_WritePin(ad9959_config.update_port, ad9959_config.update_pin,
                    GPIO_PIN_RESET);
    AD9959_WritePin(ad9959_config.sdio0_port, ad9959_config.sdio0_pin,
                    GPIO_PIN_RESET);
    AD9959_WritePin(ad9959_config.reset_port, ad9959_config.reset_pin,
                    GPIO_PIN_RESET);

    ad9959_initialized = 1U;
    if ((AD9959_Reset() != HAL_OK) ||
        (AD9959_WriteRegisterRaw(AD9959_REG_FR1, fr1, sizeof(fr1)) != HAL_OK) ||
        (AD9959_WriteRegisterRaw(AD9959_REG_FR2, fr2, sizeof(fr2)) != HAL_OK) ||
        (AD9959_SelectChannelsRaw(AD9959_CHANNEL_ALL) != HAL_OK) ||
        (AD9959_WriteRegisterRaw(AD9959_REG_CFR, cfr, sizeof(cfr)) != HAL_OK) ||
        (AD9959_IOUpdate() != HAL_OK))
    {
        ad9959_initialized = 0U;
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef AD9959_WriteRegister(uint8_t address, const uint8_t *data,
                                        uint8_t length)
{
    if (ad9959_initialized == 0U)
    {
        return HAL_ERROR;
    }

    return AD9959_WriteRegisterRaw(address, data, length);
}

HAL_StatusTypeDef AD9959_SelectChannels(AD9959_ChannelMask channels)
{
    if ((ad9959_initialized == 0U) ||
        (AD9959_ValidateChannels(channels) != HAL_OK))
    {
        return HAL_ERROR;
    }

    return AD9959_SelectChannelsRaw(channels);
}

HAL_StatusTypeDef AD9959_SetFrequency(AD9959_ChannelMask channels,
                                      uint32_t frequency_hz)
{
    uint64_t ftw;
    uint8_t data[4];

    if ((ad9959_initialized == 0U) ||
        (AD9959_ValidateChannels(channels) != HAL_OK) ||
        ((uint64_t)frequency_hz > ((uint64_t)ad9959_config.system_clock_hz / 2ULL)))
    {
        return HAL_ERROR;
    }

    ftw = (((uint64_t)frequency_hz << 32U) +
           ((uint64_t)ad9959_config.system_clock_hz / 2ULL)) /
          (uint64_t)ad9959_config.system_clock_hz;
    data[0] = (uint8_t)(ftw >> 24U);
    data[1] = (uint8_t)(ftw >> 16U);
    data[2] = (uint8_t)(ftw >> 8U);
    data[3] = (uint8_t)ftw;

    if (AD9959_SelectChannelsRaw(channels) != HAL_OK)
    {
        return HAL_ERROR;
    }
    return AD9959_WriteRegisterRaw(AD9959_REG_CFTW0, data, sizeof(data));
}

HAL_StatusTypeDef AD9959_SetAmplitude(AD9959_ChannelMask channels,
                                      uint16_t amplitude_code)
{
    uint16_t acr;
    uint8_t data[3] = {0U, 0U, 0U};

    if ((ad9959_initialized == 0U) ||
        (AD9959_ValidateChannels(channels) != HAL_OK) ||
        (amplitude_code > AD9959_AMPLITUDE_CODE_MAX))
    {
        return HAL_ERROR;
    }

    acr = (uint16_t)(0x1000U | amplitude_code);
    data[1] = (uint8_t)(acr >> 8U);
    data[2] = (uint8_t)acr;

    if (AD9959_SelectChannelsRaw(channels) != HAL_OK)
    {
        return HAL_ERROR;
    }
    return AD9959_WriteRegisterRaw(AD9959_REG_ACR, data, sizeof(data));
}

HAL_StatusTypeDef AD9959_SetPhase(AD9959_ChannelMask channels,
                                  uint16_t phase_code)
{
    uint8_t data[2];

    if ((ad9959_initialized == 0U) ||
        (AD9959_ValidateChannels(channels) != HAL_OK) ||
        (phase_code > AD9959_PHASE_CODE_MAX))
    {
        return HAL_ERROR;
    }

    data[0] = (uint8_t)(phase_code >> 8U);
    data[1] = (uint8_t)phase_code;
    if (AD9959_SelectChannelsRaw(channels) != HAL_OK)
    {
        return HAL_ERROR;
    }
    return AD9959_WriteRegisterRaw(AD9959_REG_CPOW0, data, sizeof(data));
}

HAL_StatusTypeDef AD9959_ConfigureSingleTone(AD9959_ChannelMask channels,
                                             uint32_t frequency_hz,
                                             uint16_t amplitude_code,
                                             uint16_t phase_code)
{
    if ((AD9959_SetFrequency(channels, frequency_hz) != HAL_OK) ||
        (AD9959_SetAmplitude(channels, amplitude_code) != HAL_OK) ||
        (AD9959_SetPhase(channels, phase_code) != HAL_OK))
    {
        return HAL_ERROR;
    }

    return AD9959_IOUpdate();
}

uint32_t AD9959_GetSystemClockHz(void)
{
    return (ad9959_initialized != 0U) ? ad9959_config.system_clock_hz : 0U;
}
