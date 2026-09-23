/* pwr_seq.h - 上电 / 下电时序控制 (PA1 双边沿触发, PB1/PB10/PB12/PB14 依次动作)
 *
 * 由内部 STM32H563 参考工程 (pwr_seq.c) 移植而来,
 * 时序算法与原工程完全一致, 仅替换引脚与定时器:
 *
 *   原工程                    本工程
 *   ----------------------    --------------------------
 *   PE9  触发输入 (EXTI9)  -> PA1  触发输入 (EXTI1)
 *   PE10 通道 0            -> PB1  通道 0
 *   PE11 通道 1            -> PB10 通道 1
 *   PE12 通道 2            -> PB12 通道 2
 *   PE13 通道 3            -> PB14 通道 3
 *   TIM7 0.1ms 时基        -> TIM4 0.1ms 时基 (C8T6 无 TIM6/TIM7)
 *
 * 管脚:
 *   PA1  = 下电触发输入 (EXTI1, 上升沿 + 下降沿)
 *   PB1 / PB10 / PB12 / PB14 = 通道 0..3 (推挽输出, 复位默认低)
 *
 * 触发策略 (与参考工程不同):
 *   上电: MCU 复位启动后立即无条件执行一次正时序 (拉高), 不检测 PA1;
 *         即上电不依赖任何边沿/电平条件。
 *   下电: PA1 下降沿 (高->低): 反时序拉低 —— 按上电顺序的【倒序】拉低,
 *                        即延迟最大的最先拉低, 延迟最小的最后拉低,
 *                        时间轴镜像: 通道 i 在 (Tmax - Ti) 时刻拉低。
 *   PA1 上升沿 (低->高): 重新执行正时序 (各路在自己的延迟点 Ti 拉高)。
 *
 * 上电默认延迟 (写死, 串口 `seq set` 可改写):
 *   PB1 = 10.0ms   PB10 = 0.0ms   PB12 = 0.0ms   PB14 = 0.0ms
 *   上电: PB10/PB12/PB14 立即拉高 -> PB1 延迟 10ms 拉高
 *   下电: PB1 立即拉低            -> PB10/PB12/PB14 延迟 10ms 拉低
 *
 * 时间参数: 单位 ms, 精度 0.1ms, 单路 0 ~ 10000ms (10s)。
 */
#ifndef PWR_SEQ_H
#define PWR_SEQ_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PWR_SEQ_CH_COUNT  4          /* PB1 / PB10 / PB12 / PB14 */
#define PWR_SEQ_MAX_MS    10000.0f
#define PWR_SEQ_TICK_MS   0.1f

/* 默认延迟 (ms): ch0=PB1, ch1=PB10, ch2=PB12, ch3=PB14 */
#define PWR_SEQ_DEF_MS0   10.0f
#define PWR_SEQ_DEF_MS1   0.0f
#define PWR_SEQ_DEF_MS2   0.0f
#define PWR_SEQ_DEF_MS3   0.0f

/* 时序方向 / 状态 */
#define PWR_SEQ_MODE_IDLE  0U        /* 空闲 */
#define PWR_SEQ_MODE_UP    1U        /* 正在执行上电时序 (依次拉高) */
#define PWR_SEQ_MODE_DOWN  2U        /* 正在执行下电时序 (反序拉低) */

typedef struct {
    float    delay_ms[PWR_SEQ_CH_COUNT];  /* 各路延迟 (ms), 0 ~ 10000 */
    uint32_t en;                          /* PA1 联动使能 (默认 1, 串口可改) */
    uint32_t in_level;                    /* PA1 当前电平 (0=低, 1=高) */
    uint32_t mode;                        /* PWR_SEQ_MODE_* */
    uint32_t running;                     /* 时序正在计时执行 */
    uint32_t output_levels;               /* 当前 4 路输出电平 (bit0..bit3) */
    uint32_t seq_up;                      /* 累计上电次数 */
    uint32_t seq_down;                    /* 累计下电次数 */
} pwr_seq_status_t;

/* 初始化 PA1(EXTI 双边沿) + PB1/PB10/PB12/PB14(输出) + TIM4 时基,
 * 并在返回前自动执行一次上电 (正) 时序 */
void PowerSeq_Init(void);

/* 配置四路延迟 (ms, 超出范围自动钳位) */
void PowerSeq_SetAll(float d0, float d1, float d2, float d3);

/* 恢复默认延迟 (PWR_SEQ_DEF_MSx) */
void PowerSeq_SetDefault(void);

/* PA1 联动使能 (1=响应 PA1 边沿, 0=忽略 PA1, 仅手动 up/down 可用) */
void PowerSeq_SetEnabled(uint32_t en);

/* 复位: 中止计时, 4 路输出全部拉低 (不改动"联动使能"状态) */
void PowerSeq_Reset(void);

/* 软件手动触发: 上电时序 / 下电时序 (不受联动开关限制) */
void PowerSeq_TriggerUp(void);
void PowerSeq_TriggerDown(void);

void PowerSeq_GetStatus(pwr_seq_status_t *out);

/* 通道信息 (供串口命令打印) */
const char   *PowerSeq_ChName(uint32_t ch);      /* "PB1" / "PB10" / ... */
uint32_t      PowerSeq_ChDelayTick(uint32_t ch); /* 当前延迟, 单位 0.1ms */

/* 中断服务入口 (由 stm32f10x_it.c 调用) */
void PowerSeq_OnExtiIsr(void);   /* PA1 边沿: 判断上升/下降并清标志 */
void PowerSeq_OnTickIsr(void);   /* TIM4 更新: 0.1ms 步进 */

#ifdef __cplusplus
}
#endif

#endif /* PWR_SEQ_H */
