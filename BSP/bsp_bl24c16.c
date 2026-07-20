#include "r_cg_macrodriver.h"
#include "r_cg_serial.h"
#include "bsp_bl24c16.h"

static volatile uint8_t g_i2c_done;
static volatile uint8_t g_i2c_error;
static volatile uint8_t g_bl24c16_last_error;

static uint8_t I2C_WaitDone(uint16_t timeout_ms);

static uint8_t BL24C16_WriteDevAddr(uint16_t mem_addr)
{
    return (uint8_t)(0xA0U | ((mem_addr >> 7U) & 0x0EU));
}

static uint8_t BL24C16_ReadDevAddr(uint16_t mem_addr)
{
    return (uint8_t)(BL24C16_WriteDevAddr(mem_addr) | 0x01U);
}

static uint8_t BL24C16_WordAddr(uint16_t mem_addr)
{
    return (uint8_t)(mem_addr & 0x00FFU);
}

void BSP_BL24C16_Init(void)
{
}

uint8_t BSP_BL24C16_Detect(void)
{
    uint8_t tx_buf[1];
    uint8_t dev_addr;
    uint8_t retry;

    dev_addr = BL24C16_WriteDevAddr(0x0000U);
    tx_buf[0] = BL24C16_WordAddr(0x0000U);

    for (retry = 0U; retry < 3U; retry++)
    {
        MD_STATUS status;

        g_i2c_done = 0U;
        g_i2c_error = 0U;
        g_bl24c16_last_error = 0U;

        status = R_IICA0_Master_Send(dev_addr, tx_buf, 1U, 255U);
        if (status == MD_ERROR1)
        {
            g_bl24c16_last_error = 0x01U;
        }
        else if (status == MD_ERROR2)
        {
            g_bl24c16_last_error = 0x02U;
        }
        else if (status != MD_OK)
        {
            g_bl24c16_last_error = 0x06U;
        }
        else if (I2C_WaitDone(BL24C16_WRITE_TIMEOUT_MS))
        {
            R_IICA0_StopCondition();
            return 1U;
        }

        R_IICA0_StopCondition();

        {
            volatile uint16_t to2 = 10000U;
            while (IICBSY0 != 0U && to2 > 0U)
            {
                to2--;
            }
        }
    }

    return 0U;
}

void BSP_I2C_CallbackSendEnd(void)
{
    g_i2c_done = 1U;
}

void BSP_I2C_CallbackRecvEnd(void)
{
    g_i2c_done = 1U;
}

void BSP_I2C_CallbackError(void)
{
    g_i2c_error = 1U;
    g_bl24c16_last_error = 0x03U;
}

void BSP_I2C_CallbackErrorCode(uint8_t code)
{
    g_i2c_error = 1U;
    if (code == MD_NACK)
    {
        g_bl24c16_last_error = 0x03U;
    }
    else if (code == MD_SPT)
    {
        g_bl24c16_last_error = 0x04U;
    }
    else
    {
        g_bl24c16_last_error = 0x06U;
    }
}

uint8_t BSP_BL24C16_GetLastError(void)
{
    return g_bl24c16_last_error;
}

static uint8_t I2C_WaitDone(uint16_t timeout_ms)
{
    volatile uint16_t i;
    for (i = 0U; i < (timeout_ms * 3000U); i++)
    {
        if (g_i2c_done != 0U) return 1U;
        if (g_i2c_error != 0U) return 0U;
    }
    g_bl24c16_last_error = 0x05U;
    return 0U;
}

static void BL24C16_WaitWriteCycle(void)
{
    /*
     * 24Cxx EEPROM needs several milliseconds for the internal write cycle
     * after a byte/page write.  During that time the next write may be NACKed.
     */
    volatile uint16_t i;
    for (i = 0U; i < 60000U; i++)
    {
        ;
    }
}

