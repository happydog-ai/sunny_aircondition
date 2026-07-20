#include "r_cg_macrodriver.h"
#include "bsp_bl24c16.h"
#include "app_config.h"

typedef struct {
    uint8_t  magic_hi;
    uint8_t  magic_lo;
    uint8_t  version;
    uint8_t  payload_length;
    uint8_t  sequence_bytes[4];
    uint8_t  data[APP_CONFIG_DATA_BYTES];
    uint8_t  padding[24];
    uint8_t  crc_low;
    uint8_t  crc_high;
    uint8_t  reserved2[3];
    uint8_t  commit_flag;
} config_slot_t;

static uint16_t g_config_ram[APP_CONFIG_PARAM_COUNT];
static uint8_t  g_config_state;
static uint8_t  g_config_source;
static uint32_t g_config_sequence;
static uint16_t g_config_save_count;
static uint16_t g_config_error_count;
static uint32_t g_config_dirty_timer;
static uint8_t  g_save_pending;
static uint8_t  g_eeprom_online;
static uint8_t  g_last_result;

static const uint16_t g_config_defaults[APP_CONFIG_PARAM_COUNT] = {
    0U, 250U, 1U, 500U,
    0U, 0U,
    8U, 0U, 0U, 600U,
    1U, 0U, 0U,
};

static const uint8_t g_config_addr_to_index[] = {
    0, 1, 2, 3, 4, 5,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    6, 7, 8, 9,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    10, 11, 12,
};
#define APP_CONFIG_MAX_REG_ADDR (sizeof(g_config_addr_to_index) - 1U)

static const uint16_t g_config_slot_offset[APP_CONFIG_SLOT_COUNT] = {
    0x0000U, 0x0040U, 0x0080U, 0x00C0U,
};

#define CONFIG_CRC_RANGE (58U)

static uint16_t AppConfig_Crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t j;

    for (i = 0U; i < length; i++)
    {
        crc ^= (uint16_t)data[i];
        for (j = 0U; j < 8U; j++)
        {
            if (crc & 0x0001U)
                crc = (uint16_t)((crc >> 1U) ^ 0xA001U);
            else
                crc >>= 1U;
        }
    }
    return crc;
}

static uint32_t AppConfig_ReadSequence(const config_slot_t *slot)
{
    uint32_t seq;
    seq  = (uint32_t)slot->sequence_bytes[0];
    seq |= (uint32_t)slot->sequence_bytes[1] << 8U;
    seq |= (uint32_t)slot->sequence_bytes[2] << 16U;
    seq |= (uint32_t)slot->sequence_bytes[3] << 24U;
    return seq;
}

static void AppConfig_WriteSequence(config_slot_t *slot, uint32_t seq)
{
    slot->sequence_bytes[0] = (uint8_t)(seq & 0xFFU);
    slot->sequence_bytes[1] = (uint8_t)((seq >> 8U) & 0xFFU);
    slot->sequence_bytes[2] = (uint8_t)((seq >> 16U) & 0xFFU);
    slot->sequence_bytes[3] = (uint8_t)((seq >> 24U) & 0xFFU);
}

static void AppConfig_ResetRam(void)
{
    uint8_t i;
    for (i = 0U; i < APP_CONFIG_PARAM_COUNT; i++)
        g_config_ram[i] = g_config_defaults[i];
    g_config_sequence = 0U;
}

static uint8_t AppConfig_CheckSlot(uint8_t slot_idx, config_slot_t *slot)
{
    uint16_t eeprom_addr;
    uint16_t calc_crc;
    uint16_t stor_crc;

    eeprom_addr = g_config_slot_offset[slot_idx];

    BSP_BL24C16_Read(eeprom_addr, (uint8_t *)slot, APP_CONFIG_SLOT_SIZE);

    if (slot->magic_hi != APP_CONFIG_MAGIC_HI || slot->magic_lo != APP_CONFIG_MAGIC_LO)
        return 0U;
    if (slot->version != APP_CONFIG_VERSION) return 0U;
    if (slot->payload_length != APP_CONFIG_DATA_BYTES) return 0U;
    if (slot->commit_flag != APP_CONFIG_COMMIT_FLAG) return 0U;

    calc_crc = AppConfig_Crc16((const uint8_t *)slot, CONFIG_CRC_RANGE);
    stor_crc = (uint16_t)((uint16_t)slot->crc_high << 8U) | (uint16_t)slot->crc_low;

    if (calc_crc != stor_crc) return 0U;
    return 1U;
}

