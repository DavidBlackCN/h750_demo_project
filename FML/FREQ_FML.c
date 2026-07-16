#include "FREQ_FML.h"

#include "tim.h"
#include <math.h>

#define FREQ_MIN_HZ                       (1.0f)
#define FREQ_MAX_HZ                       (1000000.0f)
#define FREQ_MAX_ACCEPT_HZ                (1010000.0f)
#define FREQ_DISPLAY_HIGH_THRESHOLD_HZ    (10000.0f)
#define FREQ_HIGH_MODE_ENTER_HZ            (1000.0f)
#define FREQ_HIGH_MODE_EXIT_HZ              (700.0f)
#define FREQ_LOW_WINDOW_MS                  (200U)
#define FREQ_HIGH_WINDOW_MS                 (200U)
#define FREQ_PROBE_WINDOW_MS                 (10U)
#define FREQ_HIGH_TIMEOUT_MS                (600U)
#define FREQ_NO_SIGNAL_TIMEOUT_MS          (2200U)
#define FREQ_DMA_CAPTURE_DIV                  (8U)
#define FREQ_DMA_PROBE_LENGTH               (128U)
#define FREQ_DMA_MAX_LENGTH               (25024U)

typedef enum
{
    FREQ_STATE_IDLE = 0,
    FREQ_STATE_PROBE,
    FREQ_STATE_LOW,
    FREQ_STATE_HIGH,
    FREQ_STATE_ERROR
} freq_state_t;

static uint32_t s_dma_capture[FREQ_DMA_MAX_LENGTH]
    __attribute__((section(".dma_buffer"), aligned(32)));

static volatile freq_state_t s_state = FREQ_STATE_IDLE;
static volatile uint8_t s_dma_complete = 0U;
static volatile uint8_t s_dma_error = 0U;
static volatile uint8_t s_request_probe = 0U;
static volatile uint8_t s_low_have_first = 0U;
static volatile uint8_t s_low_result_pending = 0U;
static volatile uint32_t s_low_first_capture = 0U;
static volatile uint32_t s_low_periods = 0U;
static volatile uint32_t s_low_result_ticks = 0U;
static volatile uint32_t s_low_result_periods = 0U;
static volatile uint32_t s_last_edge_ms = 0U;

static uint32_t s_timer_clock_hz = 0U;
static uint32_t s_state_started_ms = 0U;
static uint16_t s_dma_length = 0U;
static freq_result_t s_result;

static uint32_t freq_get_timer_clock_hz(void)
{
    RCC_ClkInitTypeDef clock_config = {0};
    uint32_t flash_latency = 0U;
    uint32_t timer_clock = HAL_RCC_GetPCLK1Freq();

    HAL_RCC_GetClockConfig(&clock_config, &flash_latency);
    if (clock_config.APB1CLKDivider != RCC_APB1_DIV1)
    {
        timer_clock *= 2U;
    }

    return timer_clock;
}

static void freq_dma_prepare(uint16_t length)
{
    uint32_t bytes = ((uint32_t)length * sizeof(s_dma_capture[0]) + 31U) & ~31U;

    SCB_CleanInvalidateDCache_by_Addr(s_dma_capture, (int32_t)bytes);
    __DSB();
}

static void freq_dma_finish(uint16_t length)
{
    uint32_t bytes = ((uint32_t)length * sizeof(s_dma_capture[0]) + 31U) & ~31U;

    SCB_InvalidateDCache_by_Addr(s_dma_capture, (int32_t)bytes);
    __DSB();
}

static void freq_set_capture_prescaler(uint32_t prescaler)
{
    MODIFY_REG(htim2.Instance->CCMR1, TIM_CCMR1_IC1PSC, prescaler);
}

static void freq_set_error(void)
{
    s_state = FREQ_STATE_ERROR;
    s_result.status = FREQ_STATUS_ERROR;
}

