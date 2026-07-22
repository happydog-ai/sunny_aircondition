#include "r_cg_macrodriver.h"
#include "r_cg_adc.h"
#include "app_config.h"
#include "bsp_switch_mux.h"
#include "temperature_sensor.h"

#if (TEMPERATURE_DEBUG_OUTPUT_ENABLE != 0U) && (TEMPERATURE_DEBUG_OUTPUT_RS485 != 0U)
#include "bsp_rs485.h"
#endif

#define TEMP_STATE_IDLE          (0U)
#define TEMP_STATE_SETTLE        (1U)
#define TEMP_STATE_START_ADC     (2U)
#define TEMP_STATE_WAIT_ADC      (3U)

typedef struct {
    int16_t temperature_0p1c;
    uint32_t resistance_ohm;
} ntc_rt_point_t;

static const ntc_rt_point_t g_mf58_502x_rt_table[] = {
    {  -300,  69549UL }, /*  -30C */
    {  -250,  52168UL }, /*  -25C */
    {  -200,  39579UL }, /*  -20C */
    {  -150,  30350UL }, /*  -15C */
    {  -100,  23509UL }, /*  -10C */
    {   -50,  18385UL }, /*   -5C */
    {     0,  14218UL }, /*    0C, spec check point: 14217.8ohm */
    {    50,  11545UL }, /*    5C */
    {   100,   9263UL }, /*   10C */
    {   150,   7488UL }, /*   15C */
    {   200,   6053UL }, /*   20C, spec check point: 6052.6ohm */
    {   250,   5000UL }, /*   25C, R25 */
    {   300,   4095UL }, /*   30C, spec check point */
    {   350,   3427UL }, /*   35C */
    {   400,   2843UL }, /*   40C, spec check point: 2842.7ohm */
    {   450,   2406UL }, /*   45C */
    {   500,   2032UL }, /*   50C, spec check point: 2032.1ohm */
    {   550,   1725UL }, /*   55C */
    {   600,   1472UL }, /*   60C */
    {   650,   1262UL }, /*   65C */
    {   700,   1087UL }, /*   70C */
    {   750,    940UL }, /*   75C */
    {   800,    816UL }, /*   80C */
    {   850,    694UL }, /*   85C, spec check point: 694.2ohm */
    {   900,    623UL }, /*   90C */
    {   950,    547UL }, /*   95C */
    {  1000,    482UL }, /*  100C */
    {  1050,    392UL }  /*  105C, spec check point: 392.4ohm */
};

#define NTC_RT_TABLE_COUNT ((uint8_t)(sizeof(g_mf58_502x_rt_table) / sizeof(g_mf58_502x_rt_table[0])))

static temperature_th1_data_t g_th1_data;
static volatile uint16_t g_temp_tick_ms;
static volatile uint32_t g_temp_sample_tick_ms;
static volatile uint8_t g_adc_done;
static volatile uint16_t g_adc_value;
static uint8_t g_temp_state;
static uint8_t g_force_sample_pending;
static uint16_t g_sample_sequence;
static uint8_t g_discard_next;
static uint8_t g_sample_count;
static uint32_t g_sample_sum;
static uint8_t g_debug_pending;

static void Temperature_ResetSequence(void);
static void Temperature_FinishSequence(uint16_t raw);

#if (TEMPERATURE_DEBUG_OUTPUT_ENABLE != 0U)
static void Temperature_DebugOutput(void);
static void Temperature_SendChar(char ch);
static void Temperature_SendString(const char *text);
static void Temperature_SendU16(uint16_t value);
static void Temperature_SendU32(uint32_t value);
static void Temperature_SendTempX10(int16_t temp_x10);
#endif

void Temperature_Init(void)
{
    g_th1_data.raw_average = 0U;
    g_th1_data.voltage_mv = 0U;
    g_th1_data.resistance_ohm = 0UL;
    g_th1_data.temperature_0p1c = TEMPERATURE_TEMP_INVALID_0P1C;
    g_th1_data.status = TEMPERATURE_SENSOR_STATUS_NTC_NOT_CONFIGURED;
    g_th1_data.sample_valid = 0U;

    g_temp_tick_ms = 0U;
    g_temp_sample_tick_ms = TEMPERATURE_SAMPLE_PERIOD_MS;
    g_adc_done = 0U;
    g_adc_value = 0U;
    g_temp_state = TEMP_STATE_IDLE;
    g_force_sample_pending = 0U;
    g_sample_sequence = 0U;
    g_debug_pending = 0U;
    Temperature_ResetSequence();

    R_ADC_Stop();
    R_ADC_Set_OperationOn();
}

