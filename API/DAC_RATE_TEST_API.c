#include "DAC_RATE_TEST_API.h"

#include "DAC_RATE_TEST_FML.h"

HAL_StatusTypeDef DAC_RateTest_API_Init(void)
{
    return DAC_RateTest_FML_Start();
}
