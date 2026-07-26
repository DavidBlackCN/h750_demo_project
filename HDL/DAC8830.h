#ifndef DAC8830_H
#define DAC8830_H

#include <stdint.h>

/* Module-manual transfer ranges for the J3/J4 and J7/J8 output jumpers. */
#define DAC8830_BIPOLAR_10V_MIN_MV      (-10000L)
#define DAC8830_BIPOLAR_10V_MAX_MV      (10000L)
#define DAC8830_BIPOLAR_5V_MIN_MV       (-5000L)
#define DAC8830_BIPOLAR_5V_MAX_MV       (5000L)
#define DAC8830_UNIPOLAR_5V_MIN_MV      (0L)
#define DAC8830_UNIPOLAR_5V_MAX_MV      (5000L)
#define DAC8830_UNIPOLAR_10V_MIN_MV     (0L)
#define DAC8830_UNIPOLAR_10V_MAX_MV     (10000L)

#define DAC8830_CODE_STEPS              (65536UL)
#define DAC8830_CODE_MAX                (65535UL)

/* Compatibility values used by the DAC8830 driver from I_250730. */
#define OUTPUT_DC                        (0U)
#define OUTPUT_WAVE                      (1U)

/* The external module uses a 2.5 V DAC reference and an output amplifier. */
#define DAC8830_REFERENCE_MV             (2500.0)
#define VREF                             DAC8830_REFERENCE_MV
#define DAC8830_COMPAT_WAVE_SAMPLES      (4096U)

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

/*
 * I_250730-compatible voltage interfaces. The voltage arguments use volts.
 * Set_Wave() is a foreground diagnostic stream.
 */
void DAC8830_Set_Direct_Current(double voltage);
void DAC8830_Set_Wave(double *data, uint16_t data_size);
void DAC8830_Generate_Wave_Data(double amplitude);
int DAC8830_Set_Mode_Voltage(uint8_t mode, double voltage);

#endif
