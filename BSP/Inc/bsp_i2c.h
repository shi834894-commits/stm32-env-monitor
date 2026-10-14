#ifndef __BSP_I2C_H
#define __BSP_I2C_H

#include "main.h"

#define I2C_SCL_HIGH()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET)
#define I2C_SCL_LOW()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET)
#define I2C_SDA_HIGH()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET)
#define I2C_SDA_LOW()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET)
#define I2C_SDA_READ()  HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9)

void I2C_Init(void);
void I2C_Start(void);
void I2C_Stop(void);
void I2C_SendAck(void);
void I2C_SendNack(void);
uint8_t I2C_WaitAck(void);
void I2C_SendByte(uint8_t byte);
uint8_t I2C_RecvByte(void);

#endif