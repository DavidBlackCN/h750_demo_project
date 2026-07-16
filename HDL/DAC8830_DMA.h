#ifndef DAC8830_DMA_H
#define DAC8830_DMA_H

#include "main.h"

HAL_StatusTypeDef DAC8830_DMA_StartWaveform(const uint16_t *codes,
                                            uint16_t length,
                                            uint32_t sampleRateHz);
void DAC8830_DMA_Stop(void);
HAL_StatusTypeDef DAC8830_DMA_GetLastStatus(void);
uint32_t DAC8830_DMA_GetErrorCount(void);

#endif /* DAC8830_DMA_H */
