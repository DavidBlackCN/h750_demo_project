#include "G_CONSOLE_API.h"

#include "G_BASIC_API.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define G_CONSOLE_API_LINE_LENGTH  40U
#define G_CONSOLE_API_IDLE_COMMIT_MS  100U
#define G_CONSOLE_API_DEFAULT_FREQUENCY_HZ  1000U
#define G_CONSOLE_API_DEFAULT_TARGET_TENTHS  20U

static UART_HandleTypeDef *g_uart;
static uint8_t g_requirement;
static char g_line[G_CONSOLE_API_LINE_LENGTH];
static uint8_t g_line_length;
static bool g_line_overflow;
static uint32_t g_last_rx_tick;

static float G_ConsoleAPI_GetDirectOutputMvpp(uint16_t amplitude_code)
{
    return ((float)amplitude_code * AD9910_OUTPUT_FULL_SCALE_MVPP) /
           ((float)AD9910_AMPLITUDE_CODE_MAX *
            AD9910_OUTPUT_AMPLIFIER_GAIN);
}

static void G_ConsoleAPI_SkipSpaces(const char **cursor)
{
    while ((**cursor == ' ') || (**cursor == '\t'))
    {
        (*cursor)++;
    }
}

static bool G_ConsoleAPI_ParseUnsigned(const char **cursor, uint32_t *value)
{
    uint32_t parsed = 0U;
    bool has_digit = false;

    G_ConsoleAPI_SkipSpaces(cursor);
    while ((**cursor >= '0') && (**cursor <= '9'))
    {
        uint32_t digit = (uint32_t)(**cursor - '0');

        if (parsed > ((UINT32_MAX - digit) / 10U))
        {
            return false;
        }
        parsed = parsed * 10U + digit;
        has_digit = true;
        (*cursor)++;
    }

    if (!has_digit)
    {
        return false;
    }
    *value = parsed;
    return true;
}

static bool G_ConsoleAPI_AtEnd(const char *cursor)
{
    G_ConsoleAPI_SkipSpaces(&cursor);
    return *cursor == '\0';
}

static bool G_ConsoleAPI_ParseRequirementToken(const char **cursor,
                                               uint8_t requirement)
{
    const char *token;

    G_ConsoleAPI_SkipSpaces(cursor);
    token = *cursor;
    if (((token[0] != 'r') && (token[0] != 'R')) ||
        (token[1] != (char)('0' + requirement)) ||
        ((token[2] != '\0') && (token[2] != ' ') &&
         (token[2] != '\t')))
    {
        return false;
    }

    *cursor += 2;
    G_ConsoleAPI_SkipSpaces(cursor);
    return true;
}

static bool G_ConsoleAPI_ParseTargetTenths(const char **cursor,
                                           uint8_t *target_tenths)
{
    uint32_t whole;
    uint32_t tenths = 0U;

    if (!G_ConsoleAPI_ParseUnsigned(cursor, &whole))
    {
        return false;
    }
    if (**cursor == '.')
    {
        (*cursor)++;
        if ((**cursor < '0') || (**cursor > '9'))
        {
            return false;
        }
        tenths = (uint32_t)(**cursor - '0');
        (*cursor)++;
        if ((**cursor >= '0') && (**cursor <= '9'))
        {
            return false;
        }
    }
    if ((whole > 25U) || ((whole * 10U + tenths) > UINT8_MAX))
    {
        return false;
    }

    *target_tenths = (uint8_t)(whole * 10U + tenths);
    return true;
}

