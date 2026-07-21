#include "driver_board_comm.h"

static uint8_t g_driver_compressor_running;
static uint8_t g_driver_stop_requested;
static uint8_t g_driver_restore_requested;
static uint16_t g_driver_compressor_frequency_hz;
static volatile uint32_t g_driver_compressor_run_elapsed_ms;

void DriverBoardComm_Init(void)
{
    g_driver_compressor_running = 0U;
    g_driver_stop_requested = 0U;
    g_driver_restore_requested = 0U;
    g_driver_compressor_frequency_hz = 0U;
    g_driver_compressor_run_elapsed_ms = 0UL;
    (void)BSP_Relay_Off(COMPRESSOR_RUN_RELAY);
}

void DriverBoardComm_Task(void)
{
    /*
     * Placeholder for the real driver-board communication task.
     * Replace this layer with the actual stop/frequency-0 command and feedback parser.
     */
    if (g_driver_stop_requested != 0U)
    {
        (void)BSP_Relay_Off(COMPRESSOR_RUN_RELAY);
        g_driver_compressor_running = 0U;
        g_driver_compressor_frequency_hz = 0U;
        g_driver_stop_requested = 0U;
    }

    if (g_driver_restore_requested != 0U)
    {
        /*
         * Real restore command should be sent here after the driver-board
         * communication interface is confirmed.
         */
        (void)BSP_Relay_On(COMPRESSOR_RUN_RELAY);
        g_driver_compressor_running = 1U;
        g_driver_compressor_frequency_hz = 50U;
        g_driver_compressor_run_elapsed_ms = 0UL;
        g_driver_restore_requested = 0U;
    }
}

void DriverBoardComm_TimerTick1ms(void)
{
    if (g_driver_compressor_running != 0U)
    {
        if (g_driver_compressor_run_elapsed_ms < 0xFFFFFFFFUL)
        {
            g_driver_compressor_run_elapsed_ms++;
        }
    }
    else
    {
        g_driver_compressor_run_elapsed_ms = 0UL;
    }
}

uint8_t DriverBoard_RequestCompressorStop(void)
{
    (void)BSP_Relay_Off(COMPRESSOR_RUN_RELAY);
    g_driver_stop_requested = 1U;
    return 1U;
}

uint8_t DriverBoard_RequestRestorePreviousState(void)
{
    g_driver_restore_requested = 1U;
    return 1U;
}

uint8_t DriverBoard_IsCompressorRunning(void)
{
    return g_driver_compressor_running;
}

uint8_t DriverBoard_IsCompressorStopped(void)
{
    return (g_driver_compressor_running == 0U &&
            g_driver_compressor_frequency_hz == 0U) ? 1U : 0U;
}

uint16_t DriverBoard_GetCompressorFrequencyHz(void)
{
    return g_driver_compressor_frequency_hz;
}

uint16_t DriverBoard_GetCompressorRunElapsedSec(void)
{
    return (uint16_t)(g_driver_compressor_run_elapsed_ms / 1000UL);
}

void DriverBoard_DebugSetCompressorRunning(uint8_t running)
{
    uint8_t old_running;

    old_running = g_driver_compressor_running;
    g_driver_compressor_running = (running != 0U) ? 1U : 0U;
    g_driver_compressor_frequency_hz = (running != 0U) ? 50U : 0U;
    if (running != 0U)
    {
        (void)BSP_Relay_On(COMPRESSOR_RUN_RELAY);
        g_driver_stop_requested = 0U;
        if (old_running == 0U)
        {
            g_driver_compressor_run_elapsed_ms = 0UL;
        }
    }
    else
    {
        (void)BSP_Relay_Off(COMPRESSOR_RUN_RELAY);
        g_driver_compressor_run_elapsed_ms = 0UL;
    }
}
