#ifndef AD9910_CONSTANTS_H
#define AD9910_CONSTANTS_H

/* AD9910 amplitude scale factor is a 14-bit unsigned value. */
#define AD9910_AMPLITUDE_CODE_MAX  0x3FFFU

/*
 * AD9910 + 固定十倍放大后的实测满量程输出，单位 mVpp。
 * 幅度重新标定时只修改这一项：Amp ≈ 0x3FFF * mvpp / 本宏。
 */
#define AD9910_OUTPUT_FULL_SCALE_MVPP  7646.0f

/* 固定后级放大倍数，仅用于直接 DDS 输出值与末级输出值之间的换算。 */
#define AD9910_OUTPUT_AMPLIFIER_GAIN     10.0f

#endif
