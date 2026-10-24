// ============================================================================
// 文件：src/breathing.c
// 用途：外接 LED 的 PWM 呼吸调光。
//
// 用 P2.5 控制外接 LED；正极经限流电阻接 P2.5，输出高电平时点亮。
// update_breath_pwm() 每 250 微秒处理一片，32 片组成 8 毫秒 PWM 周期。
// 亮度从 0～32 共 33 级，代表每周期点亮 0～32 片；由暗到亮再到暗共改变 64 次。
// update_breath_level() 每 1 毫秒检查进度：剩 30～11 秒每 31ms 改一级，最后 10 秒每 12ms 改一级。
// 暂停、归零或超过 30 秒时熄灭；两个更新函数都由 scheduler.c 的中断调用。
// ============================================================================

#include "breathing.h"
#include "countdown.h"

// PWM：32 片 × 250us = 8ms；亮度有 0～32 共 33 级。
// 每 31ms 或 12ms 改一级，一轮呼吸 64 次变化。
static unsigned char breath_pwm_phase = 0;                                      // 0～31：PWM 周期位置
static unsigned char breath_level = 0;                                          // 0～32：0 全灭，32 全亮
static unsigned char breath_step_ms = 0;                                        // 距上次改变亮度经过的毫秒数
static unsigned char breath_rising = 1;                                         // 1 变亮，0 变暗

// 每 250 微秒调用一次：只负责当前时间片亮还是灭，不改变呼吸速度。
// 函数用途：update_breath_pwm() —— 产生 P2.5 的 PWM 输出。
// 工作说明：每 250 微秒调用一次；推进时间片编号，按当前亮度等级决定 LED 亮或灭。
void update_breath_pwm(void)
{
    /* 32 个 250 微秒节拍构成一个 8ms 的 PWM 周期，即每秒 125 次。
     * 如果 level=8，phase 为 0～7 的 8 份点亮，其余 24 份熄灭：
     * LED 在快速闪动，但人眼看到约 25% 的亮度。
     * level=0 永远不满足比较，所以全灭；level=32 则始终满足，全亮。 */
    if (++breath_pwm_phase >= BREATH_PWM_STEPS) breath_pwm_phase = 0;           // PWM 时间片位置每次中断加 1，到 32 回到 0，循环经过 0～31。
    if (breath_pwm_phase < breath_level) P2 |= BREATH_BIT;                      // 片号小于目标亮度时点亮 P2.5，例如亮度 8 时有 8/32 的时间点亮。
    else P2 &= (unsigned char)~BREATH_BIT;                                      // 片号不在点亮区间就清除 P2.5，形成 PWM 的熄灭部分。

}

// 函数用途：update_breath_level() —— 计算呼吸灯的目标亮度。
// 工作说明：每 1 毫秒调用一次；按剩余时间选择 31/12 毫秒间隔，让亮度逐级升降，暂停或归零时熄灭。
void update_breath_level(void)                                                  // 控制呼吸灯亮度由暗到亮、由亮到暗循环。
{                                                                               // 开始这一段语句块；配对的右花括号表示这段结束。
    unsigned char step_length;                                                  // 保存这一级亮度应保持多少毫秒，稍后选 12 或 31。

    /* 三种情况下不应亮：计时暂停、已经归零、还没有进入最后 30 秒。
     * 同时把呼吸进度清零；下次开始计时时才会从暗处慢慢变亮。
     * !running 等价于 running==0，|| 的意思是“或者”。 */
    if (!running || remaining_seconds == 0 || remaining_seconds > 30) {         // 暂停、归零、剩余超过 30 秒，任一成立都要熄灭呼吸灯。
        breath_level = 0;                                                       // 目标亮度归零，PWM 没有任何时间片点亮。
        breath_step_ms = 0;                                                     // 清除亮度变化的毫秒计数，从当前时刻重新累计。
        breath_rising = 1;                                                      // 下一轮从逐渐变亮开始，而不是从变暗方向继续。
        P2 &= (unsigned char)~BREATH_BIT;                                       // 将 P2.5 清零，使正极接该引脚的外接 LED 熄灭。
        return;                                                                 // 立即结束本次函数调用，回到调用它的位置继续执行。
    }                                                                           // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。

    /* 这里没有改变 PWM 的 125Hz 频率，只改变“多久调一级亮度”。
     * 因此最后 10 秒看起来呼吸加快，而不会突然改变闪烁频率。 */
    step_length = (remaining_seconds <= 10) ?                                   // 用条件运算选择速度：剩余不超过 10 秒时选问号后的快速值。
                  BREATH_FAST_STEP_MS : BREATH_SLOW_STEP_MS;                    // 条件成立取 12 毫秒，否则取 31 毫秒，赋给上一行的 step_length。
    // 尚未达到 12ms 或 31ms 时保持原亮度；达到后清零，进入下一级。
    if (++breath_step_ms < step_length) return;                                 // 先累计 1 毫秒，未达到改变亮度的间隔就返回，维持当前亮度。
    breath_step_ms = 0;                                                         // 清除亮度变化的毫秒计数，从当前时刻重新累计。

    /* 亮度从 0→1→…→32，再从 32→31→…→0，循环往复。
     * rising 只记录方向，避免把整条亮度曲线存成数组。 */
    if (breath_rising) {                                                        // 方向标记为 1 时进入逐渐变亮阶段。
        if (++breath_level >= BREATH_PWM_STEPS) breath_rising = 0;              // 亮度先加一级，达到最高 32 级时把方向改为逐渐变暗。
    } else {                                                                    // 当前方向为变暗，每次降低一级亮度。
        if (--breath_level == 0) breath_rising = 1;                             // 变暗时先减一级，降到 0 后将方向改回逐渐变亮。
    }                                                                           // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。
}                                                                               // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。

