#include "DAC_FML.h"

#include "arm_math.h"
#include "dac.h"
#include "tim.h"

#define DAC_WAVE_MAX_CODE       4095.0f
#define DAC_WAVE_TIMER_MAX_ARR  65535U
#define DAC_WAVE_TIMER_MAX_PSC  65535U
#define DAC_WAVE_TWO_PI         6.28318530717958647692f

typedef struct
{
    uint32_t prescaler;
    uint32_t period;
} dac_wave_timer_config_t;

/* Each waveform type owns one DMA source table. */
__attribute__((section(".dma_buffer"), aligned(32)))
static uint32_t dac_sine_buffer[DAC_WAVE_BUFFER_LENGTH];

__attribute__((section(".dma_buffer"), aligned(32)))
static uint32_t dac_square_buffer[DAC_WAVE_BUFFER_LENGTH];

__attribute__((section(".dma_buffer"), aligned(32)))
static uint32_t dac_triangle_buffer[DAC_WAVE_BUFFER_LENGTH];

__attribute__((section(".dma_buffer"), aligned(32)))
static uint32_t dac_dc_buffer[DAC_WAVE_BUFFER_LENGTH];

static uint8_t dac_wave_dma_started = 0U;
static uint8_t dac_wave_dma_error = 0U;
static uint32_t dac_wave_channel = DAC_CHANNEL_1;
static uint32_t *dac_wave_active_buffer = dac_sine_buffer;
static dac_wave_config_t dac_wave_config = {
    DAC_USER_WAVE_SINE,
    1000.0f,
    1.0f,
    DAC_WAVE_ZERO_AXIS_V,
    1000.0f * (float)DAC_WAVE_BUFFER_LENGTH,
    DAC_WAVE_BUFFER_LENGTH
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

static uint32_t *dac_wave_get_buffer(dac_wave_type_t type)
{
    switch (type)
    {
    case DAC_USER_WAVE_SINE:
        return dac_sine_buffer;

    case DAC_USER_WAVE_SQUARE:
        return dac_square_buffer;

    case DAC_USER_WAVE_TRIANGLE:
        return dac_triangle_buffer;

    case DAC_USER_WAVE_DC:
        return dac_dc_buffer;

    default:
        return NULL;
    }
}

static uint32_t dac_wave_select_sample_count(float frequency_hz)
{
    uint32_t sample_count = (uint32_t)(DAC_WAVE_MAX_UPDATE_RATE_HZ /
                                       frequency_hz);

    if (sample_count > DAC_WAVE_BUFFER_LENGTH)
    {
        sample_count = DAC_WAVE_BUFFER_LENGTH;
    }

    if (sample_count < DAC_WAVE_MIN_SAMPLES_PER_CYCLE)
    {
        return 0U;
    }

    return sample_count;
}

static float dac_wave_norm(dac_wave_type_t type,
                           uint32_t index,
                           uint32_t sample_count)
{
    float phase = (float)index / (float)sample_count;

    switch (type)
    {
    case DAC_USER_WAVE_SINE:
        return arm_sin_f32(DAC_WAVE_TWO_PI * phase);

    case DAC_USER_WAVE_SQUARE:
        return (index < (sample_count / 2U)) ? 1.0f : -1.0f;

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

    case DAC_USER_WAVE_DC:
    default:
        return 0.0f;
    }
}

static void dac_wave_fill_buffer(uint32_t *buffer,
                                 dac_wave_type_t type,
                                 float vpp,
                                 float offset_v,
                                 uint32_t sample_count)
{
    float amplitude_v = dac_clampf(vpp, 0.0f, DAC_WAVE_REF_VOLTAGE) * 0.5f;
    float center_v = dac_clampf(offset_v, 0.0f, DAC_WAVE_REF_VOLTAGE);

    for (uint32_t i = 0U; i < sample_count; i++)
    {
        float voltage = center_v + (amplitude_v * dac_wave_norm(type, i,
                                                                 sample_count));
        buffer[i] = dac_voltage_to_code(voltage);
    }

    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
    {
        SCB_CleanDCache_by_Addr(buffer, sample_count * sizeof(buffer[0]));
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

static HAL_StatusTypeDef dac_wave_calculate_timer_config(float sample_rate_hz,
                                                          dac_wave_timer_config_t *config)
{
    uint32_t timer_clock_hz;
    float timer_ticks;

    if ((sample_rate_hz <= 0.0f) || (config == NULL))
    {
        return HAL_ERROR;
    }

    timer_clock_hz = dac_wave_get_tim4_clock_hz();
    timer_ticks = (float)timer_clock_hz / sample_rate_hz;
    if (timer_ticks < 2.0f)
    {
        return HAL_ERROR;
    }

    config->prescaler = (uint32_t)((timer_ticks - 1.0f) /
                                   ((float)DAC_WAVE_TIMER_MAX_ARR + 1.0f));
    if (config->prescaler > DAC_WAVE_TIMER_MAX_PSC)
    {
        return HAL_ERROR;
    }

    config->period = (uint32_t)(((float)timer_clock_hz /
                                 (sample_rate_hz * (float)(config->prescaler + 1U))) +
                                0.5f);
    if (config->period < 2U)
    {
        config->period = 2U;
    }
    if (config->period > (DAC_WAVE_TIMER_MAX_ARR + 1U))
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static void dac_wave_apply_timer_config(const dac_wave_timer_config_t *config)
{
    __HAL_TIM_SET_PRESCALER(&htim4, config->prescaler);
    __HAL_TIM_SET_AUTORELOAD(&htim4, config->period - 1U);
    __HAL_TIM_SET_COUNTER(&htim4, 0U);
    htim4.Instance->EGR = TIM_EGR_UG;
}

void HAL_DAC_DMAUnderrunCallbackCh1(DAC_HandleTypeDef *hdac)
{
    if (hdac == &hdac1)
    {
        dac_wave_dma_error = 1U;
    }
}

void HAL_DACEx_DMAUnderrunCallbackCh2(DAC_HandleTypeDef *hdac)
{
    if (hdac == &hdac1)
    {
        dac_wave_dma_error = 1U;
    }
}

HAL_StatusTypeDef DAC_Waveform_StartChannel(uint32_t channel,
                                             dac_wave_type_t type,
                                             float frequency_hz,
                                             float vpp,
                                             float offset_v)
{
    HAL_StatusTypeDef status;
    dac_wave_timer_config_t timer_config;
    dac_wave_config_t requested_config;
    uint32_t *buffer;
    float effective_vpp;
    float effective_offset_v;
    uint32_t sample_count;

    if ((frequency_hz <= 0.0f) ||
        ((channel != DAC_CHANNEL_1) && (channel != DAC_CHANNEL_2)))
    {
        return HAL_ERROR;
    }

    buffer = dac_wave_get_buffer(type);
    if (buffer == NULL)
    {
        return HAL_ERROR;
    }

    sample_count = dac_wave_select_sample_count(frequency_hz);
    if (sample_count == 0U)
    {
        return HAL_ERROR;
    }

    if (dac_wave_dma_started != 0U)
    {
        if ((dac_wave_dma_error != 0U) ||
            ((HAL_DAC_GetError(&hdac1) &
              (HAL_DAC_ERROR_DMA | HAL_DAC_ERROR_DMAUNDERRUNCH1 |
               HAL_DAC_ERROR_DMAUNDERRUNCH2)) != 0U))
        {
            return HAL_ERROR;
        }

        return HAL_BUSY;
    }

    effective_vpp = dac_clampf(vpp, 0.0f, DAC_WAVE_REF_VOLTAGE);
    effective_offset_v = dac_clampf(offset_v, 0.0f, DAC_WAVE_REF_VOLTAGE);
    requested_config.type = type;
    requested_config.frequency_hz = frequency_hz;
    requested_config.vpp = effective_vpp;
    requested_config.offset_v = effective_offset_v;
    requested_config.sample_rate_hz = frequency_hz * (float)sample_count;
    requested_config.samples_per_cycle = sample_count;

    status = dac_wave_calculate_timer_config(requested_config.sample_rate_hz, &timer_config);
    if (status != HAL_OK)
    {
        return status;
    }

    dac_wave_fill_buffer(buffer, type, effective_vpp, effective_offset_v,
                          sample_count);
    dac_wave_apply_timer_config(&timer_config);
    hdac1.ErrorCode &= ~(HAL_DAC_ERROR_DMA | HAL_DAC_ERROR_DMAUNDERRUNCH1 |
                         HAL_DAC_ERROR_DMAUNDERRUNCH2);

    status = HAL_DAC_Start_DMA(&hdac1,
                               channel,
                               buffer,
                               sample_count,
                               DAC_ALIGN_12B_R);
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_TIM_Base_Start(&htim4);
    if (status != HAL_OK)
    {
        (void)HAL_DAC_Stop_DMA(&hdac1, channel);
        return status;
    }

    dac_wave_config = requested_config;
    dac_wave_active_buffer = buffer;
    dac_wave_channel = channel;
    dac_wave_dma_error = 0U;
    dac_wave_dma_started = 1U;

    return HAL_OK;
}

HAL_StatusTypeDef DAC_Waveform_Apply(dac_wave_type_t type,
                                     float frequency_hz,
                                     float vpp,
                                     float offset_v)
{
    return DAC_Waveform_StartChannel(DAC_CHANNEL_1, type, frequency_hz, vpp, offset_v);
}

HAL_StatusTypeDef DAC_Waveform_Start(dac_wave_type_t type,
                                     float frequency_hz,
                                     float vpp,
                                     float offset_v)
{
    return DAC_Waveform_StartChannel(DAC_CHANNEL_1, type, frequency_hz, vpp, offset_v);
}

HAL_StatusTypeDef DAC_Waveform_Stop(void)
{
    HAL_StatusTypeDef dac_status;
    HAL_StatusTypeDef tim_status;

    tim_status = HAL_TIM_Base_Stop(&htim4);
    dac_status = HAL_DAC_Stop_DMA(&hdac1, dac_wave_channel);
    dac_wave_dma_started = 0U;
    dac_wave_dma_error = 0U;

    return (dac_status == HAL_OK) ? tim_status : dac_status;
}

const dac_wave_config_t *DAC_Waveform_GetConfig(void)
{
    return &dac_wave_config;
}

const uint32_t *DAC_Waveform_GetBuffer(void)
{
    return dac_wave_active_buffer;
}

uint32_t DAC_Waveform_GetBufferLength(void)
{
    return dac_wave_config.samples_per_cycle;
}
