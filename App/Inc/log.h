#ifndef __LOG_H
#define __LOG_H

#include "main.h"

HAL_StatusTypeDef Log_Init(void);
HAL_StatusTypeDef Log_Printf(const char *format, ...);

#define LOG(...) Log_Printf(__VA_ARGS__)

#endif