static void freq_publish(float raw_hz, uint32_t ticks, uint32_t periods,
                         freq_mode_t mode)
{
    float display_source_hz = raw_hz;

    s_result.raw_hz = raw_hz;
    s_result.period_ticks = ticks;
    s_result.measured_periods = periods;
    s_result.mode = mode;

    if (raw_hz < FREQ_MIN_HZ)
    {
        s_result.display_hz = 0.0f;
        s_result.status = FREQ_STATUS_BELOW_RANGE;
    }
    else if (raw_hz > FREQ_MAX_ACCEPT_HZ)
    {
        s_result.display_hz = FREQ_MAX_HZ;
        s_result.status = FREQ_STATUS_ABOVE_RANGE;
    }
    else
    {
        if (display_source_hz > FREQ_MAX_HZ)
        {
            display_source_hz = FREQ_MAX_HZ;
        }

        if (display_source_hz < FREQ_DISPLAY_HIGH_THRESHOLD_HZ)
        {
            s_result.display_hz = roundf(display_source_hz * 2.0f) * 0.5f;
        }
        else
        {
            s_result.display_hz = roundf(display_source_hz / 10.0f) * 10.0f;
        }
        s_result.status = FREQ_STATUS_VALID;
    }

    s_result.update_count++;
}

static float freq_calculate_dma(uint16_t count, uint32_t *ticks_out,
                                uint32_t *periods_out)
{
    uint32_t ticks = s_dma_capture[count - 1U] - s_dma_capture[0];
    uint32_t periods = ((uint32_t)count - 1U) * FREQ_DMA_CAPTURE_DIV;

    *ticks_out = ticks;
    *periods_out = periods;
    if (ticks == 0U)
    {
        return 0.0f;
    }

    return (((float)s_timer_clock_hz * (float)periods) / (float)ticks) *
           FREQ_CALIBRATION_FACTOR;
}

static HAL_StatusTypeDef freq_start_dma(freq_state_t state, uint16_t length)
{
    HAL_NVIC_DisableIRQ(TIM2_IRQn);
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_CC1 | TIM_IT_UPDATE | TIM_IT_TRIGGER);
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);

    s_dma_complete = 0U;
    s_dma_error = 0U;
    s_dma_length = length;
    s_state_started_ms = HAL_GetTick();
    s_state = state;
    s_result.mode = (state == FREQ_STATE_PROBE) ? FREQ_MODE_PROBE :
                                                  FREQ_MODE_DMA_CAPTURE;
    if (state == FREQ_STATE_PROBE)
    {
        s_result.status = FREQ_STATUS_MEASURING;
    }

    freq_set_capture_prescaler(TIM_ICPSC_DIV8);
    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_CC1 | TIM_FLAG_CC1OF);
    freq_dma_prepare(length);

    if (HAL_TIM_IC_Start_DMA(&htim2, TIM_CHANNEL_1, s_dma_capture, length) != HAL_OK)
    {
        freq_set_error();
        return HAL_ERROR;
    }

    /* High-rate capture is polled through NDTR.  Keeping the DMA IRQ disabled
       avoids HAL completion/abort/restart races while the input is swept. */
    __HAL_DMA_DISABLE_IT(htim2.hdma[TIM_DMA_ID_CC1],
                         DMA_IT_TC | DMA_IT_HT | DMA_IT_TE | DMA_IT_DME);
    __HAL_DMA_DISABLE_IT(htim2.hdma[TIM_DMA_ID_CC1], DMA_IT_FE);
    HAL_NVIC_DisableIRQ(DMA1_Stream5_IRQn);
    HAL_NVIC_ClearPendingIRQ(DMA1_Stream5_IRQn);

    return HAL_OK;
}

static HAL_StatusTypeDef freq_start_probe(void)
{
    return freq_start_dma(FREQ_STATE_PROBE, FREQ_DMA_PROBE_LENGTH);
}

