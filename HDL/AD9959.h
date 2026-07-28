#ifndef TEST_9959_H
#define TEST_9959_H

#include "stdint.h"
#include "stm32h7xx_hal.h"
#include "main.h"

#define AD9959_CS_1 HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, GPIO_PIN_SET)
#define AD9959_SCLK_1 HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port, AD9959_SCLK_Pin, GPIO_PIN_SET)
#define AD9959_UPDATE_1 HAL_GPIO_WritePin(AD9959_UPDATE_GPIO_Port, AD9959_UPDATE_Pin, GPIO_PIN_SET)
#define AD9959_SDIO0_1 HAL_GPIO_WritePin(AD9959_SDIO0_GPIO_Port, AD9959_SDIO0_Pin, GPIO_PIN_SET)
#define AD9959_PDC_1 HAL_GPIO_WritePin(AD9959_PDC_GPIO_Port, AD9959_PDC_Pin, GPIO_PIN_SET)
#define AD9959_RESET_1 HAL_GPIO_WritePin(AD9959_RESET_GPIO_Port, AD9959_RESET_Pin, GPIO_PIN_SET)

#define AD9959_CS_0 HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, GPIO_PIN_RESET)
#define AD9959_SCLK_0 HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port, AD9959_SCLK_Pin, GPIO_PIN_RESET)
#define AD9959_UPDATE_0 HAL_GPIO_WritePin(AD9959_UPDATE_GPIO_Port, AD9959_UPDATE_Pin, GPIO_PIN_RESET)
#define AD9959_SDIO0_0 HAL_GPIO_WritePin(AD9959_SDIO0_GPIO_Port, AD9959_SDIO0_Pin, GPIO_PIN_RESET)
#define AD9959_PDC_0 HAL_GPIO_WritePin(AD9959_PDC_GPIO_Port, AD9959_PDC_Pin, GPIO_PIN_RESET)
#define AD9959_RESET_0 HAL_GPIO_WritePin(AD9959_RESET_GPIO_Port, AD9959_RESET_Pin, GPIO_PIN_RESET)

#define CSR_ADD   0x00
#define FR1_ADD   0x01
#define FR2_ADD   0x02
#define CFR_ADD   0x03
#define CFTW0_ADD 0x04
#define CPOW0_ADD 0x05
#define ACR_ADD   0x06
#define LSRR_ADD  0x07
#define RDW_ADD   0x08
#define FDW_ADD   0x09
#define PROFILE_ADDR_BASE 0x0A

#define CH0 0x10
#define CH1 0x20
#define CH2 0x40
#define CH3 0x80

#define LEVEL_MOD_2  0x00
#define LEVEL_MOD_4  0x01
#define LEVEL_MOD_8  0x02
#define LEVEL_MOD_16 0x03

#define DISABLE_Mod 0x00
#define ASK         0x40
#define FSK         0x80
#define PSK         0xC0

#define SWEEP_ENABLE  0x40
#define SWEEP_DISABLE 0x00

void delay1(uint32_t length);
void IntReset(void);
void IO_Update(void);
void Intserve(void);
void AD9959_Init(void);
void AD9959_WriteData(uint8_t RegisterAddress,
                      uint8_t NumberofRegisters,
                      uint8_t *RegisterData);
void Write_CFTW0(uint32_t fre);
void Write_ACR(uint16_t Ampli);
void Write_CPOW0(uint16_t Phase);
void Write_LSRR(uint8_t rsrr, uint8_t fsrr);
void Write_RDW(uint32_t r_delta);
void Write_FDW(uint32_t f_delta);
void Write_Profile_Fre(uint8_t profile, uint32_t data);
void Write_Profile_Ampli(uint8_t profile, uint16_t data);
void Write_Profile_Phase(uint8_t profile, uint16_t data);
void AD9959_Set_Fre(uint8_t Channel, uint32_t Freq);
void AD9959_Set_Amp(uint8_t Channel, uint16_t Ampli);
void AD9959_Set_Phase(uint8_t Channel, uint16_t Phase);
void AD9959_Modulation_Init(uint8_t Channel,
                            uint8_t Modulation,
                            uint8_t Sweep_en,
                            uint8_t Nlevel);
void AD9959_SetFSK(uint8_t Channel, uint32_t *data, uint16_t Phase);
void AD9959_SetASK(uint8_t Channel,
                   uint16_t *data,
                   uint32_t fre,
                   uint16_t Phase);
void AD9959_SetPSK(uint8_t Channel, uint16_t *data, uint32_t Freq);
void AD9959_SetFre_Sweep(uint8_t Channel,
                         uint32_t s_data,
                         uint32_t e_data,
                         uint32_t r_delta,
                         uint32_t f_delta,
                         uint8_t rsrr,
                         uint8_t fsrr,
                         uint16_t Ampli,
                         uint16_t Phase);
void AD9959_SetAmp_Sweep(uint8_t Channel,
                         uint32_t s_Ampli,
                         uint16_t e_Ampli,
                         uint32_t r_delta,
                         uint32_t f_delta,
                         uint8_t rsrr,
                         uint8_t fsrr,
                         uint32_t fre,
                         uint16_t Phase);
void AD9959_SetPhase_Sweep(uint8_t Channel,
                           uint16_t s_data,
                           uint16_t e_data,
                           uint16_t r_delta,
                           uint16_t f_delta,
                           uint8_t rsrr,
                           uint8_t fsrr,
                           uint32_t fre,
                           uint16_t Ampli);
void AD9959_setsine(uint8_t Channel,
                    uint32_t Freq,
                    uint16_t Ampli,
                    uint16_t Phase);

#endif
