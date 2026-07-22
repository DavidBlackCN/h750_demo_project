#ifndef __G_DIGITAL_MODEL_API_H__
#define __G_DIGITAL_MODEL_API_H__

#include "main.h"

/**
 * @brief  初始化数字已知模型的独立按键入口
 * @note   PB1 使用内部上拉，按下接地；PC13 默认低电平点亮。
 *         本功能不使用 USART3 串口屏命令。
 */
void G_DigitalModelAPI_Init(void);

/**
 * @brief  在主循环中扫描 PB1 并执行按键消抖
 */
void G_DigitalModelAPI_Process(void);

/**
 * @brief  停止数字已知模型
 */
HAL_StatusTypeDef G_DigitalModelAPI_Stop(void);

#endif
