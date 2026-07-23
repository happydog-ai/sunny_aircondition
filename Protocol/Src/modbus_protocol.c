#include "r_cg_macrodriver.h"
#include "bsp_rs485.h"
#include "modbus_protocol.h"
#include "app_config.h"
#include "switch_input.h"
#include "high_pressure_protection.h"
#include "four_way_valve.h"
#include "driver_board_comm.h"
#include "temperature_sensor.h"
#include "eev_control.h"

/*
 * Modbus RTU version.
 *
 * This module is intentionally independent from protocol.c, which implements
 * the existing AA55 protocol.  It is compiled as an alternative protocol layer
 * and is not called by r_main.c unless the application is switched to it.
 *
 * Supported functions:
 *   0x01 Read Coils
 *        coil 0: P77 LED status
 *
 *   0x03 Read Holding Registers
 *        register 0x0000: version major = 1
 *        register 0x0001: version minor = 0
 *        register 0x0002: LED status, 0 or 1
 *
 *   0x05 Write Single Coil
 *        coil 0: P77 LED, value 0xFF00 = on, 0x0000 = off
 *
 *   0x06 Write Single Register
 *        register 0x0002: P77 LED status, value 0 or 1
 *
 *   0x08 Diagnostics
 *        subfunction 0x0000: return query data, used as a Modbus-style echo.
 */

#define MODBUS_FRAME_TIMEOUT_MS         (5U)
#define MODBUS_CRC_LENGTH               (2U)
#define MODBUS_MIN_FRAME_LENGTH         (4U)

#define MODBUS_FUNC_READ_COILS          (0x01U)
#define MODBUS_FUNC_READ_DISCRETE_INPUTS (0x02U)
#define MODBUS_FUNC_READ_HOLDING_REGS   (0x03U)
#define MODBUS_FUNC_READ_INPUT_REGS     (0x04U)
#define MODBUS_FUNC_WRITE_SINGLE_COIL   (0x05U)
#define MODBUS_FUNC_WRITE_SINGLE_REG    (0x06U)
#define MODBUS_FUNC_DIAGNOSTICS         (0x08U)
#define MODBUS_FUNC_WRITE_MULTIPLE_REGS (0x10U)

#define MODBUS_EX_ILLEGAL_FUNCTION      (0x01U)
#define MODBUS_EX_ILLEGAL_DATA_ADDR     (0x02U)
#define MODBUS_EX_ILLEGAL_DATA_VALUE    (0x03U)

#define MODBUS_COIL_LED                 (0x0000U)
#define MODBUS_COIL_SYSTEM_ENABLE       (0x0000U)
#define MODBUS_COIL_FAN_ENABLE          (0x0002U)
#define MODBUS_COIL_FAULT_CLEAR         (0x0005U)
#define MODBUS_COIL_EEPROM_SAVE         (0x0006U)
#define MODBUS_COIL_COMPRESSOR_DEBUG    (0x0007U)
#define MODBUS_COIL_FOUR_WAY_COOL       (0x0008U)
#define MODBUS_COIL_FOUR_WAY_HEAT       (0x0009U)
#define MODBUS_COIL_MAX                 (0x0009U)

#define MODBUS_DI_K_STABLE_BASE         (0x0000U)
#define MODBUS_DI_K_RAW_BASE            (0x000AU)
#define MODBUS_DI_HP_SWITCH_CLOSED      (0x0014U)
#define MODBUS_DI_HP_FAULT_ACTIVE       (0x0015U)
#define MODBUS_DI_HP_LOCKED             (0x0016U)
#define MODBUS_DI_COMPRESSOR_RUNNING    (0x0017U)
#define MODBUS_DI_MAX                   (0x0017U)

#define MODBUS_IR_HP_FAULT_CODE         (0x0050U)
#define MODBUS_IR_HP_LOCKED             (0x0051U)
#define MODBUS_IR_HP_FAULT_COUNT        (0x0052U)
#define MODBUS_IR_FOUR_WAY_MODE         (0x0053U)
#define MODBUS_IR_FOUR_WAY_STATE        (0x0054U)
#define MODBUS_IR_HP_SWITCH_CLOSED      (0x0055U)
#define MODBUS_IR_HP_FAULT_ACTIVE       (0x0056U)
#define MODBUS_IR_HP_REMAIN_SECONDS     (0x0057U)
#define MODBUS_IR_COMPRESSOR_RUNNING    (0x0058U)
#define MODBUS_IR_COMPRESSOR_FREQ_HZ    (0x0059U)
#define MODBUS_IR_FOUR_WAY_REQUEST_MODE (0x005AU)
#define MODBUS_IR_FOUR_WAY_REMAIN_SEC   (0x005BU)
#define MODBUS_IR_HP_FAULT_ELAPSED_SEC  (0x005CU)
#define MODBUS_IR_COMP_RUN_ELAPSED_SEC  (0x005DU)
#define MODBUS_IR_TH1_RAW_AVG           (0x0060U)
#define MODBUS_IR_TH1_VOLTAGE_MV        (0x0061U)
#define MODBUS_IR_TH1_RESISTANCE_LO     (0x0062U)
#define MODBUS_IR_TH1_RESISTANCE_HI     (0x0063U)
#define MODBUS_IR_TH1_TEMP_0P1C         (0x0064U)
#define MODBUS_IR_TH1_STATUS            (0x0065U)
#define MODBUS_IR_EEV_CURRENT_STEP      (0x0070U)
#define MODBUS_IR_EEV_BUSY              (0x0071U)
#define MODBUS_IR_EEV_TARGET_STEP       (0x0072U)
#define MODBUS_IR_EEV_INTERVAL_MS       (0x0073U)
#define MODBUS_IR_MAX                   (0x0073U)

