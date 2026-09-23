/* pwr_seq.c - 上电 / 下电时序控制 (PA1 双边沿触发, PB1/PB10/PB12/PB14 依次动作)
 *
 * 由内部 STM32H563 参考工程 (pwr_seq.c) 1:1 移植,
 * 时序算法与原工程完全一致, 仅替换 HAL 为标准外设库, 引脚/定时器映射:
 *
 *   原工程 (H563)                本工程 (C8T6)
 *   --------------------------   --------------------------
 *   PE9  触发输入 (EXTI9)     -> PA1  触发输入 (EXTI1, 浮空输入)
 *   PE10 通道 0              -> PB1  通道 0
 *   PE11 通道 1              -> PB10 通道 1
 *   PE12 通道 2              -> PB12 通道 2
 *   PE13 通道 3              -> PB14 通道 3
 *   TIM7 10kHz 时基          -> TIM4 10kHz 时基 (C8T6 无 TIM6/TIM7)
 *   GPIO_PIN_x / BSRR        -> GPIO_Pin_x / BSRR (同为原子置位/复位)
 *
 * 时序算法:
 *   正时序 (MCU 启动时自动执行 / 手动 up / PA1 上升沿): 通道 i 在 Ti 时刻拉高。
 *   反时序 (PA1 下降沿 / 手动 down): 通道 i 在 (Tmax - Ti) 时刻拉低,
 *     即"上电最后起来的先下电", 各步间隔与上电完全镜像。
 *
 * 触发策略:
 *   - 上电: MCU 复位启动后立即无条件跑一次正时序, 不检测 PA1。
 *   - 下电: PA1 变低 (下降沿) -> EXTI1 中断 -> 反时序拉低。
 *   - PA1 再次变高 (上升沿) -> 重新执行正时序。
 *
 * 实现:
 *   1) MCU 启动 / PA1 边沿 -> 读 PA1 当前电平判断方向 (F1 的 EXTI 无
 *      独立上升/下降 pending 标志) -> 按方向计算各路目标 tick,
 *      置 pending 掩码, 启动 TIM4。
 *   2) TIM4 @ 10kHz (0.1ms): 72MHz/(PSC+1=72)/(ARR+1=100) = 10kHz。
 *      每 0.1ms 把"已到时"的通道用一次 BSRR 写同时动作 (延迟相同者无先后差)。
 *   3) 4 路全部动作完毕自动停 TIM4 (不长期占用中断)。
 *
 * 说明:
 *   - 同方向重复触发被忽略 (上电中再上电/下电中再下电), 反方向边沿可抢占
 *     (下电过程中 PA1 拉高会立即转为上电时序)。
 *   - EXTI1 与 TIM4 均为最高抢占优先级, 保证时序精度。
 */
#include "pwr_seq.h"
#include "board.h"

#include <stdbool.h>
#include <string.h>

#define PWRSEQ_PENDING_ALL  0x0FU        /* 4 路均待动作 */

/* 通道 -> 引脚位 (GPIOB bit1/10/12/14) */
static const uint16_t s_ch_pin[PWR_SEQ_CH_COUNT] = {
    GPIO_Pin_1, GPIO_Pin_10, GPIO_Pin_12, GPIO_Pin_14
};

static volatile float    s_delay_ms[PWR_SEQ_CH_COUNT];
static volatile uint32_t s_delay_tick[PWR_SEQ_CH_COUNT];   /* 延迟, 单位 0.1ms */
static volatile uint32_t s_target_tick[PWR_SEQ_CH_COUNT];  /* 本方向下的目标 tick */
static volatile uint32_t s_counter;         /* 当前 tick (0.1ms 计数) */
static volatile uint32_t s_pending_mask;    /* 尚未动作的通道 */
static volatile uint32_t s_mode;            /* PWR_SEQ_MODE_* */
static volatile uint32_t s_seq_up;
static volatile uint32_t s_seq_down;
static volatile bool     s_en;              /* PA1 联动使能 */
static bool              s_inited;

/* ---------------------------------------------------------------- */

