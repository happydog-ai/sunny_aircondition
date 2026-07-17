#include "r_cg_macrodriver.h"
#include "r_cg_serial.h"
#include "bsp_rs485.h"

/*
 * RS-485 hardware UART1
 *
 * P03/RXD1 <- 485_RO_2
 * P02/TXD1 -> 485_DI_2
 * P27      -> 485_EN_2
 *
 * P27 = 0：接收模式
 * P27 = 1：发送模式
 */

#define RS485_EN_PIN                     P2_bit.no7
#define RS485_EN_MODE                    PM2_bit.no7

#define RS485_RX_BUFFER_SIZE             (128U)
#define RS485_DEBUG_TX_MIRROR_SIZE       (64U)

/*
 * 收到请求后，在拉高DE之前等待上位机释放RS-485总线。
 *
 * 这是调试用粗略延时，目标约1～2ms。
 * 实际时间受CPU主频和编译优化影响。
 */
#define RS485_BUS_RELEASE_DELAY_COUNT    (12000U)

/*
 * DE拉高后，等待485发送器进入稳定状态。
 * 这个延时远短于上面的总线释放延时。
 */
#define RS485_DRIVER_ENABLE_DELAY_COUNT  (200U)

/*
 * DE拉低后，等待485接收器稳定。
 */
#define RS485_RECEIVER_ENABLE_DELAY_COUNT (50U)

#define RS485_ENTER_RX()                 \
    do                                   \
    {                                    \
        RS485_EN_PIN = 0U;               \
    } while (0)

#define RS485_ENTER_TX()                 \
    do                                   \
    {                                    \
        RS485_EN_PIN = 1U;               \
    } while (0)


/***********************************************************************************************************************
 * RS-485 internal variables
 ***********************************************************************************************************************/

static volatile uint8_t g_rs485_rx_buffer[RS485_RX_BUFFER_SIZE];
static volatile uint8_t g_rs485_rx_head;
static volatile uint8_t g_rs485_rx_tail;
static volatile uint8_t g_rs485_rx_byte;

static volatile uint8_t g_rs485_tx_done;
static volatile bsp_rs485_mode_t g_rs485_current_mode;


/***********************************************************************************************************************
 * Debug variables
 ***********************************************************************************************************************/

volatile uint16_t g_rs485_debug_send_call_count;
volatile uint16_t g_rs485_debug_send_length;
volatile uint16_t g_rs485_debug_send_status;
volatile uint8_t g_rs485_debug_send_result;

volatile uint8_t g_rs485_debug_tx_done_flag;
volatile uint8_t g_rs485_debug_mode;
volatile uint8_t g_rs485_debug_stage;
volatile uint16_t g_rs485_debug_send_done_count;

volatile uint8_t g_rs485_debug_tx_mirror[RS485_DEBUG_TX_MIRROR_SIZE];
volatile uint16_t g_rs485_debug_tx_mirror_length;

volatile uint8_t g_rs485_debug_en_before_send;
volatile uint8_t g_rs485_debug_en_after_send;

volatile uint16_t g_rs485_debug_uart_error_count;
volatile uint8_t g_rs485_debug_last_uart_error;
volatile uint16_t g_rs485_debug_rx_start_status;


/***********************************************************************************************************************
 * Internal function declarations
 ***********************************************************************************************************************/

static uint8_t RS485_GetNextIndex(uint8_t index);
static void RS485_PushRxByte(uint8_t data);
static void RS485_StartReceiveOneByte(void);

static void RS485_BusReleaseDelay(void);
static void RS485_DriverEnableDelay(void);
static void RS485_ReceiverEnableDelay(void);

static uint16_t RS485_GetLength(void);


/***********************************************************************************************************************
 * Function Name: RS485_GetNextIndex
 ***********************************************************************************************************************/

static uint8_t RS485_GetNextIndex(uint8_t index)
{
    index++;

    if (index >= RS485_RX_BUFFER_SIZE)
    {
        index = 0U;
    }

    return index;
}


/***********************************************************************************************************************
 * Function Name: RS485_PushRxByte
 ***********************************************************************************************************************/

static void RS485_PushRxByte(uint8_t data)
{
    uint8_t next_head;

    next_head = RS485_GetNextIndex(g_rs485_rx_head);

    /*
     * next_head等于tail表示环形缓冲区已满。
     * 缓冲区满时丢弃当前新字节，不覆盖旧数据。
     */
    if (next_head != g_rs485_rx_tail)
    {
        g_rs485_rx_buffer[g_rs485_rx_head] = data;
        g_rs485_rx_head = next_head;
    }
}


