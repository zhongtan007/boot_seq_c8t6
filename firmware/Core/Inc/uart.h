/* uart.h - 命令串口 (USART1, PA9=TX PA10=RX, 115200 8N1) */
#ifndef UART_H
#define UART_H

#include <stdint.h>

void UART_Init(void);                              /* GPIO/USART/NVIC 初始化 */
void UART_SendString(const char *s);               /* 发送字符串 (轮询) */
void UART_Printf(const char *fmt, ...);            /* 格式化发送 (轮询) */
int  UART_GetLine(char *buf, int maxlen);          /* 取一行命令; 收到完整行返回 1 */
void UART_OnRxIsr(void);                           /* RX 中断入口 (USART1_IRQHandler 调用) */

#endif /* UART_H */
