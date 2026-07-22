#include "r_cg_macrodriver.h"
#include "bsp_soft_uart.h"

/*==========================================================
 * GPIO定义
 *
 * P01：485_DI_2，软件UART发送
 * P00：485_RO_2，软件UART接收
 *==========================================================*/

#define SOFT_UART_TX_PIN          P0_bit.no1
#define SOFT_UART_TX_MODE         PM0_bit.no1

#define SOFT_UART_RX_PIN          P0_bit.no0
#define SOFT_UART_RX_MODE         PM0_bit.no0

#define SOFT_UART_TX_HIGH()       \
    do                            \
    {                             \
        SOFT_UART_TX_PIN = 1U;    \
    } while (0)

#define SOFT_UART_TX_LOW()        \
    do                            \
    {                             \
        SOFT_UART_TX_PIN = 0U;    \
    } while (0)

#define SOFT_UART_RX_READ()       ((uint8_t)SOFT_UART_RX_PIN)

/*==========================================================
 * 缓冲区大小
 *
 * 实际可使用容量为SIZE - 1。
 *==========================================================*/

#define SOFT_UART_TX_BUFFER_SIZE  (64U)
#define SOFT_UART_RX_BUFFER_SIZE  (64U)

/*==========================================================
 * 发送状态
 *==========================================================*/

#define SOFT_UART_TX_STATE_IDLE       (0U)
#define SOFT_UART_TX_STATE_ENABLE     (1U)
#define SOFT_UART_TX_STATE_START      (2U)
#define SOFT_UART_TX_STATE_DATA       (3U)
#define SOFT_UART_TX_STATE_STOP       (4U)

/*==========================================================
 * 接收状态
 *==========================================================*/

#define SOFT_UART_RX_STATE_IDLE       (0U)
#define SOFT_UART_RX_STATE_START      (1U)
#define SOFT_UART_RX_STATE_DATA       (2U)
#define SOFT_UART_RX_STATE_STOP       (3U)

/*==========================================================
 * 发送缓冲区
 *==========================================================*/

static volatile uint8_t g_soft_uart_tx_buffer[
    SOFT_UART_TX_BUFFER_SIZE];

static volatile uint8_t g_soft_uart_tx_head;
static volatile uint8_t g_soft_uart_tx_tail;

/*==========================================================
 * 接收缓冲区
 *==========================================================*/

static volatile uint8_t g_soft_uart_rx_buffer[
    SOFT_UART_RX_BUFFER_SIZE];

static volatile uint8_t g_soft_uart_rx_head;
static volatile uint8_t g_soft_uart_rx_tail;

/*==========================================================
 * 发送状态变量
 *==========================================================*/

static volatile uint8_t g_soft_uart_tx_state;
static volatile uint8_t g_soft_uart_tx_phase;
static volatile uint8_t g_soft_uart_tx_bit_index;
static volatile uint8_t g_soft_uart_tx_current_byte;

/*==========================================================
 * 接收状态变量
 *==========================================================*/

static volatile uint8_t g_soft_uart_rx_state;
static volatile uint8_t g_soft_uart_rx_phase;
static volatile uint8_t g_soft_uart_rx_bit_index;
static volatile uint8_t g_soft_uart_rx_current_byte;
static volatile uint8_t g_soft_uart_rx_enabled;

/*==========================================================
 * 内部函数声明
 *==========================================================*/

static uint8_t SoftUart_GetNextIndex(
    uint8_t index,
    uint8_t buffer_size);

static void SoftUart_TxIRQHandler(void);
static void SoftUart_RxIRQHandler(void);
static void SoftUart_PushRxByte(uint8_t data);
static uint8_t SoftUart_LoadNextTxByte(void);

/*==========================================================
 * 环形缓冲区下标计算
 *==========================================================*/

static uint8_t SoftUart_GetNextIndex(
    uint8_t index,
    uint8_t buffer_size)
{
    index++;

    if (index >= buffer_size)
    {
        index = 0U;
    }

    return index;
}

/*==========================================================
 * 软件UART初始化
 *==========================================================*/

