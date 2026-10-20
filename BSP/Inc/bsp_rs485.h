#ifndef __BSP_RS485_H
#define __BSP_RS485_H

#include "main.h"

#include <stdint.h>

#define RS485_RX_BUFFER_SIZE 256U

void RS485_Init(void);
void RS485_IRQHandler(void);
void RS485_SetTransmit(void);
void RS485_SetReceive(void);
uint8_t RS485_ReadByte(uint8_t *byte);
HAL_StatusTypeDef RS485_Send(const uint8_t *data, uint16_t length);
uint32_t RS485_GetOverflowCount(void);
uint32_t RS485_GetErrorCount(void);
uint32_t RS485_GetReceivedCount(void);

#endif
