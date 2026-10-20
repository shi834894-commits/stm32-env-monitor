#include "bsp_rs485.h"
#include "usart.h"

static volatile uint8_t rx_buffer[RS485_RX_BUFFER_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static volatile uint32_t rx_overflow_count;
static volatile uint32_t rx_error_count;
static volatile uint32_t rx_received_count;

void RS485_SetTransmit(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
}

void RS485_SetReceive(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
}

void RS485_Init(void)
{
    rx_head = 0U;
    rx_tail = 0U;
    rx_overflow_count = 0U;
    rx_error_count = 0U;
    rx_received_count = 0U;
    RS485_SetReceive();
    __HAL_UART_CLEAR_PEFLAG(&huart2);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_ERR);
}

uint8_t RS485_ReadByte(uint8_t *byte)
{
    uint16_t tail;

    if (byte == NULL || rx_tail == rx_head)
    {
        return 0U;
    }

    tail = rx_tail;
    *byte = rx_buffer[tail];
    rx_tail = (uint16_t)((tail + 1U) % RS485_RX_BUFFER_SIZE);
    return 1U;
}

HAL_StatusTypeDef RS485_Send(const uint8_t *data, uint16_t length)
{
    HAL_StatusTypeDef status;

    if (data == NULL || length == 0U)
    {
        return HAL_ERROR;
    }

    RS485_SetTransmit();
    status = HAL_UART_Transmit(&huart2, (uint8_t *)data, length, 100U);
    RS485_SetReceive();
    return status;
}

uint32_t RS485_GetOverflowCount(void)
{
    return rx_overflow_count;
}

uint32_t RS485_GetErrorCount(void)
{
    return rx_error_count;
}

uint32_t RS485_GetReceivedCount(void)
{
    return rx_received_count;
}

void RS485_IRQHandler(void)
{
    uint32_t status = huart2.Instance->SR;
    uint8_t data = (uint8_t)huart2.Instance->DR;

    if ((status & (USART_SR_PE | USART_SR_FE | USART_SR_ORE | USART_SR_NE)) != 0U)
    {
        rx_error_count++;
    }

    if ((status & USART_SR_RXNE) != 0U)
    {
        rx_received_count++;
        uint16_t next = (uint16_t)((rx_head + 1U) % RS485_RX_BUFFER_SIZE);
        if (next == rx_tail)
        {
            rx_overflow_count++;
        }
        else
        {
            rx_buffer[rx_head] = data;
            rx_head = next;
        }
    }
}
