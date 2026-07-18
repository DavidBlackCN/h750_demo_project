#include "AD9910.h"

static u8 Profile_All[8];

static u8 CFR3[] = {0x25U, 0x0FU, 0x41U, 0x50U};
static u8 Assist_DAC[] = {0x00U, 0x00U, 0x00U, 0x7FU};

#define AD9910_TRIANGLE_SAMPLES             50U
#define AD9910_TRIANGLE_RAM_RATE             5U
#define AD9910_TRIANGLE_POSITIVE_PHASE       0x4000U
#define AD9910_TRIANGLE_NEGATIVE_PHASE       0xC000U
#define AD9910_AMPLITUDE_CODE_MAX            16383U

static void AD9910_IO_Update(void)
{
    AD9910_IUP_Clr;
    Delay_ns(10U);
    AD9910_IUP_Set;
    Delay_ns(10U);
    AD9910_IUP_Clr;
}

static void AD9910_WriteRegister(u8 address, const u8 *data, u8 length)
{
    u8 i;

    AD9910_CSN_Clr;
    Write_8bit(address);
    for (i = 0U; i < length; i++)
    {
        Write_8bit(data[i]);
    }
    AD9910_CSN_Set;
}

void Delay_ns(u8 t)
{
    do
    {
        __NOP();
    } while (--t != 0U);
}

