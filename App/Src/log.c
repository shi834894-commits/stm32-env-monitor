#include "log.h"
#include "usart.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <stdarg.h>
#include <stdio.h>

#define LOG_BUFFER_SIZE       192
#define LOG_MUTEX_TIMEOUT_MS  1000
#define LOG_TX_TIMEOUT_MS     1000

static SemaphoreHandle_t logMutex;

HAL_StatusTypeDef Log_Init(void)
{
    if (logMutex != NULL)
    {
        return HAL_ERROR;
    }

    logMutex = xSemaphoreCreateMutex();
    return (logMutex != NULL) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef Log_Printf(const char *format, ...)
{
    char buffer[LOG_BUFFER_SIZE];
    va_list args;

    if (format == NULL || logMutex == NULL)
    {
        return HAL_ERROR;
    }

    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length < 0 || (size_t)length >= sizeof(buffer))
    {
        return HAL_ERROR;
    }

    BaseType_t mutexTaken = pdFALSE;
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(LOG_MUTEX_TIMEOUT_MS)) != pdTRUE)
        {
            return HAL_TIMEOUT;
        }
        mutexTaken = pdTRUE;
    }

    HAL_StatusTypeDef status = HAL_UART_Transmit(
        &huart1, (uint8_t *)buffer, (uint16_t)length, LOG_TX_TIMEOUT_MS);

    if (mutexTaken == pdTRUE)
    {
        xSemaphoreGive(logMutex);
    }

    return status;
}
