#include "TJC_HMI_API.h"

#include "G_BASIC_API.h"
#include "TJC_HMI.h"

#include <stdbool.h>
#include <stdio.h>

#define TJC_HMI_BOOT_WAIT_MS             800U
#define TJC_HMI_DEFAULT_FREQUENCY_HZ    1000U
#define TJC_HMI_DEFAULT_TARGET_TENTHS     20U

#define TJC_HMI_CMD_ENTER_MENU          0x01U
#define TJC_HMI_CMD_SELECT_R2           0x02U
#define TJC_HMI_CMD_SELECT_R3           0x03U
#define TJC_HMI_CMD_SELECT_R4           0x04U
#define TJC_HMI_CMD_EDIT_FREQUENCY      0x10U
#define TJC_HMI_CMD_EDIT_VPP            0x11U
#define TJC_HMI_CMD_KEY_OK              0x20U
#define TJC_HMI_CMD_KEY_CANCEL          0x21U
#define TJC_HMI_CMD_START               0x30U
#define TJC_HMI_CMD_BACK_MENU           0x31U
#define TJC_HMI_CMD_AGAIN               0x32U
#define TJC_HMI_CMD_RESULT_HOME         0x33U

typedef enum
{
    TJC_HMI_INPUT_NONE = 0,
    TJC_HMI_INPUT_FREQUENCY,
    TJC_HMI_INPUT_VPP
} TJC_HMI_InputKind;

static uint8_t tjc_requirement;
static uint32_t tjc_frequency_hz;
static uint8_t tjc_target_tenths_vpp;
static TJC_HMI_InputKind tjc_input_kind;

static HAL_StatusTypeDef TJC_HMI_API_SetVisible(const char *object,
                                                bool visible)
{
    char command[48];
    int length;

    length = snprintf(command, sizeof(command), "vis %s,%u", object,
                      visible ? 1U : 0U);
    if ((length < 0) || ((size_t)length >= sizeof(command)))
    {
        return HAL_ERROR;
    }
    return TJC_HMI_SendCommand(command);
}

static HAL_StatusTypeDef TJC_HMI_API_ShowMenu(void)
{
    tjc_input_kind = TJC_HMI_INPUT_NONE;
    return TJC_HMI_SendCommand("page menu");
}

static const char *TJC_HMI_API_GetRequirementName(void)
{
    if (tjc_requirement == 2U)
    {
        return "REQUIREMENT 2";
    }
    if (tjc_requirement == 3U)
    {
        return "REQUIREMENT 3";
    }
    return "REQUIREMENT 4";
}

static HAL_StatusTypeDef TJC_HMI_API_ShowControl(const char *hint)
{
    char text[32];
    HAL_StatusTypeDef status;
    bool frequency_editable = (tjc_requirement != 3U);
    bool vpp_editable = (tjc_requirement == 4U);

    status = TJC_HMI_SendCommand("page control");
    if (status != HAL_OK)
    {
        return status;
    }
    status = TJC_HMI_SetText("control.t_mode",
                             TJC_HMI_API_GetRequirementName());
    if (status != HAL_OK)
    {
        return status;
    }

    (void)snprintf(text, sizeof(text), "%lu Hz",
                   (unsigned long)tjc_frequency_hz);
    status = TJC_HMI_SetText("control.t_freq", text);
    if (status != HAL_OK)
    {
        return status;
    }

    (void)snprintf(text, sizeof(text), "%u.%u Vpp",
                   tjc_target_tenths_vpp / 10U,
                   tjc_target_tenths_vpp % 10U);
    status = TJC_HMI_SetText("control.t_vpp", text);
    if (status != HAL_OK)
    {
        return status;
    }

    status = TJC_HMI_SetText("control.t_hint",
                             (hint != NULL) ? hint : "READY");
    if (status != HAL_OK)
    {
        return status;
    }

    status = TJC_HMI_API_SetVisible("b_freq", frequency_editable);
    if (status != HAL_OK)
    {
        return status;
    }
    return TJC_HMI_API_SetVisible("b_vpp", vpp_editable);
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
                       (unsigned long)tjc_frequency_hz);
        status = TJC_HMI_SetText("keypad.t_key_title", "INPUT FREQUENCY");
        if (status == HAL_OK)
        {
            status = TJC_HMI_SetText("keypad.t_unit", "Hz");
        }
        if (status == HAL_OK)
        {
            status = TJC_HMI_API_SetVisible("b_dot", false);
        }
    }
    else
    {
        (void)snprintf(text, sizeof(text), "%u.%u",
                       tjc_target_tenths_vpp / 10U,
                       tjc_target_tenths_vpp % 10U);
        status = TJC_HMI_SetText("keypad.t_key_title", "INPUT TARGET VPP");
        if (status == HAL_OK)
        {
            status = TJC_HMI_SetText("keypad.t_unit", "Vpp");
        }
        if (status == HAL_OK)
        {
            status = TJC_HMI_API_SetVisible("b_dot", true);
        }
    }

    if (status == HAL_OK)
    {
        status = TJC_HMI_SetText("keypad.t_value", text);
    }
    return status;
}

