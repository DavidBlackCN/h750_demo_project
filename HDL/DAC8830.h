#ifndef DAC8830_H
#define DAC8830_H

#include <stdint.h>

/*
 * DAC8830 is a 16-bit DAC. In unipolar +5 V jumper mode the module output
 * follows VREF, so the default full-scale is 4096 mV, not 5000 mV.
 */
#define DAC8830_VREF_MV                 (4096L)

#define DAC8830_BIPOLAR_10V_MIN_MV      (-10000L)
#define DAC8830_BIPOLAR_10V_MAX_MV      (10000L)
#define DAC8830_BIPOLAR_5V_MIN_MV       (-5000L)
#define DAC8830_BIPOLAR_5V_MAX_MV       (5000L)
#define DAC8830_UNIPOLAR_5V_MIN_MV      (0L)
#define DAC8830_UNIPOLAR_5V_MAX_MV      (DAC8830_VREF_MV)
#define DAC8830_UNIPOLAR_10V_MIN_MV     (0L)
#define DAC8830_UNIPOLAR_10V_MAX_MV     (DAC8830_VREF_MV * 2L)

#define DAC8830_CODE_STEPS              (65536UL)
#define DAC8830_CODE_MAX                (65535UL)

typedef enum {
    DAC8830_OUTPUT_BIPOLAR_10V = 0,
    DAC8830_OUTPUT_BIPOLAR_5V,
    DAC8830_OUTPUT_UNIPOLAR_5V,
    DAC8830_OUTPUT_UNIPOLAR_10V
} DAC8830_OutputMode;

typedef struct {
    int32_t minMv;
    int32_t maxMv;
} DAC8830_RangeMv;

extern int16_t DAC8830_ZeroCode[2];

void DAC8830_Init(void);
void DAC8830_SetOutputMode(DAC8830_OutputMode mode);
DAC8830_OutputMode DAC8830_GetOutputMode(void);
void DAC8830_SetRangeMv(int32_t minMv, int32_t maxMv);
DAC8830_RangeMv DAC8830_GetRangeMv(void);
void DAC8830_SelectBipolar10V(void);
void DAC8830_SelectBipolar5V(void);
void DAC8830_SelectUnipolar5V(void);
void DAC8830_SelectUnipolar10V(void);
void DAC8830_WriteChannelA(uint16_t code);
void DAC8830_WriteChannelB(uint16_t code);
void DAC8830_WriteBoth(uint16_t code);
uint16_t DAC8830_VoltageMvToCode(int32_t voltageMv, int16_t zeroCode);
void DAC8830_SetVoltageMvA(int32_t voltageMv);
void DAC8830_SetVoltageMvB(int32_t voltageMv);
void DAC8830_SetVoltageMvBoth(int32_t voltageMv);

#endif
