/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins
     PH1-OSC_OUT (PH1)   ------> RCC_OSC_OUT
     PH0-OSC_IN (PH0)   ------> RCC_OSC_IN
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, PF1_Pin|PF0_Pin|PF2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(IUP_GPIO_Port, IUP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CSN_GPIO_Port, CSN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2|GPIO_PIN_0, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, SDI_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOH, SDA_Pin|SCK_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ADS8688_CS_GPIO_Port, ADS8688_CS_Pin, GPIO_PIN_SET);

  /*Configure AD9959 software SPI and control output levels */
  HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port, AD9959_SCLK_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9959_SDIO0_GPIO_Port, AD9959_SDIO0_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOC, AD9959_UPDATE_Pin|AD9959_PDC_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9959_RESET_GPIO_Port, AD9959_RESET_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PF1_Pin PF0_Pin PF2_Pin */
  GPIO_InitStruct.Pin = PF1_Pin|PF0_Pin|PF2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : IUP_Pin CSN_Pin */
  GPIO_InitStruct.Pin = IUP_Pin|CSN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : PE2 PE0 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : SDI_Pin CS_Pin */
  GPIO_InitStruct.Pin = SDI_Pin|CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : SDA_Pin SCK_Pin */
  GPIO_InitStruct.Pin = SDA_Pin|SCK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /* PC4 is the active-low start key of the ADC1 G1 task.  ADS8688 is not
     started by this task, so its RST_PD output is intentionally not driven. */
  GPIO_InitStruct.Pin = ADS8688_RST_PD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ADS8688_RST_PD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ADS8688_CS_Pin */
  GPIO_InitStruct.Pin = ADS8688_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(ADS8688_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure AD9959 software SPI and control pins */
  GPIO_InitStruct.Pin = AD9959_SCLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(AD9959_SCLK_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = AD9959_SDIO0_Pin;
  HAL_GPIO_Init(AD9959_SDIO0_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = AD9959_CS_Pin|AD9959_UPDATE_Pin|AD9959_PDC_Pin;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = AD9959_RESET_Pin;
  HAL_GPIO_Init(AD9959_RESET_GPIO_Port, &GPIO_InitStruct);

  /*AnalogSwitch Config */
  HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PA1, SYSCFG_SWITCH_PA1_CLOSE);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
