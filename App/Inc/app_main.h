#ifndef __APP_MAIN_H
#define __APP_MAIN_H

#include "main.h"
#include <stdint.h>

typedef struct
{
    int16_t temperature_tenths;
    uint16_t humidity_tenths;
    uint16_t status;
    uint32_t sequence;
} SensorSnapshot_t;

uint8_t App_Init(void);
void App_CreateTasks(void);
uint8_t App_GetSensorSnapshot(SensorSnapshot_t *snapshot);

void SensorTask(void *argument);
void DisplayTask(void *argument);
void MonitorTask(void *argument);

#endif
