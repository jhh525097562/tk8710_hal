#ifndef SPI_FLASH_H
#define SPI_FLASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    int SpiFlash_Init(void);
    int SpiFlash_ReadJedecId(uint8_t id[3]);
    int SpiFlash_ReadStatusRegisters(uint8_t *sr1, uint8_t *sr2);
    int SpiFlash_Read(uint32_t address, uint8_t *data, uint32_t length);
    int SpiFlash_EraseSector(uint32_t address);
    int SpiFlash_PageProgram(uint32_t address, const uint8_t *data, uint32_t length);

#if defined(FPGA_PROTOCOL_HOST_TEST)
    void SpiFlash_TestEraseAll(void);
    void SpiFlash_TestSetInitResult(int result);
    void SpiFlash_TestSetProgramPersists(uint8_t persists);
    uint32_t SpiFlash_TestGetEraseCount(void);
    uint32_t SpiFlash_TestGetProgramCount(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SPI_FLASH_H */
