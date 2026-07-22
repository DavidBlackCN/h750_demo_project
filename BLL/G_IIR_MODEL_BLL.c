#include "G_IIR_MODEL_BLL.h"

/*
 * Bilinear transform at Fs=1 MHz:
 *   b0=1/40601, b1=2/40601, b2=1/40601
 *   a1=-79998/40601, a2=39401/40601
 * CMSIS-DSP DF1 stores feedback coefficients as -a1 and -a2.
 */
static float32_t g_iir_coefficients[5] =
{
    2.46299352e-5f,
    4.92598704e-5f,
    2.46299352e-5f,
    1.97034556f,
   -0.97044408f
};

static float32_t g_iir_state[4];
static arm_biquad_casd_df1_inst_f32 g_iir_instance;

void G_IIR_ModelBLL_Init(void)
{
    arm_biquad_cascade_df1_init_f32(&g_iir_instance,
                                    1U,
                                    g_iir_coefficients,
                                    g_iir_state);
}

void G_IIR_ModelBLL_Process(const float32_t *input,
                            float32_t *output,
                            uint32_t length)
{
    arm_biquad_cascade_df1_f32(&g_iir_instance,
                               input,
                               output,
                               length);
}