#define MODBUS_CONFIG_REG_SYSTEM_ENABLE (0x0000U)
#define MODBUS_HR_EEV_COMMAND           (0x0028U)
#define MODBUS_HR_EEV_STEP_COUNT        (0x0029U)
#define MODBUS_HR_EEV_TARGET_STEP       (0x002AU)
#define MODBUS_HR_EEV_INTERVAL_MS       (0x002BU)

#define MODBUS_EEV_CMD_NONE             (0U)
#define MODBUS_EEV_CMD_OPEN_STEPS       (1U)
#define MODBUS_EEV_CMD_CLOSE_STEPS      (2U)
#define MODBUS_EEV_CMD_STOP             (3U)
#define MODBUS_EEV_CMD_MOVE_TO          (4U)
#define MODBUS_EEV_CMD_HOLD_PHASE_BASE  (10U)
#define MODBUS_EEV_CMD_ALL_OFF          (18U)
#define MODBUS_EEV_DEFAULT_STEPS        (64U)

#define MODBUS_DIAG_RETURN_QUERY_DATA   (0x0000U)

static uint8_t g_modbus_rx_frame[MODBUS_RX_BUFFER_SIZE];
static uint16_t g_modbus_rx_length;
static uint16_t g_modbus_rx_expected_length;
static volatile uint8_t g_modbus_frame_timeout;
static uint8_t g_modbus_coils;
static uint16_t g_modbus_eev_step_count;
static uint16_t g_modbus_eev_target_step;

volatile uint8_t g_modbus_debug_rx_frame[MODBUS_RX_BUFFER_SIZE];
volatile uint16_t g_modbus_debug_rx_length;
volatile uint8_t g_modbus_debug_tx_frame[MODBUS_TX_BUFFER_SIZE];
volatile uint16_t g_modbus_debug_tx_length;
volatile uint8_t g_modbus_debug_last_function;
volatile uint8_t g_modbus_debug_last_exception;
volatile uint8_t g_modbus_debug_last_result;

static void ModbusProtocol_ResetRx(void);
static uint16_t ModbusProtocol_GetExpectedLength(void);
static void ModbusProtocol_ProcessFrame(void);
static void ModbusProtocol_SendFrame(uint8_t *frame, uint16_t length_without_crc);
static void ModbusProtocol_SendException(uint8_t function, uint8_t exception);
static void ModbusProtocol_HandleReadCoils(void);
static void ModbusProtocol_HandleReadDiscreteInputs(void);
static void ModbusProtocol_HandleReadHoldingRegisters(void);
static void ModbusProtocol_HandleReadInputRegisters(void);
static void ModbusProtocol_SendInputRegisterResponse(uint16_t start_address, uint16_t quantity);
static void ModbusProtocol_HandleWriteSingleCoil(void);
static void ModbusProtocol_HandleWriteSingleRegister(void);
static void ModbusProtocol_HandleWriteMultipleRegisters(void);
static void ModbusProtocol_HandleDiagnostics(void);
static uint16_t ModbusProtocol_Crc16(const uint8_t *data, uint16_t length);
static uint16_t ModbusProtocol_ReadU16(uint16_t offset);
static void ModbusProtocol_WriteU16(uint8_t *buffer, uint16_t offset, uint16_t value);
static uint16_t ModbusProtocol_GetHoldingRegister(uint16_t address);
static uint16_t ModbusProtocol_GetInputRegister(uint16_t address);
static uint8_t ModbusProtocol_IsEevHoldingRegister(uint16_t address);
static uint8_t ModbusProtocol_WriteEevHoldingRegister(uint16_t address, uint16_t value);
static uint8_t ModbusProtocol_GetDiscreteInput(uint16_t address);
static uint8_t ModbusProtocol_GetLedStatus(void);
static void ModbusProtocol_SetLedStatus(uint8_t status);
static void ModbusProtocol_ApplyLedOutput(uint8_t logical_on);
static uint8_t ModbusProtocol_GetCoil(uint16_t address);
static uint8_t ModbusProtocol_SetCoil(uint16_t address, uint8_t status);
static void ModbusProtocol_SaveRxDebug(void);
static void ModbusProtocol_SaveTxDebug(const uint8_t *frame, uint16_t length);

void ModbusProtocol_Init(void)
{
    g_modbus_coils = 0U;
    if (AppConfig_GetRegister(MODBUS_CONFIG_REG_SYSTEM_ENABLE) != 0U)
    {
        g_modbus_coils |= (uint8_t)(1U << MODBUS_COIL_SYSTEM_ENABLE);
    }
    ModbusProtocol_ApplyLedOutput(ModbusProtocol_GetLedStatus());
    g_modbus_eev_step_count = MODBUS_EEV_DEFAULT_STEPS;
    g_modbus_eev_target_step = 0U;
    ModbusProtocol_ResetRx();
}

