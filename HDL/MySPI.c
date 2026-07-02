#include "MySPI.h"

unsigned char AD9833_SPI_Write(unsigned char *data, unsigned char bytesNumber)
{
    unsigned char i;
    unsigned char j;
    unsigned char writeData[5] = {0U, 0U, 0U, 0U, 0U};

    AD9833_SCK_H;
    AD9833_FSYNC_L;

    for (i = 0U; i < bytesNumber; i++)
    {
        writeData[i] = data[i + 1U];
    }

    for (i = 0U; i < bytesNumber; i++)
    {
        for (j = 0U; j < 8U; j++)
        {
            if ((writeData[i] & 0x80U) != 0U)
            {
                AD9833_SDA_H;
            }
            else
            {
                AD9833_SDA_L;
            }

            AD9833_SCK_L;
            writeData[i] <<= 1U;
            AD9833_SCK_H;
        }
    }

    AD9833_SDA_H;
    AD9833_FSYNC_H;

    return i;
}

unsigned char AD9833_SPI_Read(unsigned char *data, unsigned char bytesNumber)
{
    (void)data;
    return bytesNumber;
}
