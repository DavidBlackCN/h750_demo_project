#ifndef __AD9833_H__
#define __AD9833_H__

#include "MySPI.h"

#define AD9833_REG_CMD     (0U << 14)
#define AD9833_REG_FREQ0   (1U << 14)
#define AD9833_REG_FREQ1   (2U << 14)
#define AD9833_REG_PHASE0  (6U << 13)
#define AD9833_REG_PHASE1  (7U << 13)

#define AD9833_B28       (1U << 13)
#define AD9833_HLB       (1U << 12)
#define AD9833_FSEL0     (0U << 11)
#define AD9833_FSEL1     (1U << 11)
#define AD9833_PSEL0     (0U << 10)
#define AD9833_PSEL1     (1U << 10)
#define AD9833_PIN_SW    (1U << 9)
#define AD9833_RESET     (1U << 8)
#define AD9833_SLEEP1    (1U << 7)
#define AD9833_SLEEP12   (1U << 6)
#define AD9833_OPBITEN   (1U << 5)
#define AD9833_SIGN_PIB  (1U << 4)
#define AD9833_DIV2      (1U << 3)
#define AD9833_MODE      (1U << 1)

#define AD9833_OUT_SINUS     ((0U << 5) | (0U << 1) | (0U << 3))
#define AD9833_OUT_TRIANGLE  ((0U << 5) | (1U << 1) | (0U << 3))
#define AD9833_OUT_MSB       ((1U << 5) | (0U << 1) | (1U << 3))
#define AD9833_OUT_MSB2      ((1U << 5) | (0U << 1) | (0U << 3))

unsigned char AD9833_Init(void);
void AD9833_Reset(void);
void AD9833_ClearReset(void);
void AD9833_SetRegisterValue(unsigned short regValue);
void AD9833_SetFrequency(unsigned short reg, float fout, unsigned short type);
void AD9833_SetFrequencyQuick(float fout, unsigned short type);
void AD9833_SetPhase(unsigned short reg, unsigned short val);
void AD9833_Setup(unsigned short freq, unsigned short phase, unsigned short type);
void AD9833_SetWave(unsigned short type);

#endif
