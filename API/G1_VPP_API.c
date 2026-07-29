#include "G1_VPP_API.h"

#include "G1_VPP_ADC_FML.h"
#include "G1_FIR_BLL.h"
#include "USART_FML.h"

#include "usart.h"

#include <stdio.h>

static g1_vpp_calibration_t s_calibration = {
    .adc_reference_volts = 3.3f,
    .front_end_gain = 1.0f,
    .calibration_scale = 1.0f,
};
static g1_vpp_result_t s_result;
static g1_fft_result_t s_fft_result;
static float s_fft_input[G1_VPP_ADC_FRAME_SAMPLES]
    __attribute__((section(".dma_buffer"), aligned(32)));
static float s_fir_output[G1_VPP_ADC_FRAME_SAMPLES]
    __attribute__((section(".dma_buffer"), aligned(32)));
static uint8_t s_error;
static uint32_t s_error_code;

typedef enum
{
    G1_VPP_STATE_WAIT_RELEASE = 0,
    G1_VPP_STATE_IDLE,
    G1_VPP_STATE_CAPTURING,
} g1_vpp_state_t;

#define G1_VPP_START_KEY_GPIO_PORT GPIOC
#define G1_VPP_START_KEY_PIN       GPIO_PIN_4
#define G1_VPP_START_KEY_DEBOUNCE_MS 30U

static g1_vpp_state_t s_state;
static GPIO_PinState s_key_sample;
static GPIO_PinState s_key_stable;
static uint32_t s_key_change_tick;

#define G1_VPP_ERROR_FFT_ANALYZE 0xF001U
#define G1_VPP_ERROR_FIT         0xF002U
#define G1_VPP_ERROR_FIR         0xF003U

static void g1_vpp_send_error(const char *stage, uint32_t code)
{
    char message[64];
    const int length = snprintf(message, sizeof(message),
                                "error stage=%s code=%lu\r\n", stage,
                                (unsigned long)code);

    if ((length > 0) && (length < (int)sizeof(message)))
    {
        (void)Usart_Send_Computer(&huart1, message);
    }
}

#define G1_VPP_HARMONIC_SEARCH_MAX_HZ 505000.0f
#define G1_VPP_RAD_TO_DEG              57.2957795130823208768f

static bool g1_vpp_start_key_is_pressed(void)
{
    return (s_key_stable == GPIO_PIN_RESET);
}

static void g1_vpp_start_key_poll(void)
{
    const GPIO_PinState sample = HAL_GPIO_ReadPin(G1_VPP_START_KEY_GPIO_PORT,
                                                   G1_VPP_START_KEY_PIN);
    const uint32_t now = HAL_GetTick();

    if (sample != s_key_sample)
    {
        s_key_sample = sample;
        s_key_change_tick = now;
    }

    if ((s_key_stable != s_key_sample) &&
        ((uint32_t)(now - s_key_change_tick) >= G1_VPP_START_KEY_DEBOUNCE_MS))
    {
        s_key_stable = s_key_sample;
    }
}

static HAL_StatusTypeDef g1_vpp_start_capture(void)
{
    s_result.valid = false;
    s_error = 0U;
    s_error_code = HAL_ADC_ERROR_NONE;
    s_fft_result.valid = false;

    if (G1_VPP_ADC_FML_Start() != HAL_OK)
    {
        s_error = 1U;
        s_error_code = G1_VPP_ADC_FML_GetErrorCode();
        g1_vpp_send_error("adc_start", s_error_code);
        return HAL_ERROR;
    }

    s_state = G1_VPP_STATE_CAPTURING;
    return HAL_OK;
}

