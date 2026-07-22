#ifndef __G_IIR_MODEL_BLL_H__
#define __G_IIR_MODEL_BLL_H__

#include "arm_math.h"
#include <stdint.h>

#define G_IIR_MODEL_SAMPLE_RATE_HZ  1000000U

/**
 * @brief  初始化已知模型的二阶 IIR 滤波器
 * @note   目标传递函数：H(s)=1/(1e-8*s^2+3e-4*s+1)
 *         按 1 MS/s 采样率用双线性变换离散化。
 */
void G_IIR_ModelBLL_Init(void);

/**
 * @brief  连续处理一块浮点采样
 * @param  input   输入数组，已去除 ADC 中点
 * @param  output  输出数组
 * @param  length  采样点数
 */
void G_IIR_ModelBLL_Process(const float32_t *input,
                            float32_t *output,
                            uint32_t length);

#endif
