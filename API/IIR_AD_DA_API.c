#include "IIR_AD_DA_API.h"

#include "IIR_ADDA_FML.h"

HAL_StatusTypeDef IIR_AD_DA_API_Init(void)
{
    return IIR_ADDA_FML_Start();
}

void IIR_AD_DA_API_Process(void)
{
    /* The continuous ADC-to-DAC path runs in ADC DMA half/full callbacks. */
}
