#include "AD9833.h"

#define AD9833_FCLK_HZ      10000000.0f
#define AD9833_FREQ_RATIO   (268435456.0f / AD9833_FCLK_HZ)

unsigned char AD9833_Init(void)
{
    AD9833_SetRegisterValue(AD9833_REG_CMD | AD9833_RESET);
    return 1U;
}

void AD9833_Reset(void)
{
    AD9833_SetRegisterValue(AD9833_REG_CMD | AD9833_RESET);
    HAL_Delay(10U);
}

void AD9833_ClearReset(void)
{
    AD9833_SetRegisterValue(AD9833_REG_CMD);
}

void AD9833_SetRegisterValue(unsigned short regValue)
{
    unsigned char data[3] = {0x03U, 0x00U, 0x00U};

    data[1] = (unsigned char)((regValue & 0xFF00U) >> 8);
    data[2] = (unsigned char)(regValue & 0x00FFU);
    AD9833_SPI_Write(data, 2U);
}

void AD9833_SetFrequencyQuick(float fout, unsigned short type)
{
    AD9833_SetFrequency(AD9833_REG_FREQ0, fout, type);
}

void AD9833_SetFrequency(unsigned short reg, float fout, unsigned short type)
{
    unsigned short freqHi = reg;
    unsigned short freqLo = reg;
    unsigned long val = (unsigned long)(AD9833_FREQ_RATIO * fout);

    freqHi |= (unsigned short)((val & 0x0FFFC000UL) >> 14);
    freqLo |= (unsigned short)(val & 0x00003FFFUL);

    AD9833_SetRegisterValue(AD9833_B28 | type);
    AD9833_SetRegisterValue(freqLo);
    AD9833_SetRegisterValue(freqHi);
}

void AD9833_SetPhase(unsigned short reg, unsigned short val)
{
    unsigned short phase = reg;

    phase |= (unsigned short)(val & 0x0FFFU);
    AD9833_SetRegisterValue(phase);
}

void AD9833_Setup(unsigned short freq, unsigned short phase, unsigned short type)
{
    AD9833_SetRegisterValue(freq | phase | type);
}

void AD9833_SetWave(unsigned short type)
{
    AD9833_SetRegisterValue(type);
}
