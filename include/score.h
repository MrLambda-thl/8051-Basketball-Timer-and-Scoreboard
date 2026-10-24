// ============================================================================
// 文件：include/score.h
// 用途：A、B 两队比分管理接口声明。
//
// 声明两队分数、比分刷新标记，以及加减分、交换、清零函数。
// 具体函数体和模块内部变量位于 src/score.c；本文件负责告诉其他模块有哪些接口可用。
// 其他 .c 通过 #include 引入本头文件，再调用声明过的函数。
// extern 只声明共享变量的名字，不分配第二份变量；头文件保护宏避免重复声明引起的问题。
// ============================================================================

#ifndef SCORE_H
#define SCORE_H

#include "board.h"

// 比分模块对外接口。
// .h 里只声明名字；变量的存储空间及函数实现由对应 .c 提供。
extern unsigned char score_a;                                                   // A 队分数：0～99
extern unsigned char score_b;                                                   // B 队分数：0～99
extern unsigned char score_dirty;                                               // 计分数字需要刷新

// 函数用途：change_score() —— 给指定队伍加一分或减一分。
// 工作说明：team 为 0 选 A 队、为 1 选 B 队；increase 为 1 加分、为 0 减分；分数限制在 0～99 并标记显示待刷新。
void change_score(unsigned char team, unsigned char increase);                  // 实现在 score.c
// 函数用途：swap_scores() —— 交换 A、B 两队的当前比分。
// 工作说明：用临时变量保存旧值完成互换，再通知主循环刷新计分显示。
void swap_scores(void);                                                         // 实现在 score.c
// 函数用途：reset_scores() —— 把两队比分同时清零。
// 工作说明：将 A、B 分数设为 0 并标记显示待刷新，不改变倒计时的剩余时间或运行状态。
void reset_scores(void);                                                        // 实现在 score.c

#endif                                                                          // SCORE_H