/***********************************************************************************************************************
 * Function Name: RS485_StartReceiveOneByte
 ***********************************************************************************************************************/

static void RS485_StartReceiveOneByte(void)
{
    MD_STATUS status;

    status = R_UART1_Receive(
        (uint8_t *)&g_rs485_rx_byte,
        1U);

    g_rs485_debug_rx_start_status = (uint16_t)status;
}


/***********************************************************************************************************************
 * Function Name: RS485_BusReleaseDelay
 * Description  : Wait before enabling this node's RS-485 transmitter.
 ***********************************************************************************************************************/

static void RS485_BusReleaseDelay(void)
{
    uint16_t count;

    /*
     * 必须在P27仍为0时执行。
     *
     * 作用：
     * 等待USB-RS485转换器结束发送并释放A/B总线。
     */
    for (count = 0U;
         count < RS485_BUS_RELEASE_DELAY_COUNT;
         count++)
    {
        NOP();
    }
}


/***********************************************************************************************************************
 * Function Name: RS485_DriverEnableDelay
 ***********************************************************************************************************************/

static void RS485_DriverEnableDelay(void)
{
    uint16_t count;

    /*
     * P27拉高以后，等待485发送驱动器稳定。
     */
    for (count = 0U;
         count < RS485_DRIVER_ENABLE_DELAY_COUNT;
         count++)
    {
        NOP();
    }
}


/***********************************************************************************************************************
 * Function Name: RS485_ReceiverEnableDelay
 ***********************************************************************************************************************/

static void RS485_ReceiverEnableDelay(void)
{
    uint16_t count;

    /*
     * P27拉低以后，等待485接收器稳定。
     */
    for (count = 0U;
         count < RS485_RECEIVER_ENABLE_DELAY_COUNT;
         count++)
    {
        NOP();
    }
}


/***********************************************************************************************************************
 * Function Name: RS485_GetLength
 ***********************************************************************************************************************/

static uint16_t RS485_GetLength(void)
{
    uint8_t head;
    uint8_t tail;

    head = g_rs485_rx_head;
    tail = g_rs485_rx_tail;

    if (head >= tail)
    {
        return (uint16_t)(head - tail);
    }

    return (uint16_t)(
        RS485_RX_BUFFER_SIZE - tail + head);
}


/***********************************************************************************************************************
 * Function Name: BSP_RS485_Init
 ***********************************************************************************************************************/

void BSP_RS485_Init(void)
{
    g_rs485_rx_head = 0U;
    g_rs485_rx_tail = 0U;
    g_rs485_rx_byte = 0U;

    g_rs485_tx_done = 1U;
    g_rs485_current_mode = BSP_RS485_MODE_RX;

    g_rs485_debug_send_call_count = 0U;
    g_rs485_debug_send_length = 0U;
    g_rs485_debug_send_status = 0U;
    g_rs485_debug_send_result = 0U;
    g_rs485_debug_tx_done_flag = 0U;
    g_rs485_debug_mode = 0U;
    g_rs485_debug_stage = 0U;
    g_rs485_debug_send_done_count = 0U;
    g_rs485_debug_tx_mirror_length = 0U;
    g_rs485_debug_en_before_send = 0U;
    g_rs485_debug_en_after_send = 0U;
    g_rs485_debug_uart_error_count = 0U;
    g_rs485_debug_last_uart_error = 0U;
    g_rs485_debug_rx_start_status = 0U;

    /*
     * P27设为数字I/O。
     */
    ADPC = 0x08U;

    /*
     * 先把输出锁存值设置为0，再设置成输出。
     * 可避免配置输出方向时出现瞬间高电平。
     */
    RS485_ENTER_RX();
    RS485_EN_MODE = 0U;

    /*
     * RXD1/P03使用TTL输入电平。
     */
    PIM0_bit.no3 = 1U;

    /*
     * 启动UART1发送和接收通道。
     */
    R_UART1_Start();

    /*
     * 进入接收模式并挂接第一次单字节接收。
     */
    BSP_RS485_SetMode(BSP_RS485_MODE_RX);
}


/***********************************************************************************************************************
 * Function Name: BSP_RS485_SetMode
 ***********************************************************************************************************************/

