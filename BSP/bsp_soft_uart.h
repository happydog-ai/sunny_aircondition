#ifndef BSP_SOFT_UART_H
#define BSP_SOFT_UART_H

/*
 * r_cg_macrodriver.h中已经包含RL78工程需要的整数类型定义，
 * 不再单独包含stdint.h，避免CC-RL重复类型警告。
 */
#include "r_cg_macrodriver.h"

/*
 * 软件串口参数：
 * 9600 bit/s，8N1
 *
 * 定时器频率：
 * 9600 × 3 = 28800 Hz
 *
 * 定时器周期：
 * 约34.722 us
 */
#define BSP_SOFT_UART_BAUD_RATE      (9600UL)
#define BSP_SOFT_UART_SAMPLE_RATE    (3UL)
#define BSP_SOFT_UART_TIMER_HZ       \
    (BSP_SOFT_UART_BAUD_RATE * BSP_SOFT_UART_SAMPLE_RATE)

void BSP_SoftUart_Init(void);

uint8_t BSP_SoftUart_SendByte(uint8_t data);

void BSP_SoftUart_WaitTxComplete(void);

uint8_t BSP_SoftUart_IsTxComplete(void);

uint16_t BSP_SoftUart_Available(void);

uint16_t BSP_SoftUart_Read(
    uint8_t *buffer,
    uint16_t max_length);

void BSP_SoftUart_ClearRx(void);

void BSP_SoftUart_EnableRx(uint8_t enable);

void BSP_SoftUart_TimerIRQHandler(void);

#endif /* BSP_SOFT_UART_H */