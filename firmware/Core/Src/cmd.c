/* cmd.c - 串口行命令解析与执行
 *
 * 命令 (大小写不敏感, 空格分隔, 回车结束):
 *   help                     显示命令列表
 *   led on|off|blink|state   LED 开 / 关 / 心跳闪烁 / 查询        [需求 5a]
 *   status                   查询 PA1/PB1/PB10/PB12/PB14 电平     [需求 5b]
 *   seq info                 查询上/下电时序延迟逻辑              [需求 5c]
 *   seq set <d0> <d1> <d2> <d3>   设置 4 路延迟 (ms): PB1 PB10 PB12 PB14
 *   seq default              恢复默认延迟
 *   seq up / seq down        手动触发上电 / 下电时序
 *   seq en <0|1>             关 / 开 PA1 联动
 *   seq reset                中止时序并强制 4 路输出拉低
 *
 * 串口输出使用 ASCII 英文, 避免终端编码差异导致乱码。
 */
#include "cmd.h"
#include "board.h"
#include "uart.h"
#include "led.h"
#include "pwr_seq.h"

#include <stdint.h>

#define CMD_LINE_MAX  96
#define CMD_ARGC_MAX  8

static char s_line[CMD_LINE_MAX];

/* 固件版本字符串: 置于 .fw_version 段, 链接脚本固定定位到 FLASH 0x08000400 */
__attribute__((section(".fw_version"), used))
static const char s_fw_version[] = FW_VERSION_STRING;

static const char *const s_mode_name[3] = { "IDLE", "UP", "DOWN" };

/* ------------------------------------------------------------------ */
/* 工具                                                                */
/* ------------------------------------------------------------------ */

/* 大小写不敏感字符串比较 */
static int cmd_eqi(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca -= ('a' - 'A');
        if (cb >= 'a' && cb <= 'z') cb -= ('a' - 'A');
        if (ca != cb) return 0;
        a++; b++;
    }
    return (*a == '\0' && *b == '\0');
}

/* 解析十进制毫秒值: "10" / "10.5" / ".5"; 合法返回 1 */
static int cmd_parse_ms(const char *s, float *out)
{
    float val = 0.0f, scale = 0.1f;
    int digits = 0;

    if (!s || !*s) return 0;
    /* 整数部分 (可空: ".5" 合法) */
    while (*s >= '0' && *s <= '9') {
        val = val * 10.0f + (float)(*s - '0');
        s++; digits++;
    }
    /* 小数部分 (可空: "5" 合法) */
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            val += (float)(*s - '0') * scale;
            scale *= 0.1f;
            s++; digits++;
        }
    }
    if (*s != '\0' || digits == 0) return 0;   /* 必须有至少一个数字, 无尾随字符 */
    *out = val;
    return 1;
}

/* 打印 tick (0.1ms 单位) 为 "x.y ms" */
#define TICK_MS(t)  (unsigned long)((t) / 10U), (unsigned long)((t) % 10U)

/* ------------------------------------------------------------------ */
/* help                                                                */
/* ------------------------------------------------------------------ */
static void cmd_help(void)
{
    UART_SendString(
        "\r\ncommands:\r\n"
        "  help                          show this list\r\n"
        "  v | version                   show firmware version\r\n"
        "  led on|off|blink|state        LED control (blink = heartbeat)\r\n"
        "  show | status                 show PA1 / PB1 / PB10 / PB12 / PB14 levels\r\n"
        "  seq info                      show power sequence delay logic\r\n"
        "  seq set <d0> <d1> <d2> <d3>   set delays in ms (PB1 PB10 PB12 PB14)\r\n"
        "  seq default                   restore default delays\r\n"
        "  seq up | seq down             manually trigger up / down sequence\r\n"
        "  seq en <0|1>                  disable / enable PA1 link\r\n"
        "  seq reset                     abort sequence, force outputs low\r\n");
}

/* ------------------------------------------------------------------ */
/* version                                                             */
/* ------------------------------------------------------------------ */
static void cmd_version(void)
{
    UART_Printf("[FW] %s\r\n", s_fw_version);
}

/* ------------------------------------------------------------------ */
/* led                                                                 */
/* ------------------------------------------------------------------ */
static void cmd_led(int argc, char *argv[])
{
    if (argc < 2) {
        UART_SendString("[LED] usage: led on|off|blink|state\r\n");
        return;
    }
    if (cmd_eqi(argv[1], "on")) {
        LED_SetHeartbeat(0);
        LED_On();
        UART_SendString("[LED] on (steady)\r\n");
    } else if (cmd_eqi(argv[1], "off")) {
        LED_SetHeartbeat(0);
        LED_Off();
        UART_SendString("[LED] off (steady)\r\n");
    } else if (cmd_eqi(argv[1], "blink")) {
        LED_SetHeartbeat(1);
        UART_SendString("[LED] heartbeat blink (500ms)\r\n");
    } else if (cmd_eqi(argv[1], "state")) {
        UART_Printf("[LED] mode=%s level=%u\r\n",
                    LED_Heartbeat() ? "blink" : "steady",
                    (unsigned)LED_State());
    } else {
        UART_SendString("[LED] usage: led on|off|blink|state\r\n");
    }
}

