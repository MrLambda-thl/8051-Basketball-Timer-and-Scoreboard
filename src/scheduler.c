// ============================================================================
// 文件：src/scheduler.c
// 用途：Timer0 中断与时间节拍调度。
//
// timer0_init() 将 Timer0 配置成 250 微秒自动重装中断，提供全程序的时间节拍。
// 每次中断更新 P2.4 方波并调用 PWM 输出；quarter_ms 累计四次后产生 1 毫秒节拍。
// 每毫秒处理上电等待、数码管扫描、倒计时、蜂鸣器节奏和呼吸亮度更新。
// key_divider 每累计 10 毫秒设置 scan_due，通知主循环执行按键扫描。
// 中断中只执行定时任务；串口命令解析与按键完整处理在主循环中执行。
// ============================================================================

#include "scheduler.h"
#include "countdown.h"
#include "display.h"
#include "buzzer.h"
#include "breathing.h"

static unsigned char quarter_ms = 0;                                            // 4 个 250 微秒组成 1 毫秒
static unsigned char key_divider = 0;                                           // 10 个 1 毫秒组成一次按键采样周期
static unsigned char square_half_ticks = 0;                                     // 两次 250 微秒中断后翻转 P2.4
volatile unsigned char scan_due = 0;                                            // 该扫描按键了：由中断置 1
#if OLED_TEST_ENABLED
volatile unsigned char startup_ms = 0;                                          // 上电后等 OLED 电源稳定 200 毫秒
#endif                                                                          // OLED_TEST_ENABLED

// 串口 Timer1 已由 serial_init() 配置；这里保留它，只设置 Timer0。
// 函数用途：timer0_init() —— 配置并启动 Timer0 定时中断。
// 工作说明：在保留 Timer1 配置的前提下设置 8 位自动重装模式、重装值和中断允许位，产生 250 微秒节拍。
void timer0_init(void)
{
    /* TMOD 低 4 位设置 Timer0 为模式 2（8 位自动重装）。
     * TH0/TL0=6：从 6 数到 255，共 250 个计数，即 250 微秒。
     * ET0 允许 Timer0 中断，EA 开总中断，TR0 启动 Timer0。 */
    TMOD = (TMOD & 0xf0) | 0x02;                                                // 保留 Timer1 的高四位配置，只把 Timer0 的低四位设为模式 2。
    TH0 = 6;                                                                    // Timer0 自动重装值为 6，溢出后硬件重新装入它。
    TL0 = 6;                                                                    // 当前计数也从 6 起步；12MHz/12T 下计数 250 次就是 250 微秒。
    ET0 = 1;                                                                    // 允许 Timer0 溢出中断；还需 EA=1 才能真正响应。
    EA = 1;                                                                     // 重新打开总中断；各中断还要由自己的允许位决定是否响应。
    TR0 = 1;                                                                    // 启动 Timer0 开始计数，之后自动周期性进入 timer0_isr。
}

// 函数用途：timer0_isr() —— 执行 Timer0 每次溢出后的定时任务。
// 工作说明：每 250 微秒更新方波和 PWM；累计四次后处理一次毫秒任务，并每 10 毫秒通知主循环扫描按键。
void timer0_isr(void) __interrupt(1)                                            // Timer0 中断入口；__interrupt(1) 指定中断编号 1，不是优先级或 1 秒。
{                                                                               // 开始这一段语句块；配对的右花括号表示这段结束。
    /* Timer0 每 250 微秒中断一次。每 2 次中断翻转一次 P2.4，
     * 高、低电平各保持约 500 微秒，完整周期约 1 毫秒，即约 1 kHz。 */
    if (++square_half_ticks >= 2) {                                             // 每次中断累计一次，累计两次就是 500 微秒，需要翻转方波电平。
        square_half_ticks = 0;                                                  // 清除半周期计数，重新累计下一个 500 微秒。
        P2 ^= SQUARE_BIT;                                                       // ^= 是按位异或赋值，只翻转 P2.4：原先高变低，原先低变高。
    }                                                                           // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。
    update_breath_pwm();                                                        // 每次中断更新一个 PWM 时间片，仍为 250 微秒一片。

    // 第 1、2、3 次 250us 中断在这里结束；第 4 次继续做 1ms 工作。
    if (++quarter_ms < 4) return;                                               // 累计不足四次中断就提前退出；只有每第四次才继续下面的毫秒任务。
    quarter_ms = 0;                                                             // 这四次已组成 1 毫秒，计数清零，准备累计下一毫秒。

#if OLED_TEST_ENABLED                                                           // 条件编译：宏为 1 时才编译下面的 OLED 代码；这不是运行时的 if。
    if (startup_ms < 200) ++startup_ms;                                         // 上电等待计数每毫秒加 1，到 200 停住，避免 8 位变量溢出。
#endif                                                                          // 结束前面的条件编译范围。

    scan_one_digit();                                                           // 每毫秒刷新一位数码管，八位轮流显示。
    // 按键不必每 1 毫秒都读：每 10 毫秒通知主循环扫描一次。
    if (++key_divider >= 10) {                                                  // 毫秒分频计数达到 10，表示该通知主循环采样按键了。
        key_divider = 0;                                                        // 清零十毫秒分频计数，开始累计下一次按键扫描间隔。
        scan_due = 1;                                                           // 向主循环登记一次待扫描任务；真正的按键处理不在中断里完成。
    }                                                                           // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。
    tick_countdown();                                                           // 推进一毫秒倒计时逻辑，累计满一秒时减去一秒。
    update_alert_output();                                                      // 根据当前剩余时间和鸣叫周期，更新 P1.4 蜂鸣器输出。
    update_breath_level();                                                      // 按一毫秒节拍更新目标亮度，实际 LED 明灭由中断前半段的 PWM 完成。
}                                                                               // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。

