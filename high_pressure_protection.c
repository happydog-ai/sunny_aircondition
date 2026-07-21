#include "high_pressure_protection.h"
#include "driver_board_comm.h"

typedef enum {
    HP_STATE_NORMAL = 0U,
    HP_STATE_CONFIRMING,
    HP_STATE_FAULT_ACTIVE,
    HP_STATE_RECOVERY_WAIT,
    HP_STATE_LOCKED
} hp_state_t;

static hp_state_t g_hp_state;
static volatile uint32_t g_hp_timer_ms;
static uint32_t g_hp_fault_window_ms;
static uint8_t g_hp_fault_count;
static uint16_t g_hp_fault_code;
static uint8_t g_hp_saved_running_state;
static volatile uint32_t g_hp_fault_elapsed_ms;

static void HighPressure_RecordFault(void);
static void HighPressure_EnterFault(void);
static uint8_t HighPressure_CompressorRunning(void);

void HighPressureProtection_Init(void)
{
    g_hp_state = HP_STATE_NORMAL;
    g_hp_timer_ms = 0UL;
    g_hp_fault_window_ms = 0UL;
    g_hp_fault_count = 0U;
    g_hp_fault_code = 0U;
    g_hp_saved_running_state = 0U;
    g_hp_fault_elapsed_ms = 0UL;
}

void HighPressureProtection_TimerTick1ms(void)
{
    if (g_hp_timer_ms > 0UL)
    {
        g_hp_timer_ms--;
    }

    if (g_hp_state == HP_STATE_FAULT_ACTIVE ||
        g_hp_state == HP_STATE_LOCKED ||
        g_hp_state == HP_STATE_RECOVERY_WAIT)
    {
        if (g_hp_fault_elapsed_ms < 0xFFFFFFFFUL)
        {
            g_hp_fault_elapsed_ms++;
        }
    }
    else
    {
        g_hp_fault_elapsed_ms = 0UL;
    }

    if (g_hp_fault_window_ms > 0UL)
    {
        g_hp_fault_window_ms--;
        if (g_hp_fault_window_ms == 0UL)
        {
            g_hp_fault_count = 0U;
        }
    }
}

void HighPressureProtection_Task(void)
{
    uint8_t switch_closed;

    switch_closed = HighPressureProtection_GetSwitchClosed();

    switch (g_hp_state)
    {
        case HP_STATE_NORMAL:
            g_hp_fault_code = 0U;
            if (HighPressure_CompressorRunning() && switch_closed == 0U)
            {
                g_hp_timer_ms = HIGH_PRESSURE_CONFIRM_MS;
                g_hp_state = HP_STATE_CONFIRMING;
            }
            break;

        case HP_STATE_CONFIRMING:
            if (!HighPressure_CompressorRunning() || switch_closed != 0U)
            {
                g_hp_state = HP_STATE_NORMAL;
            }
            else if (g_hp_timer_ms == 0UL)
            {
                HighPressure_EnterFault();
            }
            break;

        case HP_STATE_FAULT_ACTIVE:
            (void)DriverBoard_RequestCompressorStop();
            if (DriverBoard_IsCompressorStopped() && switch_closed != 0U)
            {
                g_hp_timer_ms = HIGH_PRESSURE_RECOVERY_STOP_MS;
                g_hp_state = HP_STATE_RECOVERY_WAIT;
            }
            break;

        case HP_STATE_RECOVERY_WAIT:
            if (switch_closed == 0U)
            {
                g_hp_state = HP_STATE_FAULT_ACTIVE;
            }
            else if (g_hp_timer_ms == 0UL)
            {
                g_hp_fault_code = 0U;
                if (g_hp_saved_running_state != 0U)
                {
                    (void)DriverBoard_RequestRestorePreviousState();
                }
                g_hp_state = HP_STATE_NORMAL;
            }
            break;

        case HP_STATE_LOCKED:
            (void)DriverBoard_RequestCompressorStop();
            if (g_hp_timer_ms == 0UL)
            {
                g_hp_fault_count = 0U;
                g_hp_fault_code = 0U;
                g_hp_state = HP_STATE_NORMAL;
            }
            break;

        default:
            g_hp_state = HP_STATE_NORMAL;
            break;
    }
}

uint8_t HighPressureProtection_GetSwitchClosed(void)
{
    return SwitchInput_GetStable(HIGH_PRESSURE_SWITCH_CHANNEL);
}

uint8_t HighPressureProtection_IsFaultActive(void)
{
    return (g_hp_state == HP_STATE_FAULT_ACTIVE ||
            g_hp_state == HP_STATE_RECOVERY_WAIT ||
            g_hp_state == HP_STATE_LOCKED) ? 1U : 0U;
}

uint8_t HighPressureProtection_IsLocked(void)
{
    return (g_hp_state == HP_STATE_LOCKED) ? 1U : 0U;
}

uint8_t HighPressureProtection_GetFaultCountInWindow(void)
{
    return g_hp_fault_count;
}

uint32_t HighPressureProtection_GetRemainMs(void)
{
    return g_hp_timer_ms;
}

uint16_t HighPressureProtection_GetFaultCode(void)
{
    return g_hp_fault_code;
}

uint8_t HighPressureProtection_GetState(void)
{
    return (uint8_t)g_hp_state;
}

uint16_t HighPressureProtection_GetTimerSec(void)
{
    return (uint16_t)(g_hp_timer_ms / 1000UL);
}

uint16_t HighPressureProtection_GetFaultElapsedSec(void)
{
    return (uint16_t)(g_hp_fault_elapsed_ms / 1000UL);
}

static void HighPressure_RecordFault(void)
{
    if (g_hp_fault_window_ms == 0UL)
    {
        g_hp_fault_window_ms = HIGH_PRESSURE_FAULT_WINDOW_MS;
        g_hp_fault_count = 0U;
    }

    if (g_hp_fault_count < 0xFFU)
    {
        g_hp_fault_count++;
    }
}

static void HighPressure_EnterFault(void)
{
    g_hp_saved_running_state = HighPressure_CompressorRunning();
    g_hp_fault_code = HIGH_PRESSURE_FAULT_CODE;
    HighPressure_RecordFault();
    (void)DriverBoard_RequestCompressorStop();

    if (g_hp_fault_count >= HIGH_PRESSURE_LOCK_COUNT)
    {
        g_hp_timer_ms = HIGH_PRESSURE_LOCK_MS;
        g_hp_state = HP_STATE_LOCKED;
    }
    else
    {
        g_hp_state = HP_STATE_FAULT_ACTIVE;
    }
}

static uint8_t HighPressure_CompressorRunning(void)
{
    return DriverBoard_IsCompressorRunning();
}
