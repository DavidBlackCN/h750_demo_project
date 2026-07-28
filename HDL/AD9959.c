#include "AD9959.h"

uint8_t FR1_DATA[3] = {0xD0, 0x00, 0x00};
uint8_t FR2_DATA[2] = {0x80, 0x00};
double ACC_FRE_FACTOR = 8.59;
uint8_t CFR_DATA[3] = {0x00, 0x03, 0x02};

void AD9959_Init(void)
{
    Intserve();
    IntReset();

    AD9959_WriteData(FR1_ADD, 3, FR1_DATA);
    AD9959_WriteData(FR2_ADD, 2, FR2_DATA);
}

void delay1(uint32_t length)
{
    length = length * 12;
    while (length--);
}

void Intserve(void)
{
    HAL_GPIO_WritePin(AD9959_PDC_GPIO_Port, AD9959_PDC_Pin, 0);
    HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, 1);
    HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port, AD9959_SCLK_Pin, 0);
    HAL_GPIO_WritePin(AD9959_UPDATE_GPIO_Port, AD9959_UPDATE_Pin, 0);
    HAL_GPIO_WritePin(AD9959_SDIO0_GPIO_Port, AD9959_SDIO0_Pin, 0);
}

void IntReset(void)
{
    HAL_GPIO_WritePin(AD9959_RESET_GPIO_Port, AD9959_RESET_Pin, 0);
    delay1(1);
    HAL_GPIO_WritePin(AD9959_RESET_GPIO_Port, AD9959_RESET_Pin, 1);
    delay1(30);
    HAL_GPIO_WritePin(AD9959_RESET_GPIO_Port, AD9959_RESET_Pin, 0);
}

void IO_Update(void)
{
    HAL_GPIO_WritePin(AD9959_UPDATE_GPIO_Port, AD9959_UPDATE_Pin, 0);
    delay1(2);
    HAL_GPIO_WritePin(AD9959_UPDATE_GPIO_Port, AD9959_UPDATE_Pin, 1);
    delay1(4);
    HAL_GPIO_WritePin(AD9959_UPDATE_GPIO_Port, AD9959_UPDATE_Pin, 0);
}

void AD9959_WriteData(uint8_t RegisterAddress,
                      uint8_t NumberofRegisters,
                      uint8_t *RegisterData)
{
    uint8_t ControlValue = 0;
    uint8_t ValueToWrite = 0;
    uint8_t RegisterIndex = 0;
    uint8_t i = 0;

    ControlValue = RegisterAddress;
    HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port, AD9959_SCLK_Pin, 0);
    HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, 0);
    for (i = 0; i < 8; i++)
    {
        HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port, AD9959_SCLK_Pin, 0);
        if (0x80 == (ControlValue & 0x80))
            HAL_GPIO_WritePin(AD9959_SDIO0_GPIO_Port, AD9959_SDIO0_Pin, 1);
        else
            HAL_GPIO_WritePin(AD9959_SDIO0_GPIO_Port, AD9959_SDIO0_Pin, 0);
        HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port, AD9959_SCLK_Pin, 1);
        ControlValue <<= 1;
    }
    HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port, AD9959_SCLK_Pin, 0);

    for (RegisterIndex = 0; RegisterIndex < NumberofRegisters; RegisterIndex++)
    {
        ValueToWrite = RegisterData[RegisterIndex];
        for (i = 0; i < 8; i++)
        {
            HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port, AD9959_SCLK_Pin, 0);
            if (0x80 == (ValueToWrite & 0x80))
                HAL_GPIO_WritePin(AD9959_SDIO0_GPIO_Port, AD9959_SDIO0_Pin, 1);
            else
                HAL_GPIO_WritePin(AD9959_SDIO0_GPIO_Port, AD9959_SDIO0_Pin, 0);
            HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port, AD9959_SCLK_Pin, 1);
            ValueToWrite <<= 1;
        }
        HAL_GPIO_WritePin(AD9959_SCLK_GPIO_Port, AD9959_SCLK_Pin, 0);
    }
    HAL_GPIO_WritePin(AD9959_CS_GPIO_Port, AD9959_CS_Pin, 1);
}

void Write_CFTW0(uint32_t fre)
{
    uint8_t CFTW0_DATA[4] = {0x00, 0x00, 0x00, 0x00};
    uint32_t Temp;
    Temp = (uint32_t)(fre * ACC_FRE_FACTOR);
    CFTW0_DATA[3] = (uint8_t)Temp;
    CFTW0_DATA[2] = (uint8_t)(Temp >> 8);
    CFTW0_DATA[1] = (uint8_t)(Temp >> 16);
    CFTW0_DATA[0] = (uint8_t)(Temp >> 24);
    AD9959_WriteData(CFTW0_ADD, 4, CFTW0_DATA);
}

void Write_ACR(uint16_t Ampli)
{
    uint32_t A_temp = 0;
    uint8_t ACR_DATA[3] = {0x00, 0x00, 0x00};
    A_temp = Ampli | 0x1000;

    ACR_DATA[1] = (uint8_t)(A_temp >> 8);
    ACR_DATA[2] = (uint8_t)A_temp;
    AD9959_WriteData(ACR_ADD, 3, ACR_DATA);
}

