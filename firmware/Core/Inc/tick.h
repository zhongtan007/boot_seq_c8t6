/* tick.h - 1ms 系统节拍 (SysTick) */
#ifndef TICK_H
#define TICK_H

#include <stdint.h>

void     Tick_Init(void);      /* SysTick 1ms 中断 */
uint32_t Tick_Millis(void);    /* 开机以来的毫秒数 */
void     Tick_OnIsr(void);     /* SysTick 中断入口 (SysTick_Handler 调用) */

#endif /* TICK_H */