static uint8_t I2C_WriteThenStop(uint8_t dev_addr, uint8_t *tx_buf, uint16_t tx_num)
{
    MD_STATUS status;

    g_i2c_done = 0U;
    g_i2c_error = 0U;
    g_bl24c16_last_error = 0U;

    status = R_IICA0_Master_Send(dev_addr, tx_buf, tx_num, 255U);
    if (status == MD_ERROR1)
    {
        g_bl24c16_last_error = 0x01U;
        return 0U;
    }
    else if (status == MD_ERROR2)
    {
        g_bl24c16_last_error = 0x02U;
        R_IICA0_StopCondition();
        return 0U;
    }
    else if (status != MD_OK)
    {
        g_bl24c16_last_error = 0x06U;
        R_IICA0_StopCondition();
        return 0U;
    }

    if (!I2C_WaitDone(BL24C16_WRITE_TIMEOUT_MS))
    {
        R_IICA0_StopCondition();
        return 0U;
    }

    R_IICA0_StopCondition();
    BL24C16_WaitWriteCycle();
    return 1U;
}

static uint8_t I2C_WriteByteAddr(uint16_t mem_addr, const uint8_t *data, uint8_t len)
{
    uint8_t tx_buf[1 + BL24C16_PAGE_SIZE];
    uint8_t dev_addr;

    if (len > BL24C16_PAGE_SIZE) return 0U;

    dev_addr = BL24C16_WriteDevAddr(mem_addr);
    tx_buf[0] = BL24C16_WordAddr(mem_addr);
    {
        uint8_t k;
        for (k = 0U; k < len; k++) tx_buf[1U + k] = data[k];
    }

    return I2C_WriteThenStop(dev_addr, tx_buf, (uint16_t)(1U + len));
}

uint8_t BSP_BL24C16_WriteByte(uint16_t addr, uint8_t data)
{
    if (addr > BL24C16_MAX_ADDR) return 0U;
    return I2C_WriteByteAddr(addr, &data, 1U);
}

uint8_t BSP_BL24C16_WritePage(uint16_t addr, const uint8_t *data, uint8_t len)
{
    uint8_t page_remaining;
    uint8_t chunk;

    if ((addr > BL24C16_MAX_ADDR) || (len == 0U) || (data == 0)) return 0U;
    if ((uint32_t)addr + (uint32_t)len > (uint32_t)(BL24C16_MAX_ADDR + 1U)) return 0U;

    page_remaining = (uint8_t)(BL24C16_PAGE_SIZE - (uint8_t)(addr & 0x0FU));
    chunk = (len > page_remaining) ? page_remaining : len;

    return I2C_WriteByteAddr(addr, data, chunk) ? chunk : 0U;
}

uint8_t BSP_BL24C16_ReadByte(uint16_t addr)
{
    uint8_t data;
    uint8_t tx_buf[1];

    if (addr > BL24C16_MAX_ADDR) return 0U;

    tx_buf[0] = BL24C16_WordAddr(addr);

    if (!I2C_WriteThenStop(BL24C16_WriteDevAddr(addr), tx_buf, 1U)) return 0U;

    g_i2c_done = 0U;
    g_i2c_error = 0U;
    R_IICA0_Master_Receive(BL24C16_ReadDevAddr(addr), &data, 1U, 255U);

    if (!I2C_WaitDone(BL24C16_WRITE_TIMEOUT_MS))
    {
        R_IICA0_StopCondition();
        return 0U;
    }

    R_IICA0_StopCondition();
    return data;
}

void BSP_BL24C16_Read(uint16_t addr, uint8_t *buf, uint16_t len)
{
    uint8_t tx_buf[1];

    if ((addr > BL24C16_MAX_ADDR) || (buf == 0) || (len == 0U)) return;
    if ((uint32_t)(addr + len) > (uint32_t)(BL24C16_MAX_ADDR + 1U)) return;

    tx_buf[0] = BL24C16_WordAddr(addr);

    if (!I2C_WriteThenStop(BL24C16_WriteDevAddr(addr), tx_buf, 1U)) return;

    g_i2c_done = 0U;
    g_i2c_error = 0U;
    R_IICA0_Master_Receive(BL24C16_ReadDevAddr(addr), buf, len, 255U);

    if (!I2C_WaitDone(BL24C16_WRITE_TIMEOUT_MS))
    {
        R_IICA0_StopCondition();
        return;
    }

    R_IICA0_StopCondition();
}

uint8_t BSP_BL24C16_Verify(uint16_t addr, const uint8_t *data, uint8_t len)
{
    uint8_t buf[BL24C16_PAGE_SIZE];
    uint8_t i;

    if (len > BL24C16_PAGE_SIZE) return 0U;

    BSP_BL24C16_Read(addr, buf, (uint16_t)len);

    for (i = 0U; i < len; i++)
    {
        if (buf[i] != data[i]) return 0U;
    }
    return 1U;
}

