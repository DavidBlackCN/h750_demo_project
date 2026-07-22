#ifndef AD9910_API_H
#define AD9910_API_H

#include "main.h"

#define AD9910_API_MAX_DIRECT_MVPP  724U
#define AD9910_API_MAX_TRIANGLE_MVPP  AD9910_API_MAX_DIRECT_MVPP

typedef enum
{
    AD9910_API_WAVE_SINE = 0,  /**< 单 Profile DDS 正弦波。 */
    AD9910_API_WAVE_TRIANGLE, /**< RAM polar 回放三角波。 */
    AD9910_API_WAVE_SQUARE    /**< RAM polar 回放方波。 */
} AD9910_API_Waveform;

/**
 * @brief 初始化 AD9910 控制 GPIO 和芯片基础寄存器。
 * @note  本函数具有幂等性：首次调用执行硬件初始化，后续调用直接返回，不会再次
 *        Master Reset。各输出接口会自动调用本函数，因此应用层也可以不显式调用。
 */
void AD9910_API_Init(void);

/**
 * @brief 设置 DDS 经十倍放大后的正弦波输出。
 * @note  输出链路：AD9910（约 0～724.6 mVpp）→ 10 倍放大器 → 示波器。
 *        `mvpp` 表示十倍放大后的目标 Vpp，换算关系为
 *        `Amp ≈ 0x3FFF * mvpp / AD9910_OUTPUT_FULL_SCALE_MVPP`。
 *        重新标定时只需修改
 *        HDL/AD9910_Constants.h 中的 AD9910_OUTPUT_FULL_SCALE_MVPP。
 *        函数随后初始化 AD9910、写入 Profile 0 并启动正弦输出。
 *        本接口不补偿已知模型电路的频率增益；模型反算由 G 题业务层完成。
 * @param hz    目标频率（Hz），范围 1 Hz～400 MHz。
 * @param mvpp  十倍放大后目标峰峰值（mVpp），范围 0～7246 mVpp。
 */
void AD9910_output_sine(uint32_t hz, uint32_t mvpp);

/**
 * @brief 初始化 AD9910，并从 Profile 0 输出指定波形。
 * @note  本接口的幅度表示 AD9910 模块直接输出端的 Vpp，不包含 G 题外接的
 *        10 倍放大器。正弦波使用单 Profile DDS；三角波和方波使用 RAM 回放。
 * @param waveform      输出波形类型，见 @ref AD9910_API_Waveform。
 * @param frequency_hz  目标频率（Hz）。正弦波支持 1 Hz～400 MHz；RAM 三角波/
 *                      方波支持 77 Hz～5 MHz。
 * @param amplitude_mvpp AD9910 模块直接输出目标幅度（mVpp），范围 0～724 mVpp。
 * @retval HAL_OK       参数有效且配置时序已经发送。
 * @retval HAL_ERROR    波形、频率或幅度越界。
 */
HAL_StatusTypeDef AD9910_API_StartWaveform(AD9910_API_Waveform waveform,
                                          uint32_t frequency_hz,
                                          uint32_t amplitude_mvpp);

/**
 * @brief 将 AD9910 模块直接输出幅度换算为 14 位 ASF 码。
 * @note  本函数与 AD9910_output_sine() 共用末级输出满量程标定值，再除以固定
 *        放大倍数得到 DDS 直接输出换算关系。只做数值换算，不初始化芯片，
 *        也不写寄存器。
 * @param amplitude_mvpp AD9910 模块直接输出目标幅度（mVpp），范围 0～724 mVpp。
 * @param amplitude_code 返回的 ASF 码，合法范围 0～0x3FFF。
 * @retval HAL_OK        换算成功。
 * @retval HAL_ERROR     输出指针为空、幅度越界或结果超过 14 位范围。
 */
HAL_StatusTypeDef AD9910_API_DirectMvppToAmplitudeCode(
    uint32_t amplitude_mvpp, uint16_t *amplitude_code);

/**
 * @brief 使用原始 14 位 ASF 码输出单频正弦波。
 * @note  首次调用时初始化控制 GPIO、复位 AD9910 并配置单 Profile 模式；后续
 *        连续调整正弦参数只重写 Profile 0 的 FTW/POW/ASF，不会重复复位芯片。
 *        该接口适合标定和手动调试；ASF 只是数字幅度码，实际 Vpp 仍由 AD9910
 *        模块及负载决定。
 * @param frequency_hz 目标频率（Hz），范围 1 Hz～400 MHz。
 * @param amplitude_code 原始 ASF 幅度码，范围 0～0x3FFF（16383）。
 * @retval HAL_OK      参数有效且配置时序已经发送。
 * @retval HAL_ERROR   频率或 ASF 码越界。
 */
HAL_StatusTypeDef AD9910_API_StartSineByAmplitudeCode(uint32_t frequency_hz,
                                                      uint16_t amplitude_code);

#endif
