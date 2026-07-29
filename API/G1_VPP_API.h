#ifndef G1_VPP_API_H
#define G1_VPP_API_H

#include "G1_VPP_BLL.h"
#include "G1_FFT_API.h"

HAL_StatusTypeDef G1_VPP_API_Init(void);
void G1_VPP_API_Process(void);
void G1_VPP_API_SetCalibration(const g1_vpp_calibration_t *calibration);
const g1_vpp_result_t *G1_VPP_API_GetResult(void);
const g1_fft_result_t *G1_VPP_API_GetFftResult(void);
bool G1_VPP_API_HasError(void);
uint32_t G1_VPP_API_GetErrorCode(void);

#endif
