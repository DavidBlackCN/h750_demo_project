#ifndef G_BASIC_API_H
#define G_BASIC_API_H

#include "G_MODEL_BLL.h"
#include "main.h"

#define G_BASIC_API_SIGNAL_FREQUENCY_MIN_HZ       100U
#define G_BASIC_API_SIGNAL_FREQUENCY_MAX_HZ   1000000U
#define G_BASIC_API_SIGNAL_FREQUENCY_STEP_HZ      100U
#define G_BASIC_API_SIGNAL_OUTPUT_MVPP            3000U

typedef enum
{
    G_BASIC_API_IDLE = 0,
    G_BASIC_API_RUNNING,
    G_BASIC_API_PARAMETER_ERROR,
    G_BASIC_API_HARDWARE_ERROR
} G_BasicAPI_State;

typedef struct
{
    G_BasicAPI_State state;
    uint8_t requirement;
    uint32_t frequency_hz;
    uint16_t target_output_mvpp;
    float source_output_mvpp;
    float model_gain;
    uint8_t gain_index;
    uint16_t amplitude_code;
    uint32_t start_elapsed_ms;
} G_BasicAPI_Result;

HAL_StatusTypeDef G_BasicAPI_StartRequirement2(uint32_t frequency_hz);
HAL_StatusTypeDef G_BasicAPI_StartRequirement3(void);
HAL_StatusTypeDef G_BasicAPI_StartRequirement4(uint32_t frequency_hz,
                                               uint8_t target_tenths_vpp);
const G_BasicAPI_Result *G_BasicAPI_GetResult(void);

#endif
