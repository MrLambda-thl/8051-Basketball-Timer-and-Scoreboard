// ============================================================================
// 文件：src/serial.c
// 用途：单片机与电脑的串口通信。
//
// 用 Timer1 产生串口波特率；电脑设置为 4800 baud、8 数据位、无校验、1 停止位。
// 从 P3.0/RXD 接收命令，经 P3.1/TXD 发送时间、比分、运行状态和最终比赛结果。
// 支持 A=12、B=08、A+/A-/B+/B-、?、R；收到回车或换行后解析，小写可转大写。
// 主循环反复调用 serial_poll_rx()/serial_poll_tx()，按字符推进，不在此等待整行传输。
// 通过 countdown.h 和 score.h 访问同一份时间、比分；这里只管理通信与命令解析。
// ============================================================================

#include "serial.h"
#include "countdown.h"
#include "score.h"

static char __xdata serial_tx_line[40];                                         // 放进扩展 RAM，留出内部 RAM 给中断
static unsigned char serial_tx_length = 0;                                      // 记录发送缓冲区已有多少个有效字符，初始为空。
static unsigned char serial_tx_position = 0;                                    // 记录下一个要发送的字符下标；数组下标从 0 开始。
static unsigned char serial_tx_active = 0;                                      // 发送忙标志：1 表示当前这行尚未发完，0 表示可以准备新行。
static char __xdata serial_rx_line[8];                                          // 在扩展数据存储区准备 8 字节接收数组，暂存电脑送来的命令。
static unsigned char serial_rx_length = 0;                                      // 记录当前命令已收到多少字符；本程序用长度判断结束，无需字符串结尾 0。
volatile unsigned char serial_status_due = 1;                                   // 上电先置 1，使主循环自动发送一次时间、比分和运行状态。
volatile unsigned char serial_result_due = 0;                                   // 最终结果的待发送标记，倒计时归零时由中断置 1。
static unsigned char serial_error_due = 0;                                      // 错误回报的待发送标记；收到无效命令时置 1。

// 函数用途：serial_init() —— 初始化 UART 串口和 Timer1 波特率时钟。
// 工作说明：配置串口模式、接收允许位和 Timer1 重装值，清除收发标志后启动 Timer1；电脑使用 4800 波特率、8N1。
void serial_init(void)                                                          // 配置串口与 Timer1 的波特率发生器。
{                                                                               // 开始这一段语句块；配对的右花括号表示这段结束。
    TMOD = (TMOD & 0x0f) | 0x20;                                                // Timer1：8 位自动重装
    PCON |= 0x80;                                                               // SMOD=1，串口波特率加倍
    TH1 = 0xf3;                                                                 // 设置 Timer1 自动重装值为 243；每次溢出后从这里重新开始计数。
    TL1 = 0xf3;                                                                 // 当前计数器也从 243 开始，使首次溢出与后续周期一致。
    SCON = 0x50;                                                                // UART 模式 1，并允许接收
    TI = 0;                                                                     // 软件清除发送完成标志，等待硬件在一个字符发送完成后再次置 1。
    RI = 0;                                                                     // 软件清除接收完成标志，为接收下一个字符做准备。
    TR1 = 1;                                                                    // 启动 Timer1，供串口产生波特率时钟。
}                                                                               // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。

// 函数用途：serial_add() —— 把一个字符添加到待发送的一行文字末尾。
// 工作说明：参数 character 是待加字符；缓冲区未满才写入 serial_tx_line，并将有效长度加一。
static void serial_add(char character)                                          // 把参数 character 的一个字符加入发送数组。
{                                                                               // 开始这一段语句块；配对的右花括号表示这段结束。
    if (serial_tx_length < sizeof(serial_tx_line))                              // sizeof 得到数组容量 40；只有未装满时才允许写入，避免越界。
        serial_tx_line[serial_tx_length++] = character;                         // 先在当前长度对应的下标放字符，再把长度加 1；这里是后缀 ++。
}                                                                               // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。

