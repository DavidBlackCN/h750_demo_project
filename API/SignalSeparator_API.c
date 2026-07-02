#include "SignalSeparator_API.h"

#include "ADC_BLL.h"
#include "ADC_FML.h"
#include "AD9833.h"
#include "FFT_FML.h"
#include "USART_BLL.h"
#include "USART_FML.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include <math.h>
#include <stdio.h>

#define SIGNAL_SEPARATOR_KEY_DEBOUNCE_MS 30U
#define SIGNAL_SEPARATOR_GRID_HZ         10000.0f
#define SIGNAL_SEPARATOR_MIN_FREQ_HZ     5000.0f
#define SIGNAL_SEPARATOR_MAX_FREQ_HZ     120000.0f
#define SIGNAL_SEPARATOR_PEAK_GUARD_BINS 3U
#define SIGNAL_SEPARATOR_HARMONIC_SPAN   1U
#define SIGNAL_SEPARATOR_INVALID_RATIO   (-1.0f)
#define SIGNAL_SEPARATOR_SECOND_MIN_RATIO 0.35f
#define SIGNAL_SEPARATOR_TRI_3RD_RATIO   0.085f
#define SIGNAL_SEPARATOR_TRI_5TH_RATIO   0.026f
#define SIGNAL_SEPARATOR_TRI_7TH_RATIO   0.014f
#define SIGNAL_SEPARATOR_TRI_SINGLE_3RD  0.08f

typedef enum
{
    SIGNAL_SEPARATOR_STATE_IDLE = 0,
    SIGNAL_SEPARATOR_STATE_CAPTURE,
    SIGNAL_SEPARATOR_STATE_OUTPUT_A,
    SIGNAL_SEPARATOR_STATE_OUTPUT_B,
    SIGNAL_SEPARATOR_STATE_ERROR
} signal_separator_state_t;

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    GPIO_PinState raw_state;
    GPIO_PinState stable_state;
    uint32_t last_change_tick;
} signal_separator_button_t;

static signal_separator_button_t auto_key = {
    AUTO_KEY_GPIO_Port,
    AUTO_KEY_Pin,
    GPIO_PIN_SET,
    GPIO_PIN_SET,
    0U
};

static signal_separator_button_t switch_key = {
    SWITCH_KEY_GPIO_Port,
    SWITCH_KEY_Pin,
    GPIO_PIN_SET,
    GPIO_PIN_SET,
    0U
};

static signal_separator_state_t separator_state = SIGNAL_SEPARATOR_STATE_IDLE;
static signal_separator_result_t signal_a;
static signal_separator_result_t signal_b;
static float separator_fft_input[FFT_LENGTH];
static uint8_t separator_detected = 0U;

static const char *wave_to_text(signal_separator_wave_t wave)
{
    return (wave == SIGNAL_SEPARATOR_WAVE_TRIANGLE) ? "triangle" : "sine";
}

static unsigned short wave_to_ad9833_type(signal_separator_wave_t wave)
{
    return (wave == SIGNAL_SEPARATOR_WAVE_TRIANGLE) ? AD9833_OUT_TRIANGLE : AD9833_OUT_SINUS;
}

static uint8_t button_update(signal_separator_button_t *button)
{
    GPIO_PinState raw_state = HAL_GPIO_ReadPin(button->port, button->pin);
    uint32_t now = HAL_GetTick();

    if (raw_state != button->raw_state)
    {
        button->raw_state = raw_state;
        button->last_change_tick = now;
    }

    if ((now - button->last_change_tick) >= SIGNAL_SEPARATOR_KEY_DEBOUNCE_MS &&
        button->stable_state != button->raw_state)
    {
        button->stable_state = button->raw_state;
        if (button->stable_state == GPIO_PIN_RESET)
        {
            return 1U;
        }
    }

    return 0U;
}

static void button_init(signal_separator_button_t *button)
{
    button->raw_state = HAL_GPIO_ReadPin(button->port, button->pin);
    button->stable_state = button->raw_state;
    button->last_change_tick = HAL_GetTick();
}

