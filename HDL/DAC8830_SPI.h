#ifndef DAC8830_SPI_H
#define DAC8830_SPI_H

#include <stdint.h>

#include "main.h"

/*
 * Hardware SPI wiring uses SPI1 with the current DAC8830 pins.
 * CS1  -> PE2
 * CS2  -> PE0
 * SDI  -> PA7 / SPI1_MOSI
 * SCLK -> PA5 / SPI1_SCK
 */
#define DAC8830_CS1_GPIO_Port    GPIOE
#define DAC8830_CS1_Pin          GPIO_PIN_2
#define DAC8830_CS2_GPIO_Port    GPIOE
#define DAC8830_CS2_Pin          GPIO_PIN_0
#define DAC8830_SDI_GPIO_Port    GPIOA
#define DAC8830_SDI_Pin          GPIO_PIN_7
#define DAC8830_SCLK_GPIO_Port   GPIOA
#define DAC8830_SCLK_Pin         GPIO_PIN_5

void DAC8830_SPI_GPIO_Init(void);
void DAC8830_SPI_WriteChannel1(uint16_t data);
void DAC8830_SPI_WriteChannel2(uint16_t data);
void DAC8830_SPI_WriteBoth(uint16_t data);
HAL_StatusTypeDef DAC8830_SPI_GetLastStatus(void);
uint32_t DAC8830_SPI_GetErrorCount(void);

#endif
