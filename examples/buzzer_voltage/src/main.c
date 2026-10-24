/*
 * 蜂鸣器工作电压临时测试程序
 *
 * 适用：STC89C52RC，外部 12 MHz 晶振，定时器 12T。
 * 板上 P1.4 为蜂鸣器控制线。通电后循环：响 5 秒、停 5 秒。
 * 每个状态保持足够长，方便万用表测蜂鸣器两个焊脚之间的直流电压。
 * 这是独立工程，不修改篮球计时器正式程序。
 */
#include <8052.h>

#define BUZZER_BIT 0x10  /* P1.4，置 1 为蜂鸣器工作状态 */

/* Timer0 工作在模式 1：12 MHz / 12 = 1 MHz，每计数一次约 1 微秒。
 * 从 0xFC18 计到 0xFFFF 后溢出，正好经过 1000 次，即约 1 毫秒。
 */
static void wait_one_millisecond(void)
{
    TR0 = 0;
    TF0 = 0;
    TH0 = 0xfc;
    TL0 = 0x18;
    TR0 = 1;
    while (!TF0) {
        /* 等待定时器溢出。此测试不需要处理中断或其他任务。 */
    }
    TR0 = 0;
}

static void wait_five_seconds(void)
{
    unsigned int i;
    for (i = 0; i < 5000; ++i)
        wait_one_millisecond();
}

void main(void)
{
    /* 数码管共用 P0 段线；位选在 P1.0~P1.3、P2.0~P2.3。
     * 测试时让数码管熄灭，减少其他电路对供电的影响。
     */
    P0 = 0x00;
    P2 = 0xff;
    P1 = 0xef;  /* P1.4 先置低，其他 P1 引脚维持高电平。 */
    TMOD = (TMOD & 0xf0) | 0x01;

    while (1) {
        P1 |= BUZZER_BIT;                    /* 响：测蜂鸣器两端电压。 */
        wait_five_seconds();
        P1 &= (unsigned char)~BUZZER_BIT;  /* 停：再测两端电压。 */
        wait_five_seconds();
    }
}
