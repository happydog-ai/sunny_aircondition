#ifndef SWITCH_INPUT_H
#define SWITCH_INPUT_H

#include "r_cg_macrodriver.h"

#define SWITCH_INPUT_COUNT          (10U)
#define SWITCH_INPUT_SCAN_PERIOD_MS (5U)
#define SWITCH_INPUT_SETTLE_MS      (1U)
#define SWITCH_INPUT_DEBOUNCE_COUNT (3U)

typedef enum {
    SWITCH_K0 = 0U,
    SWITCH_K1,
    SWITCH_K2,
    SWITCH_K3,
    SWITCH_K4,
    SWITCH_K5,
    SWITCH_K6,
    SWITCH_K7,
    SWITCH_K8,
    SWITCH_K9,
    SWITCH_INVALID = 0xFFU
} switch_channel_t;

#define SWITCH_STATE_OPEN   (0U)
#define SWITCH_STATE_CLOSED (1U)

void SwitchInput_Init(void);
void SwitchInput_Task(void);
void SwitchInput_TimerTick1ms(void);
uint8_t SwitchInput_GetStable(switch_channel_t channel);
uint8_t SwitchInput_GetRaw(switch_channel_t channel);

#endif
