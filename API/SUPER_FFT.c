#include "SUPER_FFT.h"

#include "ADC_VOFA_FML.h"
#include "FFT_FML.h"
#include "adc.h"
#include <math.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

#define SUPER_FFT_LENGTH                 4096U
#define SUPER_FFT_HIGH_RATE_HZ          400000U
#define SUPER_FFT_LOW_RATE_HZ            40000U
#define SUPER_FFT_LOW_MIN_HZ                10U
#define SUPER_FFT_LOW_MAX_HZ             12000U
#define SUPER_FFT_HIGH_MIN_HZ               10U
#define SUPER_FFT_HIGH_MAX_HZ           100000U
#define SUPER_FFT_HIGH_LOW_HANDOFF_HZ    12000U
#define SUPER_FFT_HIGH_BLOCK_COUNT           4U
#define SUPER_FFT_LOW_BLOCK_COUNT           10U
#define SUPER_FFT_FINE_SPAN_HZ               40U
#define SUPER_FFT_FINE_CANDIDATE_COUNT ((SUPER_FFT_FINE_SPAN_HZ * 2U) + 1U)

typedef enum
{
    SUPER_FFT_STATE_IDLE = 0,
    SUPER_FFT_STATE_HIGH_COARSE,
    SUPER_FFT_STATE_LOW_COARSE,
    SUPER_FFT_STATE_FINE,
    SUPER_FFT_STATE_READY,
    SUPER_FFT_STATE_ERROR
} super_fft_state_t;

/* DMA must use D2 SRAM and a cache-line aligned range on Cortex-M7. */
static uint16_t s_adc3_dma_buffer[SUPER_FFT_LENGTH]
    __attribute__((section(".dma_buffer"), aligned(32)));
static float s_centered_data[SUPER_FFT_LENGTH];
static float s_power[SUPER_FFT_LENGTH / 2U];
static float s_fine_power[SUPER_FFT_FINE_CANDIDATE_COUNT];
static TIM_HandleTypeDef s_htim6;

static volatile uint8_t s_dma_frame_ready;
static volatile uint8_t s_dma_error;
static uint8_t s_window_ready;
static uint32_t s_block_count;
static uint32_t s_fine_start_hz;
static uint32_t s_fine_candidate_count;
static float s_sample_rate_hz;
static float s_measured_frequency_hz;
static super_fft_state_t s_state = SUPER_FFT_STATE_IDLE;

static uint32_t super_fft_tim6_clock_hz(void)
{
    RCC_ClkInitTypeDef clocks = {0};
    uint32_t flash_latency = 0U;
    uint32_t timer_clock_hz = HAL_RCC_GetPCLK1Freq();

    HAL_RCC_GetClockConfig(&clocks, &flash_latency);
    if (clocks.APB1CLKDivider != RCC_APB1_DIV1)
    {
        timer_clock_hz *= 2U;
    }

    return timer_clock_hz;
}

static HAL_StatusTypeDef super_fft_configure_timer(uint32_t target_rate_hz)
{
    TIM_MasterConfigTypeDef master = {0};
    uint32_t timer_clock_hz;
    uint32_t divider;

    if (target_rate_hz == 0U)
    {
        return HAL_ERROR;
    }

    timer_clock_hz = super_fft_tim6_clock_hz();
    divider = (timer_clock_hz + (target_rate_hz / 2U)) / target_rate_hz;
    if ((divider == 0U) || (divider > 65536U))
    {
        return HAL_ERROR;
    }

    __HAL_RCC_TIM6_CLK_ENABLE();
    s_htim6.Instance = TIM6;
    s_htim6.Init.Prescaler = 0U;
    s_htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_htim6.Init.Period = divider - 1U;
    s_htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&s_htim6) != HAL_OK)
    {
        return HAL_ERROR;
    }

    master.MasterOutputTrigger = TIM_TRGO_UPDATE;
    master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&s_htim6, &master) != HAL_OK)
    {
        return HAL_ERROR;
    }

    __HAL_TIM_SET_COUNTER(&s_htim6, 0U);
    s_htim6.Instance->EGR = TIM_EGR_UG;
    s_sample_rate_hz = (float)timer_clock_hz / (float)divider;
    return HAL_OK;
}

