/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products.
* No other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws. 
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING THIS SOFTWARE, WHETHER EXPRESS, IMPLIED
* OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NON-INFRINGEMENT.  ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED.TO THE MAXIMUM EXTENT PERMITTED NOT PROHIBITED BY
* LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES SHALL BE LIABLE FOR ANY DIRECT,
* INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS SOFTWARE, EVEN IF RENESAS OR
* ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability 
* of this software. By using this software, you agree to the additional terms and conditions found by accessing the 
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2011, 2021 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/

/***********************************************************************************************************************
* File Name    : r_main.c
* Version      : CodeGenerator for RL78/G13 V2.05.06.02 [08 Nov 2021]
* Device(s)    : R5F100LG
* Tool-Chain   : CCRL
* Description  : This file implements main function.
* Creation Date: 2026/7/17
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes
***********************************************************************************************************************/
#include "r_cg_macrodriver.h"
#include "r_cg_cgc.h"
#include "r_cg_port.h"
#include "r_cg_serial.h"
#include "r_cg_timer.h"
#include "r_cg_wdt.h"
/* Start user code for include. Do not edit comment generated here */
#include "bsp_rs485.h"
/* End user code. Do not edit comment generated here */
#include "r_cg_userdefine.h"

/***********************************************************************************************************************
Pragma directive
***********************************************************************************************************************/
/* Start user code for pragma. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */

/***********************************************************************************************************************
Global variables and functions
***********************************************************************************************************************/
/* Start user code for global. Do not edit comment generated here */

#define RS485_ECHO_MAX_LENGTH      (32U)
#define RS485_TX_BUFFER_SIZE       (RS485_ECHO_MAX_LENGTH + 2U)

/*
 * 一帧最多只保存前32个普通字符。
 * 超过32个的字符继续接收，但直接丢弃。
 */
static uint8_t g_rs485_frame_buffer[RS485_ECHO_MAX_LENGTH];
static uint16_t g_rs485_frame_length;
static uint8_t g_rs485_frame_ready;

static void RS485_ProcessReceivedData(void);
static void RS485_EchoFrame(void);

/* End user code. Do not edit comment generated here */
void R_MAIN_UserInit(void);

/***********************************************************************************************************************
* Function Name: main
* Description  : This function implements main function.
* Arguments    : None
* Return Value : None
***********************************************************************************************************************/
void main(void)
{
    R_MAIN_UserInit();
    /* Start user code. Do not edit comment generated here */

    g_rs485_frame_length = 0U;
    g_rs485_frame_ready = 0U;

    /*
     * 上电提示。
     */
    
    (void)BSP_RS485_SendString("READY\r\n");
    while (1U)
    {
        /*
         * 从BSP环形缓冲区取出数据，组成完整帧。
         */
        
        
        RS485_ProcessReceivedData();

        /*
         * 只有收到'\n'，确认VOFA+整帧发送完成后，
         * 才切换到发送模式进行回显。
         */
        if (g_rs485_frame_ready != 0U)
        {
            RS485_EchoFrame();
        }

        R_WDT_Restart();
    }

    /* End user code. Do not edit comment generated here */
}

/***********************************************************************************************************************
* Function Name: R_MAIN_UserInit
* Description  : This function adds user code before implementing main function.
* Arguments    : None
* Return Value : None
***********************************************************************************************************************/
void R_MAIN_UserInit(void)
{
    /* Start user code. Do not edit comment generated here */

    /*
     * P77 LED初始化。
     */
    P7_bit.no7 = 1U;
    PM7_bit.no7 = 0U;

    /*
     * 初始化硬件UART1和RS-485。
     */
    BSP_RS485_Init();

    /*
     * LED定时器。
     */
    R_TAU0_Channel0_Start();

    /*
     * 打开CPU总中断。
     */
    EI();

    /* End user code. Do not edit comment generated here */
}

/* Start user code for adding. Do not edit comment generated here */

/***********************************************************************************************************************
* Function Name: RS485_ProcessReceivedData
* Description  : Process bytes received from the RS-485 BSP ring buffer.
*
*                VOFA+应设置为发送结尾：\r\n
*
*                普通字符：
*                    前32个保存；
*                    第33个及以后直接丢弃。
*
*                '\r'：
*                    忽略，不计入32个字符。
*
*                '\n'：
*                    表示一帧接收完成。
***********************************************************************************************************************/
static void RS485_ProcessReceivedData(void)
{
    uint8_t received_byte;

    /*
     * 上一帧还没有回显时，不继续覆盖帧缓冲区。
     */
    if (g_rs485_frame_ready != 0U)
    {
        return;
    }

    /*
     * 把BSP环形缓冲区中的数据全部取出。
     */
    while (BSP_RS485_Read(&received_byte, 1U) == 1U)
    {
        if (received_byte == (uint8_t)'\n')
        {
            /*
             * 收到换行符，说明VOFA+已经发送完整一帧。
             */
            g_rs485_frame_ready = 1U;
            break;
        }
        else if (received_byte == (uint8_t)'\r')
        {
            /*
             * 忽略回车符，不保存到帧数据中。
             */
        }
        else
        {
            /*
             * 只保存前32个普通字符。
             */
            if (g_rs485_frame_length < RS485_ECHO_MAX_LENGTH)
            {
                g_rs485_frame_buffer[g_rs485_frame_length] =
                    received_byte;

                g_rs485_frame_length++;
            }
            else
            {
                /*
                 * 超过32个字符后继续接收，但直接丢弃。
                 *
                 * 不能在这里立即回显，因为VOFA+可能仍然
                 * 在发送第33个及后续字符。
                 */
            }
        }
    }
}

/***********************************************************************************************************************
* Function Name: RS485_EchoFrame
* Description  : Echo the first 32 characters of a complete received frame.
***********************************************************************************************************************/
static void RS485_EchoFrame(void)
{
    uint8_t tx_buffer[RS485_TX_BUFFER_SIZE];
    uint16_t tx_length;
    uint16_t index;

    tx_length = 0U;

    /*
     * 复制实际保存的字符，最多32个。
     */
    for (index = 0U;
         index < g_rs485_frame_length;
         index++)
    {
        tx_buffer[tx_length] =
            g_rs485_frame_buffer[index];

        tx_length++;
    }

    /*
     * 回显末尾统一添加标准的\r\n。
     */
    tx_buffer[tx_length] = (uint8_t)'\r';
    tx_length++;

    tx_buffer[tx_length] = (uint8_t)'\n';
    tx_length++;

    /*
     * 一次性发送整帧。
     *
     * 不要把字符和\r\n分成多次发送，
     * 否则RS-485会反复切换收发方向。
     */
    (void)BSP_RS485_Send(
        tx_buffer,
        tx_length);

    /*
     * 当前帧处理完成，准备接收下一帧。
     */
    g_rs485_frame_length = 0U;
    g_rs485_frame_ready = 0U;
}

/* End user code. Do not edit comment generated here */
