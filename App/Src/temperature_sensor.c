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
    {  -400, 113871UL }, /*  -40C, TH2 Rnom */
    {  -390, 107186UL }, /*  -39C, TH2 Rnom */
    {  -380, 100968UL }, /*  -38C, TH2 Rnom */
    {  -370,  95178UL }, /*  -37C, TH2 Rnom */
    {  -360,  89778UL }, /*  -36C, TH2 Rnom */
    {  -350,  84737UL }, /*  -35C, TH2 Rnom */
    {  -340,  80025UL }, /*  -34C, TH2 Rnom */
    {  -330,  75617UL }, /*  -33C, TH2 Rnom */
    {  -320,  71488UL }, /*  -32C, TH2 Rnom */
    {  -310,  67618UL }, /*  -31C, TH2 Rnom */
    {  -300,  63986UL }, /*  -30C, TH2 Rnom */
    {  -290,  60576UL }, /*  -29C, TH2 Rnom */
    {  -280,  57372UL }, /*  -28C, TH2 Rnom */
    {  -270,  54359UL }, /*  -27C, TH2 Rnom */
    {  -260,  51524UL }, /*  -26C, TH2 Rnom */
    {  -250,  48854UL }, /*  -25C, TH2 Rnom */
    {  -240,  46339UL }, /*  -24C, TH2 Rnom */
    {  -230,  43968UL }, /*  -23C, TH2 Rnom */
    {  -220,  41732UL }, /*  -22C, TH2 Rnom */
    {  -210,  39622UL }, /*  -21C, TH2 Rnom */
    {  -200,  37630UL }, /*  -20C, TH2 Rnom */
    {  -190,  35749UL }, /*  -19C, TH2 Rnom */
    {  -180,  33972UL }, /*  -18C, TH2 Rnom */
    {  -170,  32292UL }, /*  -17C, TH2 Rnom */
    {  -160,  30704UL }, /*  -16C, TH2 Rnom */
    {  -150,  29202UL }, /*  -15C, TH2 Rnom */
    {  -140,  27782UL }, /*  -14C, TH2 Rnom */
    {  -130,  26437UL }, /*  -13C, TH2 Rnom */
    {  -120,  25164UL }, /*  -12C, TH2 Rnom */
    {  -110,  23959UL }, /*  -11C, TH2 Rnom */
    {  -100,  22818UL }, /*  -10C, TH2 Rnom */
    {   -90,  21737UL }, /*   -9C, TH2 Rnom */
    {   -80,  20713UL }, /*   -8C, TH2 Rnom */
    {   -70,  19742UL }, /*   -7C, TH2 Rnom */
    {   -60,  18822UL }, /*   -6C, TH2 Rnom */
    {   -50,  17950UL }, /*   -5C, TH2 Rnom */
    {   -40,  17123UL }, /*   -4C, TH2 Rnom */
    {   -30,  16338UL }, /*   -3C, TH2 Rnom */
    {   -20,  15594UL }, /*   -2C, TH2 Rnom */
    {   -10,  14888UL }, /*   -1C, TH2 Rnom */
    {     0,  14218UL }, /*    0C, TH2 Rnom */
    {    10,  13582UL }, /*    1C, TH2 Rnom */
    {    20,  12978UL }, /*    2C, TH2 Rnom */
    {    30,  12405UL }, /*    3C, TH2 Rnom */
    {    40,  11861UL }, /*    4C, TH2 Rnom */
    {    50,  11344UL }, /*    5C, TH2 Rnom */
    {    60,  10853UL }, /*    6C, TH2 Rnom */
    {    70,  10387UL }, /*    7C, TH2 Rnom */
    {    80,   9944UL }, /*    8C, TH2 Rnom */
    {    90,   9522UL }, /*    9C, TH2 Rnom */
    {   100,   9122UL }, /*   10C, TH2 Rnom */
    {   110,   8742UL }, /*   11C, TH2 Rnom */
    {   120,   8380UL }, /*   12C, TH2 Rnom */
    {   130,   8036UL }, /*   13C, TH2 Rnom */
    {   140,   7708UL }, /*   14C, TH2 Rnom */
    {   150,   7397UL }, /*   15C, TH2 Rnom */
    {   160,   7101UL }, /*   16C, TH2 Rnom */
    {   170,   6819UL }, /*   17C, TH2 Rnom */
    {   180,   6551UL }, /*   18C, TH2 Rnom */
    {   190,   6296UL }, /*   19C, TH2 Rnom */
    {   200,   6053UL }, /*   20C, TH2 Rnom */
    {   210,   5821UL }, /*   21C, TH2 Rnom */
    {   220,   5601UL }, /*   22C, TH2 Rnom */
    {   230,   5391UL }, /*   23C, TH2 Rnom */
    {   240,   5191UL }, /*   24C, TH2 Rnom */
    {   250,   5000UL }, /*   25C, TH2 Rnom */
    {   260,   4799UL }, /*   26C, TH2 Rnom */
    {   270,   4608UL }, /*   27C, TH2 Rnom */
    {   280,   4428UL }, /*   28C, TH2 Rnom */
    {   290,   4257UL }, /*   29C, TH2 Rnom */
    {   300,   4095UL }, /*   30C, TH2 Rnom */
    {   310,   3941UL }, /*   31C, TH2 Rnom */
    {   320,   3794UL }, /*   32C, TH2 Rnom */
    {   330,   3655UL }, /*   33C, TH2 Rnom */
    {   340,   3522UL }, /*   34C, TH2 Rnom */
    {   350,   3396UL }, /*   35C, TH2 Rnom */
    {   360,   3275UL }, /*   36C, TH2 Rnom */
    {   370,   3160UL }, /*   37C, TH2 Rnom */
    {   380,   3049UL }, /*   38C, TH2 Rnom */
    {   390,   2944UL }, /*   39C, TH2 Rnom */
    {   400,   2843UL }, /*   40C, TH2 Rnom */
    {   410,   2746UL }, /*   41C, TH2 Rnom */
    {   420,   2653UL }, /*   42C, TH2 Rnom */
    {   430,   2564UL }, /*   43C, TH2 Rnom */
    {   440,   2479UL }, /*   44C, TH2 Rnom */
    {   450,   2397UL }, /*   45C, TH2 Rnom */
    {   460,   2318UL }, /*   46C, TH2 Rnom */
    {   470,   2242UL }, /*   47C, TH2 Rnom */
    {   480,   2170UL }, /*   48C, TH2 Rnom */
    {   490,   2100UL }, /*   49C, TH2 Rnom */
    {   500,   2032UL }, /*   50C, TH2 Rnom */
    {   510,   1967UL }, /*   51C, TH2 Rnom */
    {   520,   1905UL }, /*   52C, TH2 Rnom */
    {   530,   1844UL }, /*   53C, TH2 Rnom */
    {   540,   1786UL }, /*   54C, TH2 Rnom */
    {   550,   1730UL }, /*   55C, TH2 Rnom */
    {   560,   1676UL }, /*   56C, TH2 Rnom */
    {   570,   1624UL }, /*   57C, TH2 Rnom */
    {   580,   1573UL }, /*   58C, TH2 Rnom */
    {   590,   1525UL }, /*   59C, TH2 Rnom */
    {   600,   1478UL }, /*   60C, TH2 Rnom */
    {   610,   1432UL }, /*   61C, TH2 Rnom */
    {   620,   1388UL }, /*   62C, TH2 Rnom */
    {   630,   1346UL }, /*   63C, TH2 Rnom */
    {   640,   1305UL }, /*   64C, TH2 Rnom */
    {   650,   1266UL }, /*   65C, TH2 Rnom */
    {   660,   1227UL }, /*   66C, TH2 Rnom */
    {   670,   1190UL }, /*   67C, TH2 Rnom */
    {   680,   1154UL }, /*   68C, TH2 Rnom */
    {   690,   1120UL }, /*   69C, TH2 Rnom */
    {   700,   1086UL }, /*   70C, TH2 Rnom */
    {   710,   1054UL }, /*   71C, TH2 Rnom */
    {   720,   1022UL }, /*   72C, TH2 Rnom */
    {   730,    992UL }, /*   73C, TH2 Rnom */
    {   740,    962UL }, /*   74C, TH2 Rnom */
    {   750,    934UL }, /*   75C, TH2 Rnom */
    {   760,    906UL }, /*   76C, TH2 Rnom */
    {   770,    880UL }, /*   77C, TH2 Rnom */
    {   780,    854UL }, /*   78C, TH2 Rnom */
    {   790,    829UL }, /*   79C, TH2 Rnom */
    {   800,    804UL }, /*   80C, TH2 Rnom */
    {   810,    781UL }, /*   81C, TH2 Rnom */
    {   820,    758UL }, /*   82C, TH2 Rnom */
    {   830,    736UL }, /*   83C, TH2 Rnom */
    {   840,    715UL }, /*   84C, TH2 Rnom */
    {   850,    694UL }, /*   85C, TH2 Rnom */
    {   860,    674UL }, /*   86C, TH2 Rnom */
    {   870,    655UL }, /*   87C, TH2 Rnom */
    {   880,    636UL }, /*   88C, TH2 Rnom */
    {   890,    618UL }, /*   89C, TH2 Rnom */
    {   900,    600UL }, /*   90C, TH2 Rnom */
    {   910,    583UL }, /*   91C, TH2 Rnom */
    {   920,    566UL }, /*   92C, TH2 Rnom */
    {   930,    550UL }, /*   93C, TH2 Rnom */
    {   940,    535UL }, /*   94C, TH2 Rnom */
    {   950,    520UL }, /*   95C, TH2 Rnom */
    {   960,    505UL }, /*   96C, TH2 Rnom */
    {   970,    491UL }, /*   97C, TH2 Rnom */
    {   980,    477UL }, /*   98C, TH2 Rnom */
    {   990,    464UL }, /*   99C, TH2 Rnom */
    {  1000,    451UL }, /*  100C, TH2 Rnom */
    {  1010,    439UL }, /*  101C, TH2 Rnom */
    {  1020,    426UL }, /*  102C, TH2 Rnom */
    {  1030,    415UL }, /*  103C, TH2 Rnom */
    {  1040,    403UL }, /*  104C, TH2 Rnom */
    {  1050,    392UL }, /*  105C, TH2 Rnom */
    {  1060,    382UL }, /*  106C, TH2 Rnom */
    {  1070,    371UL }, /*  107C, TH2 Rnom */
    {  1080,    361UL }, /*  108C, TH2 Rnom */
    {  1090,    352UL }, /*  109C, TH2 Rnom */
    {  1100,    342UL }, /*  110C, TH2 Rnom */
    {  1110,    333UL }, /*  111C, TH2 Rnom */
    {  1120,    324UL }, /*  112C, TH2 Rnom */
    {  1130,    316UL }, /*  113C, TH2 Rnom */
    {  1140,    307UL }, /*  114C, TH2 Rnom */
    {  1150,    299UL }, /*  115C, TH2 Rnom */
    {  1160,    291UL }, /*  116C, TH2 Rnom */
    {  1170,    284UL }, /*  117C, TH2 Rnom */
    {  1180,    276UL }, /*  118C, TH2 Rnom */
    {  1190,    269UL }, /*  119C, TH2 Rnom */
    {  1200,    262UL }, /*  120C, TH2 Rnom */
    {  1210,    256UL }, /*  121C, TH2 Rnom */
    {  1220,    249UL }, /*  122C, TH2 Rnom */
    {  1230,    243UL }, /*  123C, TH2 Rnom */
    {  1240,    236UL }, /*  124C, TH2 Rnom */
    {  1250,    231UL }, /*  125C, TH2 Rnom */
    {  1260,    225UL }, /*  126C, TH2 Rnom */
    {  1270,    219UL }, /*  127C, TH2 Rnom */
    {  1280,    214UL }, /*  128C, TH2 Rnom */
    {  1290,    208UL }, /*  129C, TH2 Rnom */
    {  1300,    203UL }, /*  130C, TH2 Rnom */
    {  1310,    198UL }, /*  131C, TH2 Rnom */
    {  1320,    194UL }, /*  132C, TH2 Rnom */
    {  1330,    189UL }, /*  133C, TH2 Rnom */
    {  1340,    184UL }, /*  134C, TH2 Rnom */
    {  1350,    180UL }, /*  135C, TH2 Rnom */
    {  1360,    176UL }, /*  136C, TH2 Rnom */
    {  1370,    172UL }, /*  137C, TH2 Rnom */
    {  1380,    168UL }, /*  138C, TH2 Rnom */
    {  1390,    164UL }, /*  139C, TH2 Rnom */
    {  1400,    160UL }, /*  140C, TH2 Rnom */
    {  1410,    156UL }, /*  141C, TH2 Rnom */
    {  1420,    153UL }, /*  142C, TH2 Rnom */
    {  1430,    149UL }, /*  143C, TH2 Rnom */
    {  1440,    146UL }, /*  144C, TH2 Rnom */
    {  1450,    143UL }, /*  145C, TH2 Rnom */
    {  1460,    140UL }, /*  146C, TH2 Rnom */
    {  1470,    136UL }, /*  147C, TH2 Rnom */
    {  1480,    133UL }, /*  148C, TH2 Rnom */
    {  1490,    130UL }, /*  149C, TH2 Rnom */
    {  1500,    128UL }  /*  150C, TH2 Rnom */
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
