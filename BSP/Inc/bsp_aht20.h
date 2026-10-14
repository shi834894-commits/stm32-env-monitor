#ifndef __BSP_AHT20_H
#define __BSP_AHT20_H

#include "main.h"

#define AHT20_I2C_ADDR 0x38

typedef struct {
    float temperature;
    float humidity;
} AHT20_Data_t;

void AHT20_Init(void);
void AHT20_Trigger(void);
uint8_t AHT20_ReadResult(AHT20_Data_t *data);

#endif