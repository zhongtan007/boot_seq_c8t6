/**
  * @file    stm32f10x_it.c
  * @brief   中断服务函数
  *
  * 本工程实际使用的 3 个外设中断:
  *   EXTI1_IRQHandler   PA1 边沿 -> 启动上/下电时序   (抢占优先级 0)
  *   TIM4_IRQHandler    0.1ms 时序时基步进            (抢占优先级 0)
  *   USART1_IRQHandler  命令接收 -> 环形缓冲区        (抢占优先级 1)
  *   SysTick_Handler    1ms 系统节拍                  (最低优先级)
  */
#include "stm32f10x_it.h"
#include "board.h"
#include "tick.h"
#include "uart.h"
#include "pwr_seq.h"

/******************************************************************************/
/*            Cortex-M3 处理器异常处理                                         */
/******************************************************************************/

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    /* 硬件错误: 停在此处便于调试器定位 */
    while (1) {
    }
}

void MemManage_Handler(void)
{
    while (1) {
    }
}

void BusFault_Handler(void)
{
    while (1) {
    }
}

void UsageFault_Handler(void)
{
    while (1) {
    }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    Tick_OnIsr();
}

/******************************************************************************/
/*            STM32F10x 外设中断处理                                           */
/******************************************************************************/

/* PA1 双边沿: 判断方向并启动上/下电时序 */
void EXTI1_IRQHandler(void)
{
    if (EXTI_GetITStatus(BOARD_SENSE_EXTI_LINE) != RESET) {
        PowerSeq_OnExtiIsr();
    }
}

/* TIM4 更新: 0.1ms 时序时基 */
void TIM4_IRQHandler(void)
{
    PowerSeq_OnTickIsr();
}

/* USART1: 命令字节接收 */
void USART1_IRQHandler(void)
{
    UART_OnRxIsr();
}
