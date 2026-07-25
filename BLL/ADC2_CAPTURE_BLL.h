#ifndef ADC2_CAPTURE_BLL_H
#define ADC2_CAPTURE_BLL_H

#include "ADC2_CAPTURE_FML.h"

extern float adc2_capture_data[ADC2_DMA_BUFFER_LENGTH];

/* Same front-end processing stage as the reference adc2_deal(). */
void adc2_deal(void);

#endif
