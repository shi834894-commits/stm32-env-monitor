#ifndef __APP_MAIN_H
#define __APP_MAIN_H

#include "main.h"

uint8_t App_Init(void);
void App_CreateTasks(void);

void SensorTask(void *argument);
void DisplayTask(void *argument);

#endif