// ============================================================================
// 文件：include/breathing.h
// 用途：外接 LED 的 PWM 呼吸调光接口声明。
//
// 声明 PWM 时间片更新和目标亮度更新函数，供 scheduler.c 按不同节拍调用。
// 具体函数体和模块内部变量位于 src/breathing.c；本文件负责告诉其他模块有哪些接口可用。
// 其他 .c 通过 #include 引入本头文件，再调用声明过的函数。
// extern 只声明共享变量的名字，不分配第二份变量；头文件保护宏避免重复声明引起的问题。
// ============================================================================

#ifndef BREATHING_H
#define BREATHING_H

#include "board.h"

// P2.5 呼吸灯 PWM模块对外接口。
// .h 里只声明名字；变量的存储空间及函数实现由对应 .c 提供。

// 函数用途：update_breath_level() —— 计算呼吸灯的目标亮度。
// 工作说明：每 1 毫秒调用一次；按剩余时间选择 31/12 毫秒间隔，让亮度逐级升降，暂停或归零时熄灭。
void update_breath_level(void);                                                 // 实现在 breathing.c
// 函数用途：update_breath_pwm() —— 产生 P2.5 的 PWM 输出。
// 工作说明：每 250 微秒调用一次；推进时间片编号，按当前亮度等级决定 LED 亮或灭。
void update_breath_pwm(void);                                                   // 实现在 breathing.c

#endif                                                                          // BREATHING_H
