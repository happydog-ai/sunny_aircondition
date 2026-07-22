#ifndef BSP_RS485_H
#define BSP_RS485_H

#include "r_cg_macrodriver.h"

typedef enum
{
    BSP_RS485_MODE_RX = 0,
    BSP_RS485_MODE_TX = 1
} bsp_rs485_mode_t;

/**
 * @brief ???PC?RS-485
 *
 * ?????
 * 9600?8N1
 */
void BSP_RS485_Init(void);

/**
 * @brief ??RS-485????
 */
void BSP_RS485_SetMode(bsp_rs485_mode_t mode);

/**
 * @brief ????????
 *
 * @return 1???
 *         0???
 */
uint8_t BSP_RS485_SendByte(uint8_t data);

/**
 * @brief ????????
 *
 * @return 1???
 *         0?????
 */
uint8_t BSP_RS485_Send(
    const uint8_t *data,
    uint16_t length);

/**
 * @brief ???'\0'??????
 */
uint8_t BSP_RS485_SendString(
    const char *string);

/**
 * @brief ??????????
 */
uint16_t BSP_RS485_Available(void);

/**
 * @brief ??????????
 */
uint16_t BSP_RS485_Read(
    uint8_t *buffer,
    uint16_t max_length);

/**
 * @brief ???????
 */
void BSP_RS485_ClearRx(void);


void BSP_RS485_UART1ReceiveCallback(void);
void BSP_RS485_UART1SoftwareOverrunCallback(uint8_t data);
void BSP_RS485_UART1SendEndCallback(void);
void BSP_RS485_UART1ErrorCallback(uint8_t err_type);

extern volatile uint16_t g_rs485_debug_send_call_count;
extern volatile uint16_t g_rs485_debug_send_length;
extern volatile uint16_t g_rs485_debug_send_status;
extern volatile uint8_t g_rs485_debug_send_result;
extern volatile uint8_t g_rs485_debug_tx_done_flag;
extern volatile uint8_t g_rs485_debug_mode;
extern volatile uint8_t g_rs485_debug_stage;
extern volatile uint16_t g_rs485_debug_send_done_count;
extern volatile uint8_t g_rs485_debug_tx_mirror[64];
extern volatile uint16_t g_rs485_debug_tx_mirror_length;
extern volatile uint8_t g_rs485_debug_en_before_send;
extern volatile uint8_t g_rs485_debug_en_after_send;

#endif
