#include "bsp_eev.h"

/*
 * CNF1 electronic expansion valve coil mapping:
 *
 * A1 = P54
 * B1 = P55
 * C1 = P15
 * D1 = P10
 *
 * U6 is a low-side driver.
 * MCU output high = corresponding coil ON.
 */

static void BSP_EEV_SetGpioOutputMode(void);

void BSP_EEV_Init(void)
{
    BSP_EEV_AllOff();
    BSP_EEV_SetGpioOutputMode();
}

void BSP_EEV_OutputPhase(uint8_t phase_mask)
{
    P5_bit.no4 = ((phase_mask & BSP_EEV_PHASE_A) != 0U) ? 1U : 0U;
    P5_bit.no5 = ((phase_mask & BSP_EEV_PHASE_B) != 0U) ? 1U : 0U;
    P1_bit.no5 = ((phase_mask & BSP_EEV_PHASE_C) != 0U) ? 1U : 0U;
    P1_bit.no0 = ((phase_mask & BSP_EEV_PHASE_D) != 0U) ? 1U : 0U;
}

void BSP_EEV_AllOff(void)
{
    P5_bit.no4 = 0U;
    P5_bit.no5 = 0U;
    P1_bit.no5 = 0U;
    P1_bit.no0 = 0U;
}

static void BSP_EEV_SetGpioOutputMode(void)
{
    PM5_bit.no4 = 0U;
    PM5_bit.no5 = 0U;
    PM1_bit.no5 = 0U;
    PM1_bit.no0 = 0U;
}

