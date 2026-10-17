#ifndef __USART_H
#define __USART_H

#include "main.h"

extern UART_HandleTypeDef huart1;

HAL_StatusTypeDef MX_USART1_UART_Init(void);

#endif
