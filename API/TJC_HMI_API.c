#include "TJC_HMI_API.h"

#include "TJC_HMI.h"

#include <stdio.h>

#define TJC_HMI_DEFAULT_FREQUENCY_HZ    1000U
#define TJC_HMI_DEFAULT_VPP_MVPP        1000U
#define TJC_HMI_FREQUENCY_MAX_HZ    10000000U
#define TJC_HMI_VPP_MAX_MVPP            20000U

#define TJC_HMI_CMD_ENTER_MENU      0x01U
#define TJC_HMI_CMD_RESET_MCU        0x02U
#define TJC_HMI_CMD_SELECT_TASK_1   0x10U
#define TJC_HMI_CMD_SELECT_TASK_2   0x11U
#define TJC_HMI_CMD_SELECT_TASK_3   0x12U
#define TJC_HMI_CMD_SELECT_TASK_4   0x13U
#define TJC_HMI_CMD_SELECT_TASK_5   0x14U
#define TJC_HMI_CMD_EDIT_FREQUENCY  0x20U
#define TJC_HMI_CMD_EDIT_VPP        0x21U
#define TJC_HMI_CMD_KEY_OK          0x22U
#define TJC_HMI_CMD_KEY_CANCEL      0x23U
#define TJC_HMI_CMD_START           0x30U
#define TJC_HMI_CMD_BACK_MENU       0x31U
#define TJC_HMI_CMD_AGAIN           0x32U
#define TJC_HMI_CMD_RESULT_HOME     0x33U
#define TJC_HMI_CMD_STOP            0x34U

typedef enum
{
    TJC_HMI_INPUT_NONE = 0,
    TJC_HMI_INPUT_FREQUENCY,
    TJC_HMI_INPUT_VPP
} TJC_HMI_InputKind;

static TJC_HMI_API_TaskConfig tjc_task_config;
static TJC_HMI_InputKind tjc_input_kind;
static TJC_HMI_API_State tjc_state;

__weak HAL_StatusTypeDef TJC_HMI_API_OnTaskStart(
    const TJC_HMI_API_TaskConfig *config)
{
    (void)config;
    return HAL_OK;
}

__weak void TJC_HMI_API_OnTaskStop(void)
{
}

__weak uint8_t TJC_HMI_API_ValidateFrequencyHz(uint8_t task_id,
                                                uint32_t frequency_hz)
{
    (void)task_id;
    return (uint8_t)((frequency_hz >= 1U) &&
                     (frequency_hz <= TJC_HMI_FREQUENCY_MAX_HZ));
}

__weak uint8_t TJC_HMI_API_ValidateVppMvpp(uint8_t task_id,
                                            uint32_t vpp_mvpp)
{
    (void)task_id;
    return (uint8_t)((vpp_mvpp >= 100U) && (vpp_mvpp <= TJC_HMI_VPP_MAX_MVPP));
}

static HAL_StatusTypeDef TJC_HMI_API_ShowHome(void)
{
    tjc_input_kind = TJC_HMI_INPUT_NONE;
    return TJC_HMI_SendCommand("page home");
}

static HAL_StatusTypeDef TJC_HMI_API_ShowMenu(void)
{
    tjc_input_kind = TJC_HMI_INPUT_NONE;
    return TJC_HMI_SendCommand("page menu");
}

