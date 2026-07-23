#ifndef EEV_CONTROL_H
#define EEV_CONTROL_H

#include "r_cg_macrodriver.h"

#define EEV_DEFAULT_STEP_INTERVAL_MS (50U)
#define EEV_MIN_STEP_INTERVAL_MS     (2U)
#define EEV_MAX_STEP_INTERVAL_MS     (100U)
#define EEV_MAX_STEPS              (4096U)
#define EEV_POWER_ON_TEST_STEPS    (64U)
#define EEV_POWER_ON_TEST_ENABLE   (0U)

void EEV_Init(void);
void EEV_TimerTick1ms(void);
void EEV_Task_1ms(void);
void EEV_TestTask(void);

void EEV_OpenSteps(uint16_t steps);
void EEV_CloseSteps(uint16_t steps);
void EEV_MoveTo(uint16_t target_step);
void EEV_Stop(void);
uint8_t EEV_IsBusy(void);
uint16_t EEV_GetCurrentStep(void);
uint8_t EEV_DebugHoldPhase(uint8_t phase_index);
uint8_t EEV_SetStepIntervalMs(uint16_t interval_ms);
uint16_t EEV_GetStepIntervalMs(void);

#endif
