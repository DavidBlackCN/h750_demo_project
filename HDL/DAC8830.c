#include "DAC8830.h"

#include "DAC8830_SPI.h"

#include "arm_math.h"

#define DAC8830_TWO_PI 6.28318530717958647692

int16_t DAC8830_ZeroCode[2] = {0, 0};

static DAC8830_OutputMode gDac8830OutputMode = DAC8830_OUTPUT_BIPOLAR_10V;
static DAC8830_RangeMv gDac8830RangeMv = {
    DAC8830_BIPOLAR_10V_MIN_MV,
    DAC8830_BIPOLAR_10V_MAX_MV
};

void DAC8830_Init(void)
{
    DAC8830_SPI_GPIO_Init();
    DAC8830_SelectBipolar10V();
}

void DAC8830_SetOutputMode(DAC8830_OutputMode mode)
{
    gDac8830OutputMode = mode;

    switch (mode)
    {
    case DAC8830_OUTPUT_BIPOLAR_5V:
        DAC8830_SetRangeMv(DAC8830_BIPOLAR_5V_MIN_MV,
                            DAC8830_BIPOLAR_5V_MAX_MV);
        break;
    case DAC8830_OUTPUT_UNIPOLAR_5V:
        DAC8830_SetRangeMv(DAC8830_UNIPOLAR_5V_MIN_MV,
                            DAC8830_UNIPOLAR_5V_MAX_MV);
        break;
    case DAC8830_OUTPUT_UNIPOLAR_10V:
        DAC8830_SetRangeMv(DAC8830_UNIPOLAR_10V_MIN_MV,
                            DAC8830_UNIPOLAR_10V_MAX_MV);
        break;
    case DAC8830_OUTPUT_BIPOLAR_10V:
    default:
        gDac8830OutputMode = DAC8830_OUTPUT_BIPOLAR_10V;
        DAC8830_SetRangeMv(DAC8830_BIPOLAR_10V_MIN_MV,
                            DAC8830_BIPOLAR_10V_MAX_MV);
        break;
    }
}

DAC8830_OutputMode DAC8830_GetOutputMode(void)
{
    return gDac8830OutputMode;
}

void DAC8830_SetRangeMv(int32_t minMv, int32_t maxMv)
{
    if (maxMv <= minMv)
    {
        return;
    }

    gDac8830RangeMv.minMv = minMv;
    gDac8830RangeMv.maxMv = maxMv;
}

DAC8830_RangeMv DAC8830_GetRangeMv(void)
{
    return gDac8830RangeMv;
}

void DAC8830_SelectBipolar10V(void)
{
    DAC8830_SetOutputMode(DAC8830_OUTPUT_BIPOLAR_10V);
}

void DAC8830_SelectBipolar5V(void)
{
    DAC8830_SetOutputMode(DAC8830_OUTPUT_BIPOLAR_5V);
}

void DAC8830_SelectUnipolar5V(void)
{
    DAC8830_SetOutputMode(DAC8830_OUTPUT_UNIPOLAR_5V);
}

void DAC8830_SelectUnipolar10V(void)
{
    DAC8830_SetOutputMode(DAC8830_OUTPUT_UNIPOLAR_10V);
}

void DAC8830_WriteChannelA(uint16_t code)
{
    DAC8830_SPI_WriteChannel1(code);
}

void DAC8830_WriteChannelB(uint16_t code)
{
    DAC8830_SPI_WriteChannel2(code);
}

void DAC8830_WriteBoth(uint16_t code)
{
    DAC8830_SPI_WriteBoth(code);
}

