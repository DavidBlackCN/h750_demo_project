#ifndef __AD9910_H__
#define __AD9910_H__

#include "gpio.h"
#include "main.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define AD9910_MRT_Set  HAL_GPIO_WritePin(MRT_GPIO_Port, MRT_Pin, GPIO_PIN_SET)
#define AD9910_MRT_Clr  HAL_GPIO_WritePin(MRT_GPIO_Port, MRT_Pin, GPIO_PIN_RESET)

#define AD9910_CSN_Set  HAL_GPIO_WritePin(CSN_GPIO_Port, CSN_Pin, GPIO_PIN_SET)
#define AD9910_CSN_Clr  HAL_GPIO_WritePin(CSN_GPIO_Port, CSN_Pin, GPIO_PIN_RESET)

#define AD9910_SCK_Set  HAL_GPIO_WritePin(SCK9_GPIO_Port, SCK9_Pin, GPIO_PIN_SET)
#define AD9910_SCK_Clr  HAL_GPIO_WritePin(SCK9_GPIO_Port, SCK9_Pin, GPIO_PIN_RESET)

#define AD9910_SDI_Set  HAL_GPIO_WritePin(SDI_GPIO_Port, SDI_Pin, GPIO_PIN_SET)
#define AD9910_SDI_Clr  HAL_GPIO_WritePin(SDI_GPIO_Port, SDI_Pin, GPIO_PIN_RESET)

#define AD9910_IUP_Set  HAL_GPIO_WritePin(IUP_GPIO_Port, IUP_Pin, GPIO_PIN_SET)
#define AD9910_IUP_Clr  HAL_GPIO_WritePin(IUP_GPIO_Port, IUP_Pin, GPIO_PIN_RESET)

#define AD9910_PF0_Set  HAL_GPIO_WritePin(PF0_GPIO_Port, PF0_Pin, GPIO_PIN_SET)
#define AD9910_PF0_Clr  HAL_GPIO_WritePin(PF0_GPIO_Port, PF0_Pin, GPIO_PIN_RESET)

#define AD9910_PF1_Set  HAL_GPIO_WritePin(PF1_GPIO_Port, PF1_Pin, GPIO_PIN_SET)
#define AD9910_PF1_Clr  HAL_GPIO_WritePin(PF1_GPIO_Port, PF1_Pin, GPIO_PIN_RESET)

#define AD9910_PF2_Set  HAL_GPIO_WritePin(PF2_GPIO_Port, PF2_Pin, GPIO_PIN_SET)
#define AD9910_PF2_Clr  HAL_GPIO_WritePin(PF2_GPIO_Port, PF2_Pin, GPIO_PIN_RESET)

void GPIO_Init_AD9910(void);
void Delay_ns(u8 t);
void Write_8bit(u8 dat);
void Write_32bit(u32 dat);
void AD9910_Init(void);
void AD9910_Singal_Profile_Init(void);
void AD9910_Parallel_Profile_Init(void);
void AD9910_Parallel_Profile_Set(void);
void AD9910_Singal_Profile_Set(u8 addr, u32 Freq, u16 Amp, u16 Pha);
void Set_Profile(u8 num);

#endif
