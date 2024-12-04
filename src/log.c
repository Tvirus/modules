#include "log.h"
#ifdef ENABLE_UART_PRINT_DMA
#include "ring_buffer.h"
#endif
#include "stm32l4xx_hal.h"
#include <stdarg.h>
#include <stdio.h>


extern UART_HandleTypeDef huart1;
#define UART_DEBUG  huart1

#ifdef ENABLE_UART_PRINT_DMA
static unsigned char uart_tx_buf[2000];
static ring_buffer_t uart_tx_rb = RBUF_INITIALIZER(uart_tx_buf, sizeof(uart_tx_buf), 1);
#endif


void log_printf(const char *fmt, ...)
{
    unsigned char buf[200];
    va_list arg;
    int len;
#ifdef ENABLE_UART_PRINT_DMA
    unsigned char *p;
    unsigned int count;
#endif

    va_start(arg, fmt);
    len = vsnprintf((char *)buf, sizeof(buf), fmt, arg);
    va_end(arg);
    if (sizeof(buf) <= len)
        len = sizeof(buf) - 1;

#ifdef ENABLE_UART_PRINT_DMA
    rbuf_put(&uart_tx_rb, buf, len);
    if (HAL_UART_STATE_READY == huart1.gState)
    {
        count = rbuf_get(&uart_tx_rb, &p, uart_tx_rb.member_count);
        if (count)
            HAL_UART_Transmit_DMA(&huart1, p, count * 1);
    }
#else
    HAL_UART_Transmit(&huart1, buf, len, len / 2);
#endif
}

#ifdef ENABLE_UART_PRINT_DMA
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    unsigned char *p;
    unsigned int count;

    count = rbuf_get(&uart_tx_rb, &p, uart_tx_rb.member_count);
    if (count)
        HAL_UART_Transmit_DMA(&huart1, p, count * 1);
}
#endif
