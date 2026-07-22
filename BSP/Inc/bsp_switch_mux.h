#ifndef BSP_SWITCH_MUX_H
#define BSP_SWITCH_MUX_H

#include "r_cg_macrodriver.h"

#define BSP_SWITCH_MUX_CHANNEL_COUNT (8U)

#define BSP_SWITCH_MUX_OWNER_NONE        (0U)
#define BSP_SWITCH_MUX_OWNER_SWITCH      (1U)
#define BSP_SWITCH_MUX_OWNER_TEMPERATURE (2U)

void BSP_SwitchMux_Init(void);
uint8_t BSP_SwitchMux_Request(uint8_t owner);
void BSP_SwitchMux_Release(uint8_t owner);
void BSP_SwitchMux_Select(uint8_t channel);
uint8_t BSP_SwitchMux_GetSelected(void);
uint8_t BSP_SwitchMux_GetOwner(void);
uint8_t BSP_SwitchMux_ReadK1Raw(void);
uint8_t BSP_SwitchMux_ReadK2Raw(void);

#endif