/* ------------------------------------------------------------------ */
/* status                                                              */
/* ------------------------------------------------------------------ */
static void cmd_status(void)
{
    pwr_seq_status_t st;
    uint32_t i;

    PowerSeq_GetStatus(&st);
    UART_Printf("[IO] %-5s = %u  (sense, trigger input)\r\n",
                BOARD_SENSE_NAME, (unsigned)st.in_level);
    for (i = 0; i < PWR_SEQ_CH_COUNT; i++) {
        UART_Printf("[IO] %-5s = %u  (pwr ch%u, delay %lu.%lu ms)\r\n",
                    PowerSeq_ChName(i),
                    (unsigned)((st.output_levels >> i) & 1U),
                    (unsigned)i,
                    TICK_MS(PowerSeq_ChDelayTick(i)));
    }
    UART_Printf("[SEQ] mode=%s running=%u link=%u up_cnt=%u down_cnt=%u\r\n",
                s_mode_name[st.mode <= 2U ? st.mode : 0U],
                (unsigned)st.running, (unsigned)st.en,
                (unsigned)st.seq_up, (unsigned)st.seq_down);
}

/* ------------------------------------------------------------------ */
/* seq info                                                            */
/* ------------------------------------------------------------------ */
static void cmd_seq_info(void)
{
    uint32_t tick[PWR_SEQ_CH_COUNT];
    uint32_t maxt = 0;
    uint8_t  up[PWR_SEQ_CH_COUNT];     /* 按上电动作先后排序的通道号 */
    uint32_t i, j;

    for (i = 0; i < PWR_SEQ_CH_COUNT; i++) {
        tick[i] = PowerSeq_ChDelayTick(i);
        if (tick[i] > maxt) maxt = tick[i];
        up[i] = (uint8_t)i;
    }
    /* 按 Ti 升序 (上电动作顺序) */
    for (i = 0; i < PWR_SEQ_CH_COUNT - 1U; i++) {
        for (j = i + 1U; j < PWR_SEQ_CH_COUNT; j++) {
            if (tick[up[j]] < tick[up[i]]) {
                uint8_t t = up[i]; up[i] = up[j]; up[j] = t;
            }
        }
    }

    UART_SendString("[SEQ] trigger " BOARD_SENSE_NAME ":\r\n");
    UART_SendString("[SEQ]   rising  edge -> power-UP   : channel i goes HIGH at its delay Ti\r\n");
    UART_SendString("[SEQ]   falling edge -> power-DOWN : channel i goes LOW at (Tmax-Ti), mirror order\r\n");
    UART_SendString("[SEQ] resolution 0.1ms, range 0..10000ms per channel\r\n");
    UART_SendString("[SEQ] ch  pin    delay      up@Ti       down@(Tmax-Ti)\r\n");
    for (i = 0; i < PWR_SEQ_CH_COUNT; i++) {
        UART_Printf("[SEQ] %u   %-5s  %3lu.%lu ms   %3lu.%lu ms   %3lu.%lu ms\r\n",
                    (unsigned)i, PowerSeq_ChName(i),
                    TICK_MS(tick[i]),
                    TICK_MS(tick[i]),
                    TICK_MS(maxt - tick[i]));
    }
    UART_SendString("[SEQ] up   order: ");
    for (j = 0; j < PWR_SEQ_CH_COUNT; j++) {
        uint32_t c = up[j];
        UART_Printf("%s@%lu.%lums%s", PowerSeq_ChName(c),
                    TICK_MS(tick[c]), (j == PWR_SEQ_CH_COUNT - 1U) ? "\r\n" : " -> ");
    }
    UART_SendString("[SEQ] down order: ");
    for (j = 0; j < PWR_SEQ_CH_COUNT; j++) {
        uint32_t c = up[PWR_SEQ_CH_COUNT - 1U - j];   /* Ti 降序 = 下电动作顺序 */
        UART_Printf("%s@%lu.%lums%s", PowerSeq_ChName(c),
                    TICK_MS(maxt - tick[c]), (j == PWR_SEQ_CH_COUNT - 1U) ? "\r\n" : " -> ");
    }
}

