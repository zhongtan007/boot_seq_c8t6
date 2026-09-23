/**
  * @file    main.c
  * @brief   boot_seq_c8t6 主程序 (STM32F103C8T6 上电时序控制器)
  *
  * 架构: 前后台系统
  *   前台 (中断): EXTI1 时序触发 / TIM4 0.1ms 时序调度 / USART1 命令接收
  *   后台 (主循环): LED 心跳 + 串口命令解析
  *
  * 初始化顺序 (均为非阻塞, 无延时等待):
 *   SysTick 1ms -> LED -> USART1 115200 -> PA1(EXTI)+PB 输出+TIM4 -> 命令行
 *   PowerSeq_Init() 返回前会自动执行一次上电时序 (上电不检测 PA1) */
#include "stm32f10x.h"
#include "board.h"
#include "tick.h"
#include "led.h"
#include "uart.h"
#include "pwr_seq.h"
#include "cmd.h"

/* 每 20s 通过串口打印一次 hello 与运行时间 (非阻塞, 基于 SysTick) */
static void Hello_Task(uint32_t now_ms)
{
    static uint32_t s_last_ms = 0;
    uint32_t elapsed = now_ms - s_last_ms;

    if (elapsed < 20000U) return;
    s_last_ms = now_ms;

    UART_Printf("hello, uptime=%lu.%03lu s\r\n",
                (unsigned long)(now_ms / 1000U),
                (unsigned long)(now_ms % 1000U));
}

int main(void)
{
    /* NVIC 分组 2: 2 位抢占 + 2 位子优先级 (EXTI1/TIM4=0.0, USART1=1.0) */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    Tick_Init();          /* SysTick 1ms (LED 心跳时基) */
    LED_Init();           /* PC13 心跳 LED, 默认 500ms 闪烁 */
    UART_Init();          /* USART1 115200 8N1 */
    PowerSeq_Init();      /* PA1(EXTI)+PB 时序输出+TIM4, 末尾自动跑一次上电时序 */
    Cmd_Init();           /* 开机横幅 */

    while (1) {
        uint32_t now_ms = Tick_Millis();
        uint32_t sense  = GPIO_ReadInputDataBit(BOARD_SENSE_PORT, BOARD_SENSE_PIN);

        /* PA1 高 -> 1s 闪烁 1 次; PA1 低/浮空 -> 1s 闪烁 3 次 */
        LED_SetBlinkMs(sense ? BOARD_LED_BLINK_MS : BOARD_LED_BLINK_FAST_MS);

        LED_Task();           /* 心跳闪烁 (芯片正常工作指示) */
        Cmd_Task();           /* 串口命令处理 */
        Hello_Task(now_ms);   /* 1s 打印一次 hello 与运行时间 */
    }
}
