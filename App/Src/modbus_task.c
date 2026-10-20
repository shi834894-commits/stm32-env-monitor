#include "modbus_task.h"
#include "app_main.h"
#include "bsp_rs485.h"
#include "log.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>

#define MODBUS_SLAVE_ADDRESS       1U
#define MODBUS_FUNCTION_READ_INPUT 0x04U
#define MODBUS_FIRST_REGISTER      0x0000U
#define MODBUS_REGISTER_COUNT      4U
#define MODBUS_FRAME_MAX_LENGTH    64U
/*
 * A hardware UART at 115200 baud has a Modbus RTU gap well below 2 ms.
 * However, a USB-RS485 adapter can deliver a received packet to the host
 * UART in several short batches.  Waiting 10 ms keeps those batches in one
 * request while remaining far below the normal interval between requests.
 */
#define MODBUS_FRAME_GAP_MS        10U

static uint16_t Modbus_Crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;

    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8U; bit++)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = (uint16_t)((crc >> 1) ^ 0xA001U);
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static void Modbus_AppendCrc(uint8_t *frame, uint16_t length)
{
    uint16_t crc = Modbus_Crc16(frame, length);
    frame[length] = (uint8_t)(crc & 0xFFU);
    frame[length + 1U] = (uint8_t)(crc >> 8);
}

static void Modbus_SendException(uint8_t address, uint8_t function, uint8_t exception)
{
    uint8_t response[5];

    response[0] = address;
    response[1] = (uint8_t)(function | 0x80U);
    response[2] = exception;
    Modbus_AppendCrc(response, 3U);
    (void)RS485_Send(response, sizeof(response));
}

static uint8_t Modbus_ReadRegister(const SensorSnapshot_t *snapshot,
                                   uint16_t address,
                                   uint16_t *value)
{
    if (snapshot == NULL || value == NULL || address >= MODBUS_REGISTER_COUNT)
    {
        return 0U;
    }

    switch (address)
    {
        case 0x0000U:
            *value = (uint16_t)snapshot->temperature_tenths;
            break;
        case 0x0001U:
            *value = snapshot->humidity_tenths;
            break;
        case 0x0002U:
            *value = snapshot->status;
            break;
        case 0x0003U:
            *value = (uint16_t)(snapshot->sequence & 0xFFFFU);
            break;
        default:
            return 0U;
    }

    return 1U;
}

static void Modbus_ProcessFrame(const uint8_t *frame, uint16_t length)
{
    uint16_t received_crc;
    uint16_t calculated_crc;
    uint16_t start_address;
    uint16_t quantity;
    SensorSnapshot_t snapshot;
    uint8_t response[MODBUS_FRAME_MAX_LENGTH];
    uint16_t response_length;

    if (frame == NULL || length < 4U || frame[0] != MODBUS_SLAVE_ADDRESS)
    {
        return;
    }

    received_crc = (uint16_t)frame[length - 2U]
                 | ((uint16_t)frame[length - 1U] << 8);
    calculated_crc = Modbus_Crc16(frame, (uint16_t)(length - 2U));
    if (received_crc != calculated_crc)
    {
        (void)LOG("[MODBUS] CRC error\r\n");
        return;
    }

    if (frame[1] != MODBUS_FUNCTION_READ_INPUT)
    {
        Modbus_SendException(frame[0], frame[1], 0x01U);
        return;
    }

    if (length != 8U)
    {
        Modbus_SendException(frame[0], frame[1], 0x03U);
        return;
    }

    start_address = ((uint16_t)frame[2] << 8) | frame[3];
    quantity = ((uint16_t)frame[4] << 8) | frame[5];
    if (quantity == 0U || quantity > MODBUS_REGISTER_COUNT ||
        start_address < MODBUS_FIRST_REGISTER ||
        start_address + quantity > MODBUS_REGISTER_COUNT)
    {
        Modbus_SendException(frame[0], frame[1], 0x02U);
        return;
    }

    if (!App_GetSensorSnapshot(&snapshot))
    {
        return;
    }

    response[0] = MODBUS_SLAVE_ADDRESS;
    response[1] = MODBUS_FUNCTION_READ_INPUT;
    response[2] = (uint8_t)(quantity * 2U);
    response_length = 3U;

    for (uint16_t i = 0; i < quantity; i++)
    {
        uint16_t value;
        if (!Modbus_ReadRegister(&snapshot, (uint16_t)(start_address + i), &value))
        {
            Modbus_SendException(frame[0], frame[1], 0x02U);
            return;
        }
        response[response_length++] = (uint8_t)(value >> 8);
        response[response_length++] = (uint8_t)(value & 0xFFU);
    }

    Modbus_AppendCrc(response, response_length);
    response_length += 2U;
    if (RS485_Send(response, response_length) != HAL_OK)
    {
        (void)LOG("[MODBUS] response transmit failed\r\n");
    }
}

void ModbusTask(void *argument)
{
    uint8_t frame[MODBUS_FRAME_MAX_LENGTH];
    uint16_t frame_length = 0U;
    TickType_t last_byte_tick = 0U;

    (void)argument;
    (void)LOG("[TASK] ModbusTask started\r\n");

    for (;;)
    {
        uint8_t byte;
        while (RS485_ReadByte(&byte))
        {
            if (frame_length < MODBUS_FRAME_MAX_LENGTH)
            {
                frame[frame_length++] = byte;
            }
            else
            {
                frame_length = 0U;
                (void)LOG("[MODBUS] frame too long\r\n");
            }
            last_byte_tick = xTaskGetTickCount();

            /*
             * The supported Modbus requests (0x04 and the diagnostic
             * requests used during commissioning) have the standard
             * 8-byte request layout.  Process a complete request as soon
             * as all 8 bytes arrive.  This is important with USB-RS485
             * adapters, which may insert a much longer host-side gap than
             * the 3.5-character Modbus RTU gap between byte batches.
             */
            if (frame_length == 8U)
            {
                Modbus_ProcessFrame(frame, frame_length);
                frame_length = 0U;
                last_byte_tick = 0U;
            }
        }

        if (frame_length > 0U &&
            (xTaskGetTickCount() - last_byte_tick) >= pdMS_TO_TICKS(MODBUS_FRAME_GAP_MS))
        {
            Modbus_ProcessFrame(frame, frame_length);
            frame_length = 0U;
        }

        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}