static void make_centered_fft_input(void)
{
    float mean = 0.0f;

    for (uint32_t i = 0U; i < ADC1_FREQ_BLOCK_LENGTH; i++)
    {
        mean += adc1_data[i];
    }
    mean /= (float)ADC1_FREQ_BLOCK_LENGTH;

    for (uint32_t i = 0U; i < ADC1_FREQ_BLOCK_LENGTH; i++)
    {
        separator_fft_input[i] = adc1_data[i] - mean;
    }

    for (uint32_t i = ADC1_FREQ_BLOCK_LENGTH; i < FFT_LENGTH; i++)
    {
        separator_fft_input[i] = 0.0f;
    }
}

static float bin_to_hz(float bin)
{
    return bin * FFT_SAMPLE_RATE / (float)FFT_LENGTH;
}

static float hz_to_bin(float freq_hz)
{
    return freq_hz * (float)FFT_LENGTH / FFT_SAMPLE_RATE;
}

static uint32_t freq_to_nearest_bin(float freq_hz)
{
    float bin = hz_to_bin(freq_hz);

    if (bin < 0.0f)
    {
        return 0U;
    }

    return (uint32_t)(bin + 0.5f);
}

static float interpolate_peak_bin(uint32_t index)
{
    float y0;
    float y1;
    float y2;
    float denom;
    float delta;

    if (index <= DC_INDEX || index + 1U >= (FFT_LENGTH / 2U))
    {
        return (float)index;
    }

    y0 = fft_out[index - 1U];
    y1 = fft_out[index];
    y2 = fft_out[index + 1U];
    denom = y0 - (2.0f * y1) + y2;

    if (fabsf(denom) < 1.0e-12f)
    {
        return (float)index;
    }

    delta = 0.5f * (y0 - y2) / denom;
    if (delta > 0.5f)
    {
        delta = 0.5f;
    }
    else if (delta < -0.5f)
    {
        delta = -0.5f;
    }

    return (float)index + delta;
}

static float snap_to_grid(float freq_hz)
{
    uint32_t grid_index;

    if (freq_hz < SIGNAL_SEPARATOR_GRID_HZ)
    {
        return SIGNAL_SEPARATOR_GRID_HZ;
    }

    grid_index = (uint32_t)((freq_hz + (SIGNAL_SEPARATOR_GRID_HZ * 0.5f)) / SIGNAL_SEPARATOR_GRID_HZ);
    if (grid_index == 0U)
    {
        grid_index = 1U;
    }

    return (float)grid_index * SIGNAL_SEPARATOR_GRID_HZ;
}

static float local_peak_value(uint32_t center_bin, uint32_t span)
{
    uint32_t start;
    uint32_t end;
    float peak = 0.0f;

    if (center_bin >= (FFT_LENGTH / 2U))
    {
        return 0.0f;
    }

    start = (center_bin > span) ? (center_bin - span) : DC_INDEX;
    end = center_bin + span;
    if (end >= (FFT_LENGTH / 2U))
    {
        end = (FFT_LENGTH / 2U) - 1U;
    }

    for (uint32_t i = start; i <= end; i++)
    {
        if (fft_out[i] > peak)
        {
            peak = fft_out[i];
        }
    }

    return peak;
}

static float local_peak_magnitude(uint32_t center_bin, uint32_t span)
{
    uint32_t start;
    uint32_t end;
    float peak = 0.0f;

    if (center_bin >= (FFT_LENGTH / 2U))
    {
        return 0.0f;
    }

    start = (center_bin > span) ? (center_bin - span) : DC_INDEX;
    end = center_bin + span;
    if (end >= (FFT_LENGTH / 2U))
    {
        end = (FFT_LENGTH / 2U) - 1U;
    }

    for (uint32_t i = start; i <= end; i++)
    {
        if (fft_magnitude[i] > peak)
        {
            peak = fft_magnitude[i];
        }
    }

    return peak;
}