void BSP_RS485_SetMode(bsp_rs485_mode_t mode)
{
    if (mode == BSP_RS485_MODE_TX)
    {
        /*
         * 发送期间屏蔽UART1接收中断，避免收发切换期间
         * 产生伪接收中断。
         */
        SRMK1 = 1U;

        g_rs485_current_mode = BSP_RS485_MODE_TX;
        g_rs485_debug_mode = 1U;

        /*
         * 使能485发送驱动器，关闭接收器。
         */
        RS485_ENTER_TX();

        /*
         * 等待485驱动器稳定后再启动UART。
         */
        RS485_DriverEnableDelay();
    }
    else
    {
        /*
         * 恢复接收时先屏蔽接收中断。
         */
        SRMK1 = 1U;

        /*
         * 清除UART1接收错误。
         */
        SIR03 = _0004_SAU_SIRMN_FECTMN |
                _0002_SAU_SIRMN_PECTMN |
                _0001_SAU_SIRMN_OVCTMN;

        /*
         * 清除残留的接收中断请求。
         */
        SRIF1 = 0U;

        /*
         * 重新挂接下一字节接收。
         */
        RS485_StartReceiveOneByte();

        /*
         * 关闭485发送驱动器，打开485接收器。
         */
        RS485_ENTER_RX();
        RS485_ReceiverEnableDelay();

        g_rs485_current_mode = BSP_RS485_MODE_RX;
        g_rs485_debug_mode = 0U;

        /*
         * 最后再允许UART1接收中断。
         */
        SRIF1 = 0U;
        SRMK1 = 0U;
    }
}


/***********************************************************************************************************************
 * Function Name: BSP_RS485_SendByte
 ***********************************************************************************************************************/

uint8_t BSP_RS485_SendByte(uint8_t data)
{
    return BSP_RS485_Send(&data, 1U);
}


/***********************************************************************************************************************
 * Function Name: BSP_RS485_Send
 ***********************************************************************************************************************/

uint8_t BSP_RS485_Send(
    const uint8_t *data,
    uint16_t length)
{
    MD_STATUS status;
    uint16_t index;

    if ((data == (const uint8_t *)0) ||
        (length == 0U))
    {
        g_rs485_debug_stage = 0xE0U;
        g_rs485_debug_send_result = 0U;

        return 0U;
    }

    /*
     * 调试信息初始化。
     */
    g_rs485_debug_stage = 1U;
    g_rs485_debug_send_call_count++;
    g_rs485_debug_send_length = length;
    g_rs485_debug_send_result = 0U;
    g_rs485_debug_tx_done_flag = 0U;
    g_rs485_debug_tx_mirror_length = 0U;

    /*
     * 复制一份发送数据，方便在Watch窗口查看。
     */
    for (index = 0U;
         (index < length) &&
         (index < RS485_DEBUG_TX_MIRROR_SIZE);
         index++)
    {
        g_rs485_debug_tx_mirror[index] = data[index];
    }

    g_rs485_debug_tx_mirror_length = index;

    /*
     * 重点：
     * 此时P27仍然是0，板子不驱动A/B总线。
     * 等待上位机USB-RS485转换器释放总线。
     */
    RS485_BusReleaseDelay();

    /*
     * 切换到发送模式。
     */
    BSP_RS485_SetMode(BSP_RS485_MODE_TX);

    g_rs485_debug_stage = 2U;
    g_rs485_debug_en_before_send = RS485_EN_PIN;

    /*
     * 必须在调用R_UART1_Send之前清零完成标志。
     */
    g_rs485_tx_done = 0U;

    /*
     * 整帧一次性发送。
     */
    status = R_UART1_Send(
        (uint8_t *)data,
        length);

    g_rs485_debug_send_status = (uint16_t)status;
    g_rs485_debug_stage = 3U;

    if (status != MD_OK)
    {
        BSP_RS485_SetMode(BSP_RS485_MODE_RX);

        g_rs485_debug_en_after_send = RS485_EN_PIN;
        g_rs485_debug_stage = 0xE1U;
        g_rs485_debug_send_result = 0U;

        return 0U;
    }

    /*
     * 等待UART1发送完成回调。
     *
     * 如果程序一直卡在这里：
     * 检查r_uart1_callback_sendend()是否调用了
     * BSP_RS485_UART1SendEndCallback()。
     */
    while (g_rs485_tx_done == 0U)
    {
        NOP();
    }

    g_rs485_debug_stage = 4U;

    /*
     * 发送完成回调触发时，最后一个停止位可能仍未完全发完。
     * 继续等待SAU0 Channel 2停止执行。
     */
    while ((SSR02 & _0040_SAU_UNDER_EXECUTE) != 0U)
    {
        NOP();
    }

    g_rs485_debug_stage = 5U;

    /*
     * 最后停止位完成后立即释放RS-485总线。
     *
     * 这里不要再执行长时间PostTransmitDelay，
     * 否则P27会持续为1，延迟上位机下一次发送。
     */
    BSP_RS485_SetMode(BSP_RS485_MODE_RX);

    g_rs485_debug_en_after_send = RS485_EN_PIN;
    g_rs485_debug_stage = 6U;

    g_rs485_debug_send_result = 1U;
    g_rs485_debug_stage = 7U;

    return 1U;
}


