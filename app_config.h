#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "r_cg_macrodriver.h"

#define APP_CONFIG_PARAM_COUNT      (13U)
#define APP_CONFIG_DATA_BYTES       (26U)
#define APP_CONFIG_SLOT_SIZE        (64U)
#define APP_CONFIG_SLOT_COUNT       (4U)
#define APP_CONFIG_EEPROM_OFFSET    (0x0000U)
#define APP_CONFIG_MAGIC_HI         (0xAAU)
#define APP_CONFIG_MAGIC_LO         (0x55U)
#define APP_CONFIG_VERSION          (2U)
#define APP_CONFIG_COMMIT_FLAG      (0x5AU)
#define APP_CONFIG_SAVE_DELAY_MS    (2000U)

/*
 * TH1 temperature acquisition configuration.
 * ADC: 10-bit, VDD/VSS reference, ANI0 selected by CS+.
 */
#define TEMPERATURE_ADC_FULL_SCALE       (1023U)
#define TEMPERATURE_ADC_VREF_MV          (5000UL)
#define TEMPERATURE_DIVIDER_RESISTOR_OHM (5100UL)
#define TEMPERATURE_TH1_MUX_CHANNEL      (0U)
#define TEMPERATURE_SAMPLE_COUNT         (8U)
#define TEMPERATURE_MUX_SETTLE_MS        (1U)
#define TEMPERATURE_SAMPLE_PERIOD_MS     (1000UL)
#define TEMPERATURE_OPEN_RAW_THRESHOLD   (5U)
#define TEMPERATURE_SHORT_RAW_THRESHOLD  (1018U)
#define TEMPERATURE_TEMP_INVALID_0P1C    ((int16_t)0x7FFF)

/*
 * Board channel name: TH1/CN5.
 * External probe: TH2 copper tube probe, MF58-502X NTC.
 * Use 5k/B3470 parameters, not 20k/B4000.
 */
#define TEMPERATURE_NTC_CONFIGURED       (1U)
#define TEMPERATURE_NTC_R25_OHM          (5000UL)
#define TEMPERATURE_NTC_B_VALUE          (3470U)
#define TEMPERATURE_MIN_TEMP_0P1C        ((int16_t)-300)
#define TEMPERATURE_MAX_TEMP_0P1C        ((int16_t)1050)
#define TEMPERATURE_DEBUG_OUTPUT_ENABLE  (0U)
#define TEMPERATURE_DEBUG_OUTPUT_RS485   (0U)

#define APP_CONFIG_STATE_IDLE         (0U)
#define APP_CONFIG_STATE_DIRTY        (1U)
#define APP_CONFIG_STATE_SAVING       (2U)
#define APP_CONFIG_STATE_ERROR        (3U)

#define APP_CONFIG_SRC_EMPTY          (0U)
#define APP_CONFIG_SRC_EEPROM_OFFLINE (1U)
#define APP_CONFIG_SRC_CRC_ERROR      (2U)
#define APP_CONFIG_SRC_EEPROM_LOADED  (3U)

typedef struct {
    uint8_t state;
    uint8_t eeprom_online;
    uint8_t active_slot;
    uint8_t dirty;
    uint8_t last_result;
    uint32_t sequence;
    uint16_t save_count;
    uint16_t error_count;
    uint8_t config_source;
} app_config_status_t;

void AppConfig_Init(void);
void AppConfig_Task(void);
void AppConfig_TimerTick1ms(void);

uint16_t AppConfig_GetRegister(uint16_t reg_addr);
void AppConfig_SetRegister(uint16_t reg_addr, uint16_t value);
uint8_t AppConfig_ValidateRegister(uint16_t reg_addr, uint16_t value);

uint8_t AppConfig_Load(void);
void AppConfig_Save(void);
void AppConfig_RestoreDefaults(void);
void AppConfig_MarkDirty(void);
uint8_t AppConfig_GetState(void);
void AppConfig_GetStatus(app_config_status_t *status);

#endif
