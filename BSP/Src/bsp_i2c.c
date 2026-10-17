#include "bsp_i2c.h"
#include "FreeRTOS.h"
#include "task.h"

static inline void i2c_enter_critical(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        taskENTER_CRITICAL();
    }
}

static inline void i2c_exit_critical(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        taskEXIT_CRITICAL();
    }
}

static void I2C_Delay(void)
{
    volatile uint8_t i;
    for (i = 0; i < 10; i++);
}

static void I2C_SDA_Mode_Output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static void I2C_SDA_Mode_Input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void I2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_8;           // PB8: I2C_SCL 时钟线
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;  // 开漏输出，符合 I2C 协议要求
    GPIO_InitStruct.Pull = GPIO_PULLUP;          // 内部上拉，确保总线空闲为高电平
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_9;           // PB9: I2C_SDA 数据线
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;  // 开漏输出，支持线与逻辑
    GPIO_InitStruct.Pull = GPIO_PULLUP;          // 内部上拉，确保总线空闲为高电平
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    I2C_SCL_HIGH();                              // 总线空闲：SCL 置高
    I2C_SDA_HIGH();                              // 总线空闲：SDA 置高
}

void I2C_Start(void)
{
    i2c_enter_critical();
    I2C_SDA_Mode_Output();
    I2C_SDA_HIGH();
    I2C_SCL_HIGH();
    I2C_Delay();
    I2C_SDA_LOW();
    I2C_Delay();
    I2C_SCL_LOW();
    I2C_Delay();
    i2c_exit_critical();
}

void I2C_Stop(void)
{
    i2c_enter_critical();
    I2C_SDA_Mode_Output();
    I2C_SCL_LOW();
    I2C_SDA_LOW();
    I2C_Delay();
    I2C_SCL_HIGH();
    I2C_Delay();
    I2C_SDA_HIGH();
    I2C_Delay();
    i2c_exit_critical();
}

void I2C_SendAck(void)
{
    i2c_enter_critical();
    I2C_SDA_Mode_Output();
    I2C_SCL_LOW();
    I2C_SDA_LOW();
    I2C_Delay();
    I2C_SCL_HIGH();
    I2C_Delay();
    I2C_SCL_LOW();
    I2C_Delay();
    i2c_exit_critical();
}

void I2C_SendNack(void)
{
    i2c_enter_critical();
    I2C_SDA_Mode_Output();
    I2C_SCL_LOW();
    I2C_SDA_HIGH();
    I2C_Delay();
    I2C_SCL_HIGH();
    I2C_Delay();
    I2C_SCL_LOW();
    I2C_Delay();
    i2c_exit_critical();
}

uint8_t I2C_WaitAck(void)
{
    uint8_t ack;
    i2c_enter_critical();
    I2C_SDA_Mode_Input();
    I2C_SDA_HIGH();
    I2C_SCL_LOW();
    I2C_Delay();
    I2C_SCL_HIGH();
    I2C_Delay();
    ack = I2C_SDA_READ();
    I2C_SCL_LOW();
    I2C_Delay();
    I2C_SDA_Mode_Output();
    i2c_exit_critical();
    return ack;
}

void I2C_SendByte(uint8_t byte)
{
    i2c_enter_critical();
    I2C_SDA_Mode_Output();
    I2C_SCL_LOW();
    for (uint8_t i = 0; i < 8; i++)
    {
        if (byte & 0x80)
            I2C_SDA_HIGH();
        else
            I2C_SDA_LOW();
        byte <<= 1;
        I2C_Delay();
        I2C_SCL_HIGH();
        I2C_Delay();
        I2C_SCL_LOW();
        I2C_Delay();
    }
    i2c_exit_critical();
}

uint8_t I2C_RecvByte(void)
{
    uint8_t byte = 0;
    i2c_enter_critical();
    I2C_SDA_Mode_Input();
    I2C_SDA_HIGH();
    for (uint8_t i = 0; i < 8; i++)
    {
        I2C_SCL_LOW();
        I2C_Delay();
        I2C_SCL_HIGH();
        I2C_Delay();
        byte <<= 1;
        if (I2C_SDA_READ())
            byte |= 0x01;
        I2C_SCL_LOW();
        I2C_Delay();
    }
    I2C_SDA_Mode_Output();
    i2c_exit_critical();
    return byte;
}