static HAL_StatusTypeDef freq_start_high(float estimated_hz)
{
    float captures = estimated_hz * ((float)FREQ_HIGH_WINDOW_MS / 1000.0f) /
                     (float)FREQ_DMA_CAPTURE_DIV;
    uint32_t length = (uint32_t)(captures + 1.5f);

    if (length < 2U)
    {
        length = 2U;
    }
    if (length > FREQ_DMA_MAX_LENGTH)
    {
        length = FREQ_DMA_MAX_LENGTH;
    }

    return freq_start_dma(FREQ_STATE_HIGH, (uint16_t)length);
}

static HAL_StatusTypeDef freq_start_low(void)
{
    s_low_have_first = 0U;
    s_low_periods = 0U;
    s_low_result_pending = 0U;
    s_request_probe = 0U;
    s_last_edge_ms = HAL_GetTick();
    s_state_started_ms = s_last_edge_ms;
    s_state = FREQ_STATE_LOW;
    s_result.mode = FREQ_MODE_INPUT_CAPTURE;
    s_result.status = FREQ_STATUS_MEASURING;

    freq_set_capture_prescaler(TIM_ICPSC_DIV1);
    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_CC1 | TIM_FLAG_CC1OF);
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1) != HAL_OK)
    {
        freq_set_error();
        return HAL_ERROR;
    }

    return HAL_OK;
}

static uint16_t freq_stop_dma_and_get_count(void)
{
    uint32_t remaining = __HAL_DMA_GET_COUNTER(htim2.hdma[TIM_DMA_ID_CC1]);
    uint16_t count = (remaining <= s_dma_length) ?
                     (uint16_t)(s_dma_length - remaining) : 0U;

    HAL_NVIC_DisableIRQ(DMA1_Stream5_IRQn);
    HAL_NVIC_ClearPendingIRQ(DMA1_Stream5_IRQn);
    (void)HAL_DMA_Abort(htim2.hdma[TIM_DMA_ID_CC1]);
    (void)HAL_TIM_IC_Stop_DMA(&htim2, TIM_CHANNEL_1);
    freq_dma_finish(s_dma_length);
    return count;
}

HAL_StatusTypeDef FREQ_FML_Init(void)
{
    s_timer_clock_hz = freq_get_timer_clock_hz();
    s_result.raw_hz = 0.0f;
    s_result.display_hz = 0.0f;
    s_result.period_ticks = 0U;
    s_result.measured_periods = 0U;
    s_result.update_count = 0U;
    s_result.status = FREQ_STATUS_MEASURING;
    s_result.mode = FREQ_MODE_PROBE;

    if ((s_timer_clock_hz == 0U) || (htim2.hdma[TIM_DMA_ID_CC1] == NULL))
    {
        freq_set_error();
        return HAL_ERROR;
    }

    return freq_start_probe();
}