uint8_t BSP_BL24C16_SelfTest(void)
{
    uint8_t bak[32], tst[32];
    uint8_t i, r;

    if (!BSP_BL24C16_Detect()) return 0U;
    r = 1U;

    BSP_BL24C16_Read(0x000U, bak, 2U);
    tst[0] = 0x55U; tst[1] = 0xAAU;
    BSP_BL24C16_WritePage(0x000U, tst, 2U);
    BSP_BL24C16_Read(0x000U, tst, 2U);
    if (tst[0] != 0x55U || tst[1] != 0xAAU) r = 0U;
    BSP_BL24C16_WritePage(0x000U, bak, 2U);

    BSP_BL24C16_Read(0x00EU, bak, 20U);
    for (i = 0U; i < 20U; i++) tst[i] = (uint8_t)(0xA0U + i);
    for (i = 0U; (i + 15U) < 20U; i += 16U)
    {
        uint8_t ch = (uint8_t)((20U - i) > 16U ? 16U : (20U - i));
        BSP_BL24C16_WritePage((uint16_t)(0x00EU + i), &tst[i], ch);
    }
    BSP_BL24C16_Read(0x00EU, tst, 20U);
    for (i = 0U; i < 20U; i++) { if (tst[i] != (uint8_t)(0xA0U+i)) { r = 0U; break; } }
    for (i = 0U; (i + 15U) < 20U; i += 16U)
    {
        uint8_t ch = (uint8_t)((20U - i) > 16U ? 16U : (20U - i));
        BSP_BL24C16_WritePage((uint16_t)(0x00EU + i), &bak[i], ch);
    }

    BSP_BL24C16_Read(0x0FEU, bak, 4U);
    tst[0]=0x11U; tst[1]=0x22U; tst[2]=0x33U; tst[3]=0x44U;
    BSP_BL24C16_WritePage(0x0FEU, tst, 2U);
    BSP_BL24C16_Read(0x0FEU, tst, 2U);
    if (tst[0]!=0x11U||tst[1]!=0x22U) r=0U;
    BSP_BL24C16_WritePage(0x100U, &tst[2], 2U);
    BSP_BL24C16_Read(0x100U, tst, 2U);
    if (tst[0]!=0x33U||tst[1]!=0x44U) r=0U;
    BSP_BL24C16_WritePage(0x0FEU, bak, 2U);
    BSP_BL24C16_WritePage(0x100U, &bak[2], 2U);

    BSP_BL24C16_Read(0x100U, bak, 4U);
    tst[0]=0x10U; tst[1]=0x20U; tst[2]=0x01U; tst[3]=0x02U;
    BSP_BL24C16_WritePage(0x100U, tst, 4U);
    BSP_BL24C16_Read(0x100U, tst, 4U);
    if (tst[0]!=0x10U) r=0U;
    BSP_BL24C16_WritePage(0x100U, bak, 4U);

    BSP_BL24C16_Read(0x200U, bak, 4U);
    tst[0]=0x20U; tst[1]=0x30U; tst[2]=0xA0U; tst[3]=0xB0U;
    BSP_BL24C16_WritePage(0x200U, tst, 4U);
    BSP_BL24C16_Read(0x200U, tst, 4U);
    if (tst[0]!=0x20U||tst[3]!=0xB0U) r=0U;
    BSP_BL24C16_WritePage(0x200U, bak, 4U);

    BSP_BL24C16_Read(0x700U, bak, 4U);
    tst[0]=0x70U; tst[1]=0x71U; tst[2]=0x72U; tst[3]=0x73U;
    BSP_BL24C16_WritePage(0x700U, tst, 4U);
    BSP_BL24C16_Read(0x700U, tst, 4U);
    if (tst[0]!=0x70U) r=0U;
    BSP_BL24C16_WritePage(0x700U, bak, 4U);

    BSP_BL24C16_Read(0x7FFU, bak, 1U);
    tst[0]=0x7FU;
    BSP_BL24C16_WriteByte(0x7FFU, tst[0]);
    BSP_BL24C16_Read(0x7FFU, tst, 1U);
    if (tst[0]!=0x7FU) r=0U;
    BSP_BL24C16_WriteByte(0x7FFU, bak[0]);

    return r;
}