static bool TJC_HMI_API_ParseFrequency(const char *text,
                                       uint32_t *frequency_hz)
{
    uint32_t value = 0U;
    const char *cursor = text;
    uint32_t maximum = (tjc_requirement == 2U) ?
                       G_BASIC_API_SIGNAL_FREQUENCY_MAX_HZ :
                       G_MODEL_BLL_FREQUENCY_MAX_HZ;

    if ((text == NULL) || (frequency_hz == NULL) || (*cursor == '\0'))
    {
        return false;
    }
    while ((*cursor >= '0') && (*cursor <= '9'))
    {
        uint32_t digit = (uint32_t)(*cursor - '0');

        if (value > ((UINT32_MAX - digit) / 10U))
        {
            return false;
        }
        value = value * 10U + digit;
        cursor++;
    }
    if ((*cursor != '\0') ||
        (value < G_BASIC_API_SIGNAL_FREQUENCY_MIN_HZ) ||
        (value > maximum) ||
        ((value % G_BASIC_API_SIGNAL_FREQUENCY_STEP_HZ) != 0U))
    {
        return false;
    }

    *frequency_hz = value;
    return true;
}

static bool TJC_HMI_API_ParseVpp(const char *text, uint8_t *target_tenths)
{
    uint32_t whole;
    uint32_t tenths = 0U;

    if ((text == NULL) || (target_tenths == NULL) ||
        (text[0] < '0') || (text[0] > '9'))
    {
        return false;
    }

    whole = (uint32_t)(text[0] - '0');
    text++;
    if (*text == '.')
    {
        text++;
        if ((*text < '0') || (*text > '9'))
        {
            return false;
        }
        tenths = (uint32_t)(*text - '0');
        text++;
    }
    if ((*text != '\0') ||
        ((whole * 10U + tenths) < G_MODEL_BLL_TARGET_MIN_TENTHS_VPP) ||
        ((whole * 10U + tenths) > G_MODEL_BLL_TARGET_MAX_TENTHS_VPP))
    {
        return false;
    }

    *target_tenths = (uint8_t)(whole * 10U + tenths);
    return true;
}

static HAL_StatusTypeDef TJC_HMI_API_ShowResult(HAL_StatusTypeDef run_status)
{
    const G_BasicAPI_Result *result = G_BasicAPI_GetResult();
    char text[40];
    float gain = result->model_gain;
    uint8_t gain_index;
    HAL_StatusTypeDef status;

    status = TJC_HMI_SendCommand("page result");
    if (status != HAL_OK)
    {
        return status;
    }
    status = TJC_HMI_SetText("result.t_status",
                             (run_status == HAL_OK) ? "RUNNING" : "ERROR");
    if (status != HAL_OK)
    {
        return status;
    }
    status = TJC_HMI_SetText("result.t_mode",
                             TJC_HMI_API_GetRequirementName());
    if (status != HAL_OK)
    {
        return status;
    }

    (void)snprintf(text, sizeof(text), "%lu Hz",
                   (unsigned long)result->frequency_hz);
    (void)TJC_HMI_SetText("result.t_freq", text);
    (void)snprintf(text, sizeof(text), "%u.%u Vpp",
                   result->target_output_mvpp / 1000U,
                   (result->target_output_mvpp % 1000U) / 100U);
    (void)TJC_HMI_SetText("result.t_target", text);

    if (result->requirement == 2U)
    {
        (void)TJC_HMI_SetText("result.t_gain", "N/A");
    }
    else if ((gain <= 0.0f) &&
        (G_ModelBLL_LookupGain(result->frequency_hz, &gain_index,
                               &gain) != G_MODEL_BLL_OK))
    {
        (void)TJC_HMI_SetText("result.t_gain", "N/A");
    }
    else
    {
        (void)snprintf(text, sizeof(text), "%.4f", gain);
        (void)TJC_HMI_SetText("result.t_gain", text);
    }

    (void)snprintf(text, sizeof(text), "%.1f mVpp",
                   result->source_output_mvpp);
    (void)TJC_HMI_SetText("result.t_source", text);
    (void)snprintf(text, sizeof(text), "%u", result->amplitude_code);
    (void)TJC_HMI_SetText("result.t_asf", text);

    printf("HMI R%u f=%luHz target=%.1fVpp source=%.1fmVpp "
           "ASF=%u status=%s\r\n",
           result->requirement, (unsigned long)result->frequency_hz,
           (float)result->target_output_mvpp / 1000.0f,
           result->source_output_mvpp, result->amplitude_code,
           (run_status == HAL_OK) ? "OK" : "ERR");
    return run_status;
}

