#include "r_cg_macrodriver.h"
#include "bsp_rs485.h"
#include "protocol.h"

#define PROTOCOL_HEAD_1             (0xAAU)
#define PROTOCOL_HEAD_2             (0x55U)

#define PROTOCOL_CTRL_REQUEST       (0x00U)
#define PROTOCOL_CTRL_RESPONSE      (0x80U)
#define PROTOCOL_CTRL_ERROR         (0xC0U)

#define PROTOCOL_CMD_PING           (0x01U)
#define PROTOCOL_CMD_GET_VERSION    (0x02U)
#define PROTOCOL_CMD_ECHO           (0x10U)
#define PROTOCOL_CMD_LED_SET        (0x20U)
#define PROTOCOL_CMD_GET_STATUS     (0x21U)

#define PROTOCOL_ERR_UNKNOWN_CMD    (0x01U)
#define PROTOCOL_ERR_LENGTH         (0x02U)
#define PROTOCOL_ERR_DATA           (0x03U)

#define PROTOCOL_TIMEOUT_MS         (50U)
#define PROTOCOL_BODY_MAX_LENGTH    (PROTOCOL_FRAME_MAX_LENGTH - 2U)

typedef enum
{
    PROTOCOL_RX_WAIT_AA = 0,
    PROTOCOL_RX_WAIT_55,
    PROTOCOL_RX_BODY
} protocol_rx_state_t;

static protocol_rx_state_t g_protocol_rx_state;
static uint8_t g_protocol_rx_body[PROTOCOL_BODY_MAX_LENGTH];
static uint8_t g_protocol_rx_index;
static uint8_t g_protocol_rx_expected;
static volatile uint8_t g_protocol_inter_byte_timeout;

static void Protocol_ResetRx(void);
static void Protocol_InputByte(uint8_t data);
static void Protocol_HandleFrame(void);
static void Protocol_SendNormalResponse(
    uint8_t addr,
    uint8_t seq,
    uint8_t cmd,
    const uint8_t *data,
    uint8_t data_length);
static void Protocol_SendErrorResponse(
    uint8_t addr,
    uint8_t seq,
    uint8_t cmd,
    uint8_t error_code);
static void Protocol_SendResponse(
    uint8_t addr,
    uint8_t seq,
    uint8_t cmd,
    uint8_t ctrl,
    const uint8_t *data,
    uint8_t data_length);
static uint16_t Protocol_Crc16Modbus(
    const uint8_t *data,
    uint8_t length);
static uint8_t Protocol_GetLedStatus(void);
static void Protocol_SetLedStatus(uint8_t status);
volatile uint8_t global_tx_buffer[PROTOCOL_FRAME_MAX_LENGTH];
volatile uint8_t global_tx_length;
void Protocol_Init(void)
{
    Protocol_ResetRx();
}

void Protocol_Task(void)
{
    uint8_t data;

    if ((g_protocol_rx_state != PROTOCOL_RX_WAIT_AA) &&
        (g_protocol_inter_byte_timeout == 0U))
    {
        Protocol_ResetRx();
    }
    // ?????????

    while (BSP_RS485_Read(&data, 1U) == 1U)
    {
        Protocol_InputByte(data);
    }
}

void Protocol_TimerTick1ms(void)
{
    if (g_protocol_inter_byte_timeout > 0U)
    {
        g_protocol_inter_byte_timeout--;
    }
}

static void Protocol_ResetRx(void)
{
    g_protocol_rx_state = PROTOCOL_RX_WAIT_AA;
    g_protocol_rx_index = 0U;
    g_protocol_rx_expected = 0U;
    g_protocol_inter_byte_timeout = 0U;
}

