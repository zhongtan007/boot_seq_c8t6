# STM32F103C8T6 上电时序控制器（boot_seq_c8t6）

基于 STM32F103C8T6-MINI 板卡的上/下电时序控制器固件。**上电时固件启动后
自动执行一次顺序上电（不检测 PA1）**；之后由 PA1 触发信号控制下电
（PA1 变低即反序下电，再变高则重新上电）。按可配置的延迟依次控制
PB1/PB10/PB12/PB14 四路电源使能输出，并通过串口命令实现 LED 控制、
IO 状态查询与时序逻辑查询。

> 时序算法 1:1 移植自内部参考工程（STM32H563，ThreadX + HAL），
> 仅替换引脚与定时器，算法行为完全一致。

---

## 1. 硬件资源

### 1.1 引脚分配

| 引脚 | 方向 | 功能 | 说明 |
|------|------|------|------|
| PA1 | 输入 | 下电触发（上电自动执行） | EXTI1 双边沿中断，浮空输入（不使用内部下拉）；启动后固件自动上电，PA1 变低才下电 |
| PB1 | 输出 | 电源控制 通道 0 | 推挽输出，复位默认低 |
| PB10 | 输出 | 电源控制 通道 1 | 推挽输出，复位默认低 |
| PB12 | 输出 | 电源控制 通道 2 | 推挽输出，复位默认低 |
| PB14 | 输出 | 电源控制 通道 3 | 推挽输出，复位默认低 |
| PC13 | 输出 | 板载 LED | 低电平点亮（PC13→R5 4.7K→LED→3V3），心跳闪烁指示芯片正常工作 |
| PA9 / PA10 | 复用 | USART1 TX / RX | 115200 8N1，命令行交互口 |

原理图：`doc/STM32F103C8T6-MINI原理图.pdf`

### 1.2 时钟配置

```
HSE 8MHz ──PLL ×9──> SYSCLK 72MHz
                     ├── AHB  72MHz
                     ├── APB2 72MHz (GPIOA/B/C, USART1, EXTI)
                     └── APB1 36MHz ──×2──> TIM4 时钟 72MHz
```

- `system_stm32f10x.c` 中 `SYSCLK_FREQ_72MHz` 已启用（MD 器件分支）。
- TIM4：72MHz / (PSC+1=72) / (ARR+1=100) = **10kHz（0.1ms 时序时基）**。

### 1.3 中断优先级（NVIC 分组 2：2 位抢占 + 2 位子优先级）

| 中断 | 抢占 | 子 | 用途 |
|------|------|----|------|
| EXTI1 | 0 | 0 | PA1 边沿，触发上/下电时序（要求最低触发延迟） |
| TIM4 | 0 | 0 | 0.1ms 时序调度（与 EXTI1 同级不嵌套） |
| USART1 | 1 | 0 | 命令字节接收 |
| SysTick | 15 | - | 1ms 系统节拍（最低，不干扰时序） |

---

## 2. 目录结构

```
boot_seq_c8t6/
├── doc/                          # 板卡原理图
│   └── STM32F103C8T6-MINI原理图.pdf
├── demo/                         # 原厂 LED 闪烁 demo (Keil, 仅参考)
│   └── LED闪烁(引脚为PC13)/
├── firmware/                     # ★ 本项目固件
│   ├── Core/
│   │   ├── Inc/                  # 头文件
│   │   │   ├── board.h           # 板级资源宏 (引脚/时钟/外设参数)
│   │   │   ├── pwr_seq.h/.c      # ★ 核心: 上/下电时序控制
│   │   │   ├── cmd.h/.c          # 串口命令解析与执行
│   │   │   ├── uart.h/.c         # USART1 驱动 (环形缓冲 + 回显)
│   │   │   ├── led.h/.c          # PC13 心跳 LED
│   │   │   ├── tick.h/.c         # SysTick 1ms 节拍
│   │   │   ├── stm32f10x_it.h/.c # 中断服务函数
│   │   │   ├── stm32f10x_conf.h  # 标准外设库配置
│   │   │   └── main.c            # 主程序
│   │   └── Src/
│   ├── Drivers/
│   │   ├── CMSIS/                # stm32f10x.h, system, core_cm3
│   │   └── StdPeriph/            # 标准外设库 V3.5 (仅裁剪保留 6 个模块)
│   ├── startup_stm32f10x_md.s    # GCC 启动文件 (向量表 + 复位初始化)
│   ├── STM32F103C8Tx_FLASH.ld    # 链接脚本 (64K Flash / 20K RAM)
│   ├── Makefile
│   └── build.bat                 # ★ 一键编译
├── .gitignore
└── README.md                     # 本文档
```