/***********************************************************************************************************************
 * Function Name: BSP_RS485_SendString
 ***********************************************************************************************************************/

uint8_t BSP_RS485_SendString(const char *string)
{
    uint16_t length;

    if (string == (const char *)0)
    {
        return 0U;
    }

    length = 0U;

    while (string[length] != '\0')
    {
        length++;
    }

    if (length == 0U)
    {
        return 0U;
    }
  
    return BSP_RS485_Send(
        (const uint8_t *)string,
        length);
}


/***********************************************************************************************************************
 * Function Name: BSP_RS485_Available
 ***********************************************************************************************************************/

uint16_t BSP_RS485_Available(void)
{
    return RS485_GetLength();
}


/***********************************************************************************************************************
 * Function Name: BSP_RS485_Read
 ***********************************************************************************************************************/

uint16_t BSP_RS485_Read(
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
           (g_rs485_rx_tail != g_rs485_rx_head))
    {
        buffer[read_length] =
            g_rs485_rx_buffer[g_rs485_rx_tail];

        g_rs485_rx_tail =
            RS485_GetNextIndex(g_rs485_rx_tail);

        read_length++;
    }

    return read_length;
}


/***********************************************************************************************************************
 * Function Name: BSP_RS485_ClearRx
 ***********************************************************************************************************************/

void BSP_RS485_ClearRx(void)
{
    /*
     * 避免中断与主程序同时修改head和tail。
     */
    SRMK1 = 1U;

    g_rs485_rx_head = 0U;
    g_rs485_rx_tail = 0U;

    SRIF1 = 0U;

    if (g_rs485_current_mode == BSP_RS485_MODE_RX)
    {
        SRMK1 = 0U;
    }
}


/***********************************************************************************************************************
 * Function Name: BSP_RS485_UART1ReceiveCallback
 ***********************************************************************************************************************/

void BSP_RS485_UART1ReceiveCallback(void)
{
    if (g_rs485_current_mode == BSP_RS485_MODE_RX)
    {
        RS485_PushRxByte(g_rs485_rx_byte);

        /*
         * 接收完当前字节后，立即挂接下一个字节。
         */
        RS485_StartReceiveOneByte();
    }
}


/***********************************************************************************************************************
 * Function Name: BSP_RS485_UART1SoftwareOverrunCallback
 ***********************************************************************************************************************/

void BSP_RS485_UART1SoftwareOverrunCallback(uint8_t data)
{
    if (g_rs485_current_mode == BSP_RS485_MODE_RX)
    {
        RS485_PushRxByte(data);
        RS485_StartReceiveOneByte();
    }
}


/***********************************************************************************************************************
 * Function Name: BSP_RS485_UART1SendEndCallback
 ***********************************************************************************************************************/

void BSP_RS485_UART1SendEndCallback(void)
{
    g_rs485_tx_done = 1U;

    g_rs485_debug_tx_done_flag = 1U;
    g_rs485_debug_send_done_count++;
}


/***********************************************************************************************************************
 * Function Name: BSP_RS485_UART1ErrorCallback
 ***********************************************************************************************************************/

void BSP_RS485_UART1ErrorCallback(uint8_t err_type)
{
    g_rs485_debug_last_uart_error = err_type;

    if (g_rs485_debug_uart_error_count < 0xFFFFU)
    {
        g_rs485_debug_uart_error_count++;
    }

    if (g_rs485_current_mode == BSP_RS485_MODE_RX)
    {
        RS485_StartReceiveOneByte();
    }
}