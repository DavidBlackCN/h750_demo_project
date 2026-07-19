#include "ADS8688_BLL.h"

float ADS8688_BLL_CodeToVolts(uint16_t code, ADS8688_Range range)
{
    float minimum_volts;
    float span_volts;

    switch (range)
    {
    case ADS8688_RANGE_BIPOLAR_5V12:
        minimum_volts = -5.12f;
        span_volts = 10.24f;
        break;
    case ADS8688_RANGE_BIPOLAR_2V56:
        minimum_volts = -2.56f;
        span_volts = 5.12f;
        break;
    case ADS8688_RANGE_UNIPOLAR_10V24:
        minimum_volts = 0.0f;
        span_volts = 10.24f;
        break;
    case ADS8688_RANGE_UNIPOLAR_5V12:
        minimum_volts = 0.0f;
        span_volts = 5.12f;
        break;
    case ADS8688_RANGE_BIPOLAR_10V24:
    default:
        minimum_volts = -10.24f;
        span_volts = 20.48f;
        break;
    }

    return minimum_volts + ((float)code * span_volts / 65536.0f);
}