void BSP_SoftUart_Init(void)
{
    /*
     * 设置输出锁存器后，再设置为输出，
     * 防止初始化时出现一个短暂低电平。
     */
    SOFT_UART_TX_HIGH();
    SOFT_UART_TX_MODE = 0U;

    /* P00设置为输入 */
    SOFT_UART_RX_MODE = 1U;

    g_soft_uart_tx_head = 0U;
    g_soft_uart_tx_tail = 0U;

    g_soft_uart_rx_head = 0U;
    g_soft_uart_rx_tail = 0U;

    g_soft_uart_tx_state = SOFT_UART_TX_STATE_IDLE;
    g_soft_uart_tx_phase = 0U;
    g_soft_uart_tx_bit_index = 0U;
    g_soft_uart_tx_current_byte = 0U;

    g_soft_uart_rx_state = SOFT_UART_RX_STATE_IDLE;
    g_soft_uart_rx_phase = 0U;
    g_soft_uart_rx_bit_index = 0U;
    g_soft_uart_rx_current_byte = 0U;

    g_soft_uart_rx_enabled = 1U;
}

/*==========================================================
 * 发送一个字节
 *==========================================================*/

uint8_t BSP_SoftUart_SendByte(uint8_t data)
{
    uint8_t next_head;

    next_head = SoftUart_GetNextIndex(
        g_soft_uart_tx_head,
        SOFT_UART_TX_BUFFER_SIZE);

    /* 发送缓冲区已满 */
    if (next_head == g_soft_uart_tx_tail)
    {
        return 0U;
    }

    /*
     * 先写数据，再更新head。
     * 定时器中断只有看到head变化后才会读取数据。
     */
    g_soft_uart_tx_buffer[g_soft_uart_tx_head] = data;
    g_soft_uart_tx_head = next_head;

    return 1U;
}

/*==========================================================
 * 判断发送是否完成
 *==========================================================*/

uint8_t BSP_SoftUart_IsTxComplete(void)
{
    if ((g_soft_uart_tx_state ==
         SOFT_UART_TX_STATE_IDLE) &&
        (g_soft_uart_tx_head ==
         g_soft_uart_tx_tail))
    {
        return 1U;
    }

    return 0U;
}

/*==========================================================
 * 等待发送完成
 *==========================================================*/

void BSP_SoftUart_WaitTxComplete(void)
{
    while (BSP_SoftUart_IsTxComplete() == 0U)
    {
        /*
         * 等待定时器中断发送数据。
         *
         * 如程序一直停在这里，优先检查：
         * 1. 定时器是否启动；
         * 2. 全局中断是否打开；
         * 3. 定时器ISR是否调用了
         *    BSP_SoftUart_TimerIRQHandler()。
         */
    }
}

/*==========================================================
 * 获取接收数据量
 *==========================================================*/

uint16_t BSP_SoftUart_Available(void)
{
    uint8_t head;
    uint8_t tail;

    head = g_soft_uart_rx_head;
    tail = g_soft_uart_rx_tail;

    if (head >= tail)
    {
        return (uint16_t)(head - tail);
    }

    return (uint16_t)(
        SOFT_UART_RX_BUFFER_SIZE -
        tail +
        head);
}

/*==========================================================
 * 读取接收数据
 *==========================================================*/
uint16_t BSP_SoftUart_Read(
    uint8_t *buffer,
    uint16_t max_length)
{
    uint16_t read_length;

    if ((buffer == (uint8_t *)0) ||
        (max_length == 0U))
    {
        return 0U;
    }

    read_length = 0U;

    while ((read_length < max_length) &&
           (g_soft_uart_rx_tail !=
            g_soft_uart_rx_head))
    {
        buffer[read_length] =
            g_soft_uart_rx_buffer[
                g_soft_uart_rx_tail];

        g_soft_uart_rx_tail =
            SoftUart_GetNextIndex(
                g_soft_uart_rx_tail,
                SOFT_UART_RX_BUFFER_SIZE);

        read_length++;
    }

    return read_length;
}

/*==========================================================
 * 清空接收缓冲区
 *==========================================================*/

void BSP_SoftUart_ClearRx(void)
{
    g_soft_uart_rx_tail =
        g_soft_uart_rx_head;

    g_soft_uart_rx_state =
        SOFT_UART_RX_STATE_IDLE;

    g_soft_uart_rx_phase = 0U;
    g_soft_uart_rx_bit_index = 0U;
    g_soft_uart_rx_current_byte = 0U;
}

/*==========================================================
 * 允许或禁止接收
 *==========================================================*/

void BSP_SoftUart_EnableRx(uint8_t enable)
{
    if (enable != 0U)
    {
        g_soft_uart_rx_enabled = 1U;
    }
    else
    {
        g_soft_uart_rx_enabled = 0U;

        g_soft_uart_rx_state =
            SOFT_UART_RX_STATE_IDLE;

        g_soft_uart_rx_phase = 0U;
        g_soft_uart_rx_bit_index = 0U;
        g_soft_uart_rx_current_byte = 0U;
    }
}