static void TJC_HMI_API_SelectRequirement(uint8_t requirement)
{
    tjc_requirement = requirement;
    tjc_frequency_hz = TJC_HMI_DEFAULT_FREQUENCY_HZ;
    tjc_target_tenths_vpp = (requirement == 2U) ? 30U :
                            TJC_HMI_DEFAULT_TARGET_TENTHS;
    (void)TJC_HMI_API_ShowControl(NULL);
}

static void TJC_HMI_API_Start(void)
{
    HAL_StatusTypeDef status;

    if (tjc_requirement == 2U)
    {
        status = G_BasicAPI_StartRequirement2(tjc_frequency_hz);
    }
    else if (tjc_requirement == 3U)
    {
        status = G_BasicAPI_StartRequirement3();
    }
    else
    {
        status = G_BasicAPI_StartRequirement4(tjc_frequency_hz,
                                              tjc_target_tenths_vpp);
    }
    (void)TJC_HMI_API_ShowResult(status);
}

static void TJC_HMI_API_HandleCommand(uint8_t command)
{
    switch (command)
    {
    case TJC_HMI_CMD_ENTER_MENU:
    case TJC_HMI_CMD_BACK_MENU:
        (void)TJC_HMI_API_ShowMenu();
        break;
    case TJC_HMI_CMD_SELECT_R2:
        TJC_HMI_API_SelectRequirement(2U);
        break;
    case TJC_HMI_CMD_SELECT_R3:
        TJC_HMI_API_SelectRequirement(3U);
        break;
    case TJC_HMI_CMD_SELECT_R4:
        TJC_HMI_API_SelectRequirement(4U);
        break;
    case TJC_HMI_CMD_EDIT_FREQUENCY:
        if (tjc_requirement != 3U)
        {
            (void)TJC_HMI_API_ShowKeypad(TJC_HMI_INPUT_FREQUENCY);
        }
        break;
    case TJC_HMI_CMD_EDIT_VPP:
        if (tjc_requirement == 4U)
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
        TJC_HMI_API_Start();
        break;
    case TJC_HMI_CMD_RESULT_HOME:
        (void)TJC_HMI_SendCommand("page home");
        break;
    default:
        break;
    }
}

static void TJC_HMI_API_HandleText(const char *text)
{
    bool valid = false;

    if (tjc_input_kind == TJC_HMI_INPUT_FREQUENCY)
    {
        valid = TJC_HMI_API_ParseFrequency(text, &tjc_frequency_hz);
    }
    else if (tjc_input_kind == TJC_HMI_INPUT_VPP)
    {
        valid = TJC_HMI_API_ParseVpp(text, &tjc_target_tenths_vpp);
    }

    if (!valid)
    {
        (void)TJC_HMI_SetText("keypad.t_key_title", "INVALID INPUT");
        return;
    }

    tjc_input_kind = TJC_HMI_INPUT_NONE;
    (void)TJC_HMI_API_ShowControl("VALUE UPDATED");
}

HAL_StatusTypeDef TJC_HMI_API_Init(UART_HandleTypeDef *huart)
{
    HAL_StatusTypeDef status;

    if (huart == NULL)
    {
        return HAL_ERROR;
    }

    TJC_HMI_Init(huart);
    tjc_requirement = 4U;
    tjc_frequency_hz = TJC_HMI_DEFAULT_FREQUENCY_HZ;
    tjc_target_tenths_vpp = TJC_HMI_DEFAULT_TARGET_TENTHS;
    tjc_input_kind = TJC_HMI_INPUT_NONE;

    HAL_Delay(TJC_HMI_BOOT_WAIT_MS);
    status = TJC_HMI_SendCommand("bkcmd=0");
    if (status == HAL_OK)
    {
        status = TJC_HMI_SendCommand("page home");
    }
    return status;
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
        (void)TJC_HMI_SendCommand("page home");
    }
}
