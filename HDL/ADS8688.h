#ifndef ADS8688_H
#define ADS8688_H

#include "main.h"

typedef enum
{
    ADS8688_RANGE_BIPOLAR_10V24 = 0x00U,
    ADS8688_RANGE_BIPOLAR_5V12  = 0x01U,
    ADS8688_RANGE_BIPOLAR_2V56  = 0x02U,
    ADS8688_RANGE_UNIPOLAR_10V24 = 0x03U,
    ADS8688_RANGE_UNIPOLAR_5V12  = 0x04U
} ADS8688_Range;

HAL_StatusTypeDef ADS8688_Init(void);
HAL_StatusTypeDef ADS8688_WriteCommand(uint16_t command);
HAL_StatusTypeDef ADS8688_WriteProgramRegister(uint8_t address, uint8_t data);
HAL_StatusTypeDef ADS8688_ReadProgramRegister(uint8_t address, uint8_t *data);
HAL_StatusTypeDef ADS8688_SetChannelRange(uint8_t channel, ADS8688_Range range);
HAL_StatusTypeDef ADS8688_SetAutoScanMask(uint8_t channel_mask);
HAL_StatusTypeDef ADS8688_SetChannelPowerDownMask(uint8_t channel_mask);
HAL_StatusTypeDef ADS8688_StartAutoScan(void);
HAL_StatusTypeDef ADS8688_SelectManualChannel(uint8_t channel);
HAL_StatusTypeDef ADS8688_ReadConversion(uint16_t *code);
HAL_StatusTypeDef ADS8688_ReadAutoScan(uint16_t *codes,
                                       uint8_t channel_count);
HAL_StatusTypeDef ADS8688_ReadManualChannels(uint16_t *codes,
                                             uint8_t channel_count);
uint32_t ADS8688_GetErrorCount(void);

#endif
