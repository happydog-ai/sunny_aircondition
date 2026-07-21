#ifndef DRIVER_BOARD_COMM_H
#define DRIVER_BOARD_COMM_H

#include "r_cg_macrodriver.h"
#include "bsp_relay.h"

#define COMPRESSOR_RUN_RELAY    (RELAY_OUT_0)

void DriverBoardComm_Init(void);
void DriverBoardComm_Task(void);
void DriverBoardComm_TimerTick1ms(void);

uint8_t DriverBoard_RequestCompressorStop(void);
uint8_t DriverBoard_RequestRestorePreviousState(void);
uint8_t DriverBoard_IsCompressorRunning(void);
uint8_t DriverBoard_IsCompressorStopped(void);
uint16_t DriverBoard_GetCompressorFrequencyHz(void);
uint16_t DriverBoard_GetCompressorRunElapsedSec(void);

void DriverBoard_DebugSetCompressorRunning(uint8_t running);

#endif
