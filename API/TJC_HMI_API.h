#ifndef TJC_HMI_API_H
#define TJC_HMI_API_H

#include "main.h"

#include <stdint.h>

typedef struct
{
    uint8_t task_id;
    uint32_t frequency_hz;
    uint32_t vpp_mvpp;
} TJC_HMI_API_TaskConfig;

typedef enum
{
    TJC_HMI_API_STATE_IDLE = 0,
    TJC_HMI_API_STATE_SELECTED,
    TJC_HMI_API_STATE_CONFIGURING,
    TJC_HMI_API_STATE_RESERVED
} TJC_HMI_API_State;

HAL_StatusTypeDef TJC_HMI_API_Init(UART_HandleTypeDef *huart);
void TJC_HMI_API_Process(void);
const TJC_HMI_API_TaskConfig *TJC_HMI_API_GetTaskConfig(void);
TJC_HMI_API_State TJC_HMI_API_GetState(void);
void TJC_HMI_API_Report(const char *status, const char *detail);

/* Future task hooks. The generic template does not call them yet. */
HAL_StatusTypeDef TJC_HMI_API_OnTaskStart(const TJC_HMI_API_TaskConfig *config);
void TJC_HMI_API_OnTaskStop(void);
uint8_t TJC_HMI_API_ValidateFrequencyHz(uint8_t task_id, uint32_t frequency_hz);
uint8_t TJC_HMI_API_ValidateVppMvpp(uint8_t task_id, uint32_t vpp_mvpp);

#endif