static void G_ConsoleAPI_PrintResult(HAL_StatusTypeDef status)
{
    const G_BasicAPI_Result *result = G_BasicAPI_GetResult();
    float direct_output_mvpp;
    float amplified_output_mvpp;

    if (status != HAL_OK)
    {
        printf("ERR req=%u state=%u: check range and 100 Hz/0.1 V steps\r\n",
               result->requirement, (unsigned int)result->state);
        return;
    }

    direct_output_mvpp = G_ConsoleAPI_GetDirectOutputMvpp(
        result->amplitude_code);
    amplified_output_mvpp = direct_output_mvpp *
                            AD9910_OUTPUT_AMPLIFIER_GAIN;

    if (result->requirement == 2U)
    {
        printf("OUT R2 f=%luHz target=3.0Vpp AD9910=%.3fmVpp "
               "x10=%.3fmVpp ASF=%u model=N/A\r\n",
               (unsigned long)result->frequency_hz,
               direct_output_mvpp, amplified_output_mvpp,
               result->amplitude_code);
    }
    else
    {
        printf("OUT R%u f=%luHz target=%.1fVpp AD9910=%.3fmVpp "
               "x10=%.3fmVpp ASF=%u gain=%.7f\r\n",
               result->requirement, (unsigned long)result->frequency_hz,
               (float)result->target_output_mvpp / 1000.0f,
               direct_output_mvpp, amplified_output_mvpp,
               result->amplitude_code, result->model_gain);
    }
}

static void G_ConsoleAPI_ProcessLine(char *line)
{
    const char *cursor = line;
    uint32_t frequency_hz;
    uint8_t target_tenths;
    HAL_StatusTypeDef status = HAL_ERROR;
    bool command_parsed = false;

    G_ConsoleAPI_SkipSpaces(&cursor);
    if (*cursor == '\0')
    {
        return;
    }

    printf("RX %s\r\n", cursor);

    if (g_requirement == G_CONSOLE_API_REQUIREMENT_ALL)
    {
        const char *arguments = cursor;

        if (G_ConsoleAPI_ParseRequirementToken(&arguments, 2U) &&
            G_ConsoleAPI_ParseUnsigned(&arguments, &frequency_hz) &&
            G_ConsoleAPI_AtEnd(arguments))
        {
            command_parsed = true;
            status = G_BasicAPI_StartRequirement2(frequency_hz);
        }
        else
        {
            arguments = cursor;
            if (G_ConsoleAPI_ParseRequirementToken(&arguments, 3U) &&
                G_ConsoleAPI_AtEnd(arguments))
            {
                command_parsed = true;
                status = G_BasicAPI_StartRequirement3();
            }
            else
            {
                arguments = cursor;
                if (G_ConsoleAPI_ParseRequirementToken(&arguments, 4U) &&
                    G_ConsoleAPI_ParseUnsigned(&arguments, &frequency_hz) &&
                    G_ConsoleAPI_ParseTargetTenths(&arguments,
                                                   &target_tenths) &&
                    G_ConsoleAPI_AtEnd(arguments))
                {
                    command_parsed = true;
                    status = G_BasicAPI_StartRequirement4(frequency_hz,
                                                          target_tenths);
                }
            }
        }
    }
    else if (g_requirement == 2U)
    {
        if (G_ConsoleAPI_ParseUnsigned(&cursor, &frequency_hz) &&
            G_ConsoleAPI_AtEnd(cursor))
        {
            command_parsed = true;
            status = G_BasicAPI_StartRequirement2(frequency_hz);
        }
    }
    else if (g_requirement == 3U)
    {
        if ((strcmp(cursor, "start") == 0) ||
            (strcmp(cursor, "START") == 0))
        {
            command_parsed = true;
            status = G_BasicAPI_StartRequirement3();
        }
    }
    else if (G_ConsoleAPI_ParseUnsigned(&cursor, &frequency_hz) &&
             G_ConsoleAPI_ParseTargetTenths(&cursor, &target_tenths) &&
             G_ConsoleAPI_AtEnd(cursor))
    {
        command_parsed = true;
        status = G_BasicAPI_StartRequirement4(frequency_hz,
                                              target_tenths);
    }

    if (!command_parsed)
    {
        printf("ERR command format\r\n");
        return;
    }
    G_ConsoleAPI_PrintResult(status);
}

static void G_ConsoleAPI_ProcessBufferedLine(void)
{
    if (g_line_overflow)
    {
        printf("[UART1 RX] ERR line too long\r\n");
    }
    else if (g_line_length > 0U)
    {
        while ((g_line_length > 0U) &&
               ((g_line[g_line_length - 1U] == ' ') ||
                (g_line[g_line_length - 1U] == '\t')))
        {
            g_line_length--;
        }
        g_line[g_line_length] = '\0';
        G_ConsoleAPI_ProcessLine(g_line);
    }

    g_line_length = 0U;
    g_line_overflow = false;
}

