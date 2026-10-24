// ============================================================================
// 文件：include/countdown.h
// 用途：倒计时控制接口声明。
//
// 声明剩余秒数、运行状态、显示刷新标记，以及四个计时操作函数。
// 具体函数体和模块内部变量位于 src/countdown.c；本文件负责告诉其他模块有哪些接口可用。
// 其他 .c 通过 #include 引入本头文件，再调用声明过的函数。
// extern 只声明共享变量的名字，不分配第二份变量；头文件保护宏避免重复声明引起的问题。
// ============================================================================

#ifndef COUNTDOWN_H
#define COUNTDOWN_H

#include "board.h"

// 倒计时模块对外接口。
// .h 里只声明名字；变量的存储空间及函数实现由对应 .c 提供。
extern volatile unsigned int remaining_seconds;                                 // 保存剩余总秒数，初值 60 即 1 分钟；本工具链的 unsigned int 为 16 位。
extern volatile unsigned char running;                                          // 1：倒计时进行；0：暂停
extern volatile unsigned char display_dirty;                                    // 计时数字需要重新拆成四位

// 函数用途：reset_timer() —— 复位倒计时并暂停。
// 工作说明：把剩余时间恢复到保存的设定值，清除毫秒累计和蜂鸣器提示进度，并通知主循环刷新显示。
void reset_timer(void);                                                         // 实现在 countdown.c
// 函数用途：change_minute() —— 手动增加或减少一分钟。
// 工作说明：参数 increase 为 1 时加 60 秒，为 0 时减 60 秒；检查上下限，并把新时间保存为复位目标。
void change_minute(unsigned char increase);                                     // 实现在 countdown.c
// 函数用途：toggle_timer() —— 切换倒计时的开始与暂停。
// 工作说明：正在运行就暂停；已经暂停就开始。若剩余时间为零，先恢复保存的设定时间再尝试开始。
void toggle_timer(void);                                                        // 实现在 countdown.c
// 函数用途：tick_countdown() —— 推进倒计时的毫秒计数。
// 工作说明：每 1 毫秒调用一次；运行时累计满 1000 毫秒减一秒，在 30/10/5 秒设置提示，归零时暂停并安排结果回报。
void tick_countdown(void);                                                      // 实现在 countdown.c

#endif                                                                          // COUNTDOWN_H
