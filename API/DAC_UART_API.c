#include "DAC_UART_API.h"

#include "DAC_FML.h"
#include "usart.h"

#include <string.h>

#define DAC_UART_LINE_LENGTH  48U
#define DAC_UART_RX_PER_CALL  16U
#define DAC_UART_VALUE_LIMIT  10000000.0f

static char s_dac_uart_line[DAC_UART_LINE_LENGTH];
static uint8_t s_dac_uart_line_length;

static void dac_uart_send(const char *message)
{
    (void)HAL_UART_Transmit(&huart1, (const uint8_t *)message,
                            (uint16_t)strlen(message), 10U);
}

static void dac_uart_skip_spaces(const char **text)
{
    while ((**text == ' ') || (**text == '\t'))
    {
        (*text)++;
    }
}

static uint8_t dac_uart_parse_waveform(const char **text, int *waveform)
{
    uint8_t found_digit = 0U;
    int value = 0;

    dac_uart_skip_spaces(text);
    while ((**text >= '0') && (**text <= '9'))
    {
        found_digit = 1U;
        value = (value * 10) + (**text - '0');
        if (value > (int)DAC_USER_WAVE_DC)
        {
            return 0U;
        }
        (*text)++;
    }

    *waveform = value;
    return found_digit;
}

static uint8_t dac_uart_parse_value(const char **text, float *value)
{
    float result = 0.0f;
    float fraction = 0.1f;
    uint8_t found_digit = 0U;

    dac_uart_skip_spaces(text);
    while ((**text >= '0') && (**text <= '9'))
    {
        found_digit = 1U;
        result = (result * 10.0f) + (float)(**text - '0');
        if (result > DAC_UART_VALUE_LIMIT)
        {
            return 0U;
        }
        (*text)++;
    }

    if (**text == '.')
    {
        (*text)++;
        while ((**text >= '0') && (**text <= '9'))
        {
            found_digit = 1U;
            result += (float)(**text - '0') * fraction;
            fraction *= 0.1f;
            (*text)++;
        }
    }

    *value = result;
    return found_digit;
}

static uint8_t dac_uart_at_end(const char *text)
{
    dac_uart_skip_spaces(&text);
    return (*text == '\0') ? 1U : 0U;
}

static HAL_StatusTypeDef dac_uart_apply(int waveform,
                                        float frequency_hz,
                                        float vpp,
                                        float offset_v)
{
    HAL_StatusTypeDef status;

    if ((waveform < (int)DAC_USER_WAVE_SINE) ||
        (waveform > (int)DAC_USER_WAVE_DC) ||
        (frequency_hz <= 0.0f) ||
        (vpp < 0.0f) || (vpp > DAC_WAVE_REF_VOLTAGE) ||
        (offset_v < 0.0f) || (offset_v > DAC_WAVE_REF_VOLTAGE))
    {
        return HAL_ERROR;
    }

    status = DAC_Waveform_Stop();
    if (status != HAL_OK)
    {
        return status;
    }

    return DAC_Waveform_Start((dac_wave_type_t)waveform,
                              frequency_hz,
                              vpp,
                              offset_v);
}

static void dac_uart_process_line(void)
{
    const char *text = s_dac_uart_line;
    int waveform = 0;
    float frequency_hz = 0.0f;
    float vpp = 0.0f;
    float offset_v = 0.0f;

    if (!dac_uart_parse_waveform(&text, &waveform) ||
        !dac_uart_parse_value(&text, &frequency_hz) ||
        !dac_uart_parse_value(&text, &vpp) ||
        !dac_uart_parse_value(&text, &offset_v) ||
        !dac_uart_at_end(text) ||
        (dac_uart_apply(waveform, frequency_hz, vpp, offset_v) != HAL_OK))
    {
        dac_uart_send("err\r\n");
        return;
    }

    dac_uart_send("ok\r\n");
}

HAL_StatusTypeDef DAC_UART_API_Init(void)
{
    s_dac_uart_line_length = 0U;
    return HAL_OK;
}

void DAC_UART_API_Process(void)
{
    uint8_t byte;
    uint8_t processed = 0U;

    while ((processed < DAC_UART_RX_PER_CALL) &&
           (HAL_UART_Receive(&huart1, &byte, 1U, 0U) == HAL_OK))
    {
        processed++;

        if ((byte == '\r') || (byte == '\n'))
        {
            if (s_dac_uart_line_length != 0U)
            {
                s_dac_uart_line[s_dac_uart_line_length] = '\0';
                dac_uart_process_line();
                s_dac_uart_line_length = 0U;
            }
        }
        else if (s_dac_uart_line_length < (sizeof(s_dac_uart_line) - 1U))
        {
            s_dac_uart_line[s_dac_uart_line_length++] = (char)byte;
        }
        else
        {
            s_dac_uart_line_length = 0U;
            dac_uart_send("err\r\n");
        }
    }
}