// 函数用途：serial_add_two_digits() —— 把 0～99 的数值变成两个可显示的数字字符。
// 工作说明：参数 value 是待转换数值；先加十位再加个位，例如整数 8 会向发送缓冲区加入字符 0 和 8。
static void serial_add_two_digits(unsigned char value)                          // 将参数 value 转换为两位数字字符加入发送数组。
{                                                                               // 开始这一段语句块；配对的右花括号表示这段结束。
    /* 整数 8 与字符 '8' 不是同一个字节。先取十位、个位，
     * 再加上字符 '0' 的编码：8 最终会发送字符 '0' 和 '8'。 */
    serial_add('0' + value / 10);                                               // / 是整数除法，取十位再加字符 '0'，例如 8/10 为 0，生成字符 '0'。
    serial_add('0' + value % 10);                                               // % 是取余，取个位再加字符 '0'，例如 8%10 为 8，生成字符 '8'。
}                                                                               // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。

// 函数用途：serial_make_status() —— 组装一行当前比赛状态文字。
// 工作说明：读取时间和运行状态，生成 TIME、A、B、RUN 字段及回车换行，保存到发送缓冲区，供后续逐字符发送。
static void serial_make_status(void)                                            // 组装当前时间、比分和运行状态消息。
{                                                                               // 开始这一段语句块；配对的右花括号表示这段结束。
    unsigned int seconds;                                                       // 声明 16 位局部变量，保存这次读取的完整秒数快照。
    unsigned char is_running;                                                   // 声明 8 位局部变量，保存同一时刻的运行或暂停标记。
    // 暂停可屏蔽中断，让下面几步操作不会被定时器中途打断。
    EA = 0;                                                                     // 16 位秒数要一次读完整
    seconds = remaining_seconds;                                                // 复制一份剩余秒数，后续拆位或组装文字都使用这份快照。
    is_running = running;                                                       // 复制当前运行标记，使这一行消息中的时间与运行状态来自同一次读取。
    EA = 1;                                                                     // 重新打开总中断；各中断还要由自己的允许位决定是否响应。

    serial_tx_length = 0;                                                       // 将有效长度清零，准备从缓冲区开头组装新的一行。
    serial_add('T'); serial_add('I'); serial_add('M'); serial_add('E');         // 四次调用依次加入字符 T、I、M、E，拼出 TIME。
    serial_add('=');                                                            // 加入等号字符，连接字段名和字段值。
    serial_add_two_digits((unsigned char)(seconds / 60));                       // 总秒数除以 60 得到分钟，转成 8 位数后按两位数字加入缓冲区。
    serial_add(':');                                                            // 加入冒号，形成 MM:SS 的时间格式。
    serial_add_two_digits((unsigned char)(seconds % 60));                       // 总秒数除以 60 的余数就是秒，范围 0～59，按两位加入。
    serial_add(' '); serial_add('A'); serial_add('=');                          // 依次加入空格、A 和等号，为 A 队分数字段做准备。
    serial_add_two_digits(score_a);                                             // 将 A 队分数写成两位字符，例如 2 写成 02。
    serial_add(' '); serial_add('B'); serial_add('=');                          // 在前一字段后面加入空格和 B=。
    serial_add_two_digits(score_b);                                             // 将 B 队分数写成两位字符。
    serial_add(' '); serial_add('R'); serial_add('U'); serial_add('N');         // 加入空格与 RUN，表示后面是运行状态。
    serial_add('='); serial_add(is_running ? '1' : '0');                        // 三目运算：运行时选字符 '1'，暂停时选字符 '0'，加到等号后面。
    serial_add('\r'); serial_add('\n');                                         // 加入回车 CR 和换行 LF，让电脑串口终端从下一行显示新消息。
}                                                                               // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。