static uint32_t pwrseq_ms_to_tick(float ms)
{
    if (ms < 0.0f) ms = 0.0f;
    if (ms > PWR_SEQ_MAX_MS) ms = PWR_SEQ_MAX_MS;
    /* +0.5: 四舍五入到最近 tick (0.1ms), 保证 1.2ms -> 12 tick 而非 11 */
    return (uint32_t)(ms / PWR_SEQ_TICK_MS + 0.5f);
}

/* 把所有"已到时"的通道动作 (上电拉高 / 下电拉低); 全部完成后停表。
 * 可在 ISR 中调用。 */
static void pwrseq_apply_due(void)
{
    uint32_t act = 0;

    for (uint32_t i = 0; i < PWR_SEQ_CH_COUNT; i++) {
        if ((s_pending_mask & (1UL << i)) != 0U &&
            s_counter >= s_target_tick[i]) {
            s_pending_mask &= ~(1UL << i);
            act |= s_ch_pin[i];
        }
    }
    if (act != 0U) {
        if (s_mode == PWR_SEQ_MODE_DOWN) {
            BOARD_PWR_PORT->BSRR = act << 16;   /* 高 16 位: 写 1 复位 (拉低) */
        } else {
            BOARD_PWR_PORT->BSRR = act;         /* 低 16 位: 写 1 置位 (拉高) */
        }
    }
    if (s_pending_mask == 0U) {
        s_mode = PWR_SEQ_MODE_IDLE;
        TIM_ITConfig(BOARD_SEQ_TIM, TIM_IT_Update, DISABLE);
        TIM_Cmd(BOARD_SEQ_TIM, DISABLE);
    }
}

/* 按方向建立调度表并启动计时 */
static void pwrseq_start(uint32_t mode)
{
    uint32_t max_tick = 0;

    for (uint32_t i = 0; i < PWR_SEQ_CH_COUNT; i++) {
        if (s_delay_tick[i] > max_tick) max_tick = s_delay_tick[i];
    }
    for (uint32_t i = 0; i < PWR_SEQ_CH_COUNT; i++) {
        /* 下电: 延迟最大的最先拉低 -> 目标时刻 = Tmax - Ti (时间轴镜像) */
        s_target_tick[i] = (mode == PWR_SEQ_MODE_UP)
                           ? s_delay_tick[i]
                           : (max_tick - s_delay_tick[i]);
    }

    s_counter = 0;
    s_pending_mask = PWRSEQ_PENDING_ALL;
    s_mode = mode;
    if (mode == PWR_SEQ_MODE_UP) s_seq_up++;
    else                         s_seq_down++;

    pwrseq_apply_due();                  /* 立即处理 0 时刻通道 */
    if (s_mode == PWR_SEQ_MODE_IDLE) return;   /* 全部为 0ms, 无需计时 */

    BOARD_SEQ_TIM->CNT = 0;
    TIM_ClearFlag(BOARD_SEQ_TIM, TIM_FLAG_Update);
    TIM_ITConfig(BOARD_SEQ_TIM, TIM_IT_Update, ENABLE);
    TIM_Cmd(BOARD_SEQ_TIM, ENABLE);
}

/* ---------------------------------------------------------------- */

