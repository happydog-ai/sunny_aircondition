#ifndef BSP_BL24C16_H
#define BSP_BL24C16_H

#include "r_cg_macrodriver.h"

/*
 * BL24C16 2048-byte I2C EEPROM
 * Page size   = 16 bytes
 * Total size  = 2048 bytes (128 pages)
 * I2C address = 0x50 (7-bit base), upper 3 bits from memory A8-A10
 * WP          = hardware tied to GND, always writable
 * SCL         = P60 (SCLA0)
 * SDA         = P61 (SDAA0)
 */

#define BL24C16_PAGE_SIZE        (16U)
#define BL24C16_TOTAL_SIZE       (2048U)
#define BL24C16_MAX_ADDR         (2047U)
#define BL24C16_WRITE_TIMEOUT_MS (10U)

void BSP_BL24C16_Init(void);
uint8_t BSP_BL24C16_Detect(void);
uint8_t BSP_BL24C16_WriteByte(uint16_t addr, uint8_t data);
uint8_t BSP_BL24C16_WritePage(uint16_t addr, const uint8_t *data, uint8_t len);
uint8_t BSP_BL24C16_ReadByte(uint16_t addr);
void BSP_BL24C16_Read(uint16_t addr, uint8_t *buf, uint16_t len);
uint8_t BSP_BL24C16_Verify(uint16_t addr, const uint8_t *data, uint8_t len);
uint8_t BSP_BL24C16_SelfTest(void);
uint8_t BSP_BL24C16_GetLastError(void);

void BSP_I2C_CallbackSendEnd(void);
void BSP_I2C_CallbackRecvEnd(void);
void BSP_I2C_CallbackError(void);
void BSP_I2C_CallbackErrorCode(uint8_t code);

#endif