static uint8_t freq_close(float a_hz, float b_hz)
{
    float diff = fabsf(a_hz - b_hz);
    return (diff <= (SIGNAL_SEPARATOR_GRID_HZ * 0.5f)) ? 1U : 0U;
}

static float harmonic_ratio(float harmonic_freq_hz, float other_freq_hz, float fundamental_value)
{
    if (harmonic_freq_hz >= (FFT_SAMPLE_RATE * 0.5f))
    {
        return SIGNAL_SEPARATOR_INVALID_RATIO;
    }

    if (freq_close(harmonic_freq_hz, other_freq_hz))
    {
        return SIGNAL_SEPARATOR_INVALID_RATIO;
    }

    return local_peak_magnitude(freq_to_nearest_bin(harmonic_freq_hz), SIGNAL_SEPARATOR_HARMONIC_SPAN) / fundamental_value;
}

static signal_separator_wave_t classify_wave(float freq_hz, float other_freq_hz, float fundamental_value)
{
    float third_ratio;
    float fifth_ratio;
    float seventh_ratio;
    uint8_t valid_count = 0U;
    uint8_t evidence_count = 0U;

    if (fundamental_value <= 1.0e-6f)
    {
        return SIGNAL_SEPARATOR_WAVE_SINE;
    }

    third_ratio = harmonic_ratio(freq_hz * 3.0f, other_freq_hz, fundamental_value);
    fifth_ratio = harmonic_ratio(freq_hz * 5.0f, other_freq_hz, fundamental_value);
    seventh_ratio = harmonic_ratio(freq_hz * 7.0f, other_freq_hz, fundamental_value);

    if (third_ratio != SIGNAL_SEPARATOR_INVALID_RATIO)
    {
        valid_count++;
        if (third_ratio > SIGNAL_SEPARATOR_TRI_3RD_RATIO)
        {
            evidence_count++;
        }
    }

    if (fifth_ratio != SIGNAL_SEPARATOR_INVALID_RATIO)
    {
        valid_count++;
        if (fifth_ratio > SIGNAL_SEPARATOR_TRI_5TH_RATIO)
        {
            evidence_count++;
        }
    }

    if (seventh_ratio != SIGNAL_SEPARATOR_INVALID_RATIO)
    {
        valid_count++;
        if (seventh_ratio > SIGNAL_SEPARATOR_TRI_7TH_RATIO)
        {
            evidence_count++;
        }
    }

    if (valid_count >= 2U && evidence_count >= 2U)
    {
        return SIGNAL_SEPARATOR_WAVE_TRIANGLE;
    }

    if (valid_count == 1U && third_ratio != SIGNAL_SEPARATOR_INVALID_RATIO &&
        third_ratio > SIGNAL_SEPARATOR_TRI_SINGLE_3RD)
    {
        return SIGNAL_SEPARATOR_WAVE_TRIANGLE;
    }

    return SIGNAL_SEPARATOR_WAVE_SINE;
}

static uint8_t find_two_main_peaks(uint32_t *first_index, uint32_t *second_index)
{
    uint32_t min_bin = freq_to_nearest_bin(SIGNAL_SEPARATOR_MIN_FREQ_HZ);
    uint32_t max_bin = freq_to_nearest_bin(SIGNAL_SEPARATOR_MAX_FREQ_HZ);
    float first_value = 0.0f;
    float second_value = 0.0f;

    if (min_bin <= DC_INDEX)
    {
        min_bin = DC_INDEX + 1U;
    }
    if (max_bin >= (FFT_LENGTH / 2U))
    {
        max_bin = (FFT_LENGTH / 2U) - 1U;
    }

    *first_index = 0U;
    *second_index = 0U;

    for (uint32_t i = min_bin; i <= max_bin; i++)
    {
        if (fft_out[i] > first_value)
        {
            first_value = fft_out[i];
            *first_index = i;
        }
    }

    if (*first_index == 0U)
    {
        return 0U;
    }

    for (uint32_t i = min_bin; i <= max_bin; i++)
    {
        uint32_t distance = (i > *first_index) ? (i - *first_index) : (*first_index - i);
        if (distance <= SIGNAL_SEPARATOR_PEAK_GUARD_BINS)
        {
            continue;
        }

        if (fft_out[i] > second_value)
        {
            second_value = fft_out[i];
            *second_index = i;
        }
    }

    if (*second_index == 0U)
    {
        return 0U;
    }

    return 1U;
}

