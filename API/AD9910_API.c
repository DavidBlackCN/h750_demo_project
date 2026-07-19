#include "AD9910_API.h"

#include "AD9910.h"

#define AD9910_API_MAX_FREQUENCY_HZ       400000000UL
#define AD9910_API_AMPLITUDE_CODE_MAX     16383U
#define AD9910_API_CALIBRATION_CODE        6466UL
#define AD9910_API_CALIBRATION_MVPP        286UL
#define AD9910_API_SYSCLK_HZ               1000000000ULL
#define AD9910_API_RAM_SAMPLE_COUNT_MIN     50U
#define AD9910_API_RAM_SAMPLE_COUNT_MAX   1024U
#define AD9910_API_RAM_STEP_RATE_MAX       65535ULL
#define AD9910_API_RAM_MIN_FREQUENCY_HZ    77UL
#define AD9910_API_RAM_MAX_FREQUENCY_HZ    5000000UL

static HAL_StatusTypeDef AD9910_API_MvppToCode(uint32_t amplitude_mvpp,
                                               uint16_t *amplitude_code)
{
    uint64_t code;

    if ((amplitude_code == NULL) ||
        (amplitude_mvpp > AD9910_API_MAX_DIRECT_MVPP))
    {
        return HAL_ERROR;
    }

    code = ((uint64_t)amplitude_mvpp * AD9910_API_CALIBRATION_CODE +
            (AD9910_API_CALIBRATION_MVPP / 2U)) /
           AD9910_API_CALIBRATION_MVPP;
    if (code > AD9910_API_AMPLITUDE_CODE_MAX)
    {
        return HAL_ERROR;
    }

    *amplitude_code = (uint16_t)code;
    return HAL_OK;
}

static HAL_StatusTypeDef AD9910_API_GetRamTiming(uint32_t frequency_hz,
                                                 uint16_t *sample_count,
                                                 uint16_t *step_rate)
{
    uint64_t best_error = UINT64_MAX;
    uint16_t best_samples = 0U;
    uint16_t best_rate = 0U;
    uint16_t samples;

    if ((frequency_hz == 0U) || (sample_count == NULL) ||
        (step_rate == NULL))
    {
        return HAL_ERROR;
    }

    for (samples = AD9910_API_RAM_SAMPLE_COUNT_MIN;
         samples <= AD9910_API_RAM_SAMPLE_COUNT_MAX; samples++)
    {
        uint64_t unit = 4ULL * frequency_hz * samples;
        uint64_t rate = (AD9910_API_SYSCLK_HZ + (unit / 2ULL)) / unit;
        uint64_t denominator;
        uint64_t error;

        if ((rate == 0ULL) || (rate > AD9910_API_RAM_STEP_RATE_MAX))
        {
            continue;
        }

        denominator = unit * rate;
        error = (denominator > AD9910_API_SYSCLK_HZ) ?
                (denominator - AD9910_API_SYSCLK_HZ) :
                (AD9910_API_SYSCLK_HZ - denominator);
        if ((error < best_error) ||
            ((error == best_error) && (samples > best_samples)))
        {
            best_error = error;
            best_samples = samples;
            best_rate = (uint16_t)rate;
        }
    }

    if (best_samples == 0U)
    {
        return HAL_ERROR;
    }

    *sample_count = best_samples;
    *step_rate = best_rate;
    return HAL_OK;
}

HAL_StatusTypeDef AD9910_API_StartWaveform(AD9910_API_Waveform waveform,
                                          uint32_t frequency_hz,
                                          uint32_t amplitude_mvpp)
{
    uint16_t amplitude_code;
    uint16_t sample_count = 0U;
    uint16_t step_rate = 0U;

    if ((frequency_hz == 0U) ||
        (waveform < AD9910_API_WAVE_SINE) ||
        (waveform > AD9910_API_WAVE_SQUARE))
    {
        return HAL_ERROR;
    }

    if (AD9910_API_MvppToCode(amplitude_mvpp, &amplitude_code) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (waveform == AD9910_API_WAVE_SINE)
    {
        if (frequency_hz > AD9910_API_MAX_FREQUENCY_HZ)
        {
            return HAL_ERROR;
        }
    }
    else
    {
        if ((frequency_hz < AD9910_API_RAM_MIN_FREQUENCY_HZ) ||
            (frequency_hz > AD9910_API_RAM_MAX_FREQUENCY_HZ) ||
            (AD9910_API_GetRamTiming(frequency_hz, &sample_count,
                                     &step_rate) != HAL_OK))
        {
            return HAL_ERROR;
        }
    }

    GPIO_Init_AD9910();
    AD9910_Init();

    if (waveform == AD9910_API_WAVE_SINE)
    {
        AD9910_Singal_Profile_Init();
        AD9910_Singal_Profile_Set(0U, frequency_hz, amplitude_code, 0U);
    }
    else
    {
        AD9910_RamWave_Init(amplitude_code, sample_count, step_rate,
                           (waveform == AD9910_API_WAVE_TRIANGLE) ?
                           AD9910_RAM_WAVE_TRIANGLE :
                           AD9910_RAM_WAVE_SQUARE);
    }
    Set_Profile(0U);

    return HAL_OK;
}
