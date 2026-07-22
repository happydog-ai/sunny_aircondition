#include "switch_input.h"
#include "bsp_switch_mux.h"

typedef struct {
    switch_channel_t k1_channel;
    switch_channel_t k2_channel;
} switch_mux_map_t;

static const switch_mux_map_t g_switch_mux_map[BSP_SWITCH_MUX_CHANNEL_COUNT] = {
    { SWITCH_K1,      SWITCH_K5      }, /* 000 */
    { SWITCH_INVALID, SWITCH_INVALID }, /* 001 */
    { SWITCH_K2,      SWITCH_INVALID }, /* 010 */
    { SWITCH_K0,      SWITCH_INVALID }, /* 011 */
    { SWITCH_K3,      SWITCH_K6      }, /* 100 */
    { SWITCH_INVALID, SWITCH_K9      }, /* 101 */
    { SWITCH_K4,      SWITCH_K7      }, /* 110 */
    { SWITCH_INVALID, SWITCH_K8      }  /* 111 */
};

static uint8_t g_switch_raw[SWITCH_INPUT_COUNT];
static uint8_t g_switch_stable[SWITCH_INPUT_COUNT];
static uint8_t g_switch_candidate[SWITCH_INPUT_COUNT];
static uint8_t g_switch_count[SWITCH_INPUT_COUNT];
static volatile uint16_t g_switch_scan_tick;
static uint8_t g_switch_mux_channel;
static uint8_t g_switch_waiting_settle;

static void SwitchInput_UpdateOne(switch_channel_t channel, uint8_t raw);
static void SwitchInput_ReadSelectedChannel(void);

void SwitchInput_Init(void)
{
    uint8_t i;

    BSP_SwitchMux_Init();

    for (i = 0U; i < SWITCH_INPUT_COUNT; i++)
    {
        g_switch_raw[i] = SWITCH_STATE_OPEN;
        g_switch_stable[i] = SWITCH_STATE_OPEN;
        g_switch_candidate[i] = SWITCH_STATE_OPEN;
        g_switch_count[i] = 0U;
    }

    g_switch_scan_tick = 0U;
    g_switch_mux_channel = 0U;
    g_switch_waiting_settle = 0U;
    BSP_SwitchMux_Select(g_switch_mux_channel);
}

void SwitchInput_TimerTick1ms(void)
{
    if (g_switch_scan_tick < 0xFFFFU)
    {
        g_switch_scan_tick++;
    }
}

void SwitchInput_Task(void)
{
    if (g_switch_scan_tick < SWITCH_INPUT_SCAN_PERIOD_MS)
    {
        return;
    }

    if (BSP_SwitchMux_Request(BSP_SWITCH_MUX_OWNER_SWITCH) == 0U)
    {
        return;
    }

    g_switch_scan_tick = 0U;

    if (g_switch_waiting_settle != 0U)
    {
        SwitchInput_ReadSelectedChannel();
        g_switch_mux_channel++;
        if (g_switch_mux_channel >= BSP_SWITCH_MUX_CHANNEL_COUNT)
        {
            g_switch_mux_channel = 0U;
        }
        BSP_SwitchMux_Select(g_switch_mux_channel);
        g_switch_waiting_settle = 0U;
        BSP_SwitchMux_Release(BSP_SWITCH_MUX_OWNER_SWITCH);
    }
    else
    {
        /*
         * Select first, read on the next task slot so S0/S1/S2 have time to settle.
         */
        BSP_SwitchMux_Select(g_switch_mux_channel);
        g_switch_waiting_settle = 1U;
    }
}

uint8_t SwitchInput_GetStable(switch_channel_t channel)
{
    if ((uint8_t)channel >= SWITCH_INPUT_COUNT)
    {
        return SWITCH_STATE_OPEN;
    }
    return g_switch_stable[(uint8_t)channel];
}

uint8_t SwitchInput_GetRaw(switch_channel_t channel)
{
    if ((uint8_t)channel >= SWITCH_INPUT_COUNT)
    {
        return SWITCH_STATE_OPEN;
    }
    return g_switch_raw[(uint8_t)channel];
}

static void SwitchInput_ReadSelectedChannel(void)
{
    const switch_mux_map_t *map;

    map = &g_switch_mux_map[g_switch_mux_channel];

    SwitchInput_UpdateOne(map->k1_channel, BSP_SwitchMux_ReadK1Raw());
    SwitchInput_UpdateOne(map->k2_channel, BSP_SwitchMux_ReadK2Raw());
}

static void SwitchInput_UpdateOne(switch_channel_t channel, uint8_t raw)
{
    uint8_t index;
    uint8_t state;

    if ((uint8_t)channel >= SWITCH_INPUT_COUNT)
    {
        return;
    }

    index = (uint8_t)channel;
    state = (raw != 0U) ? SWITCH_STATE_CLOSED : SWITCH_STATE_OPEN;
    g_switch_raw[index] = state;

    if (g_switch_candidate[index] == state)
    {
        if (g_switch_count[index] < SWITCH_INPUT_DEBOUNCE_COUNT)
        {
            g_switch_count[index]++;
        }
    }
    else
    {
        g_switch_candidate[index] = state;
        g_switch_count[index] = 1U;
    }

    if (g_switch_count[index] >= SWITCH_INPUT_DEBOUNCE_COUNT)
    {
        g_switch_stable[index] = state;
    }
}
