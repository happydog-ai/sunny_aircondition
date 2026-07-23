#include "eev_control.h"
#include "bsp_eev.h"

#define EEV_DIRECTION_OPEN   (1)
#define EEV_DIRECTION_CLOSE  (-1)

typedef enum {
    EEV_TEST_IDLE = 0U,
    EEV_TEST_OPENING,
    EEV_TEST_CLOSING,
    EEV_TEST_DONE
} eev_test_state_t;

static const uint8_t g_eev_phase_table[8] = {
    BSP_EEV_PHASE_A,
    (uint8_t)(BSP_EEV_PHASE_A | BSP_EEV_PHASE_B),
    BSP_EEV_PHASE_B,
    (uint8_t)(BSP_EEV_PHASE_B | BSP_EEV_PHASE_C),
    BSP_EEV_PHASE_C,
    (uint8_t)(BSP_EEV_PHASE_C | BSP_EEV_PHASE_D),
    BSP_EEV_PHASE_D,
    (uint8_t)(BSP_EEV_PHASE_D | BSP_EEV_PHASE_A)
};

static volatile uint8_t g_eev_tick_pending;
static uint16_t g_eev_current_step;
static uint16_t g_eev_target_step;
static uint8_t g_eev_busy;
static uint8_t g_eev_phase_index;
static int8_t g_eev_direction;
static uint16_t g_eev_step_timer_ms;
static uint16_t g_eev_step_interval_ms;
static eev_test_state_t g_eev_test_state;

static void EEV_StartMoveTo(uint16_t target_step);
static void EEV_Process1ms(void);
static void EEV_DoOneStep(void);
static uint16_t EEV_LimitStep(uint16_t step);

void EEV_Init(void)
{
    BSP_EEV_Init();
    g_eev_tick_pending = 0U;
    g_eev_current_step = 0U;
    g_eev_target_step = 0U;
    g_eev_busy = 0U;
    g_eev_phase_index = 0U;
    g_eev_direction = EEV_DIRECTION_OPEN;
    g_eev_step_timer_ms = 0U;
    g_eev_step_interval_ms = EEV_DEFAULT_STEP_INTERVAL_MS;
    g_eev_test_state = EEV_TEST_IDLE;
}

void EEV_TimerTick1ms(void)
{
    if (g_eev_tick_pending < 0xFFU)
    {
        g_eev_tick_pending++;
    }
}

void EEV_Task_1ms(void)
{
    while (g_eev_tick_pending > 0U)
    {
        g_eev_tick_pending--;
        EEV_Process1ms();
    }
}

void EEV_TestTask(void)
{
#if (EEV_POWER_ON_TEST_ENABLE != 0U)
    switch (g_eev_test_state)
    {
        case EEV_TEST_IDLE:
            EEV_OpenSteps(EEV_POWER_ON_TEST_STEPS);
            g_eev_test_state = EEV_TEST_OPENING;
            break;

        case EEV_TEST_OPENING:
            if (EEV_IsBusy() == 0U)
            {
                EEV_CloseSteps(EEV_POWER_ON_TEST_STEPS);
                g_eev_test_state = EEV_TEST_CLOSING;
            }
            break;

        case EEV_TEST_CLOSING:
            if (EEV_IsBusy() == 0U)
            {
                g_eev_test_state = EEV_TEST_DONE;
            }
            break;

        case EEV_TEST_DONE:
        default:
            break;
    }
#endif
}

void EEV_OpenSteps(uint16_t steps)
{
    uint16_t target;

    if (steps > (uint16_t)(EEV_MAX_STEPS - g_eev_current_step))
    {
        target = EEV_MAX_STEPS;
    }
    else
    {
        target = (uint16_t)(g_eev_current_step + steps);
    }

    EEV_StartMoveTo(target);
}

void EEV_CloseSteps(uint16_t steps)
{
    uint16_t target;

    if (steps > g_eev_current_step)
    {
        target = 0U;
    }
    else
    {
        target = (uint16_t)(g_eev_current_step - steps);
    }

    EEV_StartMoveTo(target);
}

void EEV_MoveTo(uint16_t target_step)
{
    EEV_StartMoveTo(EEV_LimitStep(target_step));
}

void EEV_Stop(void)
{
    g_eev_busy = 0U;
    g_eev_step_timer_ms = 0U;
    BSP_EEV_AllOff();
}

uint8_t EEV_IsBusy(void)
{
    return g_eev_busy;
}

uint16_t EEV_GetCurrentStep(void)
{
    return g_eev_current_step;
}

uint8_t EEV_DebugHoldPhase(uint8_t phase_index)
{
    if (phase_index >= 8U)
    {
        return 0U;
    }

    g_eev_busy = 0U;
    g_eev_step_timer_ms = 0U;
    g_eev_phase_index = phase_index;
    BSP_EEV_OutputPhase(g_eev_phase_table[phase_index]);
    return 1U;
}

uint8_t EEV_SetStepIntervalMs(uint16_t interval_ms)
{
    if ((interval_ms < EEV_MIN_STEP_INTERVAL_MS) ||
        (interval_ms > EEV_MAX_STEP_INTERVAL_MS))
    {
        return 0U;
    }

    g_eev_step_interval_ms = interval_ms;
    return 1U;
}

uint16_t EEV_GetStepIntervalMs(void)
{
    return g_eev_step_interval_ms;
}

static void EEV_StartMoveTo(uint16_t target_step)
{
    target_step = EEV_LimitStep(target_step);
    g_eev_target_step = target_step;
    g_eev_step_timer_ms = 0U;

    if (g_eev_target_step == g_eev_current_step)
    {
        EEV_Stop();
        return;
    }

    g_eev_direction = (g_eev_target_step > g_eev_current_step) ?
        EEV_DIRECTION_OPEN : EEV_DIRECTION_CLOSE;
    g_eev_busy = 1U;
}

static void EEV_Process1ms(void)
{
    if (g_eev_busy == 0U)
    {
        return;
    }

    if (g_eev_step_timer_ms < g_eev_step_interval_ms)
    {
        g_eev_step_timer_ms++;
    }

    if (g_eev_step_timer_ms >= g_eev_step_interval_ms)
    {
        g_eev_step_timer_ms = 0U;
        EEV_DoOneStep();
    }
}

static void EEV_DoOneStep(void)
{
    BSP_EEV_OutputPhase(g_eev_phase_table[g_eev_phase_index]);

    if (g_eev_direction == EEV_DIRECTION_OPEN)
    {
        if (g_eev_current_step < EEV_MAX_STEPS)
        {
            g_eev_current_step++;
        }
        g_eev_phase_index = (uint8_t)((g_eev_phase_index + 1U) & 0x07U);
    }
    else
    {
        if (g_eev_current_step > 0U)
        {
            g_eev_current_step--;
        }
        g_eev_phase_index = (g_eev_phase_index == 0U) ? 7U : (uint8_t)(g_eev_phase_index - 1U);
    }

    if (g_eev_current_step == g_eev_target_step)
    {
        EEV_Stop();
    }
}

static uint16_t EEV_LimitStep(uint16_t step)
{
    return (step > EEV_MAX_STEPS) ? EEV_MAX_STEPS : step;
}
