#include "app_main.h"
#include "bsp_i2c.h"
#include "bsp_oled.h"
#include "bsp_aht20.h"
#include "bsp_rs485.h"
#include "log.h"
#include "modbus_task.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define SENSOR_TASK_STACK_SIZE  256
#define DISPLAY_TASK_STACK_SIZE 256
#define MODBUS_TASK_STACK_SIZE  256
#define MONITOR_TASK_STACK_SIZE 256
#define SENSOR_TASK_PRIORITY    1
#define DISPLAY_TASK_PRIORITY   2
#define MODBUS_TASK_PRIORITY    3
#define MONITOR_TASK_PRIORITY   1

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
static SemaphoreHandle_t snapshotMutex = NULL;
static uint8_t sensorInitialized = 0;
static uint8_t oledInitialized = 0;
static SensorSnapshot_t sensorSnapshot;
static TaskHandle_t sensorTaskHandle = NULL;
static TaskHandle_t displayTaskHandle = NULL;
static TaskHandle_t modbusTaskHandle = NULL;
static TaskHandle_t monitorTaskHandle = NULL;

extern volatile uint8_t g_scheduler_started;

static int16_t ToTemperatureTenths(float value)
{
    int32_t scaled = (value >= 0.0f)
        ? (int32_t)(value * 10.0f + 0.5f)
        : (int32_t)(value * 10.0f - 0.5f);

    if (scaled > INT16_MAX) scaled = INT16_MAX;
    if (scaled < INT16_MIN) scaled = INT16_MIN;
    return (int16_t)scaled;
}

static uint16_t ToHumidityTenths(float value)
{
    int32_t scaled = (int32_t)(value * 10.0f + 0.5f);

    if (scaled < 0) scaled = 0;
    if (scaled > UINT16_MAX) scaled = UINT16_MAX;
    return (uint16_t)scaled;
}

static void PublishSnapshot(const SensorMsg_t *msg)
{
    if (snapshotMutex == NULL || msg == NULL)
    {
        return;
    }

    if (xSemaphoreTake(snapshotMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        sensorSnapshot.status = oledInitialized ? 0x0002U : 0U;
        if (msg->valid)
        {
            sensorSnapshot.temperature_tenths = ToTemperatureTenths(msg->data.temperature);
            sensorSnapshot.humidity_tenths = ToHumidityTenths(msg->data.humidity);
            sensorSnapshot.status |= 0x0001U;
            sensorSnapshot.sequence++;
        }
        else
        {
            sensorSnapshot.temperature_tenths = 0;
            sensorSnapshot.humidity_tenths = 0;
        }
        xSemaphoreGive(snapshotMutex);
    }
}

uint8_t App_GetSensorSnapshot(SensorSnapshot_t *snapshot)
{
    if (snapshot == NULL || snapshotMutex == NULL)
    {
        return 0U;
    }

    if (xSemaphoreTake(snapshotMutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return 0U;
    }

    *snapshot = sensorSnapshot;
    xSemaphoreGive(snapshotMutex);
    return 1U;
}

uint8_t App_Init(void)
{
    (void)LOG("[BOOT] Initializing software I2C\r\n");
    I2C_Init();
    (void)LOG("[BOOT] Initializing OLED\r\n");
    oledInitialized = OLED_Init();
    if (oledInitialized)
    {
        /* OLED_Init() already clears the complete display RAM.  Do not draw
           a startup marker here: it can remain visible while the first
           sensor sample is being acquired and looks like stale data. */
        OLED_Clear();
        (void)LOG("[INFO] OLED initialized\r\n");
    }
    else
        (void)LOG("[WARN] OLED initialization failed; retrying from task\r\n");
    sensorInitialized = AHT20_Init();
    (void)LOG("[INFO] AHT20 %s\r\n",
              sensorInitialized ? "initialized" : "not detected; retrying");
    /* Sensor and display initialization are retried after the scheduler
       starts.  A single startup NACK must not leave the board permanently
       inside Error_Handler(). */
    return 1;
}

void App_CreateTasks(void)
{
    BaseType_t ret;

    (void)LOG("[BOOT] Creating RTOS objects\r\n");

    i2cMutex = xSemaphoreCreateMutex();
    configASSERT(i2cMutex != NULL);

    snapshotMutex = xSemaphoreCreateMutex();
    configASSERT(snapshotMutex != NULL);

    sensorQueue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorMsg_t));
    configASSERT(sensorQueue != NULL);

    ret = xTaskCreate(SensorTask, "SensorTask",
                      SENSOR_TASK_STACK_SIZE, NULL,
                      SENSOR_TASK_PRIORITY, &sensorTaskHandle);
    configASSERT(ret == pdPASS);

    ret = xTaskCreate(DisplayTask, "DisplayTask",
                      DISPLAY_TASK_STACK_SIZE, NULL,
                      DISPLAY_TASK_PRIORITY, &displayTaskHandle);
    configASSERT(ret == pdPASS);

    ret = xTaskCreate(ModbusTask, "ModbusTask",
                      MODBUS_TASK_STACK_SIZE, NULL,
                      MODBUS_TASK_PRIORITY, &modbusTaskHandle);
    configASSERT(ret == pdPASS);

    ret = xTaskCreate(MonitorTask, "MonitorTask",
                      MONITOR_TASK_STACK_SIZE, NULL,
                      MONITOR_TASK_PRIORITY, &monitorTaskHandle);
    configASSERT(ret == pdPASS);
    (void)LOG("[BOOT] RTOS objects ready\r\n");
}