static HAL_StatusTypeDef G_ConsoleAPI_StartDefaultWaveform(void)
{
    HAL_StatusTypeDef status;

    if (g_requirement == 2U)
    {
        status = G_BasicAPI_StartRequirement2(
            G_CONSOLE_API_DEFAULT_FREQUENCY_HZ);
    }
    else if (g_requirement == 3U)
    {
        status = G_BasicAPI_StartRequirement3();
    }
    else
    {
        status = G_BasicAPI_StartRequirement4(
            G_CONSOLE_API_DEFAULT_FREQUENCY_HZ,
            G_CONSOLE_API_DEFAULT_TARGET_TENTHS);
    }

    G_ConsoleAPI_PrintResult(status);
    return status;
}

HAL_StatusTypeDef G_ConsoleAPI_Init(UART_HandleTypeDef *huart,
                                    uint8_t selected_requirement)
{
    if ((huart == NULL) ||
        ((selected_requirement != G_CONSOLE_API_REQUIREMENT_ALL) &&
         ((selected_requirement < 2U) || (selected_requirement > 4U))))
    {
        return HAL_ERROR;
    }

    g_uart = huart;
    g_requirement = selected_requirement;
    g_line_length = 0U;
    g_line_overflow = false;
    g_last_rx_tick = 0U;

    (void)setvbuf(stdout, NULL, _IONBF, 0);
    if (!G_ModelBLL_SelfTest())
    {
        printf("ERR selftest\r\n");
        return HAL_ERROR;
    }

    if (g_requirement == G_CONSOLE_API_REQUIREMENT_ALL)
    {
        printf("\r\nREADY ALL UART1=%lu\r\n",
               (unsigned long)huart->Init.BaudRate);
        printf("CMD r2 <Hz> | r3 | r4 <Hz> <Vpp>\r\n");
        printf("IDLE waiting for user selection\r\n");
        return HAL_OK;
    }
    else
    {
        printf("\r\nREADY R%u UART1=%lu\r\n", g_requirement,
               (unsigned long)huart->Init.BaudRate);
        if (g_requirement == 2U)
        {
            printf("CMD <frequency_hz>; default 1000Hz/3Vpp\r\n");
        }
        else if (g_requirement == 3U)
        {
            printf("CMD start; 1000Hz model target=2.0Vpp\r\n");
        }
        else
        {
            printf("CMD <frequency_hz> <target_vpp>; "
                   "default 1000Hz/2.0Vpp\r\n");
        }
    }
    return G_ConsoleAPI_StartDefaultWaveform();
}

void G_ConsoleAPI_Process(void)
{
    uint8_t byte;
    HAL_StatusTypeDef status;

    if (g_uart == NULL)
    {
        return;
    }

    status = HAL_UART_Receive(g_uart, &byte, 1U, 0U);
    if (status != HAL_OK)
    {
        if (status == HAL_ERROR)
        {
            printf("[UART1 RX] HAL_ERROR code=0x%08lX ISR=0x%08lX\r\n",
                   (unsigned long)g_uart->ErrorCode,
                   (unsigned long)g_uart->Instance->ISR);
        }
        if ((g_line_length > 0U) &&
            ((HAL_GetTick() - g_last_rx_tick) >=
             G_CONSOLE_API_IDLE_COMMIT_MS))
        {
            G_ConsoleAPI_ProcessBufferedLine();
        }
        return;
    }

    g_last_rx_tick = HAL_GetTick();

    if ((byte == '\r') || (byte == '\n'))
    {
        G_ConsoleAPI_ProcessBufferedLine();
        return;
    }

    if (g_line_length < (G_CONSOLE_API_LINE_LENGTH - 1U))
    {
        g_line[g_line_length++] = (char)byte;
    }
    else
    {
        g_line_overflow = true;
    }
}
