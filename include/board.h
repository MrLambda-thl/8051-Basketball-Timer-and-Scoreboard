// ============================================================================
// 文件：include/board.h
// 用途：公共硬件接线与固定参数。
//
// 集中定义七个按键的端口掩码、蜂鸣器/方波/呼吸灯的引脚位，以及最大倒计时时间。
// 还定义 PWM 的时间片数、两档亮度变化间隔和 OLED 功能编译开关。
// 通过 8052.h 提供寄存器名字；其他模块包含本文件后就能使用统一的硬件定义。
// 修改接线或这些固定参数时先检查这里；本文件不创建可变的全局变量。
// ============================================================================

#ifndef BOARD_H
#define BOARD_H

#include <8052.h>                                                               // P0/P1/P2/P3、EA、定时器、串口寄存器定义

// 公共硬件配置：宏只是给固定数值起名字，不在这里定义可变的全局变量。
// 按键接地：松开时端口读到 1，按下时读到 0。这里的数值是端口位掩码。
#define KEY_MINUS 0x04                                                          // SW3：P3.2，计时减 1 分钟
#define KEY_TOGGLE 0x08                                                         // SW4：P3.3，开始／暂停
#define KEY_PLUS 0x80                                                           // SW8：P3.7，计时加 1 分钟
#define KEY_A_MINUS 0x10                                                        // SW5：P3.4，A 队减 1 分
#define KEY_B_PLUS 0x20                                                         // SW6：P3.5，B 队加 1 分
#define KEY_B_MINUS 0x40                                                        // SW7：P3.6，B 队减 1 分
#define KEY_A_PLUS 0x20                                                         // SW9：P1.5，A 队加 1 分；注意它不在 P3
#define MAX_SECONDS 5999                                                        // 四位 MMSS 显示的最大值：99 分 59 秒
#define ALERT_BIT 0x10                                                          // P1.4：蜂鸣器和板载 LED2 共用的控制线
#define SQUARE_BIT 0x10                                                         // P2.4：输出约 1 kHz 方波
#define BREATH_BIT 0x20                                                         // P2.5：外接 LED，输出高电平时点亮
/* PWM 把一个 8 毫秒周期分成 32 份：每份 250 微秒。
 * 例如亮 8 份、灭 24 份，平均亮度约为 25%。人眼把快速明灭看成亮度变化。 */
#define BREATH_PWM_STEPS 32                                                     // 用这个名字代表数字 32：每个 PWM 周期分为 32 个时间片。
/* 一轮呼吸需要从 0 级升到 32 级，再降回 0 级，共 64 次变化。
 * 普通阶段：64×31ms≈2 秒；最后 10 秒：64×12ms≈0.77 秒。 */
#define BREATH_SLOW_STEP_MS 31                                                  // 普通呼吸每隔 31 毫秒改变一级亮度。
#define BREATH_FAST_STEP_MS 12                                                  // 快速呼吸每隔 12 毫秒改变一级亮度，数值越小变化越快。
#define OLED_TEST_ENABLED 1                                                     // 上电后在 OLED 显示自定义图片


#endif                                                                          // BOARD_H