static HAL_StatusTypeDef TJC_HMI_API_ShowControl(const char *hint)
{
    char text[32];
    HAL_StatusTypeDef status;

    status = TJC_HMI_SendCommand("page control");
    if (status != HAL_OK)
    {
        return status;
    }

    (void)snprintf(text, sizeof(text), "TASK %u", tjc_task_config.task_id);
    status = TJC_HMI_SetText("control.t_task", text);
    if (status == HAL_OK)
    {
        (void)snprintf(text, sizeof(text), "%lu Hz",
                       (unsigned long)tjc_task_config.frequency_hz);
        status = TJC_HMI_SetText("control.t_freq", text);
    }
    if (status == HAL_OK)
    {
        (void)snprintf(text, sizeof(text), "%lu.%01lu Vpp",
                       (unsigned long)(tjc_task_config.vpp_mvpp / 1000U),
                       (unsigned long)((tjc_task_config.vpp_mvpp % 1000U) / 100U));
        status = TJC_HMI_SetText("control.t_vpp", text);
    }
    if (status == HAL_OK)
    {
        status = TJC_HMI_SetText("control.t_hint",
                                 (hint != NULL) ? hint : "READY");
    }
    return status;
}

static HAL_StatusTypeDef TJC_HMI_API_ShowKeypad(TJC_HMI_InputKind kind)
{
    char text[24];
    HAL_StatusTypeDef status;

    tjc_input_kind = kind;
    status = TJC_HMI_SendCommand("page keypad");
    if (status != HAL_OK)
    {
        return status;
    }

    if (kind == TJC_HMI_INPUT_FREQUENCY)
    {
        (void)snprintf(text, sizeof(text), "%lu",
                       (unsigned long)tjc_task_config.frequency_hz);
        status = TJC_HMI_SetText("keypad.t_key_title", "INPUT FREQUENCY");
        if (status == HAL_OK)
        {
            status = TJC_HMI_SetText("keypad.t_unit", "Hz");
        }
        if (status == HAL_OK)
        {
            status = TJC_HMI_SendCommand("vis b_dot,0");
        }
    }
    else
    {
        (void)snprintf(text, sizeof(text), "%lu.%01lu",
                       (unsigned long)(tjc_task_config.vpp_mvpp / 1000U),
                       (unsigned long)((tjc_task_config.vpp_mvpp % 1000U) / 100U));
        status = TJC_HMI_SetText("keypad.t_key_title", "INPUT VPP");
        if (status == HAL_OK)
        {
            status = TJC_HMI_SetText("keypad.t_unit", "Vpp");
        }
        if (status == HAL_OK)
        {
            status = TJC_HMI_SendCommand("vis b_dot,1");
        }
    }

    if (status == HAL_OK)
    {
        status = TJC_HMI_SetText("keypad.t_value", text);
    }
    return status;
}

static uint8_t TJC_HMI_API_ParseFrequency(const char *text,
                                           uint32_t *frequency_hz)
{
    uint32_t value = 0U;

    if ((text == NULL) || (frequency_hz == NULL) || (*text == '\0'))
    {
        return 0U;
    }

    while ((*text >= '0') && (*text <= '9'))
    {
        uint32_t digit = (uint32_t)(*text - '0');

        if (value > ((UINT32_MAX - digit) / 10U))
        {
            return 0U;
        }
        value = value * 10U + digit;
        text++;
    }

    if ((*text != '\0') ||
        (TJC_HMI_API_ValidateFrequencyHz(tjc_task_config.task_id, value) == 0U))
    {
        return 0U;
    }

    *frequency_hz = value;
    return 1U;
}

static uint8_t TJC_HMI_API_ParseVpp(const char *text, uint32_t *vpp_mvpp)
{
    uint32_t whole = 0U;
    uint32_t tenths = 0U;
    uint8_t digit_count = 0U;

    if ((text == NULL) || (vpp_mvpp == NULL) || (*text == '\0'))
    {
        return 0U;
    }

    while ((*text >= '0') && (*text <= '9'))
    {
        uint32_t digit = (uint32_t)(*text - '0');

        if (whole > ((UINT32_MAX - digit) / 10U))
        {
            return 0U;
        }
        whole = whole * 10U + digit;
        text++;
        digit_count++;
    }

    if (digit_count == 0U)
    {
        return 0U;
    }
    if (*text == '.')
    {
        text++;
        if ((*text < '0') || (*text > '9') ||
            ((text[1] != '\0') && (text[1] != '\r')))
        {
            return 0U;
        }
        tenths = (uint32_t)(*text - '0');
        text++;
    }
    if ((*text == '\r') && (text[1] == '\0'))
    {
        text++;
    }
    if (*text != '\0')
    {
        return 0U;
    }

    if (whole > ((UINT32_MAX - tenths * 100U) / 1000U))
    {
        return 0U;
    }
    whole = whole * 1000U + tenths * 100U;
    if (TJC_HMI_API_ValidateVppMvpp(tjc_task_config.task_id, whole) == 0U)
    {
        return 0U;
    }

    *vpp_mvpp = whole;
    return 1U;
}

