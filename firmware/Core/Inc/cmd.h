/* cmd.h - 串口行命令解析与执行 */
#ifndef CMD_H
#define CMD_H

void Cmd_Init(void);    /* 打印开机横幅与提示 */
void Cmd_Task(void);    /* 主循环轮询: 收行 -> 解析 -> 执行 */

#endif /* CMD_H */