/*==========================================================
 * 从发送缓冲区取出下一个字节
 *==========================================================*/

static uint8_t SoftUart_LoadNextTxByte(void)
{
    if (g_soft_uart_tx_tail ==
        g_soft_uart_tx_head)
    {
        return 0U;
    }

    g_soft_uart_tx_current_byte =
        g_soft_uart_tx_buffer[
            g_soft_uart_tx_tail];

    g_soft_uart_tx_tail =
        SoftUart_GetNextIndex(
            g_soft_uart_tx_tail,
            SOFT_UART_TX_BUFFER_SIZE);

    return 1U;
}

/*==========================================================
 * 将字节放入接收缓冲区
 *==========================================================*/

static void SoftUart_PushRxByte(uint8_t data)
{
    uint8_t next_head;

    next_head = SoftUart_GetNextIndex(
        g_soft_uart_rx_head,
        SOFT_UART_RX_BUFFER_SIZE);

    /*
     * 缓冲区满时丢弃最新接收的数据。
     * 不覆盖尚未读取的旧数据。
     */
    if (next_head == g_soft_uart_rx_tail)
    {
        return;
    }

    g_soft_uart_rx_buffer[
        g_soft_uart_rx_head] = data;

    g_soft_uart_rx_head = next_head;
}

/*==========================================================
 * 软件UART发送状态机
 *
 * 定时器频率为波特率的3倍。
 * 每个数据位持续3次中断。
 *==========================================================*/

static void SoftUart_TxIRQHandler(void)
{
    uint8_t tx_bit;

    switch (g_soft_uart_tx_state)
    {
        case SOFT_UART_TX_STATE_IDLE:

            if (SoftUart_LoadNextTxByte() != 0U)
            {
                /*
                 * 数据进入发送状态。
                 * 先保持一个定时器周期高电平，
                 * 为RS-485收发器使能留出时间。
                 */
                SOFT_UART_TX_HIGH();

                g_soft_uart_tx_phase = 0U;
                g_soft_uart_tx_bit_index = 0U;

                g_soft_uart_tx_state =
                    SOFT_UART_TX_STATE_ENABLE;
            }

            break;

        case SOFT_UART_TX_STATE_ENABLE:

            g_soft_uart_tx_phase++;

            /*
             * 等待1个采样周期后开始起始位。
             */
            if (g_soft_uart_tx_phase >= 1U)
            {
                g_soft_uart_tx_phase = 0U;

                SOFT_UART_TX_LOW();

                g_soft_uart_tx_state =
                    SOFT_UART_TX_STATE_START;
            }

            break;

        case SOFT_UART_TX_STATE_START:

            g_soft_uart_tx_phase++;

            /*
             * 起始位持续3次中断。
             */
            if (g_soft_uart_tx_phase >=
                BSP_SOFT_UART_SAMPLE_RATE)
            {
                g_soft_uart_tx_phase = 0U;

                /*
                 * UART低位先发，首先发送bit0。
                 */
                tx_bit =
                    (uint8_t)(
                        g_soft_uart_tx_current_byte &
                        0x01U);

                if (tx_bit != 0U)
                {
                    SOFT_UART_TX_HIGH();
                }
                else
                {
                    SOFT_UART_TX_LOW();
                }

                g_soft_uart_tx_bit_index = 1U;

                g_soft_uart_tx_state =
                    SOFT_UART_TX_STATE_DATA;
            }

            break;

        case SOFT_UART_TX_STATE_DATA:

            g_soft_uart_tx_phase++;

            if (g_soft_uart_tx_phase >=
                BSP_SOFT_UART_SAMPLE_RATE)
            {
                g_soft_uart_tx_phase = 0U;

                if (g_soft_uart_tx_bit_index < 8U)
                {
                    tx_bit =
                        (uint8_t)(
                            (g_soft_uart_tx_current_byte >>
                             g_soft_uart_tx_bit_index) &
                            0x01U);

                    if (tx_bit != 0U)
                    {
                        SOFT_UART_TX_HIGH();
                    }
                    else
                    {
                        SOFT_UART_TX_LOW();
                    }

                    g_soft_uart_tx_bit_index++;
                }
                else
                {
                    /*
                     * 8位数据发送完成，输出停止位。
                     */
                    SOFT_UART_TX_HIGH();

                    g_soft_uart_tx_state =
                        SOFT_UART_TX_STATE_STOP;
                }
            }

            break;

        case SOFT_UART_TX_STATE_STOP:

            g_soft_uart_tx_phase++;

            /*
             * 停止位必须保持完整的1bit时间。
             */
            if (g_soft_uart_tx_phase >=
                BSP_SOFT_UART_SAMPLE_RATE)
            {
                g_soft_uart_tx_phase = 0U;

                if (SoftUart_LoadNextTxByte() != 0U)
                {
                    /*
                     * 发送下一个字节。
                     * 停止位之后立即发送新的起始位。
                     */
                    SOFT_UART_TX_LOW();

                    g_soft_uart_tx_bit_index = 0U;

                    g_soft_uart_tx_state =
                        SOFT_UART_TX_STATE_START;
                }
                else
                {
                    SOFT_UART_TX_HIGH();

                    g_soft_uart_tx_state =
                        SOFT_UART_TX_STATE_IDLE;
                }
            }

            break;

        default:

            SOFT_UART_TX_HIGH();

            g_soft_uart_tx_state =
                SOFT_UART_TX_STATE_IDLE;

            g_soft_uart_tx_phase = 0U;

            break;
    }
}