static void super_fft_prepare_centered_data(void)
{
    uint64_t sum = 0U;

    for (uint32_t i = 0U; i < SUPER_FFT_LENGTH; ++i)
    {
        sum += s_adc3_dma_buffer[i] & 0x0FFFU;
    }

    const float mean = (float)sum / (float)SUPER_FFT_LENGTH;
    for (uint32_t i = 0U; i < SUPER_FFT_LENGTH; ++i)
    {
        s_centered_data[i] = (float)(s_adc3_dma_buffer[i] & 0x0FFFU) - mean;
    }
}

static uint32_t super_fft_first_bin(uint32_t minimum_hz)
{
    uint32_t first = (uint32_t)((minimum_hz * (float)SUPER_FFT_LENGTH) / s_sample_rate_hz);
    return (first < DC_INDEX) ? DC_INDEX : first;
}

static uint32_t super_fft_last_bin(uint32_t maximum_hz)
{
    uint32_t last = (uint32_t)((maximum_hz * (float)SUPER_FFT_LENGTH) / s_sample_rate_hz) + 1U;
    const uint32_t half = SUPER_FFT_LENGTH / 2U;
    return (last >= half) ? (half - 1U) : last;
}

static void super_fft_accumulate_fft_power(uint32_t first, uint32_t last)
{
    calculate_fft_fml(s_centered_data, fft_input_buffer, fft_magnitude, fft_out,
                      SUPER_FFT_LENGTH, SUPER_FFT_LENGTH);

    for (uint32_t bin = first; bin <= last; ++bin)
    {
        const float re = fft_input_buffer[2U * bin];
        const float im = fft_input_buffer[(2U * bin) + 1U];
        s_power[bin] += (re * re) + (im * im);
    }
}

static float super_fft_peak_frequency(uint32_t first, uint32_t last)
{
    uint32_t best_bin = first;
    float best_power = s_power[first];

    for (uint32_t bin = first + 1U; bin <= last; ++bin)
    {
        if (s_power[bin] > best_power)
        {
            best_power = s_power[bin];
            best_bin = bin;
        }
    }

    float peak_bin = (float)best_bin;
    if ((best_bin > first) && (best_bin < last))
    {
        const float p0 = s_power[best_bin - 1U];
        const float p2 = s_power[best_bin + 1U];
        const float denominator = p0 - (2.0f * best_power) + p2;

        if (fabsf(denominator) > 1.0e-12f)
        {
            float offset = 0.5f * (p0 - p2) / denominator;
            if (offset > 0.5f)
                offset = 0.5f;
            else if (offset < -0.5f)
                offset = -0.5f;
            peak_bin += offset;
        }
    }

    return peak_bin * s_sample_rate_hz / (float)SUPER_FFT_LENGTH;
}

static void super_fft_accumulate_fine_power(void)
{
    for (uint32_t candidate = 0U; candidate < s_fine_candidate_count; ++candidate)
    {
        const float frequency_hz = (float)(s_fine_start_hz + candidate);
        const float omega = -2.0f * PI * frequency_hz / s_sample_rate_hz;
        const float step_re = cosf(omega);
        const float step_im = sinf(omega);
        float ph_re = 1.0f;
        float ph_im = 0.0f;
        float acc_re = 0.0f;
        float acc_im = 0.0f;

        for (uint32_t i = 0U; i < SUPER_FFT_LENGTH; ++i)
        {
            const float next_re = (ph_re * step_re) - (ph_im * step_im);
            acc_re += s_centered_data[i] * ph_re;
            acc_im += s_centered_data[i] * ph_im;
            ph_im = (ph_re * step_im) + (ph_im * step_re);
            ph_re = next_re;
        }

        /* One-shot DMA frames have processing gaps; accumulate each frame's
           energy, rather than summing complex phases across those gaps. */
        s_fine_power[candidate] += (acc_re * acc_re) + (acc_im * acc_im);
    }
}

