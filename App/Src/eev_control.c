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

static volatile uint16_t g_eev_current_step;
static volatile uint16_t g_eev_target_step;
static volatile uint16_t g_eev_move_remaining;
static volatile uint8_t g_eev_busy;
static volatile uint8_t g_eev_relative_move;
static volatile uint8_t g_eev_phase_index;
static volatile int8_t g_eev_direction;
static volatile uint16_t g_eev_step_timer_ms;
static volatile uint16_t g_eev_step_interval_ms;
static eev_test_state_t g_eev_test_state;

static void EEV_StartMoveTo(uint16_t target_step);
static void EEV_StartRelativeMove(int8_t direction, uint16_t steps);
static void EEV_Process1ms(void);
static void EEV_DoOneStep(void);
static void EEV_StopInternal(void);
static uint16_t EEV_LimitStep(uint16_t step);

void EEV_Init(void)
{
    BSP_EEV_Init();
    g_eev_current_step = 0U;
    g_eev_target_step = 0U;
    g_eev_move_remaining = 0U;
    g_eev_busy = 0U;
    g_eev_relative_move = 0U;
    g_eev_phase_index = 0U;
    g_eev_direction = EEV_DIRECTION_OPEN;
    g_eev_step_timer_ms = 0U;
    g_eev_step_interval_ms = EEV_DEFAULT_STEP_INTERVAL_MS;
    g_eev_test_state = EEV_TEST_IDLE;
}

void EEV_TimerTick1ms(void)
{
    EEV_Process1ms();
}

void EEV_Task_1ms(void)
{
    /* EEV phase timing is handled in TAU0 channel 1 interrupt now.
     * Keep this API so the main loop does not need generated-code changes.
     */
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
    EEV_StartRelativeMove(EEV_DIRECTION_OPEN, steps);
}

void EEV_CloseSteps(uint16_t steps)
{
    EEV_StartRelativeMove(EEV_DIRECTION_CLOSE, steps);
}

void EEV_MoveTo(uint16_t target_step)
{
    EEV_StartMoveTo(EEV_LimitStep(target_step));
}

void EEV_Stop(void)
{
    DI();
    EEV_StopInternal();
    EI();
}

uint8_t EEV_IsBusy(void)
{
    uint8_t busy;

    DI();
    busy = g_eev_busy;
    EI();

    return busy;
}

uint16_t EEV_GetCurrentStep(void)
{
    uint16_t step;

    DI();
    step = g_eev_current_step;
    EI();

    return step;
}

uint8_t EEV_DebugHoldPhase(uint8_t phase_index)
{
    if (phase_index >= 8U)
    {
        return 0U;
    }

    DI();
    EEV_StopInternal();
    g_eev_phase_index = phase_index;
    BSP_EEV_OutputPhase(g_eev_phase_table[phase_index]);
    EI();

    return 1U;
}

uint8_t EEV_SetStepIntervalMs(uint16_t interval_ms)
{
    if ((interval_ms < EEV_MIN_STEP_INTERVAL_MS) ||
        (interval_ms > EEV_MAX_STEP_INTERVAL_MS))
    {
        return 0U;
    }

    DI();
    g_eev_step_interval_ms = interval_ms;
    EI();

    return 1U;
}

uint16_t EEV_GetStepIntervalMs(void)
{
    uint16_t interval_ms;

    DI();
    interval_ms = g_eev_step_interval_ms;
    EI();

    return interval_ms;
}

static void EEV_StartMoveTo(uint16_t target_step)
{
    target_step = EEV_LimitStep(target_step);

    DI();
    g_eev_target_step = target_step;
    g_eev_relative_move = 0U;
    g_eev_move_remaining = 0U;
    g_eev_step_timer_ms = 0U;

    if (g_eev_target_step == g_eev_current_step)
    {
        EEV_StopInternal();
        EI();
        return;
    }

    g_eev_direction = (g_eev_target_step > g_eev_current_step) ?
        EEV_DIRECTION_OPEN : EEV_DIRECTION_CLOSE;
    g_eev_busy = 1U;
    EI();
}

static void EEV_StartRelativeMove(int8_t direction, uint16_t steps)
{
    if (steps == 0U)
    {
        EEV_Stop();
        return;
    }

    DI();
    g_eev_direction = direction;
    g_eev_move_remaining = steps;
    g_eev_relative_move = 1U;
    g_eev_step_timer_ms = 0U;

    if (direction == EEV_DIRECTION_OPEN)
    {
        if (steps > (uint16_t)(EEV_MAX_STEPS - g_eev_current_step))
        {
            g_eev_target_step = EEV_MAX_STEPS;
        }
        else
        {
            g_eev_target_step = (uint16_t)(g_eev_current_step + steps);
        }
    }
    else
    {
        if (steps > g_eev_current_step)
        {
            g_eev_target_step = 0U;
        }
        else
        {
            g_eev_target_step = (uint16_t)(g_eev_current_step - steps);
        }
    }

    g_eev_busy = 1U;
    EI();
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

    if (g_eev_relative_move != 0U)
    {
        if (g_eev_move_remaining > 0U)
        {
            g_eev_move_remaining--;
        }

        if (g_eev_move_remaining == 0U)
        {
            EEV_StopInternal();
        }
    }
    else if (g_eev_current_step == g_eev_target_step)
    {
        EEV_StopInternal();
    }
}

static void EEV_StopInternal(void)
{
    g_eev_busy = 0U;
    g_eev_relative_move = 0U;
    g_eev_move_remaining = 0U;
    g_eev_step_timer_ms = 0U;
    BSP_EEV_AllOff();
}

static uint16_t EEV_LimitStep(uint16_t step)
{
    return (step > EEV_MAX_STEPS) ? EEV_MAX_STEPS : step;
}