/*==========================================================
 * 软件UART接收状态机
 *
 * 3倍采样：
 * 发现低电平后，下一次中断检查起始位；
 * 此后每3次中断采样一个数据位。
 *==========================================================*/

static void SoftUart_RxIRQHandler(void)
{
    uint8_t rx_level;

    if (g_soft_uart_rx_enabled == 0U)
    {
        return;
    }

    rx_level = SOFT_UART_RX_READ();

    switch (g_soft_uart_rx_state)
    {
        case SOFT_UART_RX_STATE_IDLE:

            /*
             * UART空闲状态为高电平。
             * 检测到低电平，可能出现起始位。
             */
            if (rx_level == 0U)
            {
                g_soft_uart_rx_phase = 0U;
                g_soft_uart_rx_bit_index = 0U;
                g_soft_uart_rx_current_byte = 0U;

                g_soft_uart_rx_state =
                    SOFT_UART_RX_STATE_START;
            }

            break;

        case SOFT_UART_RX_STATE_START:

            g_soft_uart_rx_phase++;

            /*
             * 下一采样点仍为低电平，
             * 认为起始位有效。
             */
            if (g_soft_uart_rx_phase >= 1U)
            {
                g_soft_uart_rx_phase = 0U;

                if (rx_level == 0U)
                {
                    g_soft_uart_rx_bit_index = 0U;
                    g_soft_uart_rx_current_byte = 0U;

                    g_soft_uart_rx_state =
                        SOFT_UART_RX_STATE_DATA;
                }
                else
                {
                    /*
                     * 低电平脉冲过短，属于干扰。
                     */
                    g_soft_uart_rx_state =
                        SOFT_UART_RX_STATE_IDLE;
                }
            }

            break;

        case SOFT_UART_RX_STATE_DATA:

            g_soft_uart_rx_phase++;

            if (g_soft_uart_rx_phase >=
                BSP_SOFT_UART_SAMPLE_RATE)
            {
                g_soft_uart_rx_phase = 0U;

                if (rx_level != 0U)
                {
                    g_soft_uart_rx_current_byte |=
                        (uint8_t)(
                            1U <<
                            g_soft_uart_rx_bit_index);
                }

                g_soft_uart_rx_bit_index++;

                if (g_soft_uart_rx_bit_index >= 8U)
                {
                    g_soft_uart_rx_state =
                        SOFT_UART_RX_STATE_STOP;
                }
            }

            break;

        case SOFT_UART_RX_STATE_STOP:

            g_soft_uart_rx_phase++;

            if (g_soft_uart_rx_phase >=
                BSP_SOFT_UART_SAMPLE_RATE)
            {
                g_soft_uart_rx_phase = 0U;

                /*
                 * 停止位应为高电平。
                 */
                if (rx_level != 0U)
                {
                    SoftUart_PushRxByte(
                        g_soft_uart_rx_current_byte);
                }

                /*
                 * 停止位为低时认为帧错误，
                 * 当前字节直接丢弃。
                 */
                g_soft_uart_rx_state =
                    SOFT_UART_RX_STATE_IDLE;
            }

            break;

        default:

            g_soft_uart_rx_state =
                SOFT_UART_RX_STATE_IDLE;

            g_soft_uart_rx_phase = 0U;

            break;
    }
}

/*==========================================================
 * 定时器中断入口
 *==========================================================*/

void BSP_SoftUart_TimerIRQHandler(void)
{
    SoftUart_TxIRQHandler();
    SoftUart_RxIRQHandler();
}