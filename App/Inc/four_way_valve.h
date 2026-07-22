#ifndef FOUR_WAY_VALVE_H
#define FOUR_WAY_VALVE_H

#include "r_cg_macrodriver.h"
#include "bsp_relay.h"

#define FOUR_WAY_VALVE_RELAY              (RELAY_OUT_INVALID)
#define FOUR_WAY_SYSTEM_STABLE_WAIT_MS    (5000UL)
#define FOUR_WAY_VALVE_ACTION_WAIT_MS     (3000UL)

#define FOUR_WAY_MODE_HEAT (0U)
#define FOUR_WAY_MODE_COOL (1U)

typedef enum {
    FOUR_WAY_STATE_IDLE = 0U,
    FOUR_WAY_STATE_WAIT_COMPRESSOR_STOP,
    FOUR_WAY_STATE_WAIT_SYSTEM_STABLE,
    FOUR_WAY_STATE_SET_RELAY,
    FOUR_WAY_STATE_WAIT_VALVE_STABLE,
    FOUR_WAY_STATE_DONE,
    FOUR_WAY_STATE_BLOCKED
} four_way_state_t;

void FourWayValve_Init(void);
void FourWayValve_Task(void);
void FourWayValve_TimerTick1ms(void);
void FourWayValve_RequestCooling(void);
void FourWayValve_RequestHeating(void);
uint8_t FourWayValve_GetMode(void);
uint8_t FourWayValve_GetRequestedMode(void);
four_way_state_t FourWayValve_GetState(void);
uint32_t FourWayValve_GetRemainMs(void);

#endif