void Write_CPOW0(uint16_t Phase)
{
    uint8_t CPOW0_data[2] = {0x00, 0x00};
    CPOW0_data[1] = (uint8_t)Phase;
    CPOW0_data[0] = (uint8_t)(Phase >> 8);

    AD9959_WriteData(CPOW0_ADD, 2, CPOW0_data);
}

void Write_LSRR(uint8_t rsrr, uint8_t fsrr)
{
    uint8_t LSRR_data[2] = {0x00, 0x00};

    LSRR_data[1] = rsrr;
    LSRR_data[0] = fsrr;
    AD9959_WriteData(LSRR_ADD, 2, LSRR_data);
}

void Write_RDW(uint32_t r_delta)
{
    uint8_t RDW_data[4] = {0x00, 0x00, 0x00, 0x00};

    RDW_data[3] = (uint8_t)r_delta;
    RDW_data[2] = (uint8_t)(r_delta >> 8);
    RDW_data[1] = (uint8_t)(r_delta >> 16);
    RDW_data[0] = (uint8_t)(r_delta >> 24);
    AD9959_WriteData(RDW_ADD, 4, RDW_data);
}

void Write_FDW(uint32_t f_delta)
{
    uint8_t FDW_data[4] = {0x00, 0x00, 0x00, 0x00};

    FDW_data[3] = (uint8_t)f_delta;
    FDW_data[2] = (uint8_t)(f_delta >> 8);
    FDW_data[1] = (uint8_t)(f_delta >> 16);
    FDW_data[0] = (uint8_t)(f_delta >> 24);
    AD9959_WriteData(FDW_ADD, 4, FDW_data);
}

void Write_Profile_Fre(uint8_t profile, uint32_t data)
{
    uint8_t profileAddr;
    uint8_t Profile_data[4] = {0x00, 0x00, 0x00, 0x00};
    uint32_t Temp;
    Temp = (uint32_t)(data * ACC_FRE_FACTOR);
    Profile_data[3] = (uint8_t)Temp;
    Profile_data[2] = (uint8_t)(Temp >> 8);
    Profile_data[1] = (uint8_t)(Temp >> 16);
    Profile_data[0] = (uint8_t)(Temp >> 24);
    profileAddr = PROFILE_ADDR_BASE + profile;

    AD9959_WriteData(profileAddr, 4, Profile_data);
}

void Write_Profile_Ampli(uint8_t profile, uint16_t data)
{
    uint8_t profileAddr;
    uint8_t Profile_data[4] = {0x00, 0x00, 0x00, 0x00};

    Profile_data[1] = (uint8_t)(data << 6);
    Profile_data[0] = (uint8_t)(data >> 2);
    profileAddr = PROFILE_ADDR_BASE + profile;

    AD9959_WriteData(profileAddr, 4, Profile_data);
}

void Write_Profile_Phase(uint8_t profile, uint16_t data)
{
    uint8_t profileAddr;
    uint8_t Profile_data[4] = {0x00, 0x00, 0x00, 0x00};

    Profile_data[1] = (uint8_t)(data << 2);
    Profile_data[0] = (uint8_t)(data >> 6);
    profileAddr = PROFILE_ADDR_BASE + profile;

    AD9959_WriteData(profileAddr, 4, Profile_data);
}

void AD9959_Set_Fre(uint8_t Channel, uint32_t Freq)
{
    uint8_t CHANNEL[1] = {0x00};

    CHANNEL[0] = Channel;
    AD9959_WriteData(CSR_ADD, 1, CHANNEL);
    Write_CFTW0(Freq);
}

void AD9959_Set_Amp(uint8_t Channel, uint16_t Ampli)
{
    uint8_t CHANNEL[1] = {0x00};

    CHANNEL[0] = Channel;
    AD9959_WriteData(CSR_ADD, 1, CHANNEL);
    Write_ACR(Ampli);
}

void AD9959_Set_Phase(uint8_t Channel, uint16_t Phase)
{
    uint8_t CHANNEL[1] = {0x00};
    CHANNEL[0] = Channel;

    AD9959_WriteData(CSR_ADD, 1, CHANNEL);
    Write_CPOW0(Phase);
}

void AD9959_Modulation_Init(uint8_t Channel,
                            uint8_t Modulation,
                            uint8_t Sweep_en,
                            uint8_t Nlevel)
{
    uint8_t i = 0;
    uint8_t CHANNEL[1] = {0x00};
    uint8_t FR1_data[3];
    uint8_t FR2_data[2];
    uint8_t CFR_data[3];
    for (i = 0; i < 3; i++)
    {
        FR1_data[i] = FR1_DATA[i];
        CFR_data[i] = CFR_DATA[i];
    }
    FR2_data[0] = FR2_DATA[0];
    FR2_data[1] = FR2_DATA[1];

    CHANNEL[0] = Channel;
    AD9959_WriteData(CSR_ADD, 1, CHANNEL);

    FR1_data[1] = Nlevel;
    CFR_data[0] = Modulation;
    CFR_data[1] |= Sweep_en;
    CFR_data[2] = 0x00;

    if (Channel != 0)
    {
        AD9959_WriteData(FR1_ADD, 3, FR1_data);
        AD9959_WriteData(FR2_ADD, 2, FR2_data);
        AD9959_WriteData(CFR_ADD, 3, CFR_data);
    }
}