---

## 3. 软件架构

### 3.1 总体结构：前后台系统

```
                    ┌────────────────────────────────────────────┐
   PA1 边沿 ──────► │ EXTI1_IRQHandler → PowerSeq_OnExtiIsr()    │
   (下电触发)       │   判方向 → 建调度表 → 启动 TIM4              │  前
   MCU 启动 ──────► │ PowerSeq_Init() 末尾直接启动上电时序         │
                    ├────────────────────────────────────────────┤  台
   0.1ms ─────────► │ TIM4_IRQHandler → PowerSeq_OnTickIsr()     │  (中
   (时序时基)       │   到期通道 BSRR 原子动作, 全部完成自动停表     │   断)
                    ├────────────────────────────────────────────┤
   命令字节 ───────► │ USART1_IRQHandler → UART_OnRxIsr()         │
   (115200)         │   收字节入环形缓冲 + 回显                    │
                    ├────────────────────────────────────────────┤
   1ms ───────────► │ SysTick_Handler → Tick_OnIsr()              │
                    └────────────────────────────────────────────┘
                                        │
                    ┌───────────────────┼──────────────────────┐
                    │            main() while(1) 主循环          │  后台
                    │   LED_Task()   心跳闪烁 (500ms 翻转)       │  (轮询)
                    │   Cmd_Task()   取整行 → 解析 → 执行命令      │
                    └───────────────────┴──────────────────────┘
```

- **时序路径全部在中断中闭环**（EXTI 触发 → TIM4 调度 → BSRR 动作），
  主循环阻塞或串口繁忙均不影响时序精度。
- 输出动作用 `GPIOx->BSRR` 一次写完成，同一 tick 到期的多路无先后差。

### 3.2 模块职责

| 模块 | 职责 | 关键接口 |
|------|------|----------|
| pwr_seq | 时序核心算法（移植自参考工程） | `PowerSeq_Init/TriggerUp/TriggerDown/SetAll/GetStatus` |
| board.h | 板级资源集中定义（引脚/定时器/串口参数） | 全部 `BOARD_*` 宏 |
| uart | 串口驱动：RX 中断→环形缓冲，TX 轮询，Printf | `UART_Printf/GetLine/OnRxIsr` |
| cmd | 行命令解析与执行 | `Cmd_Init/Cmd_Task` |
| led | PC13 心跳 LED，非阻塞闪烁 | `LED_Task/On/Off/SetHeartbeat` |
| tick | SysTick 1ms 节拍 | `Tick_Millis` |
| stm32f10x_it | 中断向量分发到各模块 ISR 入口 | `EXTI1/TIM4/USART1_IRQHandler` |

---

## 4. 核心功能：上电时序算法

### 4.1 触发与动作

| 触发条件 | 动作 | 规则 |
|----------|------|------|
| MCU 启动（复位后，不检测 PA1） | **正时序**：4 路依次**拉高** | 通道 i 在启动后 Ti 时刻拉高 |
| PA1 上升沿（低→高） | **正时序**：4 路依次**拉高** | 通道 i 在触发后 Ti 时刻拉高 |
| PA1 下降沿（高→低） | **反时序**：4 路反序**拉低** | 通道 i 在触发后 (Tmax−Ti) 时刻拉低 |

- **上电不检测 PA1**：MCU 每次复位启动都无条件跑一次正时序，与 PA1 电平无关
- **下电检测 PA1**：只有 PA1 变低才下电；PA1 再变高则恢复上电

- Ti = 通道 i 的延迟配置（单位 ms，精度 0.1ms，范围 0~10000ms）
- Tmax = 4 路延迟中的最大值
- 下电是上电的**时间轴镜像**：上电最后起来的最先下电，各步间隔与上电完全一致
- 延迟相同的通道在同一 tick 同时动作（BSRR 一次写）

### 4.2 默认延迟配置与时间线

| 通道 | 引脚 | 默认延迟 Ti |
|------|------|------------|
| 0 | PB1 | 10.0 ms |
| 1 | PB10 | 0.0 ms |
| 2 | PB12 | 0.0 ms |
| 3 | PB14 | 0.0 ms |

