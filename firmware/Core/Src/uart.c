/* uart.c - 命令串口驱动
 *
 * USART1 @ 115200 8N1:
 *   RX: RXNE 中断 -> 环形缓冲区 (并回显字符, '\r' 归一化为 '\n')
 *   TX: 轮询 TXE 发送 (命令交互数据量小, 无需 DMA/中断)
 *   主循环 UART_GetLine() 从环形缓冲区取整行命令交给 cmd 模块解析。
 */
#include "uart.h"
#include "board.h"

#include <stdarg.h>
#include <stdio.h>

static volatile uint8_t  s_rxbuf[BOARD_UART_RXBUF_SIZE];
static volatile uint32_t s_rd;    /* 主循环读指针 */
static volatile uint32_t s_wr;    /* 中断写指针 */

void UART_Init(void)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef  nvic;

    RCC_APB2PeriphClockCmd(BOARD_UART_GPIO_CLK | BOARD_UART_PERIPH_CLK, ENABLE);

    /* PA9 = TX: 复用推挽; PA10 = RX: 上拉输入 */
    gpio.GPIO_Pin   = BOARD_UART_TX_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BOARD_UART_PORT, &gpio);

    gpio.GPIO_Pin  = BOARD_UART_RX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(BOARD_UART_PORT, &gpio);

    usart.USART_BaudRate   = BOARD_UART_BAUD;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits   = USART_StopBits_1;
    usart.USART_Parity     = USART_Parity_No;
    usart.USART_Mode       = USART_Mode_Rx | USART_Mode_Tx;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(BOARD_UART, &usart);

    nvic.NVIC_IRQChannel = BOARD_UART_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = BOARD_UART_PREEMPT_PRI;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    USART_ITConfig(BOARD_UART, USART_IT_RXNE, ENABLE);
    USART_Cmd(BOARD_UART, ENABLE);
}

void UART_SendString(const char *s)
{
    while (*s) {
        while (USART_GetFlagStatus(BOARD_UART, USART_FLAG_TXE) == RESET) {}
        USART_SendData(BOARD_UART, (uint16_t)(uint8_t)*s++);
    }
}

void UART_Printf(const char *fmt, ...)
{
    static char buf[128];     /* static: 避免主循环/中断栈压力 */
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    UART_SendString(buf);
}

int UART_GetLine(char *buf, int maxlen)
{
    uint32_t r = s_rd;
    uint32_t w = s_wr;
    int n = 0;

    while (r != w) {
        char c = (char)s_rxbuf[r];
        r = (r + 1U) % BOARD_UART_RXBUF_SIZE;
        if (c == '\n') {                 /* 收到行结束 */
            s_rd = r;
            buf[n] = '\0';
            return 1;
        }
        if (c != '\r' && n < maxlen - 1) {
            buf[n++] = c;
        }
    }
    return 0;                            /* 还没有完整行 */
}

void UART_OnRxIsr(void)
{
    if (USART_GetITStatus(BOARD_UART, USART_IT_RXNE) != RESET) {
        uint8_t  c    = (uint8_t)USART_ReceiveData(BOARD_UART);   /* 读 DR 同时清 ORE */
        uint32_t next;

        if (c == '\r') c = '\n';         /* CR/LF 统一按行结束处理 */

        while (USART_GetFlagStatus(BOARD_UART, USART_FLAG_TXE) == RESET) {}
        USART_SendData(BOARD_UART, c);   /* 回显 */

        next = (s_wr + 1U) % BOARD_UART_RXBUF_SIZE;
        if (next != s_rd) {              /* 满则丢弃新字节 (命令最长 96, 正常不会满) */
            s_rxbuf[s_wr] = c;
            s_wr = next;
        }
    }
}