void ModbusProtocol_Task(void)
{
    if ((g_modbus_rx_length > 0U) &&
        ((g_modbus_frame_timeout == 0U) ||
         ((g_modbus_rx_expected_length > 0U) &&
          (g_modbus_rx_length >= g_modbus_rx_expected_length))))
    {
        ModbusProtocol_ProcessFrame();
        ModbusProtocol_ResetRx();
    }
}

void ModbusProtocol_ProcessByte(uint8_t data)
{
    if (g_modbus_rx_length < MODBUS_RX_BUFFER_SIZE)
    {
        g_modbus_rx_frame[g_modbus_rx_length] = data;
        g_modbus_rx_length++;
        g_modbus_rx_expected_length = ModbusProtocol_GetExpectedLength();
    }
    else
    {
        ModbusProtocol_ResetRx();
    }

    g_modbus_frame_timeout = MODBUS_FRAME_TIMEOUT_MS;
}

void ModbusProtocol_TimerTick1ms(void)
{
    if (g_modbus_frame_timeout > 0U)
    {
        g_modbus_frame_timeout--;
    }

}

static void ModbusProtocol_ResetRx(void)
{
    g_modbus_rx_length = 0U;
    g_modbus_rx_expected_length = 0U;
    g_modbus_frame_timeout = 0U;
}

static uint16_t ModbusProtocol_GetExpectedLength(void)
{
    uint8_t function;

    if (g_modbus_rx_length < 2U)
    {
        return 0U;
    }

    function = g_modbus_rx_frame[1];

    switch (function)
    {
        case MODBUS_FUNC_READ_COILS:
        case MODBUS_FUNC_READ_DISCRETE_INPUTS:
        case MODBUS_FUNC_READ_HOLDING_REGS:
        case MODBUS_FUNC_READ_INPUT_REGS:
        case MODBUS_FUNC_WRITE_SINGLE_COIL:
        case MODBUS_FUNC_WRITE_SINGLE_REG:
        case MODBUS_FUNC_DIAGNOSTICS:
            return 8U;

        case MODBUS_FUNC_WRITE_MULTIPLE_REGS:
            if (g_modbus_rx_length >= 7U)
            {
                return (uint16_t)(9U + g_modbus_rx_frame[6U]);
            }
            break;

        default:
            break;
    }

    return 0U;
}

static void ModbusProtocol_ProcessFrame(void)
{
    uint16_t crc_calc;
    uint16_t crc_recv;
    uint8_t address;
    uint8_t function;

    ModbusProtocol_SaveRxDebug();

    g_modbus_debug_last_result = 0U;
    g_modbus_debug_last_exception = 0U;

    if (g_modbus_rx_length < MODBUS_MIN_FRAME_LENGTH)
    {
        return;
    }

    address = g_modbus_rx_frame[0];
    function = g_modbus_rx_frame[1];
    g_modbus_debug_last_function = function;

    if (address != MODBUS_DEVICE_ADDR)
    {
        return;
    }

    crc_calc = ModbusProtocol_Crc16(
        g_modbus_rx_frame,
        (uint16_t)(g_modbus_rx_length - MODBUS_CRC_LENGTH));

    crc_recv = (uint16_t)g_modbus_rx_frame[g_modbus_rx_length - 2U];
    crc_recv |= (uint16_t)((uint16_t)g_modbus_rx_frame[g_modbus_rx_length - 1U] << 8U);

    if (crc_calc != crc_recv)
    {
        return;
    }

    switch (function)
    {
        case MODBUS_FUNC_READ_COILS:
            ModbusProtocol_HandleReadCoils();
            break;

        case MODBUS_FUNC_READ_DISCRETE_INPUTS:
            ModbusProtocol_HandleReadDiscreteInputs();
            break;

        case MODBUS_FUNC_READ_HOLDING_REGS:
            ModbusProtocol_HandleReadHoldingRegisters();
            break;

        case MODBUS_FUNC_READ_INPUT_REGS:
            ModbusProtocol_HandleReadInputRegisters();
            break;

        case MODBUS_FUNC_WRITE_SINGLE_COIL:
            ModbusProtocol_HandleWriteSingleCoil();
            break;

        case MODBUS_FUNC_WRITE_SINGLE_REG:
            ModbusProtocol_HandleWriteSingleRegister();
            break;

        case MODBUS_FUNC_WRITE_MULTIPLE_REGS:
            ModbusProtocol_HandleWriteMultipleRegisters();
            break;

        case MODBUS_FUNC_DIAGNOSTICS:
            ModbusProtocol_HandleDiagnostics();
            break;

        default:
            ModbusProtocol_SendException(function, MODBUS_EX_ILLEGAL_FUNCTION);
            break;
    }
}

static void ModbusProtocol_HandleReadCoils(void)
{
    uint16_t start_address;
    uint16_t quantity;
    uint8_t byte_count;
    uint16_t i;
    uint8_t response[MODBUS_TX_BUFFER_SIZE];

    if (g_modbus_rx_length != 8U)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_COILS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    start_address = ModbusProtocol_ReadU16(2U);
    (void)start_address;
    quantity = ModbusProtocol_ReadU16(4U);

    if (quantity == 0U)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_COILS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    byte_count = (uint8_t)((quantity + 7U) / 8U);
    if ((uint16_t)(3U + byte_count) > MODBUS_TX_BUFFER_SIZE)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_COILS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    response[0] = MODBUS_DEVICE_ADDR;
    response[1] = MODBUS_FUNC_READ_COILS;
    response[2] = byte_count;

    for (i = 0U; i < byte_count; i++)
    {
        response[3U + i] = 0U;
    }

    if ((uint16_t)(start_address + quantity - 1U) > MODBUS_COIL_MAX)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_COILS, MODBUS_EX_ILLEGAL_DATA_ADDR);
        return;
    }

    for (i = 0U; i < quantity; i++)
    {
        if (ModbusProtocol_GetCoil((uint16_t)(start_address + i)) != 0U)
        {
            response[3U + (i / 8U)] |= (uint8_t)(1U << (i % 8U));
        }
    }

    ModbusProtocol_SendFrame(response, (uint16_t)(3U + byte_count));
}