static void Protocol_InputByte(uint8_t data)
{
    if (g_protocol_rx_state != PROTOCOL_RX_WAIT_AA)
    {
        g_protocol_inter_byte_timeout = PROTOCOL_TIMEOUT_MS;
    }

    switch (g_protocol_rx_state)
    {
        case PROTOCOL_RX_WAIT_AA:
            if (data == PROTOCOL_HEAD_1)
            {
                g_protocol_rx_state = PROTOCOL_RX_WAIT_55;
                g_protocol_inter_byte_timeout = PROTOCOL_TIMEOUT_MS;
            }
            break;

        case PROTOCOL_RX_WAIT_55:
            if (data == PROTOCOL_HEAD_2)
            {
                g_protocol_rx_state = PROTOCOL_RX_BODY;
                g_protocol_rx_index = 0U;
                g_protocol_rx_expected = 0U;
            }
            else if (data == PROTOCOL_HEAD_1)
            {
                g_protocol_rx_state = PROTOCOL_RX_WAIT_55;
            }
            else
            {
                Protocol_ResetRx();
            }
            break;

        case PROTOCOL_RX_BODY:
            if (g_protocol_rx_index >= PROTOCOL_BODY_MAX_LENGTH)
            {
                Protocol_ResetRx();
                break;
            }

            g_protocol_rx_body[g_protocol_rx_index] = data;
            g_protocol_rx_index++;

            if (g_protocol_rx_index == 1U)
            {
                if ((data < PROTOCOL_FRAME_MIN_LENGTH) ||
                    (data > PROTOCOL_FRAME_MAX_LENGTH))
                {
                    Protocol_ResetRx();
                    break;
                }

                g_protocol_rx_expected = (uint8_t)(data - 2U);
            }

            if ((g_protocol_rx_expected != 0U) &&
                (g_protocol_rx_index >= g_protocol_rx_expected))
            {
                Protocol_HandleFrame();
                Protocol_ResetRx();
            }
            break;

        default:
            Protocol_ResetRx();
            break;
    }
}

static void Protocol_HandleFrame(void)
{
    uint8_t frame_length;
    uint8_t addr;
    uint8_t seq;
    uint8_t cmd;
    uint8_t ctrl;
    uint8_t data_length;
    uint8_t *payload;
    uint16_t crc_calc;
    uint16_t crc_recv;
    uint8_t version_data[3];
    uint8_t status_data[1];

    frame_length = g_protocol_rx_body[0];
    addr = g_protocol_rx_body[1];
    seq = g_protocol_rx_body[2];
    cmd = g_protocol_rx_body[3];
    ctrl = g_protocol_rx_body[4];
    data_length = (uint8_t)(frame_length - PROTOCOL_FRAME_MIN_LENGTH);
    payload = &g_protocol_rx_body[5];

    crc_calc = Protocol_Crc16Modbus(
        g_protocol_rx_body,
        (uint8_t)(g_protocol_rx_expected - 2U));

    crc_recv = (uint16_t)g_protocol_rx_body[g_protocol_rx_expected - 2U];
    crc_recv |= (uint16_t)((uint16_t)g_protocol_rx_body[g_protocol_rx_expected - 1U] << 8U);

    if ((addr != PROTOCOL_DEVICE_ADDR) || (crc_calc != crc_recv))
    {
        return;
    }

    if (ctrl != PROTOCOL_CTRL_REQUEST)
    {
        Protocol_SendErrorResponse(addr, seq, cmd, PROTOCOL_ERR_DATA);
        return;
    }

    switch (cmd)
    {
        case PROTOCOL_CMD_PING:
            if (data_length != 0U)
            {
                Protocol_SendErrorResponse(addr, seq, cmd, PROTOCOL_ERR_LENGTH);
            }
            else
            {
                Protocol_SendNormalResponse(addr, seq, cmd, (const uint8_t *)0, 0U);
            }
            break;

        case PROTOCOL_CMD_GET_VERSION:
            if (data_length != 0U)
            {
                Protocol_SendErrorResponse(addr, seq, cmd, PROTOCOL_ERR_LENGTH);
            }
            else
            {
                version_data[0] = (uint8_t)'1';
                version_data[1] = (uint8_t)'.';
                version_data[2] = (uint8_t)'0';
                Protocol_SendNormalResponse(addr, seq, cmd, version_data, 3U);
            }
            break;

        case PROTOCOL_CMD_ECHO:
            Protocol_SendNormalResponse(addr, seq, cmd, payload, data_length);
            break;

        case PROTOCOL_CMD_LED_SET:
            if (data_length != 1U)
            {
                Protocol_SendErrorResponse(addr, seq, cmd, PROTOCOL_ERR_LENGTH);
            }
            else if ((payload[0] != 0U) && (payload[0] != 1U))
            {
                Protocol_SendErrorResponse(addr, seq, cmd, PROTOCOL_ERR_DATA);
            }
            else
            {
                Protocol_SetLedStatus(payload[0]);
                status_data[0] = Protocol_GetLedStatus();
                Protocol_SendNormalResponse(addr, seq, cmd, status_data, 1U);
            }
            break;

        case PROTOCOL_CMD_GET_STATUS:
            if (data_length != 0U)
            {
                Protocol_SendErrorResponse(addr, seq, cmd, PROTOCOL_ERR_LENGTH);
            }
            else
            {
                status_data[0] = Protocol_GetLedStatus();
                Protocol_SendNormalResponse(addr, seq, cmd, status_data, 1U);
            }
            break;

        default:
            Protocol_SendErrorResponse(addr, seq, cmd, PROTOCOL_ERR_UNKNOWN_CMD);
            break;
    }
}

