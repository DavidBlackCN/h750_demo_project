#include "TJC_HMI_API.h"

#include "TJC_HMI.h"

#define TJC_HMI_BOOT_WAIT_MS   800U
#define TJC_HMI_UPDATE_PERIOD_MS 1000U

static uint32_t tjc_demo_counter;
static uint32_t tjc_demo_last_update;
static uint8_t tjc_demo_running;

static HAL_StatusTypeDef TJC_HMI_API_ShowState(void)
{
    HAL_StatusTypeDef status;

    status = TJC_HMI_SetText("main.t0", tjc_demo_running ? "RUNNING" : "PAUSED");
    if (status != HAL_OK)
    {
        return status;
    }

    return TJC_HMI_SetText("main.b0", tjc_demo_running ? "PAUSE" : "START");
}

HAL_StatusTypeDef TJC_HMI_API_Init(UART_HandleTypeDef *huart)
{
    HAL_StatusTypeDef status;

    TJC_HMI_Init(huart);
    tjc_demo_counter = 0U;
    tjc_demo_running = 1U;

    HAL_Delay(TJC_HMI_BOOT_WAIT_MS);
    status = TJC_HMI_SendCommand("bkcmd=0");
    if (status != HAL_OK)
    {
        return status;
    }
    status = TJC_HMI_SendCommand("page main");
    if (status != HAL_OK)
    {
        return status;
    }
    status = TJC_HMI_API_ShowState();
    if (status != HAL_OK)
    {
        return status;
    }
    status = TJC_HMI_SetValue("main.n0", (int32_t)tjc_demo_counter);
    tjc_demo_last_update = HAL_GetTick();
    return status;
}

void TJC_HMI_API_Process(void)
{
    TJC_HMI_Event event = TJC_HMI_Poll();
    uint32_t now = HAL_GetTick();

    if (event.type == TJC_HMI_EVENT_DEMO_TOGGLE)
    {
        tjc_demo_running = (uint8_t)!tjc_demo_running;
        tjc_demo_last_update = now;
        (void)TJC_HMI_API_ShowState();
    }
    else if (event.type == TJC_HMI_EVENT_DEMO_RESET)
    {
        tjc_demo_counter = 0U;
        tjc_demo_last_update = now;
        (void)TJC_HMI_SetValue("main.n0", (int32_t)tjc_demo_counter);
    }

    if (tjc_demo_running && ((uint32_t)(now - tjc_demo_last_update) >= TJC_HMI_UPDATE_PERIOD_MS))
    {
        tjc_demo_last_update = now;
        tjc_demo_counter++;
        (void)TJC_HMI_SetValue("main.n0", (int32_t)tjc_demo_counter);
    }
}
