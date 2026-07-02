#ifndef __MYSPI_H__
#define __MYSPI_H__

#include "main.h"

#define AD9833_SDA_L    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_RESET)
#define AD9833_SDA_H    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET)
#define AD9833_SCK_L    HAL_GPIO_WritePin(SCK_GPIO_Port, SCK_Pin, GPIO_PIN_RESET)
#define AD9833_SCK_H    HAL_GPIO_WritePin(SCK_GPIO_Port, SCK_Pin, GPIO_PIN_SET)
#define AD9833_FSYNC_L  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET)
#define AD9833_FSYNC_H  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET)

unsigned char AD9833_SPI_Write(unsigned char *data, unsigned char bytesNumber);
unsigned char AD9833_SPI_Read(unsigned char *data, unsigned char bytesNumber);

#endif
