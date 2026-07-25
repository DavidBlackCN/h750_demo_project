#include "ADC_VOFA_API.h"
#include "ADC_VOFA_FML.h"

HAL_StatusTypeDef ADC_VOFA_API_Init(void)
{
    return ADC_VOFA_FML_Start();
}

void ADC_VOFA_API_Process(void)
{
    /*
     * 前台轮询入口：DMA 回调只置完成/错误标志，停止外设、维护 D-Cache
     * 以及 1024 点 VOFA+ 串口发送均在这里执行，不能放到中断上下文。
     */
    ADC_VOFA_FML_Process();
}
