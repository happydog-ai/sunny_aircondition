#include "bsp_switch_mux.h"

static uint8_t g_switch_mux_selected;

void BSP_SwitchMux_Init(void)
{
    ADPC = 0x05U;

    PM4_bit.no2 = 0U;
    PM4_bit.no3 = 0U;
    PM12_bit.no0 = 0U;
    PM2_bit.no5 = 1U;
    PM2_bit.no6 = 1U;

    g_switch_mux_selected = 0U;
    BSP_SwitchMux_Select(0U);
}

void BSP_SwitchMux_Select(uint8_t channel)
{
    channel &= 0x07U;

    P4_bit.no2 = (uint8_t)(channel & 0x01U);
    P4_bit.no3 = (uint8_t)((channel >> 1U) & 0x01U);
    P12_bit.no0 = (uint8_t)((channel >> 2U) & 0x01U);

    g_switch_mux_selected = channel;
}

uint8_t BSP_SwitchMux_GetSelected(void)
{
    return g_switch_mux_selected;
}

uint8_t BSP_SwitchMux_ReadK1Raw(void)
{
    return (uint8_t)(P2_bit.no6 & 0x01U);
}

uint8_t BSP_SwitchMux_ReadK2Raw(void)
{
    return (uint8_t)(P2_bit.no5 & 0x01U);
}