void Temperature_TimerTick1ms(void)
{
    if (g_temp_tick_ms < 0xFFFFU)
    {
        g_temp_tick_ms++;
    }

    if (g_temp_sample_tick_ms < 0xFFFFFFFFUL)
    {
        g_temp_sample_tick_ms++;
    }

    if ((g_temp_sample_tick_ms >= TEMPERATURE_SAMPLE_PERIOD_MS) &&
        (g_temp_state == TEMP_STATE_IDLE))
    {
        g_force_sample_pending = 1U;
    }
}

void Temperature_Task(void)
{
    switch (g_temp_state)
    {
        case TEMP_STATE_IDLE:
            if (g_force_sample_pending == 0U)
            {
                break;
            }

            if (BSP_SwitchMux_Request(BSP_SWITCH_MUX_OWNER_TEMPERATURE) == 0U)
            {
                break;
            }

            g_temp_sample_tick_ms = 0UL;
            g_force_sample_pending = 0U;
            Temperature_ResetSequence();
            BSP_SwitchMux_Select(TEMPERATURE_TH1_MUX_CHANNEL);
            g_temp_tick_ms = 0U;
            g_temp_state = TEMP_STATE_SETTLE;
            break;

        case TEMP_STATE_SETTLE:
            if (g_temp_tick_ms >= TEMPERATURE_MUX_SETTLE_MS)
            {
                g_temp_state = TEMP_STATE_START_ADC;
            }
            break;

        case TEMP_STATE_START_ADC:
            g_adc_done = 0U;
            R_ADC_Start();
            g_temp_state = TEMP_STATE_WAIT_ADC;
            break;

        case TEMP_STATE_WAIT_ADC:
            if (g_adc_done == 0U)
            {
                break;
            }

            R_ADC_Stop();
            g_adc_done = 0U;

            if (g_discard_next != 0U)
            {
                g_discard_next = 0U;
                g_temp_state = TEMP_STATE_START_ADC;
                break;
            }

            g_sample_sum += (uint32_t)g_adc_value;
            g_sample_count++;

            if (g_sample_count >= TEMPERATURE_SAMPLE_COUNT)
            {
                uint16_t average;
                average = (uint16_t)((g_sample_sum + (TEMPERATURE_SAMPLE_COUNT / 2U)) /
                                     TEMPERATURE_SAMPLE_COUNT);
                Temperature_FinishSequence(average);
                BSP_SwitchMux_Release(BSP_SWITCH_MUX_OWNER_TEMPERATURE);
                g_debug_pending = 1U;
                g_temp_state = TEMP_STATE_IDLE;
            }
            else
            {
                g_temp_state = TEMP_STATE_START_ADC;
            }
            break;

        default:
            R_ADC_Stop();
            BSP_SwitchMux_Release(BSP_SWITCH_MUX_OWNER_TEMPERATURE);
            g_temp_state = TEMP_STATE_IDLE;
            break;
    }

#if (TEMPERATURE_DEBUG_OUTPUT_ENABLE != 0U)
    if (g_debug_pending != 0U)
    {
        g_debug_pending = 0U;
        Temperature_DebugOutput();
    }
#else
    g_debug_pending = 0U;
#endif
}

void Temperature_AdcCompleteNotify(uint16_t adc_raw)
{
    if (adc_raw > TEMPERATURE_ADC_FULL_SCALE)
    {
        adc_raw = TEMPERATURE_ADC_FULL_SCALE;
    }

    g_adc_value = adc_raw;
    g_adc_done = 1U;
}

uint8_t Temperature_RequestTH1Sample(void)
{
    if (g_temp_state != TEMP_STATE_IDLE)
    {
        return 0U;
    }

    g_force_sample_pending = 1U;
    return 1U;
}

uint8_t Temperature_IsTH1Busy(void)
{
    return (g_temp_state == TEMP_STATE_IDLE) ? 0U : 1U;
}

uint16_t Temperature_GetTH1SampleSequence(void)
{
    return g_sample_sequence;
}

