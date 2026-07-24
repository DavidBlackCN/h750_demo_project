#include "DPLL_API.h"

#include "AD9833_API.h"
#include "DPLL_BLL.h"
#include "USART_FML.h"
#include "usart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DPLL_API_ACTUATOR_DIRECTION        1.0f
#define DPLL_API_INTEGRAL_LIMIT_DEG      180.0f

static dpll_bll_state_t s_dpll_state;
static dpll_bll_config_t s_dpll_config;
static uint32_t s_last_update_tick;
static dpll_bll_result_t s_last_result;
static char s_uart_line[64];
static uint8_t s_uart_line_length;

static void dpll_uart_send(const char *message)
{
    (void)Usart_Send_Computer(&huart1, (char *)message);
}

static void dpll_uart_report(void)
{
    char message[192];

    (void)snprintf(message, sizeof(message),
                   "dpll kp=%.5f ki=%.5f target=%.3f cal=%.3f dir=%.0f cmd=%.3f meas=%.3f err=%.3f\r\n",
                   (double)s_dpll_config.kp, (double)s_dpll_config.ki,
                   (double)s_dpll_config.phase_target_deg,
                   (double)s_dpll_config.phase_calibration_deg,
                   (double)s_dpll_config.actuator_direction,
                   (double)s_dpll_state.phase_command_deg,
                   (double)s_last_result.measured_phase_deg,
                   (double)s_last_result.error_deg);
    dpll_uart_send(message);
}

static void dpll_uart_process_line(void)
{
    char *value_text;
    char *value_end;
    float value;

    if (strcmp(s_uart_line, "show") == 0)
    {
        dpll_uart_report();
        return;
    }

    value_text = strchr(s_uart_line, ' ');
    if (value_text == NULL)
    {
        dpll_uart_send("dpll usage: kp <value>, ki <value>, show\r\n");
        return;
    }
    *value_text++ = '\0';
    while (*value_text == ' ')
    {
        value_text++;
    }
    value = strtof(value_text, &value_end);
    if ((value_end == value_text) || (*value_end != '\0'))
    {
        dpll_uart_send("dpll usage: kp <value>, ki <value>, show\r\n");
        return;
    }

    if ((strcmp(s_uart_line, "kp") == 0) && (value >= 0.0f))
    {
        s_dpll_config.kp = value;
    }
    else if ((strcmp(s_uart_line, "ki") == 0) && (value >= 0.0f))
    {
        s_dpll_config.ki = value;
    }
    else
    {
        dpll_uart_send("dpll invalid command\r\n");
        return;
    }

    dpll_uart_send("dpll ok\r\n");
    dpll_uart_report();
}

HAL_StatusTypeDef DPLL_API_Init(void)
{
    s_dpll_config = (dpll_bll_config_t)
    {
        .kp = DPLL_API_PROPORTIONAL_GAIN,
        .ki = DPLL_API_INTEGRAL_GAIN,
        .phase_target_deg = DPLL_API_TARGET_PHASE_DEG,
        .phase_calibration_deg = DPLL_API_PHASE_CALIBRATION_DEG,
        .initial_phase_command_deg = DPLL_API_INITIAL_PHASE_COMMAND_DEG,
        .actuator_direction = DPLL_API_ACTUATOR_DIRECTION,
        .integral_limit_deg = DPLL_API_INTEGRAL_LIMIT_DEG
    };

    if (!DPLL_BLL_Init(&s_dpll_config, &s_dpll_state))
    {
        return HAL_ERROR;
    }

    s_last_update_tick = HAL_GetTick() - DPLL_API_UPDATE_PERIOD_MS;
    s_uart_line_length = 0U;
    return HAL_OK;
}

HAL_StatusTypeDef DPLL_API_ProcessPhase(float phase_difference_deg)
{
    dpll_bll_result_t result;
    float scope_phase_difference_deg;
    uint32_t now = HAL_GetTick();

    if ((now - s_last_update_tick) < DPLL_API_UPDATE_PERIOD_MS)
    {
        return HAL_OK;
    }
    s_last_update_tick = now;

    /* DLIA reports ADC2 - ADC1. With this board's wiring ADC2 is scope CH1
       (AD9833) and ADC1 is scope CH2 (reference), so negate it before PI. */
    scope_phase_difference_deg = -phase_difference_deg;

    if (!DPLL_BLL_Update(&s_dpll_config, &s_dpll_state,
                         scope_phase_difference_deg, &result))
    {
        return HAL_ERROR;
    }

    s_last_result = result;
    return AD9833_API_SetPhaseDegrees(result.phase_command_deg);
}

void DPLL_API_ProcessUart(void)
{
    uint8_t byte;
    uint8_t processed = 0U;

    while ((processed < 8U) &&
           (HAL_UART_Receive(&huart1, &byte, 1U, 0U) == HAL_OK))
    {
        processed++;
        if ((byte == '\r') || (byte == '\n'))
        {
            if (s_uart_line_length != 0U)
            {
                s_uart_line[s_uart_line_length] = '\0';
                dpll_uart_process_line();
                s_uart_line_length = 0U;
            }
        }
        else if (s_uart_line_length < (sizeof(s_uart_line) - 1U))
        {
            s_uart_line[s_uart_line_length++] = (char)byte;
        }
        else
        {
            s_uart_line_length = 0U;
            dpll_uart_send("dpll command too long\r\n");
        }
    }
}