void AD9959_SetFSK(uint8_t Channel, uint32_t *data, uint16_t Phase)
{
    uint8_t i = 0;
    uint8_t CHANNEL[1] = {0x00};

    CHANNEL[0] = Channel;
    AD9959_WriteData(CSR_ADD, 1, CHANNEL);
    Write_CPOW0(Phase);

    Write_CFTW0(data[0]);
    for (i = 0; i < 15; i++)
        Write_Profile_Fre(i, data[i + 1]);
}

void AD9959_SetASK(uint8_t Channel,
                   uint16_t *data,
                   uint32_t fre,
                   uint16_t Phase)
{
    uint8_t i = 0;
    uint8_t CHANNEL[1] = {0x00};

    CHANNEL[0] = Channel;
    AD9959_WriteData(CSR_ADD, 1, CHANNEL);
    Write_CFTW0(fre);
    Write_CPOW0(Phase);

    Write_ACR(data[0]);
    for (i = 0; i < 15; i++)
        Write_Profile_Ampli(i, data[i + 1]);
}

void AD9959_SetPSK(uint8_t Channel, uint16_t *data, uint32_t Freq)
{
    uint8_t i = 0;
    uint8_t CHANNEL[1] = {0x00};

    CHANNEL[0] = Channel;
    AD9959_WriteData(CSR_ADD, 1, CHANNEL);
    Write_CFTW0(Freq);

    Write_CPOW0(data[0]);
    for (i = 0; i < 15; i++)
        Write_Profile_Phase(i, data[i + 1]);
}

void AD9959_SetFre_Sweep(uint8_t Channel,
                         uint32_t s_data,
                         uint32_t e_data,
                         uint32_t r_delta,
                         uint32_t f_delta,
                         uint8_t rsrr,
                         uint8_t fsrr,
                         uint16_t Ampli,
                         uint16_t Phase)
{
    uint8_t CHANNEL[1] = {0x00};
    uint32_t Fer_data = 0;

    CHANNEL[0] = Channel;
    AD9959_WriteData(CSR_ADD, 1, CHANNEL);
    Write_CPOW0(Phase);
    Write_ACR(Ampli);
    Write_LSRR(rsrr, fsrr);
    Fer_data = (uint32_t)(r_delta * ACC_FRE_FACTOR);
    Write_RDW(Fer_data);
    Fer_data = (uint32_t)(f_delta * ACC_FRE_FACTOR);
    Write_FDW(Fer_data);
    Write_CFTW0(s_data);
    Write_Profile_Fre(0, e_data);
}

void AD9959_SetAmp_Sweep(uint8_t Channel,
                         uint32_t s_Ampli,
                         uint16_t e_Ampli,
                         uint32_t r_delta,
                         uint32_t f_delta,
                         uint8_t rsrr,
                         uint8_t fsrr,
                         uint32_t fre,
                         uint16_t Phase)
{
    uint8_t CHANNEL[1] = {0x00};
    uint8_t ACR_data[3] = {0x00, 0x00, 0x00};

    CHANNEL[0] = Channel;
    AD9959_WriteData(CSR_ADD, 1, CHANNEL);
    Write_CFTW0(fre);
    Write_CPOW0(Phase);
    Write_LSRR(rsrr, fsrr);
    Write_RDW(r_delta << 22);
    Write_FDW(f_delta << 22);
    ACR_data[1] = (uint8_t)(s_Ampli >> 8);
    ACR_data[2] = (uint8_t)s_Ampli;
    AD9959_WriteData(ACR_ADD, 3, ACR_data);
    Write_Profile_Ampli(0, e_Ampli);
}

void AD9959_SetPhase_Sweep(uint8_t Channel,
                           uint16_t s_data,
                           uint16_t e_data,
                           uint16_t r_delta,
                           uint16_t f_delta,
                           uint8_t rsrr,
                           uint8_t fsrr,
                           uint32_t fre,
                           uint16_t Ampli)
{
    uint8_t CHANNEL[1] = {0x00};

    CHANNEL[0] = Channel;
    AD9959_WriteData(CSR_ADD, 1, CHANNEL);
    Write_CFTW0(fre);
    Write_ACR(Ampli);
    Write_LSRR(rsrr, fsrr);
    Write_RDW(r_delta << 18);
    Write_FDW(f_delta << 18);
    Write_CPOW0(s_data);
    Write_Profile_Phase(0, e_data);
}

void AD9959_setsine(uint8_t Channel,
                    uint32_t Freq,
                    uint16_t Ampli,
                    uint16_t Phase)
{
    AD9959_Set_Fre(Channel, Freq);
    AD9959_Set_Amp(Channel, Ampli);
    AD9959_Set_Phase(Channel, Phase);
}
