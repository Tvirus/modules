#include "log.h"
#ifdef ENABLE_LOG_BUFFER
#include "ring_buffer.h"
#endif
#include <stdarg.h>
#include <stdio.h>
#include "stm32u5xx_hal.h"


extern UART_HandleTypeDef huart1;
#define LOG_UART_HANDLE huart1

#ifdef ENABLE_LOG_BUFFER
static unsigned char uart_tx_buf[2000];
static ring_buffer_t uart_tx_rb = RBUF_INITIALIZER(uart_tx_buf, sizeof(uart_tx_buf), 1);
#endif


#if defined(ENABLE_LOG_BUFFER) && (USE_HAL_UART_REGISTER_CALLBACKS != 0)
static void uart_tx_complete_cb(UART_HandleTypeDef *huart)
{
    unsigned char *p;
    unsigned int count;

    count = rbuf_get(&uart_tx_rb, &p, uart_tx_rb.member_count);
    if (count)
        HAL_UART_Transmit_DMA(&LOG_UART_HANDLE, p, count * 1);
}
#endif

int log_init(void)
{
#if defined(ENABLE_LOG_BUFFER) && (USE_HAL_UART_REGISTER_CALLBACKS != 0)
    if (HAL_OK != HAL_UART_RegisterCallback(&LOG_UART_HANDLE, HAL_UART_TX_COMPLETE_CB_ID, uart_tx_complete_cb))
        return -1;
#endif
    return 0;
}

void log_printf(const char *fmt, ...)
{
    unsigned char buf[200];
    va_list arg;
    int len;
#ifdef ENABLE_LOG_BUFFER
    unsigned char *p;
    unsigned int count;
#endif

    va_start(arg, fmt);
    len = vsnprintf((char *)buf, sizeof(buf), fmt, arg);
    va_end(arg);
    if (sizeof(buf) <= len)
        len = sizeof(buf) - 1;

#ifdef ENABLE_LOG_BUFFER
    rbuf_put(&uart_tx_rb, buf, len, 0);
    if (HAL_UART_STATE_READY == LOG_UART_HANDLE.gState)
    {
        count = rbuf_get(&uart_tx_rb, &p, uart_tx_rb.member_count);
        if (count)
            HAL_UART_Transmit_DMA(&LOG_UART_HANDLE, p, count * 1);
    }
#else
    HAL_UART_Transmit(&LOG_UART_HANDLE, buf, len, len / 2);
#endif
}

#if defined(ENABLE_LOG_BUFFER) && (USE_HAL_UART_REGISTER_CALLBACKS == 0)
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    unsigned char *p;
    unsigned int count;

    count = rbuf_get(&uart_tx_rb, &p, uart_tx_rb.member_count);
    if (count)
        HAL_UART_Transmit_DMA(&LOG_UART_HANDLE, p, count * 1);
}
#endif