/* ------------------------------------------------------------------ */
/* seq set                                                             */
/* ------------------------------------------------------------------ */
static void cmd_seq_set(int argc, char *argv[])
{
    float d[PWR_SEQ_CH_COUNT];
    uint32_t i;

    if (argc != 6) {
        UART_SendString("[SEQ] usage: seq set <d0> <d1> <d2> <d3>   (ms, 0..10000)\r\n"
                        "[SEQ]        d0=PB1 d1=PB10 d2=PB12 d3=PB14\r\n");
        return;
    }
    for (i = 0; i < PWR_SEQ_CH_COUNT; i++) {
        if (!cmd_parse_ms(argv[2 + (int)i], &d[i])) {
            UART_Printf("[SEQ] bad value: %s\r\n", argv[2 + (int)i]);
            return;
        }
    }
    PowerSeq_SetAll(d[0], d[1], d[2], d[3]);
    UART_SendString("[SEQ] delays set (effective, 0.1ms resolution):\r\n");
    for (i = 0; i < PWR_SEQ_CH_COUNT; i++) {
        UART_Printf("[SEQ]   %-5s = %lu.%lu ms\r\n",
                    PowerSeq_ChName(i), TICK_MS(PowerSeq_ChDelayTick(i)));
    }
}

/* ------------------------------------------------------------------ */
/* seq 主命令                                                          */
/* ------------------------------------------------------------------ */
static void cmd_seq(int argc, char *argv[])
{
    if (argc < 2) {
        cmd_seq_info();
        return;
    }
    if (cmd_eqi(argv[1], "info")) {
        cmd_seq_info();
    } else if (cmd_eqi(argv[1], "set")) {
        cmd_seq_set(argc, argv);
    } else if (cmd_eqi(argv[1], "default")) {
        PowerSeq_SetDefault();
        UART_SendString("[SEQ] default delays restored (PB1=10ms, others 0ms)\r\n");
    } else if (cmd_eqi(argv[1], "up")) {
        PowerSeq_TriggerUp();
        UART_SendString("[SEQ] manual power-UP sequence triggered\r\n");
    } else if (cmd_eqi(argv[1], "down")) {
        PowerSeq_TriggerDown();
        UART_SendString("[SEQ] manual power-DOWN sequence triggered\r\n");
    } else if (cmd_eqi(argv[1], "en")) {
        if (argc >= 3 && cmd_eqi(argv[2], "1")) {
            PowerSeq_SetEnabled(1);
            UART_SendString("[SEQ] PA1 link enabled\r\n");
        } else if (argc >= 3 && cmd_eqi(argv[2], "0")) {
            PowerSeq_SetEnabled(0);
            UART_SendString("[SEQ] PA1 link disabled (seq up/down still work)\r\n");
        } else {
            UART_SendString("[SEQ] usage: seq en <0|1>\r\n");
        }
    } else if (cmd_eqi(argv[1], "reset")) {
        PowerSeq_Reset();
        UART_SendString("[SEQ] reset: all outputs forced LOW\r\n");
    } else {
        UART_Printf("[SEQ] unknown sub-command: %s (try 'seq info')\r\n", argv[1]);
    }
}

/* ------------------------------------------------------------------ */
/* 对外接口                                                            */
/* ------------------------------------------------------------------ */
void Cmd_Init(void)
{
    UART_SendString("\r\n=== boot_seq_c8t6 : STM32F103C8T6 power sequence controller ===\r\n");
    UART_Printf("fw: %s\r\n", s_fw_version);
    UART_SendString("sense: PA1 (both edges)  outputs: PB1 PB10 PB12 PB14\r\n");
    UART_SendString("type 'help' for commands\r\n");
}

void Cmd_Task(void)
{
    char *argv[CMD_ARGC_MAX];
    char *p;
    int argc = 0;

    if (!UART_GetLine(s_line, sizeof(s_line))) return;

    /* 按空格/Tab 切词 */
    p = s_line;
    while (*p && argc < CMD_ARGC_MAX) {
        while (*p == ' ' || *p == '\t') *p++ = '\0';
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    if (argc == 0) {           /* 空行 */
        UART_SendString("> ");
        return;
    }

    if (cmd_eqi(argv[0], "help")) {
        cmd_help();
    } else if (cmd_eqi(argv[0], "v") || cmd_eqi(argv[0], "version")) {
        cmd_version();
    } else if (cmd_eqi(argv[0], "led")) {
        cmd_led(argc, argv);
    } else if (cmd_eqi(argv[0], "status") || cmd_eqi(argv[0], "show")) {
        cmd_status();
    } else if (cmd_eqi(argv[0], "seq")) {
        cmd_seq(argc, argv);
    } else {
        UART_Printf("[CMD] unknown: %s  (try 'help')\r\n", argv[0]);
    }
    UART_SendString("> ");
}