uint8_t AppConfig_Load(void)
{
    config_slot_t slot;
    uint8_t best_slot = 0U;
    uint32_t best_seq = 0U;
    uint8_t found = 0U;
    uint8_t i;

    if (!g_eeprom_online)
    {
        g_config_source = APP_CONFIG_SRC_EEPROM_OFFLINE;
        return 0U;
    }

    for (i = 0U; i < APP_CONFIG_SLOT_COUNT; i++)
    {
        if (AppConfig_CheckSlot(i, &slot))
        {
            uint32_t seq = AppConfig_ReadSequence(&slot);
            if (!found || ((int32_t)(seq - best_seq) > 0))
            {
                best_slot = i;
                best_seq = seq;
                found = 1U;
            }
        }
    }

    if (found)
    {
        uint16_t eeprom_addr = g_config_slot_offset[best_slot];
        BSP_BL24C16_Read(eeprom_addr, (uint8_t *)&slot, APP_CONFIG_SLOT_SIZE);

        for (i = 0U; i < APP_CONFIG_PARAM_COUNT; i++)
        {
            g_config_ram[i] = (uint16_t)((uint16_t)slot.data[i * 2U] |
                                ((uint16_t)slot.data[i * 2U + 1U] << 8U));
        }
        g_config_sequence = best_seq + 1U;
        g_config_source = APP_CONFIG_SRC_EEPROM_LOADED;
        return 1U;
    }

    g_config_source = APP_CONFIG_SRC_CRC_ERROR;
    AppConfig_ResetRam();
    return 0U;
}

static uint8_t AppConfig_WriteSlot(uint8_t slot_idx)
{
    config_slot_t slot;
    config_slot_t verify_slot;
    uint16_t eeprom_addr;
    uint16_t crc;
    uint8_t i;

    eeprom_addr = g_config_slot_offset[slot_idx];

    slot.magic_hi = APP_CONFIG_MAGIC_HI;
    slot.magic_lo = APP_CONFIG_MAGIC_LO;
    slot.version = APP_CONFIG_VERSION;
    slot.payload_length = APP_CONFIG_DATA_BYTES;
    AppConfig_WriteSequence(&slot, g_config_sequence);

    for (i = 0U; i < APP_CONFIG_PARAM_COUNT; i++)
    {
        slot.data[i * 2U]      = (uint8_t)(g_config_ram[i] & 0xFFU);
        slot.data[i * 2U + 1U] = (uint8_t)((g_config_ram[i] >> 8U) & 0xFFU);
    }

    for (i = 0U; i < 24U; i++)
        slot.padding[i] = 0x00U;

    slot.commit_flag = 0x00U;
    for (i = 0U; i < 3U; i++) slot.reserved2[i] = 0xFFU;

    crc = AppConfig_Crc16((const uint8_t *)&slot, CONFIG_CRC_RANGE);
    slot.crc_low  = (uint8_t)(crc & 0xFFU);
    slot.crc_high = (uint8_t)((crc >> 8U) & 0xFFU);

    if (!BSP_BL24C16_WriteByte((uint16_t)(eeprom_addr + 63U), 0x00U))
    {
        g_config_error_count++;
        g_last_result = 1U;
        return 0U;
    }

    for (i = 0U; i < 63U; i += BL24C16_PAGE_SIZE)
    {
        uint8_t remain = (uint8_t)(63U - i);
        uint8_t chunk = (remain > BL24C16_PAGE_SIZE) ? BL24C16_PAGE_SIZE : remain;
        if (BSP_BL24C16_WritePage((uint16_t)(eeprom_addr + i),
              ((const uint8_t *)&slot) + i, chunk) != chunk)
        {
            g_config_error_count++;
            g_last_result = 2U;
            return 0U;
        }
    }

    BSP_BL24C16_Read(eeprom_addr, (uint8_t *)&verify_slot, APP_CONFIG_SLOT_SIZE);

    if (verify_slot.magic_hi != APP_CONFIG_MAGIC_HI ||
        verify_slot.magic_lo != APP_CONFIG_MAGIC_LO ||
        verify_slot.version != APP_CONFIG_VERSION ||
        verify_slot.payload_length != APP_CONFIG_DATA_BYTES ||
        AppConfig_ReadSequence(&verify_slot) != g_config_sequence)
    {
        g_config_error_count++;
        g_last_result = 3U;
        return 0U;
    }

    for (i = 0U; i < APP_CONFIG_DATA_BYTES; i++)
    {
        if (verify_slot.data[i] != slot.data[i])
        {
            g_config_error_count++;
            g_last_result = 4U;
            return 0U;
        }
    }

    if (!BSP_BL24C16_WriteByte((uint16_t)(eeprom_addr + 63U), APP_CONFIG_COMMIT_FLAG))
    {
        g_config_error_count++;
        g_last_result = 5U;
        return 0U;
    }

    {
        uint8_t commit_verify;
        BSP_BL24C16_Read((uint16_t)(eeprom_addr + 63U), &commit_verify, 1U);
        if (commit_verify != APP_CONFIG_COMMIT_FLAG)
        {
            g_config_error_count++;
            g_last_result = 6U;
            return 0U;
        }
    }

    g_config_sequence++;
    g_config_save_count++;
    g_last_result = 0U;
    return 1U;
}