void Temperature_GetTH1Data(temperature_th1_data_t *data)
{
    if (data == (temperature_th1_data_t *)0)
    {
        return;
    }

    *data = g_th1_data;
}

int16_t Temperature_GetTH1CelsiusX10(void)
{
    return g_th1_data.temperature_0p1c;
}

temperature_sensor_status_t Temperature_GetTH1Status(void)
{
    return g_th1_data.status;
}

const char *Temperature_GetStatusString(temperature_sensor_status_t status)
{
    switch (status)
    {
        case TEMPERATURE_SENSOR_STATUS_OK:
            return "OK";
        case TEMPERATURE_SENSOR_STATUS_OPEN:
            return "OPEN";
        case TEMPERATURE_SENSOR_STATUS_SHORT:
            return "SHORT";
        case TEMPERATURE_SENSOR_STATUS_OUT_OF_RANGE:
            return "OUT_OF_RANGE";
        case TEMPERATURE_SENSOR_STATUS_NTC_NOT_CONFIGURED:
            return "NTC_NOT_CONFIGURED";
        default:
            return "UNKNOWN";
    }
}

static void Temperature_ResetSequence(void)
{
    g_discard_next = 1U;
    g_sample_count = 0U;
    g_sample_sum = 0UL;
}

static void Temperature_FinishSequence(uint16_t raw)
{
    uint32_t voltage;
    uint32_t resistance;
    temperature_sensor_status_t status;
    int16_t temperature;

    g_th1_data.raw_average = raw;

    voltage = ((uint32_t)raw * TEMPERATURE_ADC_VREF_MV +
               (TEMPERATURE_ADC_FULL_SCALE / 2U)) /
              TEMPERATURE_ADC_FULL_SCALE;
    if (voltage > 0xFFFFUL)
    {
        voltage = 0xFFFFUL;
    }
    g_th1_data.voltage_mv = (uint16_t)voltage;

    if (raw <= TEMPERATURE_OPEN_RAW_THRESHOLD)
    {
        resistance = 0xFFFFFFFFUL;
        status = TEMPERATURE_SENSOR_STATUS_OPEN;
    }
    else if (raw >= TEMPERATURE_SHORT_RAW_THRESHOLD)
    {
        resistance = 0UL;
        status = TEMPERATURE_SENSOR_STATUS_SHORT;
    }
    else
    {
        resistance = (TEMPERATURE_DIVIDER_RESISTOR_OHM *
                      (uint32_t)(TEMPERATURE_ADC_FULL_SCALE - raw)) /
                     (uint32_t)raw;
        status = TEMPERATURE_SENSOR_STATUS_OK;
    }

    temperature = Temperature_ResistanceToTempX10(resistance, &status);

    g_th1_data.resistance_ohm = resistance;
    g_th1_data.temperature_0p1c = temperature;
    g_th1_data.status = status;
    g_th1_data.sample_valid = 1U;
    g_sample_sequence++;
}

