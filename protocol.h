#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "r_cg_macrodriver.h"

#define PROTOCOL_DEVICE_ADDR        (0x01U)
#define PROTOCOL_DATA_MAX_LENGTH    (32U)
#define PROTOCOL_FRAME_MIN_LENGTH   (9U)
#define PROTOCOL_FRAME_MAX_LENGTH   (41U)

void Protocol_Init(void);
void Protocol_Task(void);
void Protocol_TimerTick1ms(void);

extern volatile uint8_t global_tx_buffer[PROTOCOL_FRAME_MAX_LENGTH];
extern volatile uint8_t global_tx_length;

#endif

