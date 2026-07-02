#include "DAC_FML.h"

#include "arm_math.h"
#include "dac.h"
#include "tim.h"

#define DAC_WAVE_MAX_CODE       4095.0f
#define DAC_WAVE_TIMER_MAX_ARR  65535U
#define DAC_WAVE_TIMER_MAX_PSC  65535U
#define DAC_WAVE_TWO_PI         6.28318530717958647692f

__attribute__((section(".dma_buffer"), aligned(32)))
static uint32_t dac_wave_buffer[DAC_WAVE_BUFFER_LENGTH];

static uint8_t dac_wave_dma_started = 0U;
static dac_wave_config_t dac_wave_config = {
    DAC_USER_WAVE_SINE,
    1000.0f,
    1.0f,
    DAC_WAVE_ZERO_AXIS_V,
    1000.0f * (float)DAC_WAVE_BUFFER_LENGTH
};

static float dac_clampf(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

static uint32_t dac_voltage_to_code(float voltage)
{
    float limited_voltage = dac_clampf(voltage, 0.0f, DAC_WAVE_REF_VOLTAGE);
    return (uint32_t)((limited_voltage * DAC_WAVE_MAX_CODE / DAC_WAVE_REF_VOLTAGE) + 0.5f);
}

static float dac_wave_norm(dac_wave_type_t type, uint32_t index)
{
    float phase = (float)index / (float)DAC_WAVE_BUFFER_LENGTH;

    switch (type)
    {
    case DAC_USER_WAVE_SINE:
        return arm_sin_f32(DAC_WAVE_TWO_PI * phase);

    case DAC_USER_WAVE_SQUARE:
        return (index < (DAC_WAVE_BUFFER_LENGTH / 2U)) ? 1.0f : -1.0f;

    case DAC_USER_WAVE_TRIANGLE:
        if (phase < 0.25f)
        {
            return 4.0f * phase;
        }
        if (phase < 0.75f)
        {
            return 2.0f - (4.0f * phase);
        }
        return (4.0f * phase) - 4.0f;

    case DAC_USER_WAVE_SAWTOOTH:
        return (2.0f * phase) - 1.0f;

    case DAC_USER_WAVE_DC:
    default:
        return 0.0f;
    }
}

static void dac_wave_fill_buffer(dac_wave_type_t type, float vpp, float offset_v)
{
    float amplitude_v = dac_clampf(vpp, 0.0f, DAC_WAVE_REF_VOLTAGE) * 0.5f;
    float center_v = dac_clampf(offset_v, 0.0f, DAC_WAVE_REF_VOLTAGE);

    for (uint32_t i = 0U; i < DAC_WAVE_BUFFER_LENGTH; i++)
    {
        float voltage = center_v + (amplitude_v * dac_wave_norm(type, i));
        dac_wave_buffer[i] = dac_voltage_to_code(voltage);
    }

    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
    {
        SCB_CleanDCache_by_Addr((uint32_t *)dac_wave_buffer, sizeof(dac_wave_buffer));
    }
}

static uint32_t dac_wave_get_tim4_clock_hz(void)
{
    RCC_ClkInitTypeDef clk_init = {0};
    uint32_t flash_latency = 0U;
    uint32_t pclk1_hz = HAL_RCC_GetPCLK1Freq();

    HAL_RCC_GetClockConfig(&clk_init, &flash_latency);
    if (clk_init.APB1CLKDivider != RCC_HCLK_DIV1)
    {
        pclk1_hz *= 2U;
    }

    return pclk1_hz;
}

static HAL_StatusTypeDef dac_wave_set_timer_frequency(float sample_rate_hz)
{
    uint32_t timer_clock_hz;
    uint32_t prescaler;
    uint32_t period;
    float timer_ticks;

    if (sample_rate_hz <= 0.0f)
    {
        return HAL_ERROR;
    }

    timer_clock_hz = dac_wave_get_tim4_clock_hz();
    timer_ticks = (float)timer_clock_hz / sample_rate_hz;
    if (timer_ticks < 2.0f)
    {
        return HAL_ERROR;
    }

    prescaler = (uint32_t)((timer_ticks - 1.0f) / ((float)DAC_WAVE_TIMER_MAX_ARR + 1.0f));
    if (prescaler > DAC_WAVE_TIMER_MAX_PSC)
    {
        return HAL_ERROR;
    }

    period = (uint32_t)(((float)timer_clock_hz / (sample_rate_hz * (float)(prescaler + 1U))) + 0.5f);
    if (period < 2U)
    {
        period = 2U;
    }
    if (period > (DAC_WAVE_TIMER_MAX_ARR + 1U))
    {
        return HAL_ERROR;
    }

    __HAL_TIM_SET_PRESCALER(&htim4, prescaler);
    __HAL_TIM_SET_AUTORELOAD(&htim4, period - 1U);
    __HAL_TIM_SET_COUNTER(&htim4, 0U);
    htim4.Instance->EGR = TIM_EGR_UG;

    return HAL_OK;
}

HAL_StatusTypeDef DAC_Waveform_Apply(dac_wave_type_t type, float frequency_hz, float vpp, float offset_v)
{
    HAL_StatusTypeDef status;
    float sample_rate_hz;

    if (frequency_hz <= 0.0f)
    {
        return HAL_ERROR;
    }

    sample_rate_hz = frequency_hz * (float)DAC_WAVE_BUFFER_LENGTH;

    dac_wave_fill_buffer(type, vpp, offset_v);
    status = dac_wave_set_timer_frequency(sample_rate_hz);
    if (status != HAL_OK)
    {
        return status;
    }

    dac_wave_config.type = type;
    dac_wave_config.frequency_hz = frequency_hz;
    dac_wave_config.vpp = vpp;
    dac_wave_config.offset_v = offset_v;
    dac_wave_config.sample_rate_hz = sample_rate_hz;

    if (dac_wave_dma_started == 0U)
    {
        status = HAL_DAC_Start_DMA(&hdac1,
                                   DAC_CHANNEL_1,
                                   dac_wave_buffer,
                                   DAC_WAVE_BUFFER_LENGTH,
                                   DAC_ALIGN_12B_R);
        if (status != HAL_OK)
        {
            return status;
        }

        status = HAL_TIM_Base_Start(&htim4);
        if (status != HAL_OK)
        {
            return status;
        }

        dac_wave_dma_started = 1U;
    }

    return HAL_OK;
}

HAL_StatusTypeDef DAC_Waveform_Start(dac_wave_type_t type, float frequency_hz, float vpp, float offset_v)
{
    return DAC_Waveform_Apply(type, frequency_hz, vpp, offset_v);
}

HAL_StatusTypeDef DAC_Waveform_Stop(void)
{
    HAL_StatusTypeDef dac_status;
    HAL_StatusTypeDef tim_status;

    tim_status = HAL_TIM_Base_Stop(&htim4);
    dac_status = HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
    dac_wave_dma_started = 0U;

    return (dac_status == HAL_OK) ? tim_status : dac_status;
}

const dac_wave_config_t *DAC_Waveform_GetConfig(void)
{
    return &dac_wave_config;
}

const uint32_t *DAC_Waveform_GetBuffer(void)
{
    return dac_wave_buffer;
}

uint32_t DAC_Waveform_GetBufferLength(void)
{
    return DAC_WAVE_BUFFER_LENGTH;
}