int16_t Temperature_ResistanceToTempX10(uint32_t resistance, temperature_sensor_status_t *status)
{
    uint8_t i;
    uint32_t r_high;
    uint32_t r_low;
    int16_t t_high;
    int16_t t_low;
    int32_t numerator;
    int32_t denominator;
    int32_t temp;

    if (status == (temperature_sensor_status_t *)0)
    {
        return TEMPERATURE_TEMP_INVALID_0P1C;
    }

    if (*status != TEMPERATURE_SENSOR_STATUS_OK)
    {
        return TEMPERATURE_TEMP_INVALID_0P1C;
    }

#if (TEMPERATURE_NTC_CONFIGURED != 0U)
    if ((resistance > g_mf58_502x_rt_table[0].resistance_ohm) ||
        (resistance < g_mf58_502x_rt_table[NTC_RT_TABLE_COUNT - 1U].resistance_ohm))
    {
        *status = TEMPERATURE_SENSOR_STATUS_OUT_OF_RANGE;
        return TEMPERATURE_TEMP_INVALID_0P1C;
    }

    if (resistance == g_mf58_502x_rt_table[0].resistance_ohm)
    {
        *status = TEMPERATURE_SENSOR_STATUS_OK;
        return g_mf58_502x_rt_table[0].temperature_0p1c;
    }

    for (i = 0U; i < (uint8_t)(NTC_RT_TABLE_COUNT - 1U); i++)
    {
        r_high = g_mf58_502x_rt_table[i].resistance_ohm;
        r_low = g_mf58_502x_rt_table[i + 1U].resistance_ohm;

        if ((resistance <= r_high) && (resistance >= r_low))
        {
            t_high = g_mf58_502x_rt_table[i].temperature_0p1c;
            t_low = g_mf58_502x_rt_table[i + 1U].temperature_0p1c;

            if (r_high == r_low)
            {
                *status = TEMPERATURE_SENSOR_STATUS_OK;
                return t_high;
            }

            /*
             * NTC resistance decreases as temperature rises.
             * Linear interpolation between two adjacent R-T nominal points:
             * T = T_high + (R_high - R) * (T_low - T_high) / (R_high - R_low)
             */
            numerator = (int32_t)(r_high - resistance) * (int32_t)(t_low - t_high);
            denominator = (int32_t)(r_high - r_low);
            temp = (int32_t)t_high + ((numerator + (denominator / 2L)) / denominator);

            if (temp < (int32_t)TEMPERATURE_MIN_TEMP_0P1C)
            {
                temp = (int32_t)TEMPERATURE_MIN_TEMP_0P1C;
            }
            else if (temp > (int32_t)TEMPERATURE_MAX_TEMP_0P1C)
            {
                temp = (int32_t)TEMPERATURE_MAX_TEMP_0P1C;
            }

            *status = TEMPERATURE_SENSOR_STATUS_OK;
            return (int16_t)temp;
        }
    }

    *status = TEMPERATURE_SENSOR_STATUS_OUT_OF_RANGE;
#else
    *status = TEMPERATURE_SENSOR_STATUS_NTC_NOT_CONFIGURED;
#endif

    return TEMPERATURE_TEMP_INVALID_0P1C;
}

#if (TEMPERATURE_DEBUG_OUTPUT_ENABLE != 0U)
static void Temperature_DebugOutput(void)
{
    Temperature_SendString("TH1 RAW=");
    Temperature_SendU16(g_th1_data.raw_average);
    Temperature_SendString(", VOLT=");
    Temperature_SendU16(g_th1_data.voltage_mv);
    Temperature_SendString("mV, R=");
    Temperature_SendU32(g_th1_data.resistance_ohm);
    Temperature_SendString("ohm, TEMP=");
    Temperature_SendTempX10(g_th1_data.temperature_0p1c);
    Temperature_SendString("C, STATUS=");
    Temperature_SendString(Temperature_GetStatusString(g_th1_data.status));
    Temperature_SendString("\r\n");
}

static void Temperature_SendTempX10(int16_t temp_x10)
{
    uint16_t abs_value;

    if (temp_x10 == TEMPERATURE_TEMP_INVALID_0P1C)
    {
        Temperature_SendString("--");
        return;
    }

    if (temp_x10 < 0)
    {
        Temperature_SendChar('-');
        abs_value = (uint16_t)(0 - temp_x10);
    }
    else
    {
        abs_value = (uint16_t)temp_x10;
    }

    Temperature_SendU16((uint16_t)(abs_value / 10U));
    Temperature_SendChar('.');
    Temperature_SendChar((char)('0' + (char)(abs_value % 10U)));
}

static void Temperature_SendChar(char ch)
{
#if (TEMPERATURE_DEBUG_OUTPUT_RS485 != 0U)
    (void)BSP_RS485_Send((const uint8_t *)&ch, 1U);
#else
    (void)ch;
#endif
}

static void Temperature_SendString(const char *text)
{
    while ((text != (const char *)0) && (*text != '\0'))
    {
        Temperature_SendChar(*text);
        text++;
    }
}

static void Temperature_SendU16(uint16_t value)
{
    Temperature_SendU32((uint32_t)value);
}

static void Temperature_SendU32(uint32_t value)
{
    char buffer[10];
    uint8_t index;

    if (value == 0UL)
    {
        Temperature_SendChar('0');
        return;
    }

    index = 0U;
    while ((value > 0UL) && (index < sizeof(buffer)))
    {
        buffer[index] = (char)('0' + (char)(value % 10UL));
        value /= 10UL;
        index++;
    }

    while (index > 0U)
    {
        index--;
        Temperature_SendChar(buffer[index]);
    }
}
#endif
