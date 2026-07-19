#include "AD9910.h"

static u8 Profile_All[8];

static u8 CFR3[] = {0x25U, 0x0FU, 0x41U, 0x50U};
static u8 Assist_DAC[] = {0x00U, 0x00U, 0x00U, 0x7FU};

#define AD9910_RAM_DEFAULT_SAMPLES          50U
#define AD9910_RAM_MAX_SAMPLES            1024U
#define AD9910_TRIANGLE_RAM_RATE             5U
#define AD9910_RAM_POSITIVE_PHASE       0x4000U
#define AD9910_RAM_NEGATIVE_PHASE       0xC000U
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

    /* Keep every asynchronous control input in a defined idle state. */
    AD9910_MRT_Clr;
    AD9910_CSN_Set;
    AD9910_SCK_Clr;
    AD9910_SDI_Clr;
    AD9910_IUP_Clr;
    AD9910_PF0_Clr;
    AD9910_PF1_Clr;
    AD9910_PF2_Clr;
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
    HAL_Delay(5U);
    AD9910_MRT_Clr;
    HAL_Delay(1U);

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
    AD9910_RamWave_Init(amplitude_code, AD9910_RAM_DEFAULT_SAMPLES,
                        AD9910_TRIANGLE_RAM_RATE,
                        AD9910_RAM_WAVE_TRIANGLE);
}

void AD9910_RamWave_Init(u16 amplitude_code, u16 sample_count,
                         u16 address_step_rate,
                         AD9910_RamWaveform waveform)
{
    u16 end_address;
    u8 ram_profile[8];
    /*
     * CFR1[30:29] = 11 routes each RAM word to polar phase/amplitude.
     * CFR1[16] selects sine output, so phases 90/270 degrees produce
     * positive/negative samples. Keep RAM disabled while loading it.
     */
    u8 ram_load_cfr1[] = {0x60U, 0x41U, 0x00U, 0x00U};
    u8 ram_run_cfr1[] = {0xE0U, 0x41U, 0x00U, 0x00U};
    u16 sample;

    if (amplitude_code > AD9910_AMPLITUDE_CODE_MAX)
    {
        amplitude_code = AD9910_AMPLITUDE_CODE_MAX;
    }
    if (sample_count < 2U)
    {
        sample_count = 2U;
    }
    else if (sample_count > AD9910_RAM_MAX_SAMPLES)
    {
        sample_count = AD9910_RAM_MAX_SAMPLES;
    }
    if (address_step_rate == 0U)
    {
        address_step_rate = 1U;
    }

    end_address = sample_count - 1U;
    ram_profile[0] = 0x00U;
    ram_profile[1] = (u8)(address_step_rate >> 8);
    ram_profile[2] = (u8)address_step_rate;
    ram_profile[3] = (u8)(end_address >> 2);
    ram_profile[4] = (u8)((end_address & 0x03U) << 6);
    ram_profile[5] = 0x00U;
    ram_profile[6] = 0x00U;
    ram_profile[7] = 0x04U;

    /*
     * Profile 0 uses RAM addresses 0 through sample_count - 1. Playback advances at
     * SYSCLK / (4 * address_step_rate), so the waveform frequency is that
     * playback rate divided by sample_count.
     */
    AD9910_WriteRegister(0x00U, ram_load_cfr1, sizeof(ram_load_cfr1));
    AD9910_WriteRegister(0x0EU, ram_profile, sizeof(ram_profile));
    AD9910_IO_Update();

    AD9910_CSN_Clr;
    Write_8bit(0x16U);
    for (sample = 0U; sample < sample_count; sample++)
    {
        int32_t value;
        u16 magnitude;
        u16 phase;
        u32 ram_word;

        if (waveform == AD9910_RAM_WAVE_SQUARE)
        {
            value = (sample < (sample_count / 2U)) ?
                    -((int32_t)amplitude_code) : (int32_t)amplitude_code;
        }
        else
        {
            int32_t scaled = (4 * (int32_t)amplitude_code *
                              (int32_t)sample) / (int32_t)sample_count;

            value = (sample < ((sample_count + 1U) / 2U)) ?
                    -((int32_t)amplitude_code) + scaled :
                    3 * (int32_t)amplitude_code - scaled;
        }

        magnitude = (u16)((value < 0) ? -value : value);
        phase = (value < 0) ? AD9910_RAM_NEGATIVE_PHASE : AD9910_RAM_POSITIVE_PHASE;
        ram_word = ((u32)phase << 16) | ((u32)magnitude << 2);
        Write_32bit(ram_word);
    }
    AD9910_CSN_Set;

    AD9910_WriteRegister(0x00U, ram_run_cfr1, sizeof(ram_run_cfr1));
    AD9910_IO_Update();
}

void AD9910_DrgTriangle_Init(u16 amplitude_code, u16 ramp_rate,
                            u8 dac_fsc_code)
{
    const u32 ramp_steps = 50UL;
    u32 upper_limit;
    u32 step_size;
    u8 dac_control[] = {0x00U, 0x00U, 0x00U, dac_fsc_code};
    u8 drg_limit[8];
    u8 drg_step[8];
    u8 drg_rate[4] =
    {
        (u8)(ramp_rate >> 8), (u8)ramp_rate,
        (u8)(ramp_rate >> 8), (u8)ramp_rate
    };
    /* CFR1[16] selects sine output; POW=90 degrees then maps ASF to DAC. */
    u8 drg_cfr1[] = {0x00U, 0x41U, 0x40U, 0x00U};
    u8 drg_cfr2[] = {0x01U, 0x6EU, 0x08U, 0x20U};
    u8 i;

    if (amplitude_code > AD9910_AMPLITUDE_CODE_MAX)
    {
        amplitude_code = AD9910_AMPLITUDE_CODE_MAX;
    }
    if (ramp_rate == 0U)
    {
        ramp_rate = 1U;
        drg_rate[0] = 0U;
        drg_rate[1] = 1U;
        drg_rate[2] = 0U;
        drg_rate[3] = 1U;
    }

    /* Make exactly 50 equal steps per half-cycle. */
    upper_limit = (u32)amplitude_code << 18;
    step_size = (upper_limit + (ramp_steps / 2UL)) / ramp_steps;
    upper_limit = step_size * ramp_steps;

    for (i = 0U; i < 4U; i++)
    {
        drg_limit[i] = (u8)(upper_limit >> (24U - 8U * i));
        drg_limit[i + 4U] = 0U;
        drg_step[i] = (u8)(step_size >> (24U - 8U * i));
        drg_step[i + 4U] = drg_step[i];
    }

    /* FTW=0 and POW=90 degrees turn the amplitude ramp into a baseband ramp. */
    AD9910_Singal_Profile_Init();
    AD9910_Singal_Profile_Set(0U, 0U, AD9910_AMPLITUDE_CODE_MAX, 90U);

    AD9910_WriteRegister(0x03U, dac_control, sizeof(dac_control));
    AD9910_WriteRegister(0x0BU, drg_limit, sizeof(drg_limit));
    AD9910_WriteRegister(0x0CU, drg_step, sizeof(drg_step));
    AD9910_WriteRegister(0x0DU, drg_rate, sizeof(drg_rate));
    AD9910_WriteRegister(0x00U, drg_cfr1, sizeof(drg_cfr1));
    AD9910_WriteRegister(0x01U, drg_cfr2, sizeof(drg_cfr2));
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
