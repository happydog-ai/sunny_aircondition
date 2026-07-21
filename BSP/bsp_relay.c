#include "bsp_relay.h"

#define RELAY_GPIO_UNCONFIRMED (0xFFU)

typedef struct {
    uint8_t port;
    uint8_t bit;
} relay_gpio_t;

/*
 * Relay-to-GPIO mapping is intentionally left unassigned until OUT mapping is confirmed.
 * Fill port/bit here only after schematic or board test confirms the real relay output.
 */
static const relay_gpio_t g_relay_gpio[RELAY_OUT_COUNT] = {
    { RELAY_GPIO_UNCONFIRMED, 0U },
    { RELAY_GPIO_UNCONFIRMED, 0U },
    { RELAY_GPIO_UNCONFIRMED, 0U },
    { RELAY_GPIO_UNCONFIRMED, 0U }
};

static uint8_t g_relay_command_state[RELAY_OUT_COUNT];

static uint8_t BSP_Relay_WriteGpio(relay_channel_t relay, uint8_t state);

void BSP_Relay_Init(void)
{
    uint8_t i;

    for (i = 0U; i < (uint8_t)RELAY_OUT_COUNT; i++)
    {
        g_relay_command_state[i] = RELAY_STATE_OFF;
        (void)BSP_Relay_WriteGpio((relay_channel_t)i, RELAY_STATE_OFF);
    }
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
    const relay_gpio_t *gpio;

    if ((uint8_t)relay >= (uint8_t)RELAY_OUT_COUNT)
    {
        return 0U;
    }

    gpio = &g_relay_gpio[(uint8_t)relay];

    if (gpio->port == RELAY_GPIO_UNCONFIRMED)
    {
        /*
         * Mapping unknown: keep command state only and do not touch any GPIO.
         */
        return 0U;
    }

    /*
     * Add confirmed GPIO writes here, for example:
     * if (gpio->port == 7U && gpio->bit == 0U) { P7_bit.no0 = state; return 1U; }
     */
    (void)state;
    return 0U;
}