static void ModbusProtocol_HandleReadHoldingRegisters(void)
{
    uint16_t start_address;
    uint16_t quantity;
    uint16_t index;
    uint8_t response[MODBUS_TX_BUFFER_SIZE];

    if (g_modbus_rx_length != 8U)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_HOLDING_REGS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    start_address = ModbusProtocol_ReadU16(2U);
    (void)start_address;
    quantity = ModbusProtocol_ReadU16(4U);

    if (quantity == 0U)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_HOLDING_REGS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    if ((uint16_t)(3U + quantity * 2U) > MODBUS_TX_BUFFER_SIZE)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_HOLDING_REGS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    response[0] = MODBUS_DEVICE_ADDR;
    response[1] = MODBUS_FUNC_READ_HOLDING_REGS;
    response[2] = (uint8_t)(quantity * 2U);

    for (index = 0U; index < quantity; index++)
    {
        ModbusProtocol_WriteU16(
            response,
            (uint16_t)(3U + (index * 2U)),
            ModbusProtocol_GetHoldingRegister((uint16_t)(start_address + index)));
    }

    ModbusProtocol_SendFrame(response, (uint16_t)(3U + (quantity * 2U)));
}

static void ModbusProtocol_HandleReadDiscreteInputs(void)
{
    uint16_t start_address;
    uint16_t quantity;
    uint8_t byte_count;
    uint8_t response[MODBUS_TX_BUFFER_SIZE];
    uint16_t i;

    if (g_modbus_rx_length != 8U)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_DISCRETE_INPUTS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    start_address = ModbusProtocol_ReadU16(2U);
    (void)start_address;
    quantity = ModbusProtocol_ReadU16(4U);

    if (quantity == 0U)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_DISCRETE_INPUTS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    byte_count = (uint8_t)((quantity + 7U) / 8U);
    if ((uint16_t)(3U + byte_count) > MODBUS_TX_BUFFER_SIZE)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_DISCRETE_INPUTS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    response[0] = MODBUS_DEVICE_ADDR;
    response[1] = MODBUS_FUNC_READ_DISCRETE_INPUTS;
    response[2] = byte_count;

    for (i = 0U; i < byte_count; i++)
    {
        response[3U + i] = 0U;
    }

    if ((uint16_t)(start_address + quantity - 1U) > MODBUS_DI_MAX)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_DISCRETE_INPUTS, MODBUS_EX_ILLEGAL_DATA_ADDR);
        return;
    }

    for (i = 0U; i < quantity; i++)
    {
        if (ModbusProtocol_GetDiscreteInput((uint16_t)(start_address + i)) != 0U)
        {
            response[3U + (i / 8U)] |= (uint8_t)(1U << (i % 8U));
        }
    }

    ModbusProtocol_SendFrame(response, (uint16_t)(3U + byte_count));
}

static void ModbusProtocol_HandleReadInputRegisters(void)
{
    uint16_t start_address;
    uint16_t quantity;

    if (g_modbus_rx_length != 8U)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_INPUT_REGS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    start_address = ModbusProtocol_ReadU16(2U);
    (void)start_address;
    quantity = ModbusProtocol_ReadU16(4U);

    if (quantity == 0U)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_INPUT_REGS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    if ((uint16_t)(3U + quantity * 2U) > MODBUS_TX_BUFFER_SIZE)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_INPUT_REGS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    if ((uint16_t)(start_address + quantity - 1U) > MODBUS_IR_MAX)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_READ_INPUT_REGS, MODBUS_EX_ILLEGAL_DATA_ADDR);
        return;
    }

    ModbusProtocol_SendInputRegisterResponse(start_address, quantity);
}

static void ModbusProtocol_SendInputRegisterResponse(uint16_t start_address, uint16_t quantity)
{
    uint16_t index;
    uint8_t response[MODBUS_TX_BUFFER_SIZE];

    response[0] = MODBUS_DEVICE_ADDR;
    response[1] = MODBUS_FUNC_READ_INPUT_REGS;
    response[2] = (uint8_t)(quantity * 2U);

    for (index = 0U; index < quantity; index++)
    {
        ModbusProtocol_WriteU16(
            response,
            (uint16_t)(3U + (index * 2U)),
            ModbusProtocol_GetInputRegister((uint16_t)(start_address + index)));
    }

    ModbusProtocol_SendFrame(response, (uint16_t)(3U + (quantity * 2U)));
}