void FREQ_FML_Process(void)
{
    uint32_t now = HAL_GetTick();

    if (s_dma_error != 0U)
    {
        (void)HAL_TIM_IC_Stop_DMA(&htim2, TIM_CHANNEL_1);
        freq_set_error();
        s_dma_error = 0U;
        return;
    }

    if (s_state == FREQ_STATE_PROBE)
    {
        if ((s_dma_complete != 0U) ||
            (__HAL_DMA_GET_COUNTER(htim2.hdma[TIM_DMA_ID_CC1]) == 0U) ||
            ((now - s_state_started_ms) >= FREQ_PROBE_WINDOW_MS))
        {
            uint16_t count = freq_stop_dma_and_get_count();

            if (count >= 2U)
            {
                uint32_t ticks;
                uint32_t periods;
                float frequency = freq_calculate_dma(count, &ticks, &periods);

                if (frequency >= FREQ_HIGH_MODE_ENTER_HZ)
                {
                    (void)freq_start_high(frequency);
                }
                else
                {
                    (void)freq_start_low();
                }
            }
            else
            {
                (void)freq_start_low();
            }
        }
    }
    else if (s_state == FREQ_STATE_LOW)
    {
        if (s_request_probe != 0U)
        {
            (void)HAL_TIM_IC_Stop_IT(&htim2, TIM_CHANNEL_1);
            s_request_probe = 0U;
            (void)freq_start_probe();
        }
        else if (s_low_result_pending != 0U)
        {
            uint32_t ticks = s_low_result_ticks;
            uint32_t periods = s_low_result_periods;

            s_low_result_pending = 0U;
            if ((ticks != 0U) && (periods != 0U))
            {
                float frequency = (((float)s_timer_clock_hz * (float)periods) /
                                   (float)ticks) * FREQ_CALIBRATION_FACTOR;

                freq_publish(frequency, ticks, periods,
                             FREQ_MODE_INPUT_CAPTURE);
            }
        }
        else if ((now - s_last_edge_ms) >= FREQ_NO_SIGNAL_TIMEOUT_MS)
        {
            s_low_have_first = 0U;
            s_low_periods = 0U;
            s_low_result_pending = 0U;
            s_result.raw_hz = 0.0f;
            s_result.display_hz = 0.0f;
            s_result.period_ticks = 0U;
            s_result.measured_periods = 0U;
            s_result.status = FREQ_STATUS_NO_SIGNAL;
            s_result.mode = FREQ_MODE_INPUT_CAPTURE;
        }
    }
    else if (s_state == FREQ_STATE_HIGH)
    {
        if ((s_dma_complete != 0U) ||
            (__HAL_DMA_GET_COUNTER(htim2.hdma[TIM_DMA_ID_CC1]) == 0U))
        {
            uint16_t count = freq_stop_dma_and_get_count();

            if (count >= 2U)
            {
                uint32_t ticks;
                uint32_t periods;
                float frequency = freq_calculate_dma(count, &ticks, &periods);

                freq_publish(frequency, ticks, periods, FREQ_MODE_DMA_CAPTURE);
                s_last_edge_ms = now;
                if (frequency < FREQ_HIGH_MODE_EXIT_HZ)
                {
                    (void)freq_start_low();
                }
                else
                {
                    (void)freq_start_high(frequency);
                }
            }
            else
            {
                (void)freq_start_probe();
            }
        }
        else if ((now - s_state_started_ms) >= FREQ_HIGH_TIMEOUT_MS)
        {
            (void)freq_stop_dma_and_get_count();
            (void)freq_start_probe();
        }
    }
}

const freq_result_t *FREQ_FML_GetResult(void)
{
    return &s_result;
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if ((htim->Instance != TIM2) ||
        (htim->Channel != HAL_TIM_ACTIVE_CHANNEL_1))
    {
        return;
    }

    if ((s_state == FREQ_STATE_PROBE) || (s_state == FREQ_STATE_HIGH))
    {
        s_dma_complete = 1U;
        return;
    }

    if (s_state == FREQ_STATE_LOW)
    {
        uint32_t capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
        uint32_t now = HAL_GetTick();

        s_last_edge_ms = now;
        if (s_low_have_first == 0U)
        {
            s_low_first_capture = capture;
            s_low_periods = 0U;
            s_low_have_first = 1U;
        }
        else
        {
            uint32_t ticks = capture - s_low_first_capture;

            s_low_periods++;
            if ((s_low_periods == 1U) &&
                (ticks < (s_timer_clock_hz / (uint32_t)FREQ_HIGH_MODE_ENTER_HZ)))
            {
                __HAL_TIM_DISABLE_IT(htim, TIM_IT_CC1);
                s_request_probe = 1U;
            }
            else if (ticks >= ((s_timer_clock_hz / 1000U) * FREQ_LOW_WINDOW_MS))
            {
                s_low_result_ticks = ticks;
                s_low_result_periods = s_low_periods;
                s_low_result_pending = 1U;
                s_low_first_capture = capture;
                s_low_periods = 0U;
            }
        }
    }
}

void HAL_TIM_ErrorCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        s_dma_error = 1U;
    }
}
