#include "AD9910_API.h"

#include "AD9910.h"

#define AD9910_API_MAX_FREQUENCY_HZ       400000000UL
#define AD9910_API_SYSCLK_HZ               1000000000ULL
#define AD9910_API_RAM_SAMPLE_COUNT_MIN     50U
#define AD9910_API_RAM_SAMPLE_COUNT_MAX   1024U
#define AD9910_API_RAM_STEP_RATE_MAX       65535ULL
#define AD9910_API_RAM_MIN_FREQUENCY_HZ    77UL
#define AD9910_API_RAM_MAX_FREQUENCY_HZ    5000000UL

typedef enum
{
    AD9910_API_MODE_UNCONFIGURED = 0,
    AD9910_API_MODE_SINGLE_PROFILE,
    AD9910_API_MODE_RAM
} AD9910_API_OutputMode;

static uint8_t ad9910_api_initialized;
static AD9910_API_OutputMode ad9910_api_output_mode;

void AD9910_API_Init(void)
{
    if (ad9910_api_initialized != 0U)
    {
        return;
    }

    GPIO_Init_AD9910();
    AD9910_Init();
    ad9910_api_output_mode = AD9910_API_MODE_UNCONFIGURED;
    ad9910_api_initialized = 1U;
}

void AD9910_output_sine(uint32_t hz, uint32_t mvpp)
{
    uint16_t Amp;

    if ((hz == 0U) || (hz > AD9910_API_MAX_FREQUENCY_HZ) ||
        ((float)mvpp > AD9910_OUTPUT_FULL_SCALE_MVPP))
    {
        return;
    }

    /*
     * 十倍放大后输出幅度与 ASF 的换算关系：
     *
     * Amp ≈ 0x3FFF * mvpp / AD9910_OUTPUT_FULL_SCALE_MVPP
     *
     * 该宏是末级输出端实测的满量程峰峰值。重新标定时，只需修改
     * HDL/AD9910_Constants.h 中这一处定义。
     */
    Amp = (uint16_t)((float)AD9910_AMPLITUDE_CODE_MAX * (float)mvpp /
                     AD9910_OUTPUT_FULL_SCALE_MVPP);

    (void)AD9910_API_StartSineByAmplitudeCode(hz, Amp);
}

HAL_StatusTypeDef AD9910_API_DirectMvppToAmplitudeCode(
    uint32_t amplitude_mvpp, uint16_t *amplitude_code)
{
    float code;

    if ((amplitude_code == NULL) ||
        (amplitude_mvpp > AD9910_API_MAX_DIRECT_MVPP))
    {
        return HAL_ERROR;
    }

    /* 直接输出幅度同样由统一的十倍放大后满量程标定值换算。 */
    code = (float)AD9910_AMPLITUDE_CODE_MAX * (float)amplitude_mvpp *
           AD9910_OUTPUT_AMPLIFIER_GAIN / AD9910_OUTPUT_FULL_SCALE_MVPP;
    if (code > (float)AD9910_AMPLITUDE_CODE_MAX)
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

    /*
     * RAM 回放频率关系：fout = SYSCLK / (4 * samples * step_rate)。
     * 遍历允许的点数和步进率，优先选择频率误差最小的组合；误差相同时
     * 选择更多采样点，以减小三角波的可见阶梯。
     */
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

    if (AD9910_API_DirectMvppToAmplitudeCode(amplitude_mvpp,
                                             &amplitude_code) != HAL_OK)
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

    if (waveform == AD9910_API_WAVE_SINE)
    {
        /* 正弦波直接使用 Profile 0，不经过 RAM 波形表。 */
        return AD9910_API_StartSineByAmplitudeCode(frequency_hz,
                                                   amplitude_code);
    }

    /* 硬件只初始化一次；RAM 数据和播放参数仍需按本次波形重新装载。 */
    AD9910_API_Init();
    AD9910_RamWave_Init(amplitude_code, sample_count, step_rate,
                       (waveform == AD9910_API_WAVE_TRIANGLE) ?
                       AD9910_RAM_WAVE_TRIANGLE :
                       AD9910_RAM_WAVE_SQUARE);
    Set_Profile(0U);
    ad9910_api_output_mode = AD9910_API_MODE_RAM;

    return HAL_OK;
}

HAL_StatusTypeDef AD9910_API_StartSineByAmplitudeCode(uint32_t frequency_hz,
                                                      uint16_t amplitude_code)
{
    if ((frequency_hz == 0U) ||
        (frequency_hz > AD9910_API_MAX_FREQUENCY_HZ) ||
        (amplitude_code > AD9910_AMPLITUDE_CODE_MAX))
    {
        return HAL_ERROR;
    }

    /*
     * GPIO、Master Reset 和基础寄存器只初始化一次。连续修改正弦参数时只写
     * Profile 0；仅当首次输出或从 RAM 模式切回时才重新配置单 Profile 模式。
     */
    AD9910_API_Init();
    if (ad9910_api_output_mode != AD9910_API_MODE_SINGLE_PROFILE)
    {
        AD9910_Singal_Profile_Init();
        ad9910_api_output_mode = AD9910_API_MODE_SINGLE_PROFILE;
    }
    AD9910_Singal_Profile_Set(0U, frequency_hz, amplitude_code, 0U);
    Set_Profile(0U);
    return HAL_OK;
}