static void sort_signal_result(signal_separator_result_t *low, signal_separator_result_t *high)
{
    if (low->freq_hz > high->freq_hz)
    {
        signal_separator_result_t temp = *low;
        *low = *high;
        *high = temp;
    }
}

static HAL_StatusTypeDef print_signal_array(const char *name, float *data, uint32_t len)
{
    char msg[64];
    HAL_StatusTypeDef status;

    snprintf(msg, sizeof(msg), "%s begin\r\n", name);
    if (Usart_Send_Computer(&huart1, msg) != HAL_OK)
    {
        return usart_last_status;
    }

    status = Usart_Send_ADC_Data(data, &huart1, len);
    if (status != HAL_OK)
    {
        snprintf(msg, sizeof(msg), "%s tx fail status=%lu\r\n", name, (unsigned long)status);
        (void)Usart_Send_Computer(&huart1, msg);
        return status;
    }

    snprintf(msg, sizeof(msg), "%s end\r\n", name);
    if (Usart_Send_Computer(&huart1, msg) != HAL_OK)
    {
        return usart_last_status;
    }

    return HAL_OK;
}

static uint8_t analyze_signal(void)
{
    uint32_t peak1_index;
    uint32_t peak2_index;
    float peak1_freq;
    float peak2_freq;
    float peak1_value;
    float peak2_value;
    float peak1_magnitude;
    float peak2_magnitude;
    uint8_t only_one_signal = 0U;
    char msg[192];

    make_centered_fft_input();
    (void)print_signal_array("raw", adc1_data, ADC1_FREQ_BLOCK_LENGTH);

    calculate_fft(separator_fft_input, ADC1_FREQ_BLOCK_LENGTH);
    (void)print_signal_array("spectrum", fft_out, FFT_LENGTH / 2U);

    if (!find_two_main_peaks(&peak1_index, &peak2_index))
    {
        Usart_Send_Computer(&huart1, "detect fail: peaks not found\r\n");
        return 0U;
    }

    peak1_freq = snap_to_grid(bin_to_hz(interpolate_peak_bin(peak1_index)));
    peak2_freq = snap_to_grid(bin_to_hz(interpolate_peak_bin(peak2_index)));
    peak1_value = local_peak_value(peak1_index, SIGNAL_SEPARATOR_HARMONIC_SPAN);
    peak2_value = local_peak_value(peak2_index, SIGNAL_SEPARATOR_HARMONIC_SPAN);
    peak1_magnitude = local_peak_magnitude(peak1_index, SIGNAL_SEPARATOR_HARMONIC_SPAN);
    peak2_magnitude = local_peak_magnitude(peak2_index, SIGNAL_SEPARATOR_HARMONIC_SPAN);

    if (peak2_value < (peak1_value * SIGNAL_SEPARATOR_SECOND_MIN_RATIO) ||
        fabsf(peak1_freq - peak2_freq) < (SIGNAL_SEPARATOR_GRID_HZ * 0.5f))
    {
        only_one_signal = 1U;
        peak2_freq = peak1_freq;
        peak2_value = peak1_value;
        peak2_magnitude = peak1_magnitude;
    }

    signal_a.freq_hz = peak1_freq;
    signal_a.vpp = peak1_value * 2.0f;
    signal_a.wave = classify_wave(peak1_freq, peak2_freq, peak1_magnitude);

    signal_b.freq_hz = peak2_freq;
    signal_b.vpp = peak2_value * 2.0f;
    signal_b.wave = classify_wave(peak2_freq, peak1_freq, peak2_magnitude);

    sort_signal_result(&signal_a, &signal_b);
    separator_detected = 1U;

    if (only_one_signal)
    {
        snprintf(msg, sizeof(msg),
                 "detect ok\r\nsingle dominant component, B follows A\r\nA: %s %.0fHz vpp=%.3f\r\nB: %s %.0fHz vpp=%.3f\r\n",
                 wave_to_text(signal_a.wave),
                 signal_a.freq_hz,
                 signal_a.vpp,
                 wave_to_text(signal_b.wave),
                 signal_b.freq_hz,
                 signal_b.vpp);
    }
    else
    {
        snprintf(msg, sizeof(msg),
                 "detect ok\r\nA: %s %.0fHz vpp=%.3f\r\nB: %s %.0fHz vpp=%.3f\r\n",
                 wave_to_text(signal_a.wave),
                 signal_a.freq_hz,
                 signal_a.vpp,
                 wave_to_text(signal_b.wave),
                 signal_b.freq_hz,
                 signal_b.vpp);
    }
    Usart_Send_Computer(&huart1, msg);

    return 1U;
}

