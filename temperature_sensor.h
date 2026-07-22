#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include "r_cg_macrodriver.h"

typedef enum {
    TEMPERATURE_SENSOR_STATUS_OK = 0U,
    TEMPERATURE_SENSOR_STATUS_OPEN = 1U,
    TEMPERATURE_SENSOR_STATUS_SHORT = 2U,
    TEMPERATURE_SENSOR_STATUS_OUT_OF_RANGE = 3U,
    TEMPERATURE_SENSOR_STATUS_NTC_NOT_CONFIGURED = 4U
} temperature_sensor_status_t;

typedef struct {
    uint16_t raw_average;
    uint16_t voltage_mv;
    uint32_t resistance_ohm;
    int16_t temperature_0p1c;
    temperature_sensor_status_t status;
    uint8_t sample_valid;
} temperature_th1_data_t;

void Temperature_Init(void);
void Temperature_Task(void);
void Temperature_TimerTick1ms(void);
void Temperature_AdcCompleteNotify(uint16_t adc_raw);
uint8_t Temperature_RequestTH1Sample(void);
uint8_t Temperature_IsTH1Busy(void);
uint16_t Temperature_GetTH1SampleSequence(void);
int16_t Temperature_ResistanceToTempX10(uint32_t resistance_ohm, temperature_sensor_status_t *status);
void Temperature_GetTH1Data(temperature_th1_data_t *data);
int16_t Temperature_GetTH1CelsiusX10(void);
temperature_sensor_status_t Temperature_GetTH1Status(void);
const char *Temperature_GetStatusString(temperature_sensor_status_t status);

#endif
