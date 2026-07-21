#ifndef BSP_SWITCH_MUX_H
#define BSP_SWITCH_MUX_H

#include "r_cg_macrodriver.h"

#define BSP_SWITCH_MUX_CHANNEL_COUNT (8U)

void BSP_SwitchMux_Init(void);
void BSP_SwitchMux_Select(uint8_t channel);
uint8_t BSP_SwitchMux_GetSelected(void);
uint8_t BSP_SwitchMux_ReadK1Raw(void);
uint8_t BSP_SwitchMux_ReadK2Raw(void);

#endif
