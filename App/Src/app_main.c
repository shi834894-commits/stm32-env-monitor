#include "app_main.h"
#include "bsp_i2c.h"
#include "bsp_oled.h"
#include "bsp_aht20.h"
#include "log.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define SENSOR_TASK_STACK_SIZE  256
#define DISPLAY_TASK_STACK_SIZE 192
#define SENSOR_TASK_PRIORITY    1
#define DISPLAY_TASK_PRIORITY   2

#define SENSOR_READ_INTERVAL_MS 2000
#define SENSOR_QUEUE_LENGTH     1
#define I2C_MUTEX_TIMEOUT_MS    5000

#define AHT20_MEASURE_DELAY_MS 80

typedef struct {
    AHT20_Data_t data;
    uint8_t valid;
} SensorMsg_t;

static QueueHandle_t sensorQueue = NULL;
static SemaphoreHandle_t i2cMutex = NULL;
static uint8_t sensorInitialized = 0;

extern volatile uint8_t g_scheduler_started;

uint8_t App_Init(void)
{
    if (LOG("[BOOT] Initializing software I2C\r\n") != HAL_OK)
    {
        Error_Handler();
    }
    I2C_Init();
    if (LOG("[BOOT] Initializing OLED\r\n") != HAL_OK)
    {
        Error_Handler();
    }
    if (!OLED_Init())
    {
        if (LOG("[ERR] OLED initialization failed\r\n") != HAL_OK)
        {
            Error_Handler();
        }
        return 0;
    }
    if (LOG("[INFO] OLED initialized\r\n") != HAL_OK)
    {
        Error_Handler();
    }
    sensorInitialized = AHT20_Init();
    if (LOG("[INFO] AHT20 %s\r\n",
            sensorInitialized ? "initialized" : "not detected; retrying") != HAL_OK)
    {
        Error_Handler();
    }
    return 1;
}

void App_CreateTasks(void)
{
    BaseType_t ret;

    i2cMutex = xSemaphoreCreateMutex();
    configASSERT(i2cMutex != NULL);

    sensorQueue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorMsg_t));
    configASSERT(sensorQueue != NULL);

    ret = xTaskCreate(SensorTask, "SensorTask",
                      SENSOR_TASK_STACK_SIZE, NULL,
                      SENSOR_TASK_PRIORITY, NULL);
    configASSERT(ret == pdPASS);

    ret = xTaskCreate(DisplayTask, "DisplayTask",
                      DISPLAY_TASK_STACK_SIZE, NULL,
                      DISPLAY_TASK_PRIORITY, NULL);
    configASSERT(ret == pdPASS);
}

void SensorTask(void *argument)
{
    (void)argument;
    SensorMsg_t msg;

    g_scheduler_started = 1U;

    for (;;)
    {
        if (!sensorInitialized)
        {
            sensorInitialized = AHT20_Init();
            if (!sensorInitialized)
            {
                if (LOG("[WARN] AHT20 initialization failed; retrying\r\n") != HAL_OK)
                {
                    Error_Handler();
                }
                msg.valid = 0;
                xQueueOverwrite(sensorQueue, &msg);
                vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
                continue;
            }
            if (LOG("[INFO] AHT20 initialized\r\n") != HAL_OK)
            {
                Error_Handler();
            }
        }

        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
        {
            msg.valid = AHT20_Trigger();
            xSemaphoreGive(i2cMutex);
            if (!msg.valid)
            {
                if (LOG("[ERR] AHT20 measurement trigger failed\r\n") != HAL_OK)
                {
                    Error_Handler();
                }
                xQueueOverwrite(sensorQueue, &msg);
                vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
                continue;
            }
        }
        else
        {
            msg.valid = 0;
            if (LOG("[ERR] I2C mutex timeout before AHT20 trigger\r\n") != HAL_OK)
            {
                Error_Handler();
            }
            xQueueOverwrite(sensorQueue, &msg);
            vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(AHT20_MEASURE_DELAY_MS));

        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
        {
            msg.valid = AHT20_ReadResult(&msg.data);
            xSemaphoreGive(i2cMutex);
        }
        else
        {
            msg.valid = 0;
            if (LOG("[ERR] I2C mutex timeout before AHT20 read\r\n") != HAL_OK)
            {
                Error_Handler();
            }
        }

        if (msg.valid)
        {
            int32_t temperatureTenths = (int32_t)(msg.data.temperature * 10.0f);
            uint32_t absoluteTemperatureTenths = (temperatureTenths < 0)
                ? (uint32_t)(-temperatureTenths)
                : (uint32_t)temperatureTenths;
            uint32_t humidityTenths = (uint32_t)(msg.data.humidity * 10.0f);

            if (LOG("[AHT20] T=%s%lu.%lu C RH=%lu.%lu%%\r\n",
                    (temperatureTenths < 0) ? "-" : "",
                    (unsigned long)(absoluteTemperatureTenths / 10),
                    (unsigned long)(absoluteTemperatureTenths % 10),
                    (unsigned long)(humidityTenths / 10),
                    (unsigned long)(humidityTenths % 10)) != HAL_OK)
            {
                Error_Handler();
            }
        }
        else if (LOG("[ERR] AHT20 result read failed\r\n") != HAL_OK)
        {
            Error_Handler();
        }

        xQueueOverwrite(sensorQueue, &msg);

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}

void DisplayTask(void *argument)
{
    (void)argument;
    SensorMsg_t msg;

    g_scheduler_started = 1U;

    for (;;)
    {
        if (xQueueReceive(sensorQueue, &msg, portMAX_DELAY) == pdTRUE)
        {
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
            {
                OLED_Clear();

                if (msg.valid)
                {
                    OLED_DrawString(0, 0, "Temperature");
                    OLED_DrawFloat(0, 12, msg.data.temperature, 3, 1);
                    OLED_DrawChar(42, 12, 'C');

                    OLED_DrawString(0, 32, "Humidity");
                    OLED_DrawFloat(0, 44, msg.data.humidity, 3, 1);
                    OLED_DrawChar(42, 44, '%');
                }
                else
                {
                    OLED_DrawString(0, 0, "Temperature");
                    OLED_DrawString(0, 12, "ERR");
                    OLED_DrawString(0, 32, "Humidity");
                    OLED_DrawString(0, 44, "ERR");
                }

                uint8_t displayOk = OLED_Refresh();
                xSemaphoreGive(i2cMutex);
                if (!displayOk)
                {
                    Error_Handler();
                }
            }
        }
    }
}
