#ifndef BSP_EEV_H
#define BSP_EEV_H

#include "r_cg_macrodriver.h"

#define BSP_EEV_PHASE_A  (0x01U)
#define BSP_EEV_PHASE_B  (0x02U)
#define BSP_EEV_PHASE_C  (0x04U)
#define BSP_EEV_PHASE_D  (0x08U)

void BSP_EEV_Init(void);
void BSP_EEV_OutputPhase(uint8_t phase_mask);
void BSP_EEV_AllOff(void);

#endif

