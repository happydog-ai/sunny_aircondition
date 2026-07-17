#include "r_cg_macrodriver.h"
#include "r_cg_serial.h"
#include "bsp_rs485.h"

/*
 * RS-485 hardware UART1
 * P03/RXD1 -> 485_RO_2
 * P02/TXD1 -> 485_DI_2
 * P27      -> 485_EN_2
 */

#define RS485_EN_PIN          P2_bit.no7
#define RS485_EN_MODE         PM2_bit.no7

#define RS485_RX_BUFFER_SIZE  (128U)

#define RS485_ENTER_RX()      \
    do                        \
    {                         \
        RS485_EN_PIN = 0U;    \
    } while (0)

#define RS485_ENTER_TX()      \
    do                        \
    {                         \
        RS485_EN_PIN = 1U;    \
    } while (0)

static volatile uint8_t g_rs485_rx_buffer[RS485_RX_BUFFER_SIZE];
static volatile uint8_t g_rs485_rx_head;
static volatile uint8_t g_rs485_rx_tail;
static volatile uint8_t g_rs485_rx_byte;
static volatile uint8_t g_rs485_tx_done;

static uint8_t RS485_GetNextIndex(uint8_t index);
static void RS485_PushRxByte(uint8_t data);
static void RS485_StartReceiveOneByte(void);
static void RS485_TurnaroundDelay(void);
static uint16_t RS485_GetLength(void);

static uint8_t RS485_GetNextIndex(uint8_t index)
{
    index++;

    if (index >= RS485_RX_BUFFER_SIZE)
    {
        index = 0U;
    }

    return index;
}

static void RS485_PushRxByte(uint8_t data)
{
    uint8_t next_head;

    next_head = RS485_GetNextIndex(g_rs485_rx_head);

    if (next_head != g_rs485_rx_tail)
    {
        g_rs485_rx_buffer[g_rs485_rx_head] = data;
        g_rs485_rx_head = next_head;
    }
}

static void RS485_StartReceiveOneByte(void)
{
    (void)R_UART1_Receive((uint8_t *)&g_rs485_rx_byte, 1U);
}

static void RS485_TurnaroundDelay(void)
{
    uint16_t count;

    for (count = 0U; count < 200U; count++)
    {
        NOP();
    }
}

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

    return (uint16_t)(RS485_RX_BUFFER_SIZE - tail + head);
}

void BSP_RS485_Init(void)
{
    g_rs485_rx_head = 0U;
    g_rs485_rx_tail = 0U;
    g_rs485_tx_done = 1U;

    /* P27 must be digital I/O before it controls DE and /RE. */
    ADPC = 0x08U;

    RS485_ENTER_RX();
    RS485_EN_MODE = 0U;

    /* RXD1/P03 should use TTL input level for UART reception. */
    PIM0_bit.no3 = 1U;

    R_UART1_Start();
    RS485_StartReceiveOneByte();
}

void BSP_RS485_SetMode(bsp_rs485_mode_t mode)
{
    if (mode == BSP_RS485_MODE_TX)
    {
        RS485_ENTER_TX();
        RS485_TurnaroundDelay();
    }
    else
    {
        RS485_ENTER_RX();
        SIR03 = _0004_SAU_SIRMN_FECTMN |
                _0002_SAU_SIRMN_PECTMN |
                _0001_SAU_SIRMN_OVCTMN;
        SRIF1 = 0U;
        RS485_StartReceiveOneByte();
    }
}

static uint8_t RS485_SendByteInTxMode(uint8_t data)
{
    MD_STATUS status;

    g_rs485_tx_done = 0U;
    status = R_UART1_Send(&data, 1U);

    if (status != MD_OK)
    {
        return 0U;
    }

    while (g_rs485_tx_done == 0U)
    {
    }

    while ((SSR02 & _0040_SAU_UNDER_EXECUTE) != 0U)
    {
    }

    return 1U;
}
uint8_t BSP_RS485_SendByte(uint8_t data)
{
    return BSP_RS485_Send(&data, 1U);
}

// uint8_t BSP_RS485_SendByte(uint8_t data)
// {
//     uint8_t status;

//     BSP_RS485_SetMode(BSP_RS485_MODE_TX);
//     status = RS485_SendByteInTxMode(data);
//     BSP_RS485_SetMode(BSP_RS485_MODE_RX);

//     return status;
// }

// uint8_t BSP_RS485_Send(const uint8_t *data, uint16_t length)
// {
//     uint16_t index;

//     if ((data == (const uint8_t *)0) || (length == 0U))
//     {
//         return 0U;
//     }

//     BSP_RS485_SetMode(BSP_RS485_MODE_TX);

//     for (index = 0U; index < length; index++)
//     {
//         if (RS485_SendByteInTxMode(data[index]) == 0U)
//         {
//             BSP_RS485_SetMode(BSP_RS485_MODE_RX);
//             return 0U;
//         }
//     }

//     BSP_RS485_SetMode(BSP_RS485_MODE_RX);

//     return 1U;
// }

uint8_t BSP_RS485_Send(const uint8_t *data, uint16_t length)
{
    MD_STATUS status;

    if ((data == (const uint8_t *)0) || (length == 0U))
    {
        return 0U;
    }

    BSP_RS485_SetMode(BSP_RS485_MODE_TX);

    g_rs485_tx_done = 0U;

    /*
     * 整个缓冲区一次发送，不要逐字节调用R_UART1_Send。
     */
    status = R_UART1_Send((uint8_t *)data, length);

    if (status != MD_OK)
    {
        BSP_RS485_SetMode(BSP_RS485_MODE_RX);
        return 0U;
    }

    /*
     * 等待发送完成回调。
     */
    while (g_rs485_tx_done == 0U)
    {
    }

    /*
     * 回调触发时，最后一字节可能还在移位寄存器中。
     * 等待停止位真正发送完成。
     */
    while ((SSR02 & _0040_SAU_UNDER_EXECUTE) != 0U)
    {
    }

    BSP_RS485_SetMode(BSP_RS485_MODE_RX);

    return 1U;
}

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

    return BSP_RS485_Send((const uint8_t *)string, length);
}

uint16_t BSP_RS485_Available(void)
{
    return RS485_GetLength();
}

uint16_t BSP_RS485_Read(uint8_t *buffer, uint16_t max_length)
{
    uint16_t read_length;

    if ((buffer == (uint8_t *)0) || (max_length == 0U))
    {
        return 0U;
    }

    read_length = 0U;

    while ((read_length < max_length) &&
           (g_rs485_rx_tail != g_rs485_rx_head))
    {
        buffer[read_length] = g_rs485_rx_buffer[g_rs485_rx_tail];
        g_rs485_rx_tail = RS485_GetNextIndex(g_rs485_rx_tail);
        read_length++;
    }

    return read_length;
}

void BSP_RS485_ClearRx(void)
{
    g_rs485_rx_head = 0U;
    g_rs485_rx_tail = 0U;
}

void BSP_RS485_UART1ReceiveCallback(void)
{
    RS485_PushRxByte(g_rs485_rx_byte);
    RS485_StartReceiveOneByte();
}

void BSP_RS485_UART1SoftwareOverrunCallback(uint8_t data)
{
    RS485_PushRxByte(data);
    RS485_StartReceiveOneByte();
}

void BSP_RS485_UART1SendEndCallback(void)
{
    g_rs485_tx_done = 1U;
}

void BSP_RS485_UART1ErrorCallback(uint8_t err_type)
{
    (void)err_type;
    RS485_StartReceiveOneByte();
}
