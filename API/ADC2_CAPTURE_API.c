#include "ADC2_CAPTURE_API.h"

#include "ADC2_CAPTURE_BLL.h"
#include "ADC2_CAPTURE_FML.h"
#include "USART_BLL.h"
#include "usart.h"

void adc2_proc(void)
{
    adc2_deal();

    if (adc2_proc_flag != 0U)
    {
        (void)Usart_Send_ADC_Data(adc2_capture_data, &huart1,
                                  ADC2_DMA_BUFFER_LENGTH);
        adc2_proc_flag = 0U;
    }
}
