#include "AD9226_VOFA_API.h"

#include "AD9226.h"
#include "USART_FML.h"

#include "usart.h"

#include <stdio.h>

/* Calibration constants copied from the reference AD9226 input-board driver.
   They are for waveform observation only and require later board calibration. */
#define AD9226_VOFA_INPUT_SPAN_MV 10000.0f
#define AD9226_VOFA_ZERO_CODE     2458.0f
#define AD9226_VOFA_FRONTEND_GAIN 1.0f

static uint8_t s_done;
static uint8_t s_error;

static float ad9226_vofa_code_to_input_millivolts(uint16_t code)
{
    return (((float)code - AD9226_VOFA_ZERO_CODE) *
            AD9226_VOFA_INPUT_SPAN_MV) /
           (4096.0f * AD9226_VOFA_FRONTEND_GAIN);
}

HAL_StatusTypeDef AD9226_VOFA_API_Init(void)
{
    s_done = 0U;
    s_error = 0U;
    return AD9226_Init();
}

void AD9226_VOFA_API_Process(void)
{
    const uint16_t *samples;
    uint32_t sample_count;
    char message[40];

    if ((s_done != 0U) || (s_error != 0U))
    {
        return;
    }

    if (AD9226_CaptureFrame() != HAL_OK)
    {
        s_error = 1U;
        return;
    }

    samples = AD9226_GetCaptureBuffer();
    sample_count = AD9226_GetCaptureLength();
    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        const float millivolts = ad9226_vofa_code_to_input_millivolts(samples[index]);
        const int length = snprintf(message, sizeof(message),
                                    "ad9226_raw_mV:%.3f\r\n",
                                    (double)millivolts);

        if ((length <= 0) || (length >= (int)sizeof(message)) ||
            (Usart_Send_Computer(&huart3, message) != HAL_OK))
        {
            s_error = 1U;
            return;
        }
    }

    s_done = 1U;
}

bool AD9226_VOFA_API_HasError(void)
{
    return (s_error != 0U);
}
