#include "G_DIGITAL_MODEL_API.h"

#include "G_DIGITAL_MODEL_FML.h"
#include <stdio.h>

#define G_DIGITAL_MODEL_KEY_DEBOUNCE_MS  30U
#define G_DIGITAL_MODEL_KEY_PRESSED      GPIO_PIN_RESET

static GPIO_PinState g_last_key_state = GPIO_PIN_SET;
static GPIO_PinState g_stable_key_state = GPIO_PIN_SET;
static uint32_t g_key_change_tick = 0U;

static HAL_StatusTypeDef g_digital_model_start(void)
{
    HAL_StatusTypeDef status = G_DigitalModelFML_Start();

    if (status == HAL_OK)
    {
        HAL_GPIO_WritePin(G_MODEL_LED_GPIO_Port,
                          G_MODEL_LED_Pin,
                          G_MODEL_LED_ON_STATE);
        printf("MODEL ADC=PA1_C DAC=PA4 Fs=1000000 Hnum=1\r\n");
    }
    else
    {
        printf("ERR digital model start=%u\r\n", (unsigned int)status);
    }
    return status;
}

void G_DigitalModelAPI_Init(void)
{
    HAL_GPIO_WritePin(G_MODEL_LED_GPIO_Port,
                      G_MODEL_LED_Pin,
                      G_MODEL_LED_OFF_STATE);
    g_last_key_state = HAL_GPIO_ReadPin(G_MODEL_KEY_GPIO_Port,
                                        G_MODEL_KEY_Pin);
    g_stable_key_state = g_last_key_state;
    g_key_change_tick = HAL_GetTick();
    printf("MODEL idle key=PB1 led=PC13\r\n");
}

void G_DigitalModelAPI_Process(void)
{
    GPIO_PinState key_state = HAL_GPIO_ReadPin(G_MODEL_KEY_GPIO_Port,
                                               G_MODEL_KEY_Pin);
    uint32_t now = HAL_GetTick();

    if (key_state != g_last_key_state)
    {
        g_last_key_state = key_state;
        g_key_change_tick = now;
    }

    if ((key_state != g_stable_key_state) &&
        ((now - g_key_change_tick) >= G_DIGITAL_MODEL_KEY_DEBOUNCE_MS))
    {
        g_stable_key_state = key_state;
        if ((key_state == G_DIGITAL_MODEL_KEY_PRESSED) &&
            !G_DigitalModelFML_IsRunning())
        {
            (void)g_digital_model_start();
        }
    }
}

HAL_StatusTypeDef G_DigitalModelAPI_Stop(void)
{
    HAL_StatusTypeDef status = G_DigitalModelFML_Stop();

    HAL_GPIO_WritePin(G_MODEL_LED_GPIO_Port,
                      G_MODEL_LED_Pin,
                      G_MODEL_LED_OFF_STATE);
    return status;
}
