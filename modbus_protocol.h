#ifndef MODBUS_PROTOCOL_H
#define MODBUS_PROTOCOL_H

#include "r_cg_macrodriver.h"

#define MODBUS_DEVICE_ADDR              (0x01U)
#define MODBUS_RX_BUFFER_SIZE           (64U)
#define MODBUS_TX_BUFFER_SIZE           (128U)

void ModbusProtocol_Init(void);
void ModbusProtocol_Task(void);
void ModbusProtocol_ProcessByte(uint8_t data);
void ModbusProtocol_TimerTick1ms(void);

extern volatile uint8_t g_modbus_debug_rx_frame[MODBUS_RX_BUFFER_SIZE];
extern volatile uint16_t g_modbus_debug_rx_length;
extern volatile uint8_t g_modbus_debug_tx_frame[MODBUS_TX_BUFFER_SIZE];
extern volatile uint16_t g_modbus_debug_tx_length;
extern volatile uint8_t g_modbus_debug_last_function;
extern volatile uint8_t g_modbus_debug_last_exception;
extern volatile uint8_t g_modbus_debug_last_result;

#endif
