/* led.c - 板载心跳 LED (PC13 -> R5 4.7K -> LED -> 3V3, 低电平点亮)
 *
 * 心跳闪烁表示芯片正常工作 (保留自 demo 工程 PC13 500ms 闪烁逻辑,
 * 改为 SysTick 1ms 节拍非阻塞驱动, 不干扰上电时序的硬实时调度)。
 */
#include "led.h"
#include "board.h"
#include "tick.h"

static volatile uint32_t s_heartbeat = 1;   /* 默认心跳闪烁 */
static uint32_t s_last_ms;
static uint32_t s_blink_ms = BOARD_LED_BLINK_MS;   /* 心跳半周期, 默认 500ms */

#if BOARD_LED_ACTIVE_LOW
#define LED_DRIVE_ON()    GPIO_ResetBits(BOARD_LED_PORT, BOARD_LED_PIN)
#define LED_DRIVE_OFF()   GPIO_SetBits(BOARD_LED_PORT, BOARD_LED_PIN)
#else
#define LED_DRIVE_ON()    GPIO_SetBits(BOARD_LED_PORT, BOARD_LED_PIN)
#define LED_DRIVE_OFF()   GPIO_ResetBits(BOARD_LED_PORT, BOARD_LED_PIN)
#endif

void LED_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(BOARD_LED_GPIO_CLK, ENABLE);
    LED_DRIVE_OFF();                        /* 复位后先灭, 再配置成推挽输出 */
    gpio.GPIO_Pin   = BOARD_LED_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(BOARD_LED_PORT, &gpio);

    s_heartbeat = 1;
    s_last_ms = Tick_Millis();
}

void LED_On(void)
{
    LED_DRIVE_ON();
}

void LED_Off(void)
{
    LED_DRIVE_OFF();
}

void LED_Toggle(void)
{
    BOARD_LED_PORT->ODR ^= BOARD_LED_PIN;
}

uint32_t LED_State(void)
{
    uint32_t pin_high = GPIO_ReadOutputDataBit(BOARD_LED_PORT, BOARD_LED_PIN) ? 1U : 0U;
#if BOARD_LED_ACTIVE_LOW
    return pin_high ? 0U : 1U;
#else
    return pin_high;
#endif
}

uint32_t LED_Heartbeat(void)
{
    return s_heartbeat;
}

void LED_SetHeartbeat(uint32_t en)
{
    s_heartbeat = (en != 0U) ? 1U : 0U;
}

void LED_SetBlinkMs(uint32_t half_period_ms)
{
    if (half_period_ms == 0U) half_period_ms = 1U;   /* 防 0 导致紧循环 */
    s_blink_ms = half_period_ms;
}

void LED_Task(void)
{
    uint32_t now;

    if (!s_heartbeat) return;
    now = Tick_Millis();
    if ((uint32_t)(now - s_last_ms) >= s_blink_ms) {
        s_last_ms = now;
        LED_Toggle();
    }
}
