#ifndef HIGH_PRESSURE_PROTECTION_H
#define HIGH_PRESSURE_PROTECTION_H

#include "r_cg_macrodriver.h"
#include "switch_input.h"

#define HIGH_PRESSURE_SWITCH_CHANNEL           (SWITCH_K0)
#define HIGH_PRESSURE_CONFIRM_MS               (5000UL)
#define HIGH_PRESSURE_RECOVERY_STOP_MS         (180000UL)
#define HIGH_PRESSURE_FAULT_WINDOW_MS          (3600000UL)
#define HIGH_PRESSURE_LOCK_COUNT               (3U)
#define HIGH_PRESSURE_LOCK_MS                  (3600000UL)
#define HIGH_PRESSURE_FAULT_CODE               (0x0001U)

void HighPressureProtection_Init(void);
void HighPressureProtection_Task(void);
void HighPressureProtection_TimerTick1ms(void);

uint8_t HighPressureProtection_GetSwitchClosed(void);
uint8_t HighPressureProtection_IsFaultActive(void);
uint8_t HighPressureProtection_IsLocked(void);
uint8_t HighPressureProtection_GetFaultCountInWindow(void);
uint32_t HighPressureProtection_GetRemainMs(void);
uint16_t HighPressureProtection_GetFaultCode(void);
uint8_t HighPressureProtection_GetState(void);
uint16_t HighPressureProtection_GetTimerSec(void);
uint16_t HighPressureProtection_GetFaultElapsedSec(void);

#endif