void SensorTask(void *argument)
{
    (void)argument;
    SensorMsg_t msg;

    g_scheduler_started = 1U;
    (void)LOG("[TASK] SensorTask started\r\n");

    for (;;)
    {
        if (!sensorInitialized)
        {
            sensorInitialized = AHT20_Init();
            if (!sensorInitialized)
            {
                (void)LOG("[WARN] AHT20 initialization failed; retrying\r\n");
                msg.valid = 0;
                xQueueOverwrite(sensorQueue, &msg);
                PublishSnapshot(&msg);
                vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
                continue;
            }
            (void)LOG("[INFO] AHT20 initialized\r\n");
        }

        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
        {
            msg.valid = AHT20_Trigger();
            xSemaphoreGive(i2cMutex);
            if (!msg.valid)
            {
                (void)LOG("[ERR] AHT20 measurement trigger failed\r\n");
                xQueueOverwrite(sensorQueue, &msg);
                PublishSnapshot(&msg);
                vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
                continue;
            }
        }
        else
        {
            msg.valid = 0;
            (void)LOG("[ERR] I2C mutex timeout before AHT20 trigger\r\n");
            xQueueOverwrite(sensorQueue, &msg);
            PublishSnapshot(&msg);
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
            (void)LOG("[ERR] I2C mutex timeout before AHT20 read\r\n");
        }

        if (msg.valid)
        {
            int32_t temperatureTenths = (int32_t)(msg.data.temperature * 10.0f);
            uint32_t absoluteTemperatureTenths = (temperatureTenths < 0)
                ? (uint32_t)(-temperatureTenths)
                : (uint32_t)temperatureTenths;
            uint32_t humidityTenths = (uint32_t)(msg.data.humidity * 10.0f);

            (void)LOG("[AHT20] T=%s%lu.%lu C RH=%lu.%lu%%\r\n",
                    (temperatureTenths < 0) ? "-" : "",
                    (unsigned long)(absoluteTemperatureTenths / 10),
                    (unsigned long)(absoluteTemperatureTenths % 10),
                    (unsigned long)(humidityTenths / 10),
                    (unsigned long)(humidityTenths % 10));
        }
        else
        {
            (void)LOG("[ERR] AHT20 result read failed\r\n");
        }

        xQueueOverwrite(sensorQueue, &msg);
        PublishSnapshot(&msg);

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}

void MonitorTask(void *argument)
{
    (void)argument;
    (void)LOG("[TASK] MonitorTask started\r\n");

    for (;;)
    {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_8);
        (void)LOG("[MON] RS485 rx=%lu err=%lu ovf=%lu heap=%lu minheap=%lu stack(S/D/M/Mon)=%u/%u/%u/%u words\r\n",
                  (unsigned long)RS485_GetReceivedCount(),
                  (unsigned long)RS485_GetErrorCount(),
                  (unsigned long)RS485_GetOverflowCount(),
                  (unsigned long)xPortGetFreeHeapSize(),
                  (unsigned long)xPortGetMinimumEverFreeHeapSize(),
                  (unsigned int)uxTaskGetStackHighWaterMark(sensorTaskHandle),
                  (unsigned int)uxTaskGetStackHighWaterMark(displayTaskHandle),
                  (unsigned int)uxTaskGetStackHighWaterMark(modbusTaskHandle),
                  (unsigned int)uxTaskGetStackHighWaterMark(monitorTaskHandle));
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void DisplayTask(void *argument)
{
    (void)argument;
    SensorMsg_t msg;

    g_scheduler_started = 1U;
    (void)LOG("[TASK] DisplayTask started\r\n");

    for (;;)
    {
        if (xQueueReceive(sensorQueue, &msg, portMAX_DELAY) == pdTRUE)
        {
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) == pdTRUE)
            {
                if (!oledInitialized)
                {
                    oledInitialized = OLED_Init();
                    if (!oledInitialized)
                    {
                        (void)LOG("[WARN] OLED initialization retry failed\r\n");
                        I2C_Init();
                        xSemaphoreGive(i2cMutex);
                        continue;
                    }
                }

                OLED_Clear();

                if (msg.valid)
                {
                    /* Keep both values inside the upper 32 rows.  This is
                       readable on 128x64 panels and also works on common
                       128x32 0.96-inch OLED modules. */
                    OLED_DrawString(0, 0, "T:");
                    OLED_DrawFloat(18, 0, msg.data.temperature, 3, 1);
                    OLED_DrawChar(54, 0, 'C');

                    OLED_DrawString(0, 16, "H:");
                    OLED_DrawFloat(18, 16, msg.data.humidity, 3, 1);
                    OLED_DrawChar(54, 16, '%');
                }
                else
                {
                    OLED_DrawString(0, 0, "T:ERR");
                    OLED_DrawString(0, 16, "H:ERR");
                }

                uint8_t displayOk = OLED_Refresh();
                if (!displayOk)
                {
                    /* A transient NACK must not stop the scheduler forever.
                       Reset the software bus and retry on the next sample. */
                    (void)LOG("[WARN] OLED refresh failed; recovering I2C bus\r\n");
                    I2C_Init();
                    oledInitialized = 0;
                }
                xSemaphoreGive(i2cMutex);
            }
        }
    }
}
