// ============================================================================
// 文件：include/scheduler.h
// 用途：Timer0 中断与时间节拍调度接口声明。
//
// 声明按键扫描通知、OLED 上电计数、Timer0 初始化和中断入口。中断声明也让编译器生成正确的中断向量。
// 具体函数体和模块内部变量位于 src/scheduler.c；本文件负责告诉其他模块有哪些接口可用。
// 其他 .c 通过 #include 引入本头文件，再调用声明过的函数。
// extern 只声明共享变量的名字，不分配第二份变量；头文件保护宏避免重复声明引起的问题。
// ============================================================================

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "board.h"

// Timer0 中断与节拍调度模块对外接口。
// .h 里只声明名字；变量的存储空间及函数实现由对应 .c 提供。
extern volatile unsigned char scan_due;                                         // 该扫描按键了：由中断置 1
#if OLED_TEST_ENABLED
extern volatile unsigned char startup_ms;                                       // 上电后等 OLED 电源稳定 200 毫秒
#endif                                                                          // OLED_TEST_ENABLED

// 函数用途：timer0_isr() —— 执行 Timer0 每次溢出后的定时任务。
// 工作说明：每 250 微秒更新方波和 PWM；累计四次后处理一次毫秒任务，并每 10 毫秒通知主循环扫描按键。
void timer0_isr(void) __interrupt(1);                                           // 实现在 scheduler.c
// 函数用途：timer0_init() —— 配置并启动 Timer0 定时中断。
// 工作说明：在保留 Timer1 配置的前提下设置 8 位自动重装模式、重装值和中断允许位，产生 250 微秒节拍。
void timer0_init(void);                                                         // 实现在 scheduler.c

#endif                                                                          // SCHEDULER_H
