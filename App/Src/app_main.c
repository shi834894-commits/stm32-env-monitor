#include "app_main.h"
#include "bsp_i2c.h"
#include "bsp_oled.h"
#include "bsp_aht20.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define SENSOR_TASK_STACK_SIZE  128
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

void App_Init(void)
{
    I2C_Init();
    OLED_Init();
    AHT20_Init();
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

    for (;;)
    {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
        {
            AHT20_Trigger();
            xSemaphoreGive(i2cMutex);
        }
        else
        {
            msg.valid = 0;
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
        }

        xQueueOverwrite(sensorQueue, &msg);

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}

void DisplayTask(void *argument)
{
    (void)argument;
    SensorMsg_t msg;

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

                OLED_Refresh();
                xSemaphoreGive(i2cMutex);
            }
        }
    }
}