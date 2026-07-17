#ifndef BSP_RS485_H
#define BSP_RS485_H

#include "r_cg_macrodriver.h"

typedef enum
{
    BSP_RS485_MODE_RX = 0,
    BSP_RS485_MODE_TX = 1
} bsp_rs485_mode_t;

/**
 * @brief 初始化PC口RS-485
 *
 * 固定参数：
 * 9600、8N1
 */
void BSP_RS485_Init(void);

/**
 * @brief 设置RS-485收发方向
 */
void BSP_RS485_SetMode(bsp_rs485_mode_t mode);

/**
 * @brief 阻塞发送一个字节
 *
 * @return 1：成功
 *         0：失败
 */
uint8_t BSP_RS485_SendByte(uint8_t data);

/**
 * @brief 阻塞发送一组数据
 *
 * @return 1：成功
 *         0：参数错误
 */
uint8_t BSP_RS485_Send(
    const uint8_t *data,
    uint16_t length);

/**
 * @brief 发送以'\0'结束的字符串
 */
uint8_t BSP_RS485_SendString(
    const char *string);

/**
 * @brief 获取已经接收的字节数
 */
uint16_t BSP_RS485_Available(void);

/**
 * @brief 从接收缓冲区读取数据
 */
uint16_t BSP_RS485_Read(
    uint8_t *buffer,
    uint16_t max_length);

/**
 * @brief 清空接收缓冲区
 */
void BSP_RS485_ClearRx(void);


void BSP_RS485_UART1ReceiveCallback(void);
void BSP_RS485_UART1SoftwareOverrunCallback(uint8_t data);
void BSP_RS485_UART1SendEndCallback(void);
void BSP_RS485_UART1ErrorCallback(uint8_t err_type);

#endif