uint16_t DAC8830_VoltageMvToCode(int32_t voltageMv, int16_t zeroCode)
{
    int32_t spanMv;
    int32_t scaledMv;
    uint32_t code;

    if (voltageMv > gDac8830RangeMv.maxMv)
    {
        voltageMv = gDac8830RangeMv.maxMv;
    }
    else if (voltageMv < gDac8830RangeMv.minMv)
    {
        voltageMv = gDac8830RangeMv.minMv;
    }

    spanMv = gDac8830RangeMv.maxMv - gDac8830RangeMv.minMv;
    scaledMv = voltageMv - gDac8830RangeMv.minMv - (int32_t) zeroCode;
    if (scaledMv <= 0L)
    {
        return 0U;
    }
    if (scaledMv >= spanMv)
    {
        return (uint16_t) DAC8830_CODE_MAX;
    }

    code = ((uint32_t) scaledMv * DAC8830_CODE_STEPS +
            ((uint32_t) spanMv / 2UL)) /
           (uint32_t) spanMv;
    if (code > DAC8830_CODE_MAX)
    {
        code = DAC8830_CODE_MAX;
    }

    return (uint16_t) code;
}

void DAC8830_SetVoltageMvA(int32_t voltageMv)
{
    DAC8830_WriteChannelA(DAC8830_VoltageMvToCode(
        voltageMv, DAC8830_ZeroCode[0]));
}

void DAC8830_SetVoltageMvB(int32_t voltageMv)
{
    DAC8830_WriteChannelB(DAC8830_VoltageMvToCode(
        voltageMv, DAC8830_ZeroCode[1]));
}

void DAC8830_SetVoltageMvBoth(int32_t voltageMv)
{
    DAC8830_WriteChannelA(DAC8830_VoltageMvToCode(
        voltageMv, DAC8830_ZeroCode[0]));
    DAC8830_WriteChannelB(DAC8830_VoltageMvToCode(
        voltageMv, DAC8830_ZeroCode[1]));
}

static uint8_t dac8830_voltage_is_in_range(double voltage)
{
    const DAC8830_RangeMv range = DAC8830_GetRangeMv();
    const double min_voltage = (double)range.minMv / 1000.0;
    const double max_voltage = (double)range.maxMv / 1000.0;

    return (voltage == voltage) &&
           (voltage >= min_voltage) &&
           (voltage <= max_voltage);
}

static int32_t dac8830_voltage_to_mv(double voltage)
{
    const double millivolts = voltage * 1000.0;

    return (millivolts >= 0.0) ?
           (int32_t)(millivolts + 0.5) :
           (int32_t)(millivolts - 0.5);
}

void DAC8830_Set_Direct_Current(double voltage)
{
    if (dac8830_voltage_is_in_range(voltage) == 0U)
    {
        return;
    }

    DAC8830_SetVoltageMvBoth(dac8830_voltage_to_mv(voltage));
}

void DAC8830_Set_Wave(double *data, uint16_t data_size)
{
    if (data == NULL)
    {
        return;
    }

    for (uint16_t index = 0U; index < data_size; ++index)
    {
        DAC8830_Set_Direct_Current(data[index]);
    }
}

void DAC8830_Generate_Wave_Data(double amplitude)
{
    const DAC8830_OutputMode mode = DAC8830_GetOutputMode();

    for (uint32_t index = 0U;
         index < DAC8830_COMPAT_WAVE_SAMPLES;
         ++index)
    {
        const double sine = (double)arm_sin_f32((float)(DAC8830_TWO_PI *
            (double)index / (double)DAC8830_COMPAT_WAVE_SAMPLES));
        double voltage;

        if ((mode == DAC8830_OUTPUT_UNIPOLAR_5V) ||
            (mode == DAC8830_OUTPUT_UNIPOLAR_10V))
        {
            /* External driver's unipolar argument is the requested Vpp. */
            voltage = (amplitude * 0.5 * sine) + (amplitude * 0.5);
        }
        else
        {
            /* External driver's bipolar argument is the requested peak value. */
            voltage = amplitude * sine;
        }

        DAC8830_Set_Direct_Current(voltage);
    }
}

int DAC8830_Set_Mode_Voltage(uint8_t mode, double voltage)
{
    if ((mode != OUTPUT_DC) && (mode != OUTPUT_WAVE))
    {
        return -1;
    }

    if (dac8830_voltage_is_in_range(voltage) == 0U)
    {
        return -1;
    }

    if (mode == OUTPUT_DC)
    {
        DAC8830_Set_Direct_Current(voltage);
    }
    else
    {
        DAC8830_Generate_Wave_Data(voltage);
    }

    return 0;
}
