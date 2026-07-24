#include "DPLL_BLL.h"

#include "DLIA_BLL.h"

#include <stddef.h>

static float dpll_bll_wrap_accumulator(float value, float limit)
{
    while (value > limit)
    {
        value -= 2.0f * limit;
    }
    while (value <= -limit)
    {
        value += 2.0f * limit;
    }
    return value;
}

bool DPLL_BLL_Init(const dpll_bll_config_t *config, dpll_bll_state_t *state)
{
    if ((config == NULL) || (state == NULL) || (config->kp < 0.0f) ||
        (config->ki < 0.0f) || (config->actuator_direction == 0.0f) ||
        (config->integral_limit_deg <= 0.0f))
    {
        return false;
    }

    state->integral_deg = DLIA_BLL_WrapPhaseDeg(config->initial_phase_command_deg);
    state->phase_command_deg = state->integral_deg;
    return true;
}

bool DPLL_BLL_Update(const dpll_bll_config_t *config, dpll_bll_state_t *state,
                     float measured_phase_deg, dpll_bll_result_t *result)
{
    float calibrated_phase_deg;
    float controller_output_deg;

    if ((config == NULL) || (state == NULL) || (result == NULL))
    {
        return false;
    }

    calibrated_phase_deg = DLIA_BLL_WrapPhaseDeg(measured_phase_deg -
                                                  config->phase_calibration_deg);
    result->error_deg = DLIA_BLL_WrapPhaseDeg(calibrated_phase_deg -
                                               config->phase_target_deg);
    /* The AD9833 phase register is circular.  Clamp would stop frequency
       correction after one half-turn; retain the accumulated correction while
       wrapping it to the equivalent phase word range instead. */
    state->integral_deg = dpll_bll_wrap_accumulator(
        state->integral_deg + config->ki * result->error_deg,
        config->integral_limit_deg);
    controller_output_deg = config->kp * result->error_deg +
                            state->integral_deg;
    state->phase_command_deg = DLIA_BLL_WrapPhaseDeg(
        config->actuator_direction * controller_output_deg);

    result->measured_phase_deg = calibrated_phase_deg;
    result->phase_command_deg = state->phase_command_deg;
    return true;
}
