#include "four_way_valve.h"
#include "high_pressure_protection.h"
#include "driver_board_comm.h"

static uint8_t g_four_way_mode;
static uint8_t g_four_way_requested_mode;
static four_way_state_t g_four_way_state;
static volatile uint32_t g_four_way_timer_ms;

static void FourWayValve_RequestMode(uint8_t mode);

void FourWayValve_Init(void)
{
    g_four_way_mode = FOUR_WAY_MODE_HEAT;
    g_four_way_requested_mode = FOUR_WAY_MODE_HEAT;
    g_four_way_state = FOUR_WAY_STATE_IDLE;
    g_four_way_timer_ms = 0UL;
    (void)BSP_Relay_Off(FOUR_WAY_VALVE_RELAY);
}

void FourWayValve_TimerTick1ms(void)
{
    if (g_four_way_timer_ms > 0UL)
    {
        g_four_way_timer_ms--;
    }
}

void FourWayValve_Task(void)
{
    switch (g_four_way_state)
    {
        case FOUR_WAY_STATE_IDLE:
            break;

        case FOUR_WAY_STATE_WAIT_COMPRESSOR_STOP:
            (void)DriverBoard_RequestCompressorStop();
            if (DriverBoard_IsCompressorStopped())
            {
                g_four_way_timer_ms = FOUR_WAY_SYSTEM_STABLE_WAIT_MS;
                g_four_way_state = FOUR_WAY_STATE_WAIT_SYSTEM_STABLE;
            }
            break;

        case FOUR_WAY_STATE_WAIT_SYSTEM_STABLE:
            if (g_four_way_timer_ms == 0UL)
            {
                g_four_way_state = FOUR_WAY_STATE_SET_RELAY;
            }
            break;

        case FOUR_WAY_STATE_SET_RELAY:
            if (g_four_way_requested_mode == FOUR_WAY_MODE_COOL)
            {
                (void)BSP_Relay_On(FOUR_WAY_VALVE_RELAY);
            }
            else
            {
                (void)BSP_Relay_Off(FOUR_WAY_VALVE_RELAY);
            }
            g_four_way_timer_ms = FOUR_WAY_VALVE_ACTION_WAIT_MS;
            g_four_way_state = FOUR_WAY_STATE_WAIT_VALVE_STABLE;
            break;

        case FOUR_WAY_STATE_WAIT_VALVE_STABLE:
            if (g_four_way_timer_ms == 0UL)
            {
                if (HighPressureProtection_IsFaultActive() ||
                    HighPressureProtection_IsLocked())
                {
                    g_four_way_state = FOUR_WAY_STATE_BLOCKED;
                }
                else
                {
                    g_four_way_mode = g_four_way_requested_mode;
                    g_four_way_state = FOUR_WAY_STATE_DONE;
                }
            }
            break;

        case FOUR_WAY_STATE_DONE:
            g_four_way_state = FOUR_WAY_STATE_IDLE;
            break;

        case FOUR_WAY_STATE_BLOCKED:
        default:
            break;
    }
}

void FourWayValve_RequestCooling(void)
{
    FourWayValve_RequestMode(FOUR_WAY_MODE_COOL);
}

void FourWayValve_RequestHeating(void)
{
    FourWayValve_RequestMode(FOUR_WAY_MODE_HEAT);
}

uint8_t FourWayValve_GetMode(void)
{
    return g_four_way_mode;
}

uint8_t FourWayValve_GetRequestedMode(void)
{
    return g_four_way_requested_mode;
}

four_way_state_t FourWayValve_GetState(void)
{
    return g_four_way_state;
}

uint32_t FourWayValve_GetRemainMs(void)
{
    return g_four_way_timer_ms;
}

static void FourWayValve_RequestMode(uint8_t mode)
{
    g_four_way_requested_mode = (mode != 0U) ? FOUR_WAY_MODE_COOL : FOUR_WAY_MODE_HEAT;

    if (g_four_way_requested_mode == g_four_way_mode)
    {
        g_four_way_state = FOUR_WAY_STATE_IDLE;
        return;
    }

    g_four_way_state = FOUR_WAY_STATE_WAIT_COMPRESSOR_STOP;
}
