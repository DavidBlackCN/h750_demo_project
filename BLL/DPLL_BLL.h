#ifndef DPLL_BLL_H
#define DPLL_BLL_H

#include <stdbool.h>

typedef struct
{
    float kp;
    float ki;
    float phase_target_deg;
    float phase_calibration_deg;
    float initial_phase_command_deg;
    float actuator_direction;
    float integral_limit_deg;
} dpll_bll_config_t;

typedef struct
{
    float integral_deg;
    float phase_command_deg;
} dpll_bll_state_t;

typedef struct
{
    float measured_phase_deg;
    float error_deg;
    float phase_command_deg;
} dpll_bll_result_t;

bool DPLL_BLL_Init(const dpll_bll_config_t *config, dpll_bll_state_t *state);
bool DPLL_BLL_Update(const dpll_bll_config_t *config, dpll_bll_state_t *state,
                     float measured_phase_deg, dpll_bll_result_t *result);

#endif