```
上电 (MCU 启动 / PA1 ↑):
                     PB10/PB12/PB14 ──────────────────────── 高
                    /
PB 输出 ──────────┘        PB1 ──────────────────────────── 高
                   ┌──────┐
                   └──────┘
                   0     10ms                     (Tmax=10ms)

下电 (PA1 ↓):  时间轴镜像
PB1 ────────────────────────┐
                             \          PB10/PB12/PB14 ──────── 低
                              └───────────────────────────────
                              0       10ms
                              (PB1 立即拉低, 其余三路 10ms 后同时拉低)
```

### 4.3 实现机制

1. **上电触发**：`PowerSeq_Init()` 末尾直接调用 `pwrseq_start(PWR_SEQ_MODE_UP)`，
   不读 PA1 电平、不等边沿，保证复位启动后必定顺序上电。
2. **下电触发**：EXTI1 双边沿中断 → 清挂起标志 → 读 PA1 当前电平判断方向
   （STM32F1 的 EXTI 无独立上升/下降挂起标志）→ `pwrseq_start()`。
3. **建表**：按方向计算各通道目标 tick
   `target[i] = (上电) ? Ti : (Tmax − Ti)`，置 pending 掩码 0xF，
   立即处理 0 时刻通道，启动 TIM4。
4. **调度**：TIM4 每 0.1ms 中断，`counter++` 后把所有
   `counter ≥ target[i]` 的待动作通道用一次 BSRR 写同时拉高/拉低。
5. **收尾**：pending 清零（4 路全部动作完）后自动关 TIM4 中断并停表，
   空闲期零中断开销。

### 4.4 重复触发与抢占规则

- 上电时序执行中再收到 PA1 上升沿：**忽略**
- 下电时序执行中再收到 PA1 下降沿：**忽略**
- 反方向边沿**可抢占**：下电执行中 PA1 拉高 → 立即转为上电时序（反之亦然）
- MCU 启动时的自动上电不受 `seq en` / PA1 电平影响
- `seq en 0` 可关闭 PA1 联动（不影响启动自动上电与手动 `seq up/down`）
- `seq reset`：随时中止当前时序并强制 4 路全部拉低

---

## 5. 串口命令手册

串口参数：**115200 8N1**，行文本协议（回车结束，支持 CR/LF，带字符回显，
命令大小写不敏感）。

| 命令 | 功能 | 对应需求 |
|------|------|----------|
| `help` | 显示命令列表 | - |
| `led on` / `led off` | LED 常亮 / 常灭 | 5a 开关 LED |
| `led blink` | 恢复心跳闪烁（默认，500ms） | 4 芯片工作指示 |
| `led state` | 查询 LED 模式与状态 | 5a |
| `status` | 查询 PA1/PB1/PB10/PB12/PB14 电平及时序状态 | 5b 查询 IO 状态 |
| `seq info` | 查询时序延迟逻辑（延迟表 + 上/下电动作时刻与顺序） | 5c 查询时序逻辑 |
| `seq set <d0> <d1> <d2> <d3>` | 设置 4 路延迟（ms），如 `seq set 10 5 0 20` | 扩展 |
| `seq default` | 恢复默认延迟（PB1=10ms，其余 0） | 扩展 |
| `seq up` / `seq down` | 手动触发上电 / 下电时序 | 扩展 |
| `seq en 1` / `seq en 0` | 使能 / 关闭 PA1 联动 | 扩展 |
| `seq reset` | 中止时序，4 路强制拉低 | 扩展 |

### 5.1 `status` 输出示例

```
[IO] PA1   = 0  (sense, trigger input)
[IO] PB1   = 0  (pwr ch0, delay 10.0 ms)
[IO] PB10  = 0  (pwr ch1, delay 0.0 ms)
[IO] PB12  = 0  (pwr ch2, delay 0.0 ms)
[IO] PB14  = 0  (pwr ch3, delay 0.0 ms)
[SEQ] mode=IDLE running=0 link=1 up_cnt=0 down_cnt=0
```

### 5.2 `seq info` 输出示例（默认延迟）