static void ModbusProtocol_HandleWriteSingleCoil(void)
{
    uint16_t coil_address;
    uint16_t value;

    if (g_modbus_rx_length != 8U)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_WRITE_SINGLE_COIL, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    coil_address = ModbusProtocol_ReadU16(2U);
    value = ModbusProtocol_ReadU16(4U);

    if (coil_address > MODBUS_COIL_MAX)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_WRITE_SINGLE_COIL, MODBUS_EX_ILLEGAL_DATA_ADDR);
        return;
    }

    if (value == 0xFF00U)
    {
        if (ModbusProtocol_SetCoil(coil_address, 1U) == 0U)
        {
            ModbusProtocol_SendException(MODBUS_FUNC_WRITE_SINGLE_COIL, MODBUS_EX_ILLEGAL_DATA_ADDR);
            return;
        }
    }
    else if (value == 0x0000U)
    {
        if (ModbusProtocol_SetCoil(coil_address, 0U) == 0U)
        {
            ModbusProtocol_SendException(MODBUS_FUNC_WRITE_SINGLE_COIL, MODBUS_EX_ILLEGAL_DATA_ADDR);
            return;
        }
    }
    else
    {
        ModbusProtocol_SendException(MODBUS_FUNC_WRITE_SINGLE_COIL, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    /*
     * Modbus writes echo the request PDU.
     */
    ModbusProtocol_SendFrame(g_modbus_rx_frame, 6U);
}

static void ModbusProtocol_HandleWriteSingleRegister(void)
{
    uint16_t register_address;
    uint16_t value;

    if (g_modbus_rx_length != 8U)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_WRITE_SINGLE_REG, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    register_address = ModbusProtocol_ReadU16(2U);
    value = ModbusProtocol_ReadU16(4U);

    if (!ModbusProtocol_IsEevHoldingRegister(register_address) &&
        !AppConfig_ValidateRegister(register_address, value))
    {
        ModbusProtocol_SendException(MODBUS_FUNC_WRITE_SINGLE_REG, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    if (register_address == MODBUS_CONFIG_REG_SYSTEM_ENABLE)
    {
        ModbusProtocol_SetLedStatus((uint8_t)(value != 0U));
    }
    else
    {
        if (ModbusProtocol_IsEevHoldingRegister(register_address))
        {
            if (ModbusProtocol_WriteEevHoldingRegister(register_address, value) == 0U)
            {
                ModbusProtocol_SendException(MODBUS_FUNC_WRITE_SINGLE_REG, MODBUS_EX_ILLEGAL_DATA_VALUE);
                return;
            }
        }
        else
        {
            AppConfig_SetRegister(register_address, value);
        }
    }

    ModbusProtocol_SendFrame(g_modbus_rx_frame, 6U);
}

static void ModbusProtocol_HandleWriteMultipleRegisters(void)
{
    uint16_t start_address;
    uint16_t quantity;
    uint8_t byte_count;
    uint8_t i;

    if (g_modbus_rx_length < 9U)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_WRITE_MULTIPLE_REGS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    start_address = ModbusProtocol_ReadU16(2U);
    quantity = ModbusProtocol_ReadU16(4U);
    byte_count = g_modbus_rx_frame[6];

    if (quantity == 0U || quantity > 123U ||
        (uint16_t)byte_count != (uint16_t)(quantity * 2U) ||
        (uint16_t)(9U + byte_count) != g_modbus_rx_length)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_WRITE_MULTIPLE_REGS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    for (i = 0U; i < quantity; i++)
    {
        uint16_t reg_addr = (uint16_t)(start_address + (uint16_t)i);
        uint16_t value = (uint16_t)((uint16_t)g_modbus_rx_frame[7U + i * 2U] << 8U) |
                         (uint16_t)g_modbus_rx_frame[8U + i * 2U];

        if (!ModbusProtocol_IsEevHoldingRegister(reg_addr) &&
            !AppConfig_ValidateRegister(reg_addr, value))
        {
            ModbusProtocol_SendException(MODBUS_FUNC_WRITE_MULTIPLE_REGS, MODBUS_EX_ILLEGAL_DATA_VALUE);
            return;
        }
    }

    for (i = 0U; i < quantity; i++)
    {
        uint16_t reg_addr = (uint16_t)(start_address + (uint16_t)i);
        uint16_t value = (uint16_t)((uint16_t)g_modbus_rx_frame[7U + i * 2U] << 8U) |
                         (uint16_t)g_modbus_rx_frame[8U + i * 2U];

        if (reg_addr == MODBUS_CONFIG_REG_SYSTEM_ENABLE)
        {
            ModbusProtocol_SetLedStatus((uint8_t)(value != 0U));
        }
        else
        {
            if (ModbusProtocol_IsEevHoldingRegister(reg_addr))
            {
                if (ModbusProtocol_WriteEevHoldingRegister(reg_addr, value) == 0U)
                {
                    ModbusProtocol_SendException(MODBUS_FUNC_WRITE_MULTIPLE_REGS, MODBUS_EX_ILLEGAL_DATA_VALUE);
                    return;
                }
            }
            else
            {
                AppConfig_SetRegister(reg_addr, value);
            }
        }
    }

    {
        uint8_t response[8];
        response[0] = MODBUS_DEVICE_ADDR;
        response[1] = MODBUS_FUNC_WRITE_MULTIPLE_REGS;
        ModbusProtocol_WriteU16(response, 2U, start_address);
        ModbusProtocol_WriteU16(response, 4U, quantity);
        ModbusProtocol_SendFrame(response, 6U);
    }
}

static void ModbusProtocol_HandleDiagnostics(void)
{
    uint16_t sub_function;

    if (g_modbus_rx_length < 8U)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_DIAGNOSTICS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    sub_function = ModbusProtocol_ReadU16(2U);

    if (sub_function != MODBUS_DIAG_RETURN_QUERY_DATA)
    {
        ModbusProtocol_SendException(MODBUS_FUNC_DIAGNOSTICS, MODBUS_EX_ILLEGAL_DATA_VALUE);
        return;
    }

    /*
     * Function 0x08 / subfunction 0x0000 returns the request data field.
     */
    ModbusProtocol_SendFrame(
        g_modbus_rx_frame,
        (uint16_t)(g_modbus_rx_length - MODBUS_CRC_LENGTH));
}

static void ModbusProtocol_SendException(uint8_t function, uint8_t exception)
{
    uint8_t response[5];

    g_modbus_debug_last_exception = exception;

    response[0] = MODBUS_DEVICE_ADDR;
    response[1] = (uint8_t)(function | 0x80U);
    response[2] = exception;

    ModbusProtocol_SendFrame(response, 3U);
}

static void ModbusProtocol_SendFrame(uint8_t *frame, uint16_t length_without_crc)
{
    uint16_t crc;

    if ((length_without_crc + MODBUS_CRC_LENGTH) > MODBUS_TX_BUFFER_SIZE)
    {
        g_modbus_debug_last_result = 0xEEU;
        return;
    }

    crc = ModbusProtocol_Crc16(frame, length_without_crc);
    frame[length_without_crc] = (uint8_t)(crc & 0x00FFU);
    frame[length_without_crc + 1U] = (uint8_t)((crc >> 8U) & 0x00FFU);

    ModbusProtocol_SaveTxDebug(frame, (uint16_t)(length_without_crc + MODBUS_CRC_LENGTH));
    (void)BSP_RS485_Send(frame, (uint16_t)(length_without_crc + MODBUS_CRC_LENGTH));

    g_modbus_debug_last_result = 1U;
}

static uint16_t ModbusProtocol_Crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc;
    uint16_t index;
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

static uint16_t ModbusProtocol_ReadU16(uint16_t offset)
{
    uint16_t value;

    value = (uint16_t)((uint16_t)g_modbus_rx_frame[offset] << 8U);
    value |= (uint16_t)g_modbus_rx_frame[offset + 1U];

    return value;
}

static void ModbusProtocol_WriteU16(uint8_t *buffer, uint16_t offset, uint16_t value)
{
    buffer[offset] = (uint8_t)((value >> 8U) & 0x00FFU);
    buffer[offset + 1U] = (uint8_t)(value & 0x00FFU);
}

static uint16_t ModbusProtocol_GetHoldingRegister(uint16_t address)
{
    switch (address)
    {
        case MODBUS_IR_HP_FAULT_CODE:
            return HighPressureProtection_GetFaultCode();

        case MODBUS_IR_HP_LOCKED:
            return (uint16_t)(HighPressureProtection_IsLocked() ? 1U : 0U);

        case MODBUS_IR_HP_FAULT_COUNT:
            return (uint16_t)HighPressureProtection_GetFaultCountInWindow();

        case MODBUS_IR_FOUR_WAY_MODE:
            return (uint16_t)FourWayValve_GetMode();

        case MODBUS_IR_FOUR_WAY_STATE:
            return (uint16_t)FourWayValve_GetState();

        case MODBUS_HR_EEV_COMMAND:
            return MODBUS_EEV_CMD_NONE;

        case MODBUS_HR_EEV_STEP_COUNT:
            return g_modbus_eev_step_count;

        case MODBUS_HR_EEV_TARGET_STEP:
            return g_modbus_eev_target_step;

        case MODBUS_HR_EEV_INTERVAL_MS:
            return EEV_GetStepIntervalMs();

        default:
            break;
    }

    return AppConfig_GetRegister(address);
}

static uint16_t ModbusProtocol_GetInputRegister(uint16_t address)
{
    switch (address)
    {
        case MODBUS_IR_HP_FAULT_CODE:
            return HighPressureProtection_GetFaultCode();

        case MODBUS_IR_HP_LOCKED:
            return (uint16_t)HighPressureProtection_IsLocked();

        case MODBUS_IR_HP_FAULT_COUNT:
            return (uint16_t)HighPressureProtection_GetFaultCountInWindow();

        case MODBUS_IR_FOUR_WAY_MODE:
            return (uint16_t)FourWayValve_GetMode();

        case MODBUS_IR_FOUR_WAY_STATE:
            return (uint16_t)FourWayValve_GetState();

        case MODBUS_IR_HP_SWITCH_CLOSED:
            return (uint16_t)HighPressureProtection_GetSwitchClosed();

        case MODBUS_IR_HP_FAULT_ACTIVE:
            return (uint16_t)HighPressureProtection_IsFaultActive();

        case MODBUS_IR_HP_REMAIN_SECONDS:
            return (uint16_t)(HighPressureProtection_GetRemainMs() / 1000UL);

        case MODBUS_IR_COMPRESSOR_RUNNING:
            return (uint16_t)DriverBoard_IsCompressorRunning();

        case MODBUS_IR_COMPRESSOR_FREQ_HZ:
            return DriverBoard_GetCompressorFrequencyHz();

        case MODBUS_IR_FOUR_WAY_REQUEST_MODE:
            return (uint16_t)FourWayValve_GetRequestedMode();

        case MODBUS_IR_FOUR_WAY_REMAIN_SEC:
            return (uint16_t)(FourWayValve_GetRemainMs() / 1000UL);

        case MODBUS_IR_HP_FAULT_ELAPSED_SEC:
            return HighPressureProtection_GetFaultElapsedSec();

        case MODBUS_IR_COMP_RUN_ELAPSED_SEC:
            return DriverBoard_GetCompressorRunElapsedSec();

        case MODBUS_IR_TH1_RAW_AVG:
        case MODBUS_IR_TH1_VOLTAGE_MV:
        case MODBUS_IR_TH1_RESISTANCE_LO:
        case MODBUS_IR_TH1_RESISTANCE_HI:
        case MODBUS_IR_TH1_TEMP_0P1C:
        case MODBUS_IR_TH1_STATUS:
        {
            temperature_th1_data_t th1;
            Temperature_GetTH1Data(&th1);

            switch (address)
            {
                case MODBUS_IR_TH1_RAW_AVG:
                    return th1.raw_average;
                case MODBUS_IR_TH1_VOLTAGE_MV:
                    return th1.voltage_mv;
                case MODBUS_IR_TH1_RESISTANCE_LO:
                    return (uint16_t)(th1.resistance_ohm & 0xFFFFUL);
                case MODBUS_IR_TH1_RESISTANCE_HI:
                    return (uint16_t)((th1.resistance_ohm >> 16U) & 0xFFFFUL);
                case MODBUS_IR_TH1_TEMP_0P1C:
                    return (uint16_t)th1.temperature_0p1c;
        case MODBUS_IR_TH1_STATUS:
            return (uint16_t)th1.status;
                default:
                    break;
            }
        }
        break;

        case MODBUS_IR_EEV_CURRENT_STEP:
            return EEV_GetCurrentStep();

        case MODBUS_IR_EEV_BUSY:
            return (uint16_t)EEV_IsBusy();

        case MODBUS_IR_EEV_TARGET_STEP:
            return g_modbus_eev_target_step;

        case MODBUS_IR_EEV_INTERVAL_MS:
            return EEV_GetStepIntervalMs();

        default:
            break;
    }

    return 0U;
}

static uint8_t ModbusProtocol_IsEevHoldingRegister(uint16_t address)
{
    return ((address == MODBUS_HR_EEV_COMMAND) ||
            (address == MODBUS_HR_EEV_STEP_COUNT) ||
            (address == MODBUS_HR_EEV_TARGET_STEP) ||
            (address == MODBUS_HR_EEV_INTERVAL_MS)) ? 1U : 0U;
}

static uint8_t ModbusProtocol_WriteEevHoldingRegister(uint16_t address, uint16_t value)
{
    switch (address)
    {
        case MODBUS_HR_EEV_STEP_COUNT:
            if (value == 0U || value > EEV_MAX_STEPS)
            {
                return 0U;
            }
            g_modbus_eev_step_count = value;
            return 1U;

        case MODBUS_HR_EEV_TARGET_STEP:
            if (value > EEV_MAX_STEPS)
            {
                return 0U;
            }
            g_modbus_eev_target_step = value;
            return 1U;

        case MODBUS_HR_EEV_INTERVAL_MS:
            return EEV_SetStepIntervalMs(value);

        case MODBUS_HR_EEV_COMMAND:
            if ((value >= MODBUS_EEV_CMD_HOLD_PHASE_BASE) &&
                (value < (uint16_t)(MODBUS_EEV_CMD_HOLD_PHASE_BASE + 8U)))
            {
                return EEV_DebugHoldPhase((uint8_t)(value - MODBUS_EEV_CMD_HOLD_PHASE_BASE));
            }

            switch (value)
            {
                case MODBUS_EEV_CMD_NONE:
                    return 1U;

                case MODBUS_EEV_CMD_OPEN_STEPS:
                    EEV_OpenSteps(g_modbus_eev_step_count);
                    g_modbus_eev_target_step = EEV_GetCurrentStep();
                    if ((uint16_t)(EEV_MAX_STEPS - g_modbus_eev_target_step) < g_modbus_eev_step_count)
                    {
                        g_modbus_eev_target_step = EEV_MAX_STEPS;
                    }
                    else
                    {
                        g_modbus_eev_target_step = (uint16_t)(g_modbus_eev_target_step + g_modbus_eev_step_count);
                    }
                    return 1U;

                case MODBUS_EEV_CMD_CLOSE_STEPS:
                    EEV_CloseSteps(g_modbus_eev_step_count);
                    if (EEV_GetCurrentStep() < g_modbus_eev_step_count)
                    {
                        g_modbus_eev_target_step = 0U;
                    }
                    else
                    {
                        g_modbus_eev_target_step = (uint16_t)(EEV_GetCurrentStep() - g_modbus_eev_step_count);
                    }
                    return 1U;

                case MODBUS_EEV_CMD_STOP:
                    EEV_Stop();
                    g_modbus_eev_target_step = EEV_GetCurrentStep();
                    return 1U;

                case MODBUS_EEV_CMD_MOVE_TO:
                    EEV_MoveTo(g_modbus_eev_target_step);
                    return 1U;

                case MODBUS_EEV_CMD_ALL_OFF:
                    EEV_Stop();
                    g_modbus_eev_target_step = EEV_GetCurrentStep();
                    return 1U;

                default:
                    return 0U;
            }

        default:
            break;
    }

    return 0U;
}

static uint8_t ModbusProtocol_GetDiscreteInput(uint16_t address)
{
    if ((address >= MODBUS_DI_K_STABLE_BASE) &&
        (address < (uint16_t)(MODBUS_DI_K_STABLE_BASE + SWITCH_INPUT_COUNT)))
    {
        return SwitchInput_GetStable((switch_channel_t)(address - MODBUS_DI_K_STABLE_BASE));
    }

    if ((address >= MODBUS_DI_K_RAW_BASE) &&
        (address < (uint16_t)(MODBUS_DI_K_RAW_BASE + SWITCH_INPUT_COUNT)))
    {
        return SwitchInput_GetRaw((switch_channel_t)(address - MODBUS_DI_K_RAW_BASE));
    }

    switch (address)
    {
        case MODBUS_DI_HP_SWITCH_CLOSED:
            return HighPressureProtection_GetSwitchClosed();

        case MODBUS_DI_HP_FAULT_ACTIVE:
            return HighPressureProtection_IsFaultActive();

        case MODBUS_DI_HP_LOCKED:
            return HighPressureProtection_IsLocked();

        case MODBUS_DI_COMPRESSOR_RUNNING:
            return DriverBoard_IsCompressorRunning();

        default:
            break;
    }

    return 0U;
}

static uint8_t ModbusProtocol_GetLedStatus(void)
{
    return (uint8_t)(AppConfig_GetRegister(MODBUS_CONFIG_REG_SYSTEM_ENABLE) != 0U);
}

static void ModbusProtocol_SetLedStatus(uint8_t status)
{
    uint8_t logical_on;

    logical_on = (status != 0U) ? 1U : 0U;
    AppConfig_SetRegister(MODBUS_CONFIG_REG_SYSTEM_ENABLE, (uint16_t)logical_on);
    ModbusProtocol_ApplyLedOutput(logical_on);

    if (logical_on == 0U)
    {
        g_modbus_coils &= (uint8_t)~(uint8_t)(1U << MODBUS_COIL_SYSTEM_ENABLE);
    }
    else
    {
        g_modbus_coils |= (uint8_t)(1U << MODBUS_COIL_SYSTEM_ENABLE);
    }
}

static void ModbusProtocol_ApplyLedOutput(uint8_t logical_on)
{
    /*
     * P77 LED is active-low on this board.
     */
    P7_bit.no7 = (logical_on != 0U) ? 0U : 1U;
}

static uint8_t ModbusProtocol_GetCoil(uint16_t address)
{
    if (address == MODBUS_COIL_SYSTEM_ENABLE)
    {
        return ModbusProtocol_GetLedStatus();
    }

    if (address == MODBUS_COIL_FOUR_WAY_COOL)
    {
        return (FourWayValve_GetRequestedMode() == FOUR_WAY_MODE_COOL) ? 1U : 0U;
    }

    if (address == MODBUS_COIL_FOUR_WAY_HEAT)
    {
        return (FourWayValve_GetRequestedMode() == FOUR_WAY_MODE_HEAT) ? 1U : 0U;
    }

    if (address == MODBUS_COIL_COMPRESSOR_DEBUG)
    {
        return DriverBoard_IsCompressorRunning();
    }

    if (address <= 7U)
    {
        return (uint8_t)((g_modbus_coils >> address) & 0x01U);
    }

    return 0U;
}

static uint8_t ModbusProtocol_SetCoil(uint16_t address, uint8_t status)
{
    uint8_t mask;

    if (address > MODBUS_COIL_MAX)
    {
        return 0U;
    }

    if (address == MODBUS_COIL_FOUR_WAY_COOL)
    {
        if (status != 0U)
        {
            FourWayValve_RequestCooling();
        }
        return 1U;
    }

    if (address == MODBUS_COIL_FOUR_WAY_HEAT)
    {
        if (status != 0U)
        {
            FourWayValve_RequestHeating();
        }
        return 1U;
    }

    mask = (uint8_t)(1U << address);

    if (status == 0U)
    {
        g_modbus_coils &= (uint8_t)~mask;
    }
    else
    {
        g_modbus_coils |= mask;
    }

    if (address == MODBUS_COIL_SYSTEM_ENABLE)
    {
        ModbusProtocol_SetLedStatus(status);
    }
    else if (address == MODBUS_COIL_EEPROM_SAVE)
    {
        if (status != 0U)
        {
            AppConfig_Save();
        }
    }
    else if (address == MODBUS_COIL_COMPRESSOR_DEBUG)
    {
        DriverBoard_DebugSetCompressorRunning(status);
    }

    return 1U;
}

static void ModbusProtocol_SaveRxDebug(void)
{
    uint16_t index;

    g_modbus_debug_rx_length = g_modbus_rx_length;

    for (index = 0U; index < g_modbus_rx_length; index++)
    {
        g_modbus_debug_rx_frame[index] = g_modbus_rx_frame[index];
    }
}

static void ModbusProtocol_SaveTxDebug(const uint8_t *frame, uint16_t length)
{
    uint16_t index;

    g_modbus_debug_tx_length = length;

    for (index = 0U; index < length; index++)
    {
        g_modbus_debug_tx_frame[index] = frame[index];
    }
}
