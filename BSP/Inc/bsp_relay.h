#ifndef BSP_RELAY_H
#define BSP_RELAY_H

#include "r_cg_macrodriver.h"

#define RELAY_STATE_OFF (0U)
#define RELAY_STATE_ON  (1U)

typedef enum {
    RELAY_OUT_0 = 0U,
    RELAY_OUT_1,
    RELAY_OUT_2,
    RELAY_OUT_3,
    RELAY_OUT_4,
    RELAY_OUT_5,
    RELAY_OUT_6,
    RELAY_OUT_7,
    RELAY_OUT_8,
    RELAY_OUT_COUNT,
    RELAY_OUT_INVALID = 0xFFU
} relay_channel_t;

void BSP_Relay_Init(void);
uint8_t BSP_Relay_On(relay_channel_t relay);
uint8_t BSP_Relay_Off(relay_channel_t relay);
uint8_t BSP_Relay_Set(relay_channel_t relay, uint8_t state);
uint8_t BSP_Relay_GetCommandState(relay_channel_t relay);
void BSP_Relay_AllOff(void);

#endif
