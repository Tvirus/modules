#include "emsg_user.h"
#include "emsg.h"
#include <stddef.h>
#include "ring_buffer.h"
#include "stm32l4xx_hal.h"
#include "stm32l4xx_ll_usart.h"




extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef hdma_usart3_rx;

uint8_t uart3_conn_id;

uint8_t uart3_rx_dma_buf[EMSG_ENCODE_LEN_MAX];
uint8_t uart3_rx_buf[EMSG_ENCODE_LEN_MAX * 4];
ring_buffer_t uart3_rx_rbuf = RBUF_INITIALIZER(uart3_rx_buf, sizeof(uart3_rx_buf), 1);


int emsg_user_init(void)
{
    HAL_UART_ReceiverTimeout_Config(&huart3, 100);
    HAL_UART_EnableReceiverTimeout(&huart3);
    LL_USART_EnableIT_RTO(huart3.Instance);
    HAL_UART_Receive_DMA(&huart3, uart3_rx_dma_buf, sizeof(uart3_rx_dma_buf));
    return 0;
}

void emsg_user_task(void)
{
    uint32_t current_ts;
    static uint32_t uart3_ts = 0;
    uint8_t *uart3_data;
    uint32_t uart3_data_len;

    current_ts = HAL_GetTick();

    if (5 < (uint32_t)(current_ts - uart3_ts))
    {
        uart3_ts = current_ts;

        uart3_data_len = rbuf_get(&uart3_rx_rbuf, &uart3_data, EMSG_ENCODE_LEN_MAX);
        if (uart3_data_len)
            emsg_recv(uart3_conn_id, uart3_data, uart3_data_len);
    }
}


uint32_t uart3_sender(const uint8_t *data, uint32_t len)
{
    HAL_UART_Transmit(&huart3, data, len, len);
    return len;
}
void uart3_irq_handler(void)
{
    if (huart3.ErrorCode)
    {
        if (huart3.ErrorCode & HAL_UART_ERROR_RTO)
            rbuf_put(&uart3_rx_rbuf, uart3_rx_dma_buf, sizeof(uart3_rx_dma_buf) - __HAL_DMA_GET_COUNTER(&hdma_usart3_rx);
        HAL_UART_Receive_DMA(&huart3, uart3_rx_dma_buf, sizeof(uart3_rx_dma_buf));
    }
}
/*
void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart3);
    uart3_irq_handler();
}
*/