// 函数用途：serial_make_result() —— 组装一行最终比分与胜者文字。
// 工作说明：生成 FINAL、两队比分和 WINNER 字段；A/B 分数相等时写 DRAW，末尾加入回车换行。
static void serial_make_result(void)                                            // 组装最终比分和胜者消息。
{                                                                               // 开始这一段语句块；配对的右花括号表示这段结束。
    serial_tx_length = 0;                                                       // 将有效长度清零，准备从缓冲区开头组装新的一行。
    serial_add('F'); serial_add('I'); serial_add('N'); serial_add('A');         // 先拼出最终结果行开头的 FINA。
    serial_add('L'); serial_add(' '); serial_add('A'); serial_add('=');         // 接着加入 L、空格和 A=，与上一行合起来是 FINAL A=。
    serial_add_two_digits(score_a);                                             // 将 A 队分数写成两位字符，例如 2 写成 02。
    serial_add(' '); serial_add('B'); serial_add('=');                          // 在前一字段后面加入空格和 B=。
    serial_add_two_digits(score_b);                                             // 将 B 队分数写成两位字符。
    serial_add(' '); serial_add('W'); serial_add('I'); serial_add('N');         // 加入空格和 WIN，为胜者字段 WINNER 的前半部分。
    serial_add('N'); serial_add('E'); serial_add('R'); serial_add('=');         // 补齐 NER=，完整字段名为 WINNER=。
    if (score_a > score_b) serial_add('A');                                     // A 分数更高时，在胜者字段中写 A。
    else if (score_b > score_a) serial_add('B');                                // 否则若 B 分数更高，就写 B。
    else {                                                                      // 前面的判断都不成立时，执行这个默认分支。
        serial_add('D'); serial_add('R'); serial_add('A'); serial_add('W');     // 两队分数相等时拼出 DRAW，表示平局。
    }                                                                           // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。
    serial_add('\r'); serial_add('\n');                                         // 加入回车 CR 和换行 LF，让电脑串口终端从下一行显示新消息。
}                                                                               // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。

// 函数用途：serial_start_tx() —— 开始发送已经组装好的那一行文字。
// 工作说明：设置发送状态与后续字符位置，清除 TI 后把第一个字符写入 SBUF，其余字符交给发送轮询续发。
static void serial_start_tx(void)                                               // 启动发送缓冲区中的一行消息。
{                                                                               // 开始这一段语句块；配对的右花括号表示这段结束。
    serial_tx_position = 1;                                                     // 第 0 个字符马上发送，所以后续续发从下标 1 开始。
    serial_tx_active = 1;                                                       // 将发送状态标为忙，防止当前行还没发完就被下一行覆盖。
    TI = 0;                                                                     // 软件清除发送完成标志，等待硬件在一个字符发送完成后再次置 1。
    SBUF = serial_tx_line[0];                                                   // 向发送寄存器写入第一个字符，硬件开始从 TXD 引脚发出。
}                                                                               // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。

