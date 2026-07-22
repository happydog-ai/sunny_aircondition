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
* Creation Date: 2026/7/20
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
#include "protocol.h"
#include "modbus_protocol.h"
#include "app_config.h"
#include "bsp_relay.h"
#include "switch_input.h"
#include "driver_board_comm.h"
#include "high_pressure_protection.h"
#include "four_way_valve.h"
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
    while (1U)
    {
        uint8_t data;

        while (BSP_RS485_Read(&data, 1U) == 1U)
        {
            Protocol_ProcessByte(data);
            ModbusProtocol_ProcessByte(data);
        }

        Protocol_Task();
        ModbusProtocol_Task();
        AppConfig_Task();
        SwitchInput_Task();
        DriverBoardComm_Task();
        HighPressureProtection_Task();
        FourWayValve_Task();
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
     * P77 LED????
     */
    P7_bit.no7 = 1U;
    PM7_bit.no7 = 0U;

    /*
     * ?????UART1?RS-485?
     */
    BSP_RS485_Init();

    /*
     * LED????
     */
    TDR00 = APP_SYSTEM_TICK_TDR00_1MS;
    R_TAU0_Channel0_Start();

    /*
     * ??CPU????
     */
    EI();

    /*
     * EEPROM/I2C uses interrupt callbacks, so load persistent config only
     * after global interrupts are enabled.
     */
    AppConfig_Init();
    Protocol_Init();
    ModbusProtocol_Init();

    BSP_Relay_Init();
    SwitchInput_Init();
    DriverBoardComm_Init();
    HighPressureProtection_Init();
    FourWayValve_Init();

    /* End user code. Do not edit comment generated here */
}

/* Start user code for adding. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */
