/* tick.c - 1ms 系统节拍 (SysTick), 供 LED 心跳等非阻塞定时使用 */
#include "tick.h"
#include "board.h"

static volatile uint32_t s_millis;

void Tick_Init(void)
{
    s_millis = 0;
    /* 72MHz / 1000 = 1ms 重装载 */
    SysTick_Config(SystemCoreClock / BOARD_TICK_HZ);
    NVIC_SetPriority(SysTick_IRQn, 0x0FU);   /* 最低优先级, 不抢时序中断 */
}

uint32_t Tick_Millis(void)
{
    return s_millis;
}

void Tick_OnIsr(void)
{
    s_millis++;
}