void PowerSeq_Init(void)
{
    GPIO_InitTypeDef    gpio;
    EXTI_InitTypeDef    exti;
    NVIC_InitTypeDef    nvic;
    TIM_TimeBaseInitTypeDef tim;

    if (s_inited) return;
    s_inited = true;

    /* 默认时序 (写死在 MCU 逻辑里, 串口 `seq set` 可改写):
     *   PB1 = 10ms   PB10 = 0ms   PB12 = 0ms   PB14 = 0ms
     * 上电: PB10/PB12/PB14 立即拉高, PB1 延迟 10ms 拉高;
     * 下电: PB1 立即拉低, PB10/PB12/PB14 延迟 10ms 拉低 (反序镜像)。
     * 上电时序在 MCU 启动时自动执行一次 (见函数末尾), 不等 PA1 边沿;
     * 串口可随时改写延迟。 */
    static const float def_ms[PWR_SEQ_CH_COUNT] = {
        PWR_SEQ_DEF_MS0, PWR_SEQ_DEF_MS1, PWR_SEQ_DEF_MS2, PWR_SEQ_DEF_MS3
    };
    for (uint32_t i = 0; i < PWR_SEQ_CH_COUNT; i++) {
        s_delay_ms[i]    = def_ms[i];
        s_delay_tick[i]  = pwrseq_ms_to_tick(def_ms[i]);
        s_target_tick[i] = 0U;
    }
    s_counter      = 0;
    s_pending_mask = 0U;
    s_mode         = PWR_SEQ_MODE_IDLE;
    s_seq_up       = 0;
    s_seq_down     = 0;
    s_en           = true;

    RCC_APB2PeriphClockCmd(BOARD_PWR_GPIO_CLK | BOARD_SENSE_GPIO_CLK |
                           RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(BOARD_SEQ_TIM_CLK, ENABLE);

    /* PB1/PB10/PB12/PB14: 推挽输出, 先写低再配置 (避免配置瞬间输出不确定) */
    GPIO_ResetBits(BOARD_PWR_PORT, BOARD_PWR_PIN_ALL);
    gpio.GPIO_Pin   = BOARD_PWR_PIN_ALL;
    gpio.GPIO_Mode  = BOARD_PWR_OUT_MODE;
    gpio.GPIO_Speed = BOARD_PWR_OUT_SPEED;
    GPIO_Init(BOARD_PWR_PORT, &gpio);

    /* PA1: 触发输入 (浮空输入) */
    gpio.GPIO_Pin  = BOARD_SENSE_PIN;
    gpio.GPIO_Mode = BOARD_SENSE_IN_MODE;
    GPIO_Init(BOARD_SENSE_PORT, &gpio);

    /* EXTI1: PA1 上升沿 (再次上电) + 下降沿 (反序下电) 均触发中断 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource1);
    exti.EXTI_Line    = BOARD_SENSE_EXTI_LINE;
    exti.EXTI_Mode    = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);
    EXTI_ClearITPendingBit(BOARD_SENSE_EXTI_LINE);   /* 清掉配置期间可能的挂起 */

    nvic.NVIC_IRQChannel = BOARD_SENSE_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = BOARD_SENSE_PREEMPT_PRI;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    /* TIM4: 0.1ms 时基 (72MHz/72/100 = 10kHz), 初始不使能中断 */
    tim.TIM_Prescaler     = BOARD_SEQ_TIM_PSC;   /* 72MHz/72 = 1MHz */
    tim.TIM_CounterMode   = TIM_CounterMode_Up;
    tim.TIM_Period        = BOARD_SEQ_TIM_ARR;   /* 1MHz/100 = 10kHz */
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    tim.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(BOARD_SEQ_TIM, &tim);
    TIM_ITConfig(BOARD_SEQ_TIM, TIM_IT_Update, DISABLE);
    TIM_Cmd(BOARD_SEQ_TIM, DISABLE);
    TIM_ClearFlag(BOARD_SEQ_TIM, TIM_FLAG_Update);   /* TimeBaseInit 装载 PSC 会置一次 UIF */

    nvic.NVIC_IRQChannel = BOARD_SEQ_TIM_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = BOARD_SEQ_PREEMPT_PRI;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    /* MCU 启动即无条件执行一次正时序 (上电), 不检测 PA1 电平/边沿。
     * 此后 PA1 仅作为下电触发: 下降沿执行反序下电, 再次变高则恢复上电。 */
    pwrseq_start(PWR_SEQ_MODE_UP);
}

void PowerSeq_SetAll(float d0, float d1, float d2, float d3)
{
    float d[PWR_SEQ_CH_COUNT];
    d[0] = d0; d[1] = d1; d[2] = d2; d[3] = d3;

    for (uint32_t i = 0; i < PWR_SEQ_CH_COUNT; i++) {
        s_delay_tick[i] = pwrseq_ms_to_tick(d[i]);   /* 先写 tick (运行中生效) */
        s_delay_ms[i]   = d[i] < 0.0f ? 0.0f
                        : (d[i] > PWR_SEQ_MAX_MS ? PWR_SEQ_MAX_MS : d[i]);
    }
}

void PowerSeq_SetDefault(void)
{
    PowerSeq_SetAll(PWR_SEQ_DEF_MS0, PWR_SEQ_DEF_MS1,
                    PWR_SEQ_DEF_MS2, PWR_SEQ_DEF_MS3);
}

void PowerSeq_SetEnabled(uint32_t en)
{
    /* 只改联动开关, 不打断正在执行的时序 (让当前时序跑完) */
    s_en = (en != 0U);
}

void PowerSeq_Reset(void)
{
    s_mode = PWR_SEQ_MODE_IDLE;
    s_pending_mask = 0U;
    TIM_ITConfig(BOARD_SEQ_TIM, TIM_IT_Update, DISABLE);
    TIM_Cmd(BOARD_SEQ_TIM, DISABLE);
    TIM_ClearFlag(BOARD_SEQ_TIM, TIM_FLAG_Update);
    s_counter = 0;
    BOARD_PWR_PORT->BSRR = BOARD_PWR_PIN_ALL << 16;   /* 高 16 位: 写 1 复位 (拉低) */
}

void PowerSeq_OnExtiIsr(void)
{
    /* 由 stm32f10x_it.c 的 EXTI1_IRQHandler 调用 (已确认 EXTI_Line1 有挂起) */
    bool rising;

    EXTI_ClearITPendingBit(BOARD_SENSE_EXTI_LINE);
    /* F1 的 EXTI 只有一个 pending 标志: 读 PA1 当前电平判断方向。
     * 极窄脉冲 (短于中断响应时间) 有判反可能, 常规电源信号无影响。 */
    rising = (BOARD_SENSE_PORT->IDR & BOARD_SENSE_PIN) != 0U;

    if (!s_inited || !s_en) return;
    if (rising) {
        if (s_mode != PWR_SEQ_MODE_UP) pwrseq_start(PWR_SEQ_MODE_UP);
    } else {
        if (s_mode != PWR_SEQ_MODE_DOWN) pwrseq_start(PWR_SEQ_MODE_DOWN);
    }
}

void PowerSeq_OnTickIsr(void)
{
    /* 由 stm32f10x_it.c 的 TIM4_IRQHandler 调用 */
    if (TIM_GetITStatus(BOARD_SEQ_TIM, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(BOARD_SEQ_TIM, TIM_IT_Update);
        if (s_mode != PWR_SEQ_MODE_IDLE) {
            s_counter++;
            pwrseq_apply_due();
        }
    }
}

void PowerSeq_TriggerUp(void)
{
    if (!s_inited) return;
    pwrseq_start(PWR_SEQ_MODE_UP);           /* 手动触发不受联动开关限制 */
}

void PowerSeq_TriggerDown(void)
{
    if (!s_inited) return;
    pwrseq_start(PWR_SEQ_MODE_DOWN);
}

void PowerSeq_GetStatus(pwr_seq_status_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    for (uint32_t i = 0; i < PWR_SEQ_CH_COUNT; i++) {
        out->delay_ms[i] = s_delay_ms[i];
    }
    out->en       = s_en ? 1U : 0U;
    out->in_level = (BOARD_SENSE_PORT->IDR & BOARD_SENSE_PIN) ? 1U : 0U;   /* PA1 电平 */
    out->mode     = s_mode;
    out->running  = (s_mode == PWR_SEQ_MODE_IDLE) ? 0U : 1U;
    out->output_levels = 0;
    for (uint32_t i = 0; i < PWR_SEQ_CH_COUNT; i++) {
        if (BOARD_PWR_PORT->IDR & s_ch_pin[i]) {
            out->output_levels |= (1UL << i);
        }
    }
    out->seq_up   = s_seq_up;
    out->seq_down = s_seq_down;
}

const char *PowerSeq_ChName(uint32_t ch)
{
    static const char *const names[PWR_SEQ_CH_COUNT] = { "PB1", "PB10", "PB12", "PB14" };
    return (ch < PWR_SEQ_CH_COUNT) ? names[ch] : "?";
}

uint32_t PowerSeq_ChDelayTick(uint32_t ch)
{
    return (ch < PWR_SEQ_CH_COUNT) ? s_delay_tick[ch] : 0U;
}
