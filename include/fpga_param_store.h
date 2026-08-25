#ifndef FPGA_PARAM_STORE_H
#define FPGA_PARAM_STORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FPGA_PARAM_STORE_BANK7_SECTOR2_BASE 0xF0208000UL
#define FPGA_PARAM_STORE_BANK7_SECTOR2_SIZE 0x00004000UL

#define FPGA_BOOT_FLAG_STORE_BANK7_SECTOR1_BASE 0xF0204000UL
#define FPGA_BOOT_FLAG_STORE_BANK7_SECTOR1_SIZE 0x00004000UL

    typedef struct
    {
        uint8_t workMode;
        uint8_t rateMode;
        uint8_t slotConfig;
        uint8_t txPower;
        uint32_t centerFreqHz;
        uint8_t rfMask;
        uint8_t dcValidMask;
        int16_t dcI[8U];
        int16_t dcQ[8U];
    } FpgaStoredParams;

    int FpgaParamStore_Load(FpgaStoredParams *params);
    int FpgaParamStore_Save(const FpgaStoredParams *params);
    int FpgaParamStore_LoadBootFlag(uint8_t *flag);
    int FpgaParamStore_SaveBootFlag(uint8_t flag);
    int FpgaParamStore_LoadResetCount(uint32_t *count);
    int FpgaParamStore_SaveResetCount(uint32_t count);

#if defined(FPGA_PROTOCOL_HOST_TEST)
    void FpgaParamStore_TestErase(void);
    void FpgaParamStore_TestCorrupt(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* FPGA_PARAM_STORE_H */
