#include "bsp_relay.h"

/*
 * Confirmed relay mapping:
 *
 * OUT1 = RL1 = P06
 * OUT2 = RL2 = P70
 * OUT3 = RL3 = P71
 * OUT4 = RL4 = P72
 * OUT5 = RL5 = P73
 * OUT6 = RL6 = P74
 * OUT7 = RL7 = P75
 * OUT8 = RL8 = P76
 * OUT9 = RL9 = P31
 *
 * Control polarity:
 *   MCU output high = relay ON
 *   MCU output low  = relay OFF
 */

static uint8_t g_relay_command_state[RELAY_OUT_COUNT];

static uint8_t BSP_Relay_WriteGpio(relay_channel_t relay, uint8_t state);
static void BSP_Relay_ConfigGpioOutput(void);

void BSP_Relay_Init(void)
{
    uint8_t i;

    /*
     * Latch all relay outputs low before enabling output mode.
     */
    for (i = 0U; i < (uint8_t)RELAY_OUT_COUNT; i++)
    {
        g_relay_command_state[i] = RELAY_STATE_OFF;
        (void)BSP_Relay_WriteGpio((relay_channel_t)i, RELAY_STATE_OFF);
    }

    BSP_Relay_ConfigGpioOutput();
}

uint8_t BSP_Relay_On(relay_channel_t relay)
{
    return BSP_Relay_Set(relay, RELAY_STATE_ON);
}

uint8_t BSP_Relay_Off(relay_channel_t relay)
{
    return BSP_Relay_Set(relay, RELAY_STATE_OFF);
}

uint8_t BSP_Relay_Set(relay_channel_t relay, uint8_t state)
{
    uint8_t normalized;

    if ((uint8_t)relay >= (uint8_t)RELAY_OUT_COUNT)
    {
        return 0U;
    }

    normalized = (state != 0U) ? RELAY_STATE_ON : RELAY_STATE_OFF;
    g_relay_command_state[(uint8_t)relay] = normalized;

    return BSP_Relay_WriteGpio(relay, normalized);
}

uint8_t BSP_Relay_GetCommandState(relay_channel_t relay)
{
    if ((uint8_t)relay >= (uint8_t)RELAY_OUT_COUNT)
    {
        return RELAY_STATE_OFF;
    }
    return g_relay_command_state[(uint8_t)relay];
}

void BSP_Relay_AllOff(void)
{
    uint8_t i;

    for (i = 0U; i < (uint8_t)RELAY_OUT_COUNT; i++)
    {
        (void)BSP_Relay_Off((relay_channel_t)i);
    }
}

static uint8_t BSP_Relay_WriteGpio(relay_channel_t relay, uint8_t state)
{
    if ((uint8_t)relay >= (uint8_t)RELAY_OUT_COUNT)
    {
        return 0U;
    }

    state = (state != 0U) ? 1U : 0U;

    switch (relay)
    {
        case RELAY_OUT_0:
            P0_bit.no6 = state;
            return 1U;

        case RELAY_OUT_1:
            P7_bit.no0 = state;
            return 1U;

        case RELAY_OUT_2:
            P7_bit.no1 = state;
            return 1U;

        case RELAY_OUT_3:
            P7_bit.no2 = state;
            return 1U;

        case RELAY_OUT_4:
            P7_bit.no3 = state;
            return 1U;

        case RELAY_OUT_5:
            P7_bit.no4 = state;
            return 1U;

        case RELAY_OUT_6:
            P7_bit.no5 = state;
            return 1U;

        case RELAY_OUT_7:
            P7_bit.no6 = state;
            return 1U;

        case RELAY_OUT_8:
            P3_bit.no1 = state;
            return 1U;

        default:
            break;
    }

    return 0U;
}

static void BSP_Relay_ConfigGpioOutput(void)
{
    PM0_bit.no6 = 0U;
    PM7_bit.no0 = 0U;
    PM7_bit.no1 = 0U;
    PM7_bit.no2 = 0U;
    PM7_bit.no3 = 0U;
    PM7_bit.no4 = 0U;
    PM7_bit.no5 = 0U;
    PM7_bit.no6 = 0U;
    PM3_bit.no1 = 0U;
}