```
[SEQ] trigger PA1:
[SEQ]   rising  edge -> power-UP   : channel i goes HIGH at its delay Ti
[SEQ]   falling edge -> power-DOWN : channel i goes LOW at (Tmax-Ti), mirror order
[SEQ] resolution 0.1ms, range 0..10000ms per channel
[SEQ] ch  pin    delay      up@Ti       down@(Tmax-Ti)
[SEQ] 0   PB1    10.0 ms    10.0 ms     0.0 ms
[SEQ] 1   PB10    0.0 ms     0.0 ms    10.0 ms
[SEQ] 2   PB12    0.0 ms     0.0 ms    10.0 ms
[SEQ] 3   PB14    0.0 ms     0.0 ms    10.0 ms
[SEQ] up   order: PB10@0.0ms -> PB12@0.0ms -> PB14@0.0ms -> PB1@10.0ms
[SEQ] down order: PB1@0.0ms -> PB10@10.0ms -> PB12@10.0ms -> PB14@10.0ms
```

---

## 6. 编译与烧录

### 6.1 一键编译

双击 `firmware\build.bat`（或在固件目录命令行执行，加 `-q` 跳过暂停）：

```
build\boot_seq_c8t6.hex     # hex 固件
build\boot_seq_c8t6.bin     # bin 固件
build\hex\boot_seq_c8t6.hex # 烧录用归档
```

- 工具链：STM32CubeIDE 2.2.0 自带 GNU Arm 14.3 + GNU make
  （本机路径保存在本地文件 `firmware/build.local.bat`，该文件已被 `.gitignore`
  排除，不会进入版本库；CubeIDE 升级后在其中更新两行即可）。
- 编译成功自动清理中间文件，只保留烧录产物；失败保留中间文件便于排查。
- 也可手动：`make GCC_PATH=<arm-none-eabi 工具链 bin 目录>`。

### 6.2 资源占用

```
   text    data     bss     dec     hex
  12548      96    2336   14980    3a84    (Flash 12.6K/64K, RAM 2.4K/20K)
```

### 6.3 烧录

任意支持 STM32F1 的烧录器均可，使用 SWD（PA13/PA14）或串口 ISP：
将 `build\boot_seq_c8t6.hex` 烧入 0x08000000。

---

## 7. 从参考工程的移植对照

| 项目 | 参考工程（STM32H563） | 本工程（STM32F103C8T6） |
|------|----------------------|------------------------|
| 框架 | ThreadX + HAL | 前后台 + 标准外设库 V3.5 |
| 触发输入 | PE9 / EXTI9 | PA1 / EXTI1（浮空输入） |
| 通道 0~3 | PE10 / PE11 / PE12 / PE13 | PB1 / PB10 / PB12 / PB14 |
| 时序时基 | TIM7 @10kHz | TIM4 @10kHz（C8T6 无 TIM6/7） |
| 输出动作 | BSRR 原子写 | BSRR 原子写（相同） |
| 命令口 | USB CDC (JSON) | USART1 115200（行文本命令） |
| 方向判定 | RPR1/FPR1 独立挂起标志 | 清标志后读引脚电平（F1 限制） |
| 默认延迟 | 10/0/0/0 ms | 10/0/0/0 ms（相同） |
| 镜像算法 | target = Tmax − Ti | target = Tmax − Ti（相同） |
| 抢占规则 | 同向忽略 / 反向抢占 | 同向忽略 / 反向抢占（相同） |

时序算法（`pwr_seq.c` 中 `pwrseq_ms_to_tick / pwrseq_apply_due / pwrseq_start`）
与参考工程逐行对应，行为完全一致。

---

## 8. 已知限制与说明

1. **F1 EXTI 方向判定**：STM32F1 的 EXTI 每线只有一个挂起标志，
   方向由"清标志后读 PA1 电平"推断。宽度小于中断响应时间（约亚微秒级）的
   极窄脉冲可能判反方向；常规电源使能/PGOOD 信号（毫秒级以上）无影响。
   参考工程（H563）用独立的上升/下降挂起寄存器，无此限制。
2. **上电初始状态**：MCU 每次复位启动都会自动顺序上电，与 PA1 电平无关
   （上电不检测 PA1）；若启动瞬间 PA1 已为低电平，启动仍会上电，
   此时需 PA1 出现一次下降沿才会下电。
3. **延迟精度**：0.1ms 分辨率，四舍五入到最近 tick；`seq set 1.25` 生效为
   1.3ms（可通过 `seq info` 查看生效值）。
4. **串口缓冲**：接收环形缓冲 128 字节，单条命令 ≤96 字符；输入不回显
   退格处理（终端发送的退格字符会被当作普通字符）。
