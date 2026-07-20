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