void AppConfig_Save(void)
{
    uint8_t slot_idx;

    if (!g_eeprom_online)
    {
        g_eeprom_online = BSP_BL24C16_Detect();
        if (!g_eeprom_online)
        {
            g_config_state = APP_CONFIG_STATE_IDLE;
            g_last_result = (uint8_t)(0x70U + BSP_BL24C16_GetLastError());
            return;
        }
    }

    if (g_config_state == APP_CONFIG_STATE_SAVING) return;

    g_config_state = APP_CONFIG_STATE_SAVING;
    g_save_pending = 0U;

    slot_idx = (uint8_t)(g_config_sequence % (uint32_t)APP_CONFIG_SLOT_COUNT);

    if (AppConfig_WriteSlot(slot_idx))
        g_config_state = APP_CONFIG_STATE_IDLE;
    else
        g_config_state = APP_CONFIG_STATE_ERROR;
}

void AppConfig_MarkDirty(void)
{
    if (!g_eeprom_online) return;

    if (g_config_state != APP_CONFIG_STATE_SAVING)
    {
        g_config_state = APP_CONFIG_STATE_DIRTY;
        g_config_dirty_timer = 0U;
        g_save_pending = 0U;
    }
}

uint8_t AppConfig_GetState(void) { return g_config_state; }

void AppConfig_GetStatus(app_config_status_t *st)
{
    st->state          = g_config_state;
    st->eeprom_online  = g_eeprom_online;
    st->active_slot    = (uint8_t)(g_config_sequence % (uint32_t)APP_CONFIG_SLOT_COUNT);
    st->dirty          = (g_config_state == APP_CONFIG_STATE_DIRTY) ? 1U : 0U;
    st->last_result    = g_last_result;
    st->sequence       = g_config_sequence;
    st->save_count     = g_config_save_count;
    st->error_count    = g_config_error_count;
    st->config_source  = g_config_source;
}

uint16_t AppConfig_GetRegister(uint16_t reg_addr)
{
    uint8_t idx;
    if (reg_addr > APP_CONFIG_MAX_REG_ADDR) return 0U;
    idx = g_config_addr_to_index[reg_addr];
    if (idx == 0xFFU) return 0U;
    return g_config_ram[idx];
}

uint8_t AppConfig_ValidateRegister(uint16_t reg_addr, uint16_t value)
{
    switch (reg_addr)
    {
        case 0U:  if (value > 1U) return 0U; break;
        case 1U:  if (value > 1000U) return 0U; break;
        case 2U:  if (value > 4U) return 0U; break;
        case 3U:  if (value > 1000U) return 0U; break;
        case 4U:  if (value > 1000U) return 0U; break;
        case 5U:  if (value > 1000U) return 0U; break;
        case 16U: if (value < 1U || value > 64U) return 0U; break;
        case 19U: if (value > 1500U) return 0U; break;
        case 32U: if (value < 1U || value > 247U) return 0U; break;
        case 33U: if (value > 5U) return 0U; break;
        case 34U: if (value > 2U) return 0U; break;
        default: return 1U;
    }
    return 1U;
}

void AppConfig_SetRegister(uint16_t reg_addr, uint16_t value)
{
    uint8_t idx;
    if (reg_addr > APP_CONFIG_MAX_REG_ADDR) return;
    idx = g_config_addr_to_index[reg_addr];
    if (idx == 0xFFU) return;
    if (!AppConfig_ValidateRegister(reg_addr, value)) return;

    if (g_config_ram[idx] != value)
    {
        g_config_ram[idx] = value;
        AppConfig_MarkDirty();
    }
}

void AppConfig_RestoreDefaults(void)
{
    uint8_t i;
    for (i = 0U; i < APP_CONFIG_PARAM_COUNT; i++)
        g_config_ram[i] = g_config_defaults[i];

    if (g_eeprom_online)
    {
        g_config_sequence = 0U;
        AppConfig_MarkDirty();
    }
}

void AppConfig_Init(void)
{
    BSP_BL24C16_Init();

    AppConfig_ResetRam();
    g_config_state = APP_CONFIG_STATE_IDLE;
    g_config_source = APP_CONFIG_SRC_EMPTY;
    g_config_dirty_timer = 0U;
    g_save_pending = 0U;
    g_config_save_count = 0U;
    g_config_error_count = 0U;
    g_last_result = 0U;

    g_eeprom_online = BSP_BL24C16_Detect();

    if (g_eeprom_online)
        AppConfig_Load();
    else
    {
        g_config_source = APP_CONFIG_SRC_EEPROM_OFFLINE;
        g_last_result = (uint8_t)(0x70U + BSP_BL24C16_GetLastError());
    }
}

void AppConfig_TimerTick1ms(void)
{
    if (!g_eeprom_online) return;

    if (g_config_state == APP_CONFIG_STATE_DIRTY)
    {
        g_config_dirty_timer++;
        if (g_config_dirty_timer >= APP_CONFIG_SAVE_DELAY_MS)
        {
            g_save_pending = 1U;
        }
    }
}

void AppConfig_Task(void)
{
    if (g_save_pending)
    {
        g_save_pending = 0U;
        AppConfig_Save();
    }
}