static void TJC_HMI_API_ReserveTask(void)
{
    tjc_state = TJC_HMI_API_STATE_RESERVED;
    TJC_HMI_API_Report("RESERVED", "TASK NOT CONNECTED");
}

static void TJC_HMI_API_SelectTask(uint8_t task_id)
{
    tjc_task_config.task_id = task_id;
    tjc_task_config.frequency_hz = TJC_HMI_DEFAULT_FREQUENCY_HZ;
    tjc_task_config.vpp_mvpp = TJC_HMI_DEFAULT_VPP_MVPP;
    tjc_state = TJC_HMI_API_STATE_SELECTED;

    if (task_id <= 2U)
    {
        TJC_HMI_API_ReserveTask();
    }
    else
    {
        tjc_state = TJC_HMI_API_STATE_CONFIGURING;
        (void)TJC_HMI_API_ShowControl(NULL);
    }
}

static void TJC_HMI_API_HandleText(const char *text)
{
    uint8_t valid = 0U;

    if (tjc_input_kind == TJC_HMI_INPUT_FREQUENCY)
    {
        valid = TJC_HMI_API_ParseFrequency(text, &tjc_task_config.frequency_hz);
    }
    else if (tjc_input_kind == TJC_HMI_INPUT_VPP)
    {
        valid = TJC_HMI_API_ParseVpp(text, &tjc_task_config.vpp_mvpp);
    }

    if (valid == 0U)
    {
        (void)TJC_HMI_SetText("keypad.t_key_title", "INVALID INPUT");
        return;
    }

    tjc_input_kind = TJC_HMI_INPUT_NONE;
    tjc_state = TJC_HMI_API_STATE_CONFIGURING;
    (void)TJC_HMI_API_ShowControl("VALUE UPDATED");
}

static void TJC_HMI_API_HandleCommand(uint8_t command)
{
    switch (command)
    {
    case TJC_HMI_CMD_ENTER_MENU:
    case TJC_HMI_CMD_BACK_MENU:
        (void)TJC_HMI_API_ShowMenu();
        break;
    case TJC_HMI_CMD_RESET_MCU:
        NVIC_SystemReset();
        break;
    case TJC_HMI_CMD_SELECT_TASK_1:
    case TJC_HMI_CMD_SELECT_TASK_2:
    case TJC_HMI_CMD_SELECT_TASK_3:
    case TJC_HMI_CMD_SELECT_TASK_4:
    case TJC_HMI_CMD_SELECT_TASK_5:
        TJC_HMI_API_SelectTask((uint8_t)(command - TJC_HMI_CMD_SELECT_TASK_1 + 1U));
        break;
    case TJC_HMI_CMD_EDIT_FREQUENCY:
        if (tjc_task_config.task_id >= 3U)
        {
            (void)TJC_HMI_API_ShowKeypad(TJC_HMI_INPUT_FREQUENCY);
        }
        break;
    case TJC_HMI_CMD_EDIT_VPP:
        if (tjc_task_config.task_id >= 3U)
        {
            (void)TJC_HMI_API_ShowKeypad(TJC_HMI_INPUT_VPP);
        }
        break;
    case TJC_HMI_CMD_KEY_OK:
        if (tjc_input_kind != TJC_HMI_INPUT_NONE)
        {
            (void)TJC_HMI_SendCommand("get keypad.t_value.txt");
        }
        break;
    case TJC_HMI_CMD_KEY_CANCEL:
    case TJC_HMI_CMD_AGAIN:
        (void)TJC_HMI_API_ShowControl(NULL);
        break;
    case TJC_HMI_CMD_START:
        if (tjc_task_config.task_id >= 3U)
        {
            TJC_HMI_API_ReserveTask();
        }
        break;
    case TJC_HMI_CMD_RESULT_HOME:
        tjc_state = TJC_HMI_API_STATE_IDLE;
        (void)TJC_HMI_API_ShowHome();
        break;
    case TJC_HMI_CMD_STOP:
        tjc_state = TJC_HMI_API_STATE_SELECTED;
        TJC_HMI_API_Report("STOPPED", "NO TASK CONNECTED");
        break;
    default:
        break;
    }
}