static HAL_StatusTypeDef g1_vpp_analyze_and_send_fft(const uint16_t *samples,
                                                     uint32_t sample_count)
{
    const g1_fft_input_t fft_input = {
        .sample_rate_hz = G1_VPP_ADC_FML_GetSampleRateHz(),
        .search_min_frequency_hz = 10000.0f,
        /* Leave margin above the 500 kHz task edge so a peak close to the
           boundary is not discarded when its nearest FFT bin lies above it. */
        .search_max_frequency_hz = G1_VPP_HARMONIC_SEARCH_MAX_HZ,
        .min_peak_amplitude_volts = 0.001f,
    };
    char message[96];
    int length;
    uint32_t harmonic_count = 0U;

    if (sample_count > G1_VPP_ADC_FRAME_SAMPLES)
    {
        return HAL_ERROR;
    }

    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        s_fft_input[index] = G1_VPP_BLL_CodeToInputVolts(samples[index],
                                                          &s_calibration);
    }

    if (!G1_FIR_BLL_FilterFrame(s_fft_input, s_fir_output, sample_count))
    {
        s_error_code = G1_VPP_ERROR_FIR;
        return HAL_ERROR;
    }

    if (!G1_FFT_API_Analyze(s_fir_output, sample_count, &fft_input, &s_fft_result))
    {
        s_error_code = G1_VPP_ERROR_FFT_ANALYZE;
        return HAL_ERROR;
    }
    if (!G1_FFT_BLL_FitHarmonics(s_fir_output, sample_count, &s_fft_result))
    {
        s_error_code = G1_VPP_ERROR_FIT;
        return HAL_ERROR;
    }

    s_result.input_vpp_volts = s_fft_result.fitted_vpp_volts;
    s_result.input_rms_volts = s_fft_result.fitted_ac_rms_volts;
    s_result.input_mean_volts = s_fft_result.fitted_dc_volts;

    for (uint32_t index = 0U; index < s_fft_result.component_count; ++index)
    {
        if (s_fft_result.components[index].harmonic_order != 0U)
        {
            ++harmonic_count;
        }
    }

    length = snprintf(message, sizeof(message),
                      "begin count=%lu\r\nfa_j=%.0f\r\nUpp=%.3f\r\nUrms=%.3f\r\n",
                      (unsigned long)harmonic_count,
                      (double)s_fft_result.fundamental_hz,
                      (double)(s_fft_result.fitted_vpp_volts * 1000.0f),
                      (double)(s_fft_result.fitted_ac_rms_volts * 1000.0f));
    if ((length <= 0) || (length >= (int)sizeof(message)) ||
        (Usart_Send_Computer(&huart1, message) != HAL_OK))
    {
        return HAL_ERROR;
    }

    for (uint32_t index = 0U; index < s_fft_result.component_count; ++index)
    {
        const g1_fft_component_t *component = &s_fft_result.components[index];

        if (component->harmonic_order == 0U)
        {
            continue;
        }

        length = snprintf(message, sizeof(message),
                          "n=%lu f_Hz=%.0f fit_Hz=%.0f amp_mVpp=%.3f phase_deg=%.3f\r\n",
                          (unsigned long)component->harmonic_order,
                          (double)component->frequency_hz,
                          (double)component->fitted_frequency_hz,
                          (double)(component->peak_amplitude_volts * 2000.0f),
                          (double)(component->phase_radians * G1_VPP_RAD_TO_DEG));
        if ((length <= 0) || (length >= (int)sizeof(message)) ||
            (Usart_Send_Computer(&huart1, message) != HAL_OK))
        {
            return HAL_ERROR;
        }
    }

    if (Usart_Send_Computer(&huart1, "end\r\n") != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef G1_VPP_API_Init(void)
{
    const GPIO_PinState initial_key = HAL_GPIO_ReadPin(G1_VPP_START_KEY_GPIO_PORT,
                                                        G1_VPP_START_KEY_PIN);

    s_result.valid = false;
    s_error = 0U;
    s_error_code = HAL_ADC_ERROR_NONE;
    s_fft_result.valid = false;
    s_state = G1_VPP_STATE_WAIT_RELEASE;
    s_key_sample = initial_key;
    s_key_stable = initial_key;
    s_key_change_tick = HAL_GetTick();

    if (Usart_Send_Computer(&huart1, "ok\r\n") != HAL_OK)
    {
        s_error = 1U;
        s_error_code = HAL_ERROR;
        return HAL_ERROR;
    }

    return HAL_OK;
}

void G1_VPP_API_Process(void)
{
    const uint16_t *samples;
    uint32_t sample_count;
    g1_vpp_start_key_poll();

    if (s_state == G1_VPP_STATE_WAIT_RELEASE)
    {
        if (!g1_vpp_start_key_is_pressed())
        {
            s_state = G1_VPP_STATE_IDLE;
        }
        return;
    }

    if (s_state == G1_VPP_STATE_IDLE)
    {
        if (g1_vpp_start_key_is_pressed())
        {
            if (g1_vpp_start_capture() != HAL_OK)
            {
                /* Do not retry a failed start while the key remains held. */
                s_state = G1_VPP_STATE_WAIT_RELEASE;
            }
        }
        return;
    }

    G1_VPP_ADC_FML_Poll();
    if (G1_VPP_ADC_FML_HasError())
    {
        s_error = 1U;
        s_error_code = G1_VPP_ADC_FML_GetErrorCode();
        g1_vpp_send_error("adc_capture", s_error_code);
        s_state = G1_VPP_STATE_WAIT_RELEASE;
        return;
    }

    if (!G1_VPP_ADC_FML_TakeFrame(&samples, &sample_count))
    {
        return;
    }

    if (!G1_VPP_BLL_Analyze(samples, sample_count, &s_calibration, &s_result))
    {
        s_error = 1U;
        s_error_code = HAL_ERROR;
        g1_vpp_send_error("waveform", s_error_code);
        s_state = G1_VPP_STATE_WAIT_RELEASE;
        return;
    }

    if (g1_vpp_analyze_and_send_fft(samples, sample_count) != HAL_OK)
    {
        s_error = 1U;
        g1_vpp_send_error((s_error_code == G1_VPP_ERROR_FIT) ? "fit" :
                          ((s_error_code == G1_VPP_ERROR_FIR) ? "fir" : "fft"),
                          s_error_code);
        s_state = G1_VPP_STATE_WAIT_RELEASE;
        return;
    }

    /* Keep ADC/TIM stopped.  A new capture requires a fresh key release and
       press, so a long press or any press during capture cannot retrigger. */
    s_state = G1_VPP_STATE_WAIT_RELEASE;
}

void G1_VPP_API_SetCalibration(const g1_vpp_calibration_t *calibration)
{
    if ((calibration != NULL) && (calibration->adc_reference_volts > 0.0f) &&
        (calibration->front_end_gain > 0.0f) &&
        (calibration->calibration_scale > 0.0f))
    {
        s_calibration = *calibration;
        s_result.valid = false;
    }
}

const g1_vpp_result_t *G1_VPP_API_GetResult(void)
{
    return &s_result;
}

const g1_fft_result_t *G1_VPP_API_GetFftResult(void)
{
    return &s_fft_result;
}

bool G1_VPP_API_HasError(void)
{
    return (s_error != 0U);
}

uint32_t G1_VPP_API_GetErrorCode(void)
{
    return s_error_code;
}
