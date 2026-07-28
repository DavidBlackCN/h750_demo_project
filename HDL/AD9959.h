#ifndef AD9959_H
#define AD9959_H

#include "stm32h7xx_hal.h"

/*
 * AD9959 uses a four-wire serial control interface. Pin assignment is passed
 * at initialization because this board has no fixed AD9959 wiring yet.
 */
typedef struct
{
    GPIO_TypeDef *sclk_port;
    uint16_t sclk_pin;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *update_port;
    uint16_t update_pin;
    GPIO_TypeDef *sdio0_port;
    uint16_t sdio0_pin;
    GPIO_TypeDef *reset_port;
    uint16_t reset_pin;
    uint32_t system_clock_hz;
    uint32_t sclk_half_period_nops;
} AD9959_Config;

typedef enum
{
    AD9959_CHANNEL_0 = 0x10U,
    AD9959_CHANNEL_1 = 0x20U,
    AD9959_CHANNEL_2 = 0x40U,
    AD9959_CHANNEL_3 = 0x80U,
    AD9959_CHANNEL_ALL = 0xF0U
} AD9959_ChannelMask;

#define AD9959_DEFAULT_SYSTEM_CLOCK_HZ 500000000UL
#define AD9959_AMPLITUDE_CODE_MAX      1023U
#define AD9959_PHASE_CODE_MAX          16383U

HAL_StatusTypeDef AD9959_Init(const AD9959_Config *config);
HAL_StatusTypeDef AD9959_Reset(void);
HAL_StatusTypeDef AD9959_IOUpdate(void);
HAL_StatusTypeDef AD9959_WriteRegister(uint8_t address,
                                        const uint8_t *data,
                                        uint8_t length);
HAL_StatusTypeDef AD9959_SelectChannels(AD9959_ChannelMask channels);
HAL_StatusTypeDef AD9959_SetFrequency(AD9959_ChannelMask channels,
                                      uint32_t frequency_hz);
HAL_StatusTypeDef AD9959_SetAmplitude(AD9959_ChannelMask channels,
                                      uint16_t amplitude_code);
HAL_StatusTypeDef AD9959_SetPhase(AD9959_ChannelMask channels,
                                  uint16_t phase_code);
HAL_StatusTypeDef AD9959_ConfigureSingleTone(AD9959_ChannelMask channels,
                                             uint32_t frequency_hz,
                                             uint16_t amplitude_code,
                                             uint16_t phase_code);
uint32_t AD9959_GetSystemClockHz(void);

#endif
