// ============================================================================
// 文件：include/serial.h
// 用途：单片机与电脑的串口通信接口声明。
//
// 声明串口初始化、收发轮询函数，以及状态/结果的待发送标记。
// 具体函数体和模块内部变量位于 src/serial.c；本文件负责告诉其他模块有哪些接口可用。
// 其他 .c 通过 #include 引入本头文件，再调用声明过的函数。
// extern 只声明共享变量的名字，不分配第二份变量；头文件保护宏避免重复声明引起的问题。
// ============================================================================

#ifndef SERIAL_H
#define SERIAL_H

#include "board.h"

// 电脑串口通信模块对外接口。
// .h 里只声明名字；变量的存储空间及函数实现由对应 .c 提供。
extern volatile unsigned char serial_status_due;                                // 上电先置 1，使主循环自动发送一次时间、比分和运行状态。
extern volatile unsigned char serial_result_due;                                // 最终结果的待发送标记，倒计时归零时由中断置 1。

// 函数用途：serial_init() —— 初始化 UART 串口和 Timer1 波特率时钟。
// 工作说明：配置串口模式、接收允许位和 Timer1 重装值，清除收发标志后启动 Timer1；电脑使用 4800 波特率、8N1。
void serial_init(void);                                                         // 实现在 serial.c
// 函数用途：serial_poll_tx() —— 检查串口发送进度并安排下一条消息。
// 工作说明：前一个字符发完就续发下一个；整行结束后按状态、结果、错误的优先顺序选择新消息，不等待整行发送完才返回。
void serial_poll_tx(void);                                                      // 实现在 serial.c
// 函数用途：serial_poll_rx() —— 检查并收集电脑发来的串口字符。
// 工作说明：RI 为 1 时读取 SBUF；普通字符放入接收数组，小写转大写；收到回车或换行后解析非空命令，越界时登记错误。
void serial_poll_rx(void);                                                      // 实现在 serial.c

#endif                                                                          // SERIAL_H
