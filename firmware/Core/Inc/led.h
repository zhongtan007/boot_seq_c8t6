/* led.h - 板载心跳 LED (PC13, 低电平点亮) */
#ifndef LED_H
#define LED_H

#include <stdint.h>

void     LED_Init(void);             /* GPIO 初始化, 默认开启心跳闪烁 */
void     LED_On(void);               /* 常亮 */
void     LED_Off(void);              /* 常灭 */
void     LED_Toggle(void);           /* 翻转 */
uint32_t LED_State(void);            /* 1=亮 0=灭 */
uint32_t LED_Heartbeat(void);        /* 1=心跳闪烁模式 0=手动常亮/常灭 */
void     LED_SetHeartbeat(uint32_t en); /* 1=心跳闪烁(默认) 0=由 On/Off 手动控制 */
void     LED_SetBlinkMs(uint32_t half_period_ms); /* 设置心跳半周期(ms) */
void     LED_Task(void);             /* 主循环轮询: 心跳闪烁 (500ms 翻转) */

#endif /* LED_H */
