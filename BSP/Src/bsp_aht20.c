#include "bsp_aht20.h"
#include "bsp_i2c.h"

static uint8_t AHT20_IsCalibrated(void)
{
    I2C_Start();
    I2C_SendByte((AHT20_I2C_ADDR << 1) | 0x00);
    if (I2C_WaitAck() != GPIO_PIN_RESET)
    {
        I2C_Stop();
        return 0;
    }
    I2C_SendByte(0x71);
    if (I2C_WaitAck() != GPIO_PIN_RESET)
    {
        I2C_Stop();
        return 0;
    }
    I2C_Stop();

    I2C_Start();
    I2C_SendByte((AHT20_I2C_ADDR << 1) | 0x01);
    if (I2C_WaitAck() != GPIO_PIN_RESET)
    {
        I2C_Stop();
        return 0;
    }
    uint8_t status = I2C_RecvByte();
    I2C_SendNack();
    I2C_Stop();

    return (status & 0x08) ? 1 : 0;
}

static uint8_t AHT20_Calibrate(void)
{
    I2C_Start();
    I2C_SendByte((AHT20_I2C_ADDR << 1) | 0x00);
    if (I2C_WaitAck() != GPIO_PIN_RESET)
    {
        I2C_Stop();
        return 0;
    }
    I2C_SendByte(0xBE);
    if (I2C_WaitAck() != GPIO_PIN_RESET)
    {
        I2C_Stop();
        return 0;
    }
    I2C_SendByte(0x08);
    if (I2C_WaitAck() != GPIO_PIN_RESET)
    {
        I2C_Stop();
        return 0;
    }
    I2C_SendByte(0x00);
    if (I2C_WaitAck() != GPIO_PIN_RESET)
    {
        I2C_Stop();
        return 0;
    }
    I2C_Stop();
    HAL_Delay(10);
    return 1;
}

uint8_t AHT20_Init(void)
{
    HAL_Delay(40);
    if (!AHT20_IsCalibrated())
    {
        return AHT20_Calibrate();
    }
    return 1;
}

uint8_t AHT20_Trigger(void)
{
    I2C_Start();
    I2C_SendByte((AHT20_I2C_ADDR << 1) | 0x00);
    if (I2C_WaitAck() != GPIO_PIN_RESET)
    {
        I2C_Stop();
        return 0;
    }
    I2C_SendByte(0xAC);
    if (I2C_WaitAck() != GPIO_PIN_RESET)
    {
        I2C_Stop();
        return 0;
    }
    I2C_SendByte(0x33);
    if (I2C_WaitAck() != GPIO_PIN_RESET)
    {
        I2C_Stop();
        return 0;
    }
    I2C_SendByte(0x00);
    if (I2C_WaitAck() != GPIO_PIN_RESET)
    {
        I2C_Stop();
        return 0;
    }
    I2C_Stop();
    return 1;
}

uint8_t AHT20_ReadResult(AHT20_Data_t *data)
{
    uint8_t buf[6];

    if (data == NULL)
    {
        return 0;
    }

    I2C_Start();
    I2C_SendByte((AHT20_I2C_ADDR << 1) | 0x01);
    if (I2C_WaitAck() != GPIO_PIN_RESET)
    {
        I2C_Stop();
        return 0;
    }

    for (uint8_t i = 0; i < 6; i++)
    {
        buf[i] = I2C_RecvByte();
        if (i < 5)
            I2C_SendAck();
        else
            I2C_SendNack();
    }
    I2C_Stop();

    if ((buf[0] & 0x80) != 0)
    {
        return 0;
    }

    uint32_t hum_raw = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | ((uint32_t)buf[3] >> 4);
    uint32_t temp_raw = (((uint32_t)buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | (uint32_t)buf[5];

    data->humidity = (float)hum_raw / 1048576.0f * 100.0f;
    data->temperature = (float)temp_raw / 1048576.0f * 200.0f - 50.0f;

    return 1;
}