static void super_fft_finish_fine_stage(void)
{
    uint32_t best = 0U;
    float best_power = s_fine_power[0];

    for (uint32_t candidate = 1U; candidate < s_fine_candidate_count; ++candidate)
    {
        if (s_fine_power[candidate] > best_power)
        {
            best_power = s_fine_power[candidate];
            best = candidate;
        }
    }

    s_measured_frequency_hz = (float)(s_fine_start_hz + best);
    if ((best > 0U) && ((best + 1U) < s_fine_candidate_count))
    {
        const float p0 = s_fine_power[best - 1U];
        const float p2 = s_fine_power[best + 1U];
        const float denominator = p0 - (2.0f * best_power) + p2;
        if (fabsf(denominator) > 1.0e-12f)
        {
            float offset = 0.5f * (p0 - p2) / denominator;
            if (offset > 0.5f)
                offset = 0.5f;
            else if (offset < -0.5f)
                offset = -0.5f;
            s_measured_frequency_hz += offset;
        }
    }

    s_state = SUPER_FFT_STATE_READY;
}

static HAL_StatusTypeDef super_fft_start_frame(uint32_t target_rate_hz)
{
    if (super_fft_configure_timer(target_rate_hz) != HAL_OK)
    {
        return HAL_ERROR;
    }

    s_dma_frame_ready = 0U;
    s_dma_error = 0U;
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)s_adc3_dma_buffer,
                                      sizeof(s_adc3_dma_buffer));
    if (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)s_adc3_dma_buffer,
                          SUPER_FFT_LENGTH) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (HAL_TIM_Base_Start(&s_htim6) != HAL_OK)
    {
        (void)HAL_ADC_Stop_DMA(&hadc3);
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef super_fft_start_low_stage(void)
{
    memset(s_power, 0, sizeof(s_power));
    memset(s_fine_power, 0, sizeof(s_fine_power));
    s_block_count = 0U;
    s_fine_start_hz = 0U;
    s_fine_candidate_count = 0U;
    s_state = SUPER_FFT_STATE_LOW_COARSE;
    return super_fft_start_frame(SUPER_FFT_LOW_RATE_HZ);
}

HAL_StatusTypeDef SUPER_FFT_Start(void)
{
    if ((hadc3.Instance != ADC3) || ADC_VOFA_FML_IsActive())
    {
        return HAL_ERROR;
    }

    if (s_state != SUPER_FFT_STATE_IDLE && s_state != SUPER_FFT_STATE_READY &&
        s_state != SUPER_FFT_STATE_ERROR)
    {
        return HAL_BUSY;
    }

    if (s_window_ready == 0U)
    {
        generate_hanning_window();
        s_window_ready = 1U;
    }

    memset(s_power, 0, sizeof(s_power));
    memset(s_fine_power, 0, sizeof(s_fine_power));
    s_block_count = 0U;
    s_measured_frequency_hz = 0.0f;
    s_state = SUPER_FFT_STATE_HIGH_COARSE;

    if (super_fft_start_frame(SUPER_FFT_HIGH_RATE_HZ) != HAL_OK)
    {
        s_state = SUPER_FFT_STATE_ERROR;
        return HAL_ERROR;
    }

    return HAL_OK;
}

void SUPER_FFT_Process(void)
{
    uint32_t first;
    uint32_t last;

    if ((s_state != SUPER_FFT_STATE_HIGH_COARSE) &&
        (s_state != SUPER_FFT_STATE_LOW_COARSE) &&
        (s_state != SUPER_FFT_STATE_FINE))
    {
        return;
    }

    if (s_dma_error != 0U)
    {
        SUPER_FFT_Stop();
        s_state = SUPER_FFT_STATE_ERROR;
        return;
    }
    if (s_dma_frame_ready == 0U)
    {
        return;
    }

    s_dma_frame_ready = 0U;
    (void)HAL_ADC_Stop_DMA(&hadc3);
    SCB_InvalidateDCache_by_Addr((uint32_t *)s_adc3_dma_buffer,
                                 sizeof(s_adc3_dma_buffer));
    super_fft_prepare_centered_data();

    if (s_state == SUPER_FFT_STATE_HIGH_COARSE)
    {
        /* Search the complete band first. A low-frequency signal must be
           detected here before the state machine can enter its 40 kS/s
           coarse/fine measurement path. */
        first = super_fft_first_bin(SUPER_FFT_HIGH_MIN_HZ);
        last = super_fft_last_bin(SUPER_FFT_HIGH_MAX_HZ);
        super_fft_accumulate_fft_power(first, last);
        ++s_block_count;
        if (s_block_count >= SUPER_FFT_HIGH_BLOCK_COUNT)
        {
            s_measured_frequency_hz = super_fft_peak_frequency(first, last);
            if (s_measured_frequency_hz >= SUPER_FFT_HIGH_LOW_HANDOFF_HZ)
            {
                s_state = SUPER_FFT_STATE_READY;
                return;
            }
            if (super_fft_start_low_stage() != HAL_OK)
            {
                s_state = SUPER_FFT_STATE_ERROR;
            }
            return;
        }
        if (super_fft_start_frame(SUPER_FFT_HIGH_RATE_HZ) != HAL_OK)
        {
            s_state = SUPER_FFT_STATE_ERROR;
        }
        return;
    }

    if (s_state == SUPER_FFT_STATE_LOW_COARSE)
    {
        first = super_fft_first_bin(SUPER_FFT_LOW_MIN_HZ);
        last = super_fft_last_bin(SUPER_FFT_LOW_MAX_HZ);
        super_fft_accumulate_fft_power(first, last);
        ++s_block_count;
        if (s_block_count >= SUPER_FFT_LOW_BLOCK_COUNT)
        {
            const float coarse_hz = super_fft_peak_frequency(first, last);
            uint32_t centre_hz = (uint32_t)(coarse_hz + 0.5f);
            uint32_t end_hz;

            if (centre_hz < SUPER_FFT_LOW_MIN_HZ)
                centre_hz = SUPER_FFT_LOW_MIN_HZ;
            else if (centre_hz > SUPER_FFT_LOW_MAX_HZ)
                centre_hz = SUPER_FFT_LOW_MAX_HZ;
            s_fine_start_hz = (centre_hz > SUPER_FFT_FINE_SPAN_HZ) ?
                              (centre_hz - SUPER_FFT_FINE_SPAN_HZ) : SUPER_FFT_LOW_MIN_HZ;
            if (s_fine_start_hz < SUPER_FFT_LOW_MIN_HZ)
                s_fine_start_hz = SUPER_FFT_LOW_MIN_HZ;
            end_hz = centre_hz + SUPER_FFT_FINE_SPAN_HZ;
            if (end_hz > SUPER_FFT_LOW_MAX_HZ)
                end_hz = SUPER_FFT_LOW_MAX_HZ;
            s_fine_candidate_count = end_hz - s_fine_start_hz + 1U;
            memset(s_fine_power, 0, sizeof(s_fine_power));
            s_block_count = 0U;
            s_state = SUPER_FFT_STATE_FINE;
        }
        if (super_fft_start_frame(SUPER_FFT_LOW_RATE_HZ) != HAL_OK)
        {
            s_state = SUPER_FFT_STATE_ERROR;
        }
        return;
    }

    super_fft_accumulate_fine_power();
    ++s_block_count;
    if (s_block_count >= SUPER_FFT_LOW_BLOCK_COUNT)
    {
        super_fft_finish_fine_stage();
        return;
    }
    if (super_fft_start_frame(SUPER_FFT_LOW_RATE_HZ) != HAL_OK)
    {
        s_state = SUPER_FFT_STATE_ERROR;
    }
}

void SUPER_FFT_Stop(void)
{
    (void)HAL_TIM_Base_Stop(&s_htim6);
    (void)HAL_ADC_Stop_DMA(&hadc3);
    s_dma_frame_ready = 0U;
    s_state = SUPER_FFT_STATE_IDLE;
}

bool SUPER_FFT_IsActive(void)
{
    return (s_state == SUPER_FFT_STATE_HIGH_COARSE) ||
           (s_state == SUPER_FFT_STATE_LOW_COARSE) ||
           (s_state == SUPER_FFT_STATE_FINE);
}

bool SUPER_FFT_IsReady(void)
{
    return (s_state == SUPER_FFT_STATE_READY);
}

float SUPER_FFT_GetFrequencyHz(void)
{
    return s_measured_frequency_hz;
}

float SUPER_FFT_GetSampleRateHz(void)
{
    return s_sample_rate_hz;
}

void SUPER_FFT_OnAdcComplete(ADC_HandleTypeDef *hadc)
{
    if ((hadc->Instance == ADC3) && SUPER_FFT_IsActive())
    {
        /* ISR responsibility is deliberately limited to publishing completion. */
        __HAL_TIM_DISABLE(&s_htim6);
        s_dma_frame_ready = 1U;
    }
}

void SUPER_FFT_OnAdcError(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC3)
    {
        __HAL_TIM_DISABLE(&s_htim6);
        s_dma_error = 1U;
    }
}