static void Protocol_SendNormalResponse(
    uint8_t addr,
    uint8_t seq,
    uint8_t cmd,
    const uint8_t *data,
    uint8_t data_length)
{
    Protocol_SendResponse(
        addr,
        seq,
        cmd,
        PROTOCOL_CTRL_RESPONSE,
        data,
        data_length);
}

static void Protocol_SendErrorResponse(
    uint8_t addr,
    uint8_t seq,
    uint8_t cmd,
    uint8_t error_code)
{
    uint8_t data[1];

    data[0] = error_code;

    Protocol_SendResponse(
        addr,
        seq,
        cmd,
        PROTOCOL_CTRL_ERROR,
        data,
        1U);
}

static void Protocol_SendResponse(
    uint8_t addr,
    uint8_t seq,
    uint8_t cmd,
    uint8_t ctrl,
    const uint8_t *data,
    uint8_t data_length)
{
    uint8_t tx_buffer[PROTOCOL_FRAME_MAX_LENGTH];
    uint8_t index;
    uint8_t frame_length;
    uint16_t crc;

    if (data_length > PROTOCOL_DATA_MAX_LENGTH)
    {
        data_length = PROTOCOL_DATA_MAX_LENGTH;
    }

    frame_length = (uint8_t)(PROTOCOL_FRAME_MIN_LENGTH + data_length);

    tx_buffer[0] = PROTOCOL_HEAD_1;
    tx_buffer[1] = PROTOCOL_HEAD_2;
    tx_buffer[2] = frame_length;
    tx_buffer[3] = addr;
    tx_buffer[4] = seq;
    tx_buffer[5] = cmd;
    tx_buffer[6] = ctrl;
    for (index = 0U; index < data_length; index++)
    {
        if (data != (const uint8_t *)0)
        {
            tx_buffer[7U + index] = data[index];
        }
        else
        {
            tx_buffer[7U + index] = 0U;
        }
    }

    crc = Protocol_Crc16Modbus(
        &tx_buffer[2],
        (uint8_t)(5U + data_length));

    tx_buffer[7U + data_length] = (uint8_t)(crc & 0x00FFU);
    tx_buffer[8U + data_length] = (uint8_t)((crc >> 8U) & 0x00FFU);

    global_tx_length = frame_length;

    for (index = 0U; index < frame_length; index++)
    {
        global_tx_buffer[index] = tx_buffer[index];
    }

    (void)BSP_RS485_Send(tx_buffer, frame_length);
}

static uint16_t Protocol_Crc16Modbus(
    const uint8_t *data,
    uint8_t length)
{
    uint16_t crc;
    uint8_t index;
    uint8_t bit_index;

    crc = 0xFFFFU;

    for (index = 0U; index < length; index++)
    {
        crc ^= (uint16_t)data[index];

        for (bit_index = 0U; bit_index < 8U; bit_index++)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = (uint16_t)((crc >> 1U) ^ 0xA001U);
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

static uint8_t Protocol_GetLedStatus(void)
{
    return (uint8_t)(P7_bit.no7 & 0x01U);
}

static void Protocol_SetLedStatus(uint8_t status)
{
    if (status == 0U)
    {
        P7_bit.no7 = 0U;
    }
    else
    {
        P7_bit.no7 = 1U;
    }
}