// 函数用途：serial_poll_tx() —— 检查串口发送进度并安排下一条消息。
// 工作说明：前一个字符发完就续发下一个；整行结束后按状态、结果、错误的优先顺序选择新消息，不等待整行发送完才返回。
void serial_poll_tx(void)                                                       // 轮询发送进度，每次只推进可执行的一小步。
{                                                                               // 开始这一段语句块；配对的右花括号表示这段结束。
    unsigned char message_type = 0;                                             // 本次待发消息类型先设为 0，表示暂时没有选中任何消息。
    /* 发送一整行要花很多毫秒；这里每次只在前一个字节发完后
     * 放入下一个字节。这样主循环仍能及时扫描按键和刷新显示。 */
    if (serial_tx_active && TI) {                                               // 当前正在发一行，而且硬件报告前一个字符发完，才继续处理。
        TI = 0;                                                                 // 软件清除发送完成标志，等待硬件在一个字符发送完成后再次置 1。
        if (serial_tx_position < serial_tx_length)                              // 下标还小于有效长度，说明缓冲区仍有字符尚未发送。
            SBUF = serial_tx_line[serial_tx_position++];                        // 发出当前下标的字符，然后将下标加 1，为下次发送做准备。
        else serial_tx_active = 0;                                              // 没有剩余字符，而且前一个已发完，将发送状态改为空闲。
    }                                                                           // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。
    if (serial_tx_active) return;                                               // 这一行还没发完就退出本次轮询，主循环继续处理其他任务。

    // 关中断只用来一次性选择消息；若此刻刚好归零，状态总排在结局前。
    EA = 0;                                                                     // 暂停可屏蔽中断，让下面几步操作不会被定时器中途打断。
    if (serial_status_due) {                                                    // 先检查是否需要发送状态；它的优先级高于结果和错误。
        serial_status_due = 0;                                                  // 本次已接下状态发送任务，清掉对应待发送标记。
        message_type = 1;                                                       // 约定类型 1 代表时间、比分和运行状态行。
    } else if (serial_result_due) {                                             // 没有待发状态行时，再检查最终结果行。
        serial_result_due = 0;                                                  // 接下最终结果发送任务，清掉它的待发送标记。
        message_type = 2;                                                       // 约定类型 2 代表最终分数与胜者行。
    } else if (serial_error_due) {                                              // 状态与结果都不待发时，再处理错误回报。
        serial_error_due = 0;                                                   // 清掉已接下的错误回报任务标记。
        message_type = 3;                                                       // 约定类型 3 代表 ERR 错误消息。
    }                                                                           // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。
    EA = 1;                                                                     // 重新打开总中断；各中断还要由自己的允许位决定是否响应。

    if (message_type == 1) {                                                    // 选中状态消息时，执行下面的状态行组装和发送。
        serial_make_status();                                                   // 调用函数，把 TIME、A、B、RUN 字段拼进发送数组。
        serial_start_tx();                                                      // 当前行已组装好，启动第一个字符的发送。
    } else if (message_type == 2) {                                             // 类型为 2 时改为准备最终结果行。
        serial_make_result();                                                   // 组装 FINAL、两队分数及 WINNER 字段。
        serial_start_tx();                                                      // 当前行已组装好，启动第一个字符的发送。
    } else if (message_type == 3) {                                             // 类型为 3 时准备简短的错误消息。
        serial_tx_length = 0;                                                   // 将有效长度清零，准备从缓冲区开头组装新的一行。
        serial_add('E'); serial_add('R'); serial_add('R');                      // 依次加入 E、R、R，电脑会看到 ERR。
        serial_add('\r'); serial_add('\n');                                     // 加入回车 CR 和换行 LF，让电脑串口终端从下一行显示新消息。
        serial_start_tx();                                                      // 当前行已组装好，启动第一个字符的发送。
    }                                                                           // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。
}                                                                               // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。

