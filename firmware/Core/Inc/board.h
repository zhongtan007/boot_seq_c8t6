/* board.h - STM32F103C8T6-MINI 板级资源定义 (引脚 / 时钟 / 外设)
 *
 * 时钟: HSE 8MHz -> PLL x9 -> SYSCLK 72MHz (system_stm32f10x.c 默认配置)
 *       AHB = 72MHz, APB2 = 72MHz, APB1 = 36MHz (APB1 定时器时钟 x2 = 72MHz)
 *
 * 资源占用一览:
 *   PA1        上电感知输入 (EXTI1, 双边沿)
 *   PB1/PB10/PB12/PB14  4 路上/下电控制输出 (推挽, 复位默认低)
 *   PC13       板载 USER LED (低电平点亮, 心跳指示)
 *   PA9/PA10   USART1 TX/RX (115200 8N1, 命令口)
 *   TIM4       0.1ms 时序时基
 *   SysTick    1ms 系统节拍 (LED 闪烁 / 串口超时)
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"

/* ------------------------------------------------------------------ */
/* 固件版本信息                                                        */
/*   ASCII 字符串, 编译期由 __DATE__/__TIME__ 自动生成版本日期/时间,     */
/*   单独置于 .fw_version 段并固定烧写到 FLASH 0x08000400, 便于上位机     */
/*   按地址读取 (参考 pc_iic_monitor 的版本号存放设计)。                 */
/* ------------------------------------------------------------------ */
#define FW_NAME           "boot_seq_c8t6"
#define FW_VERSION        "1.0.0"
#define FW_VERSION_STRING FW_NAME " v" FW_VERSION " built " __DATE__ " " __TIME__

/* ------------------------------------------------------------------ */
/* 上电感知输入: PA1                                                     */
/* ------------------------------------------------------------------ */
#define BOARD_SENSE_GPIO_CLK    RCC_APB2Periph_GPIOA
#define BOARD_SENSE_PORT        GPIOA
#define BOARD_SENSE_PIN         GPIO_Pin_1
#define BOARD_SENSE_NAME        "PA1"
#define BOARD_SENSE_EXTI_LINE   EXTI_Line1
#define BOARD_SENSE_IRQn        EXTI1_IRQn
/* 输入模式: 下拉输入。板上信号为推挽输出时下拉无影响; 若为开漏/悬空信号,
 * 内部下拉可避免引脚悬空导致 EXTI 双边沿误触发。 */
#define BOARD_SENSE_IN_MODE     GPIO_Mode_IPD
/* EXTI 抢占优先级: 最高, 保证边沿到时序启动的延迟最小 */
#define BOARD_SENSE_PREEMPT_PRI 0

/* ------------------------------------------------------------------ */
/* 4 路上 / 下电控制输出 (全部在 GPIOB)                                   */
/*   ch0 = PB1  ch1 = PB10  ch2 = PB12  ch3 = PB14                      */
/* ------------------------------------------------------------------ */
#define BOARD_PWR_GPIO_CLK      RCC_APB2Periph_GPIOB
#define BOARD_PWR_PORT          GPIOB
#define BOARD_PWR_PIN_ALL       (GPIO_Pin_1 | GPIO_Pin_10 | GPIO_Pin_12 | GPIO_Pin_14)
#define BOARD_PWR_OUT_MODE      GPIO_Mode_Out_PP
#define BOARD_PWR_OUT_SPEED     GPIO_Speed_2MHz

/* ------------------------------------------------------------------ */
/* 心跳 LED: PC13 (PC13 -> R5 4.7K -> LED -> 3V3, 低电平点亮)            */
/* ------------------------------------------------------------------ */
#define BOARD_LED_GPIO_CLK      RCC_APB2Periph_GPIOC
#define BOARD_LED_PORT          GPIOC
#define BOARD_LED_PIN           GPIO_Pin_13
#define BOARD_LED_ACTIVE_LOW    1        /* 1 = 输出低电平点亮 */
#define BOARD_LED_BLINK_MS      500      /* 半周期: 亮 500ms / 灭 500ms (1s 一轮) */
#define BOARD_LED_BLINK_FAST_MS 166      /* 半周期: ~166ms (1s 3 次闪烁, PA1 低时) */

/* ------------------------------------------------------------------ */
/* 命令串口: USART1 (PA9 = TX, PA10 = RX), 115200 8N1                    */
/* ------------------------------------------------------------------ */
#define BOARD_UART              USART1
#define BOARD_UART_PERIPH_CLK   RCC_APB2Periph_USART1
#define BOARD_UART_GPIO_CLK     RCC_APB2Periph_GPIOA
#define BOARD_UART_PORT         GPIOA
#define BOARD_UART_TX_PIN       GPIO_Pin_9
#define BOARD_UART_RX_PIN       GPIO_Pin_10
#define BOARD_UART_IRQn         USART1_IRQn
#define BOARD_UART_BAUD         115200
#define BOARD_UART_RXBUF_SIZE   128      /* 接收环形缓冲区 (字节) */
#define BOARD_UART_PREEMPT_PRI  1        /* 低于 EXTI1 / TIM4 */

/* ------------------------------------------------------------------ */
/* 时序时基: TIM4 @ 10kHz (0.1ms)                                       */
/*   STM32F103C8T6 为中容量器件, 没有 TIM6/TIM7, 故用 TIM4 替代          */
/*   原工程 (STM32H563) 中的 TIM7 时基。                                 */
/*   72MHz / (71+1) = 1MHz, 1MHz / (99+1) = 10kHz                       */
/* ------------------------------------------------------------------ */
#define BOARD_SEQ_TIM           TIM4
#define BOARD_SEQ_TIM_CLK       RCC_APB1Periph_TIM4
#define BOARD_SEQ_TIM_IRQn      TIM4_IRQn
#define BOARD_SEQ_TIM_PSC       71
#define BOARD_SEQ_TIM_ARR       99
#define BOARD_SEQ_PREEMPT_PRI   0        /* 与 EXTI1 同级, 高于串口 */

/* ------------------------------------------------------------------ */
/* 系统节拍: SysTick @ 1kHz (1ms)                                       */
/* ------------------------------------------------------------------ */
#define BOARD_TICK_HZ           1000

#endif /* BOARD_H */
