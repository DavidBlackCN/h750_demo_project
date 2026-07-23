#include "ADC_VOFA_API.h"
#include "ADC_VOFA_FML.h"

HAL_StatusTypeDef ADC_VOFA_API_Init(void)
{
    return ADC_VOFA_FML_Start();
}

void ADC_VOFA_API_Process(void)
{
    ADC_VOFA_FML_Process();
}