// 函数用途：serial_handle_command() —— 解析电脑已经发送完整的一条命令。
// 工作说明：识别 A=12、B=08、加减分、? 和 R 等命令，修改比分或登记回报任务；不符合格式时登记 ERR 回报。
static void serial_handle_command(void)                                         // 解析接收数组中的一条完整命令。
{                                                                               // 开始这一段语句块；配对的右花括号表示这段结束。
    unsigned char value;                                                        // 暂存由两位数字字符换算出来的分数，范围 0～99。
    char team = serial_rx_line[0];                                              // 读取命令的第一个字符，可能是队名 A/B，也可能是 ? 或 R。

    if (serial_rx_length == 1 && team == '?') {                                 // 命令只有一个字符且是问号时，安排发送当前状态。
        serial_status_due = 1;                                                  // 登记状态回报任务，稍后由发送轮询组装并发出。
    } else if (serial_rx_length == 1 && team == 'R') {                          // 单个大写 R 表示请求最终结果；小写已在接收时转大写。
        serial_result_due = 1;                                                  // 登记最终结果回报任务，稍后发送两队分数和胜者。
    } else if (serial_rx_length == 4 && serial_rx_line[1] == '=' &&             // 设置分数必须恰好四字符，且下标 1 是等号，例如 A=12。
               (team == 'A' || team == 'B') &&                                  // 第一个字符必须为 A 或 B；&& 表示还要同时满足后续条件。
               serial_rx_line[2] >= '0' && serial_rx_line[2] <= '9' &&          // 检查十位字符确实在 '0'～'9' 的编码范围内。
               serial_rx_line[3] >= '0' && serial_rx_line[3] <= '9') {          // 同样检查个位也是数字，全部条件成立才接受这条设置命令。
        /* 例如收到 A=12：字符 '1'、'2' 分别减去 '0'，得到数字
         * 1、2；再计算 1×10+2，得到真正的整数 12。 */
        value = (unsigned char)((serial_rx_line[2] - '0') * 10 +                // 字符十位减去 '0' 得到数值，再乘 10，接着加下一行的个位。
                                 serial_rx_line[3] - '0');                      // 将个位字符也换成数值，合成最终分数，例如 1*10+2 得 12。
        if (team == 'A') score_a = value;                                       // 队名为 A 时，将新分数直接写入 A 队。
        else score_b = value;                                                   // 前面已确认队名只能是 A/B，所以否则就是设置 B 队。
        score_dirty = 1;                                                        // 标记比分已改变，让主循环刷新 U2，并安排向电脑报告。
    } else if (serial_rx_length == 2 &&                                         // 若命令恰好两个字符，继续检查是否为 A+、A-、B+ 或 B-。
               (team == 'A' || team == 'B') &&                                  // 第一个字符必须为 A 或 B；&& 表示还要同时满足后续条件。
               (serial_rx_line[1] == '+' || serial_rx_line[1] == '-')) {        // 第二字符必须是加号或减号，才是合法的单分操作。
        change_score(team == 'B', serial_rx_line[1] == '+');                    // 比较式成立得到 1，否则得到 0；因此 B 对应 team=1，加号对应 increase=1。
    } else {                                                                    // 结束前面的成立分支，条件不成立时执行下面这段。
        serial_error_due = 1;                                                   // 标记收到非法或过长命令，主循环稍后向电脑发送 ERR。
    }                                                                           // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。
}                                                                               // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。

// 函数用途：serial_poll_rx() —— 检查并收集电脑发来的串口字符。
// 工作说明：RI 为 1 时读取 SBUF；普通字符放入接收数组，小写转大写；收到回车或换行后解析非空命令，越界时登记错误。
void serial_poll_rx(void)                                                       // 轮询串口接收，按字符组装电脑命令。
{                                                                               // 开始这一段语句块；配对的右花括号表示这段结束。
    char character;                                                             // 声明局部变量，用来暂存这次收到的一个字符。
    if (!RI) return;                                                            // ! 表示逻辑非：RI 为 0 说明没有新字符，直接返回，避免一直等待。
    /* RI 是单片机的“收到字符”标志；先取走字符，再清掉标志。
     * 末尾收到回车或换行时，才把缓冲区的一整条命令交给解析函数。 */
    character = SBUF;                                                           // 从串口接收寄存器取出刚收到的字节，保存到局部变量。
    RI = 0;                                                                     // 软件清除接收完成标志，为接收下一个字符做准备。
    if (character == '\r' || character == '\n') {                               // 收到回车或换行就认为这一行命令已经结束。
        if (serial_rx_length) serial_handle_command();                          // 仅非空命令才解析，所以连续收到 CR、LF 不会重复执行。
        serial_rx_length = 0;                                                   // 丢弃当前行的有效长度，从数组开头开始接收下一条命令。
    } else if (serial_rx_length < sizeof(serial_rx_line)) {                     // 不是行结束符且接收数组还有空位时，才保存字符。
        if (character >= 'a' && character <= 'z') character -= 32;              // ASCII 小写比大写编码大 32，减 32 让 a/b/r 也可识别。
        serial_rx_line[serial_rx_length++] = character;                         // 把字符写到当前末尾，再将接收长度加 1。
    } else {                                                                    // 结束前面的成立分支，条件不成立时执行下面这段。
        serial_rx_length = 0;                                                   // 丢弃当前行的有效长度，从数组开头开始接收下一条命令。
        serial_error_due = 1;                                                   // 标记收到非法或过长命令，主循环稍后向电脑发送 ERR。
    }                                                                           // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。
}                                                                               // 结束这一层语句块，按外层循环、判断或函数的结构继续执行。