void GPIO_Init_AD9910(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.Pin = MRT_Pin;
    HAL_GPIO_Init(MRT_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = PF0_Pin;
    HAL_GPIO_Init(PF0_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = PF1_Pin;
    HAL_GPIO_Init(PF1_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = PF2_Pin;
    HAL_GPIO_Init(PF2_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = IUP_Pin;
    HAL_GPIO_Init(IUP_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = CSN_Pin;
    HAL_GPIO_Init(CSN_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SDI_Pin;
    HAL_GPIO_Init(SDI_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SCK9_Pin;
    HAL_GPIO_Init(SCK9_GPIO_Port, &GPIO_InitStruct);
}

void Write_8bit(u8 dat)
{
    u8 i;
    u8 com = 0x80U;

    AD9910_SCK_Clr;
    for (i = 0U; i < 8U; i++)
    {
        if ((dat & com) == 0U)
        {
            AD9910_SDI_Clr;
        }
        else
        {
            AD9910_SDI_Set;
        }

        AD9910_SCK_Set;
        com >>= 1U;
        AD9910_SCK_Clr;
    }
}

void Write_32bit(u32 dat)
{
    u8 i;
    u32 com = 0x80000000UL;

    AD9910_SCK_Clr;
    for (i = 0U; i < 32U; i++)
    {
        if ((dat & com) == 0UL)
        {
            AD9910_SDI_Clr;
        }
        else
        {
            AD9910_SDI_Set;
        }

        AD9910_SCK_Set;
        com >>= 1U;
        AD9910_SCK_Clr;
    }
}

void AD9910_Init(void)
{
    int j;

    AD9910_MRT_Set;
    AD9910_MRT_Clr;

    AD9910_CSN_Clr;
    Write_8bit(0x02U);
    for (j = 0; j < 4; j++)
    {
        Write_8bit(CFR3[j]);
    }
    AD9910_CSN_Set;
    Delay_ns(10U);

    AD9910_CSN_Clr;
    Write_8bit(0x03U);
    for (j = 0; j < 4; j++)
    {
        Write_8bit(Assist_DAC[j]);
    }
    AD9910_CSN_Set;

    Delay_ns(10U);
    AD9910_IUP_Clr;
    Delay_ns(10U);
    AD9910_IUP_Set;
    Delay_ns(10U);
    AD9910_IUP_Clr;
}

void AD9910_Singal_Profile_Init(void)
{
    int j;
    u8 single_cfr1[] = {0x00U, 0x40U, 0x00U, 0x00U};
    u8 single_cfr2[] = {0x01U, 0x40U, 0x08U, 0x20U};

    AD9910_CSN_Clr;
    Write_8bit(0x00U);
    for (j = 0; j < 4; j++)
    {
        Write_8bit(single_cfr1[j]);
    }
    AD9910_CSN_Set;
    Delay_ns(10U);

    AD9910_CSN_Clr;
    Write_8bit(0x01U);
    for (j = 0; j < 4; j++)
    {
        Write_8bit(single_cfr2[j]);
    }
    AD9910_CSN_Set;
    Delay_ns(10U);

    AD9910_IUP_Clr;
    Delay_ns(10U);
    AD9910_IUP_Set;
    Delay_ns(10U);
    AD9910_IUP_Clr;
}

void AD9910_Parallel_Profile_Init(void)
{
    int j;
    u8 parallel_cfr1[] = {0x00U, 0x40U, 0x00U, 0x00U};
    u8 parallel_cfr2[] = {0x01U, 0x40U, 0x08U, 0x30U};

    AD9910_CSN_Clr;
    Write_8bit(0x00U);
    for (j = 0; j < 4; j++)
    {
        Write_8bit(parallel_cfr1[j]);
    }
    AD9910_CSN_Set;
    Delay_ns(10U);

    AD9910_CSN_Clr;
    Write_8bit(0x01U);
    for (j = 0; j < 4; j++)
    {
        Write_8bit(parallel_cfr2[j]);
    }
    AD9910_CSN_Set;
    Delay_ns(10U);

    AD9910_IUP_Clr;
    Delay_ns(10U);
    AD9910_IUP_Set;
    Delay_ns(10U);
    AD9910_IUP_Clr;
}

void AD9910_Parallel_Profile_Set(void)
{
}

void AD9910_Singal_Profile_Set(u8 addr, u32 Freq, u16 Amp, u16 Pha)
{
    u32 Temp_Fre;
    u32 Temp_Amp;
    u32 Temp_Pha;
    u8 Temp_addr = 0x0EU;
    int k;

    Temp_Fre = (u32)((float)Freq * 4.294967296f);
    Temp_Pha = (u32)((float)Pha * 182.044444444f);
    Temp_Amp = (u32)Amp;

    switch (addr)
    {
    case 0U:
        Temp_addr = 0x0EU;
        break;
    case 1U:
        Temp_addr = 0x0FU;
        break;
    case 2U:
        Temp_addr = 0x10U;
        break;
    case 3U:
        Temp_addr = 0x11U;
        break;
    case 4U:
        Temp_addr = 0x12U;
        break;
    case 5U:
        Temp_addr = 0x13U;
        break;
    case 6U:
        Temp_addr = 0x14U;
        break;
    case 7U:
        Temp_addr = 0x15U;
        break;
    default:
        break;
    }

    Profile_All[7] = (u8)Temp_Fre;
    Profile_All[6] = (u8)(Temp_Fre >> 8);
    Profile_All[5] = (u8)(Temp_Fre >> 16);
    Profile_All[4] = (u8)(Temp_Fre >> 24);
    Profile_All[3] = (u8)Temp_Pha;
    Profile_All[2] = (u8)(Temp_Pha >> 8);
    Profile_All[1] = (u8)Temp_Amp;
    Profile_All[0] = (u8)(Temp_Amp >> 8);

    AD9910_CSN_Clr;
    Write_8bit(Temp_addr);
    for (k = 0; k < 8; k++)
    {
        Write_8bit(Profile_All[k]);
    }
    AD9910_CSN_Set;

    Delay_ns(10U);
    AD9910_IUP_Set;
    Delay_ns(10U);
    AD9910_IUP_Clr;
}

void AD9910_TriangleWave_Init(u16 amplitude_code)
{
    u8 ram_profile[8] =
    {
        0x00U,
        0x00U,
        AD9910_TRIANGLE_RAM_RATE,
        0x0CU,
        0x40U,
        0x00U,
        0x00U,
        0x04U
    };
    u8 ram_cfr1[] = {0xE0U, 0x40U, 0x00U, 0x00U};
    u8 sample;

    if (amplitude_code > AD9910_AMPLITUDE_CODE_MAX)
    {
        amplitude_code = AD9910_AMPLITUDE_CODE_MAX;
    }

    /*
     * Profile 0 uses RAM addresses 0 through 49. With a 1 GHz SYSCLK,
     * an address step rate of 5 gives 50 MHz playback and a 1 MHz waveform.
     */
    AD9910_WriteRegister(0x0EU, ram_profile, sizeof(ram_profile));
    AD9910_IO_Update();

    AD9910_CSN_Clr;
    Write_8bit(0x16U);
    for (sample = 0U; sample < AD9910_TRIANGLE_SAMPLES; sample++)
    {
        int32_t value;
        u16 magnitude;
        u16 phase;
        u32 ram_word;

        if (sample <= 24U)
        {
            value = -((int32_t)amplitude_code) +
                    ((2 * (int32_t)amplitude_code * (int32_t)sample + 12) / 24);
        }
        else
        {
            value = (int32_t)amplitude_code -
                    ((2 * (int32_t)amplitude_code * ((int32_t)sample - 24) + 12) / 25);
        }

        magnitude = (u16)((value < 0) ? -value : value);
        phase = (value < 0) ? AD9910_TRIANGLE_NEGATIVE_PHASE : AD9910_TRIANGLE_POSITIVE_PHASE;
        ram_word = ((u32)phase << 16) | ((u32)magnitude << 2);
        Write_32bit(ram_word);
    }
    AD9910_CSN_Set;

    /* CFR1: RAM enabled, polar playback (phase + 14-bit amplitude), sine output. */
    AD9910_WriteRegister(0x00U, ram_cfr1, sizeof(ram_cfr1));
    AD9910_IO_Update();
}

void Set_Profile(u8 num)
{
    switch (num)
    {
    case 0U:
        AD9910_PF2_Clr;
        AD9910_PF1_Clr;
        AD9910_PF0_Clr;
        break;
    case 1U:
        AD9910_PF2_Clr;
        AD9910_PF1_Clr;
        AD9910_PF0_Set;
        break;
    case 2U:
        AD9910_PF2_Clr;
        AD9910_PF1_Set;
        AD9910_PF0_Clr;
        break;
    case 3U:
        AD9910_PF2_Clr;
        AD9910_PF1_Set;
        AD9910_PF0_Set;
        break;
    case 4U:
        AD9910_PF2_Set;
        AD9910_PF1_Clr;
        AD9910_PF0_Clr;
        break;
    case 5U:
        AD9910_PF2_Set;
        AD9910_PF1_Clr;
        AD9910_PF0_Set;
        break;
    case 6U:
        AD9910_PF2_Set;
        AD9910_PF1_Set;
        AD9910_PF0_Clr;
        break;
    case 7U:
        AD9910_PF2_Set;
        AD9910_PF1_Set;
        AD9910_PF0_Set;
        break;
    default:
        break;
    }

    AD9910_IUP_Clr;
    Delay_ns(10U);
    AD9910_IUP_Set;
    Delay_ns(10U);
    AD9910_IUP_Clr;
}