HAL_StatusTypeDef TJC_HMI_API_Init(UART_HandleTypeDef *huart)
{
    HAL_StatusTypeDef status;

    if (huart == NULL)
    {
        return HAL_ERROR;
    }

    TJC_HMI_Init(huart);
    tjc_task_config.task_id = 0U;
    tjc_task_config.frequency_hz = TJC_HMI_DEFAULT_FREQUENCY_HZ;
    tjc_task_config.vpp_mvpp = TJC_HMI_DEFAULT_VPP_MVPP;
    tjc_input_kind = TJC_HMI_INPUT_NONE;
    tjc_state = TJC_HMI_API_STATE_IDLE;

    status = TJC_HMI_SendCommand("bkcmd=0");
    return (status == HAL_OK) ? TJC_HMI_API_ShowHome() : status;
}

void TJC_HMI_API_Process(void)
{
    TJC_HMI_Event event = TJC_HMI_Poll();

    if (event.type == TJC_HMI_EVENT_COMMAND)
    {
        TJC_HMI_API_HandleCommand(event.command);
    }
    else if (event.type == TJC_HMI_EVENT_TEXT)
    {
        TJC_HMI_API_HandleText(event.text);
    }
    else if (event.type == TJC_HMI_EVENT_READY)
    {
        (void)TJC_HMI_API_ShowHome();
    }
}

const TJC_HMI_API_TaskConfig *TJC_HMI_API_GetTaskConfig(void)
{
    return &tjc_task_config;
}

TJC_HMI_API_State TJC_HMI_API_GetState(void)
{
    return tjc_state;
}

void TJC_HMI_API_Report(const char *status, const char *detail)
{
    char text[32];

    (void)TJC_HMI_SendCommand("page result");
    (void)TJC_HMI_SetText("result.t_status",
                          (status != NULL) ? status : "UNKNOWN");
    (void)snprintf(text, sizeof(text), "TASK %u", tjc_task_config.task_id);
    (void)TJC_HMI_SetText("result.t_task", text);
    (void)snprintf(text, sizeof(text), "%lu Hz",
                   (unsigned long)tjc_task_config.frequency_hz);
    (void)TJC_HMI_SetText("result.t_freq", text);
    (void)snprintf(text, sizeof(text), "%lu.%01lu Vpp",
                   (unsigned long)(tjc_task_config.vpp_mvpp / 1000U),
                   (unsigned long)((tjc_task_config.vpp_mvpp % 1000U) / 100U));
    (void)TJC_HMI_SetText("result.t_vpp", text);
    (void)TJC_HMI_SetText("result.t_detail",
                          (detail != NULL) ? detail : "NO DETAIL");

    printf("HMI task=%u freq=%luHz vpp=%lumVpp status=%s detail=%s\r\n",
           tjc_task_config.task_id,
           (unsigned long)tjc_task_config.frequency_hz,
           (unsigned long)tjc_task_config.vpp_mvpp,
           (status != NULL) ? status : "UNKNOWN",
           (detail != NULL) ? detail : "NO DETAIL");
}