static void output_signal(const signal_separator_result_t *signal, const char *name)
{
    char msg[96];

    AD9833_SetFrequencyQuick(signal->freq_hz, wave_to_ad9833_type(signal->wave));
    AD9833_SetPhase(AD9833_REG_PHASE0, 0U);
    AD9833_ClearReset();

    snprintf(msg, sizeof(msg), "output %s: %s %.0fHz\r\n", name, wave_to_text(signal->wave), signal->freq_hz);
    Usart_Send_Computer(&huart1, msg);
}

static void output_a(void)
{
    output_signal(&signal_a, "A");
    separator_state = SIGNAL_SEPARATOR_STATE_OUTPUT_A;
}

static void output_b(void)
{
    output_signal(&signal_b, "B");
    separator_state = SIGNAL_SEPARATOR_STATE_OUTPUT_B;
}

static void start_capture(void)
{
    HAL_StatusTypeDef status;

    adc1_half_flag = 0U;
    adc1_deal_flag = 0U;
    adc1_proc_flag = 0U;

    HAL_TIM_Base_Stop(&htim1);
    HAL_ADC_Stop_DMA(&hadc1);

    status = MY_ADC1_Init();
    if (status != HAL_OK)
    {
        separator_state = SIGNAL_SEPARATOR_STATE_ERROR;
        Usart_Send_Computer(&huart1, "capture start fail\r\n");
        return;
    }

    separator_state = SIGNAL_SEPARATOR_STATE_CAPTURE;
    Usart_Send_Computer(&huart1, "capture start\r\n");
}

void SignalSeparator_Init(void)
{
    button_init(&auto_key);
    button_init(&switch_key);
    generate_hanning_window();

    AD9833_Init();
    Usart_Send_Computer(&huart1, "separator ready: B1 detect, C4 switch\r\n");
}

void SignalSeparator_Process(void)
{
    uint8_t auto_pressed = button_update(&auto_key);
    uint8_t switch_pressed = button_update(&switch_key);

    if (auto_pressed && separator_state != SIGNAL_SEPARATOR_STATE_CAPTURE)
    {
        separator_detected = 0U;
        start_capture();
    }

    if (switch_pressed && separator_detected)
    {
        if (separator_state == SIGNAL_SEPARATOR_STATE_OUTPUT_A)
        {
            output_b();
        }
        else
        {
            output_a();
        }
    }

    if (separator_state == SIGNAL_SEPARATOR_STATE_CAPTURE)
    {
        adc1_deal();
        if (adc1_proc_flag)
        {
            adc1_proc_flag = 0U;
            if (analyze_signal())
            {
                output_a();
            }
            else
            {
                separator_state = SIGNAL_SEPARATOR_STATE_ERROR;
            }
        }
    }
}

const signal_separator_result_t *SignalSeparator_GetSignalA(void)
{
    return &signal_a;
}

const signal_separator_result_t *SignalSeparator_GetSignalB(void)
{
    return &signal_b;
}
