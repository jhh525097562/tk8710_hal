/* Host test: gcc -O2 -fwhole-program -DPLATFORM_TMS570 with project includes. */
#include <assert.h>
#include <stdio.h>
#include "../source/tk8710_sat_payload_app.c"

TRMLogConfig g_trmLogConfig;
static slotCfg_t configured;
static TK8710DriverCallbacks registered;
static unsigned writes;
static int failWrite;
static int started;
static int resetFail;
int TK8710GpioIrqEnable(uint8_t pin, uint8_t enable) { return 0; }
int TK8710CaptureCancel(void) { return 0; }
int TK8710Reset(uint8_t type) { return resetFail ? -1 : TK8710_OK; }
int TRM_StopFrequencySweep(void) { return TRM_OK; }
TK8710HalError TK8710HalReset(void) { return TK8710_HAL_OK; }
void TRM_LogOutput(TRMLogLevel level, const char* tag, const char* file,
                   int line, const char* func, const char* fmt, ...) {}
void TK8710RegisterCallbacks(const TK8710DriverCallbacks* cb)
{
    if (cb) registered = *cb;
    else memset(&registered, 0, sizeof(registered));
}
int TK8710Init(const ChipConfig* cfg)
{
    assert(cfg->ant_en == 255 && cfg->tx_bcn_en == 1);
    assert(cfg->rfConfig->rxgain == 0x7e);
    return TK8710_OK;
}
int TK8710SetConfig(TK8710ConfigType type, const void* cfg)
{
    assert(type == TK8710_CFG_TYPE_SLOT_CFG);
    configured = *(const slotCfg_t*)cfg;
    return TK8710_OK;
}
int TK8710WriteReg(uint8_t type, uint16_t address, uint32_t value)
{
    assert(address == RX_FE_BASE + offsetof(struct rx_top, ddc) + writes * 0x1000U);
    assert(value == 0x01b33333U);
    writes++;
    return failWrite ? -1 : TK8710_OK;
}
int TK8710Start(uint8_t type, uint8_t mode)
{
    assert(type == TK8710_MODE_SLAVE && mode == TK8710_WORK_MODE_CONTINUOUS);
    started++;
    return TK8710_OK;
}
int TK8710GetRxUserSignalQuality(uint8_t user, uint32_t* rssi, uint8_t* snr, uint32_t* freq)
{
    *rssi = 1800; *snr = 24; *freq = 0x03ffff80;
    return user == 0 ? TK8710_OK : -1;
}
int main(void)
{
    static const uint8_t rates[] = {5,6,7,8,9,10,11,18};
    static const uint32_t lengths[] = {135072,69536,36768,20384,12192,8096,6048,6048};
    SatPayloadWorkParams params;
    TK8710IrqResult irq;
    unsigned i;
    memset(&params, 0, sizeof(params));
    params.centerFreqHz = 509100000;
    params.rateCount = 1;
    for (i = 0; i < sizeof(rates); i++) {
        params.rates[0].rateMode = rates[i];
        writes = 0;
        assert(SatPayloadStartSensitivity(&params) == SAT_PAYLOAD_OK);
        assert(writes == 8);
        assert(configured.s3Cfg[0].da_m == lengths[i]);
        assert(configured.s3Cfg[0].byteLen == (rates[i] == 18 ? 36 : 22));
        assert(configured.local_sync == 0 && configured.s1Cfg[0].byteLen == 0);
    }
    memset(&irq, 0, sizeof(irq));
    irq.irq_type = TK8710_IRQ_MD_DATA;
    registered.onRxData(&irq);
    assert(g_sensitivityWindows == 1 && g_sensitivityLost == 1);
    irq.crcValidCount = 1; irq.crcResults[0].crcValid = 1;
    registered.onRxData(&irq);
    assert(g_sensitivityWindows == 2 && g_sensitivityLost == 1 && g_sensitivityValidUsers == 1);
    irq.crcResults[0].crcValid = 0;
    irq.crcResults[1].crcValid = 1; /* CRC passes but quality read fails. */
    registered.onRxData(&irq);
    assert(g_sensitivityLost == 2);
    irq.irq_type = TK8710_IRQ_RX_BCN;
    registered.onRxData(&irq);
    assert(g_sensitivityWindows == 3);
    irq.irq_type = TK8710_IRQ_MD_DATA;
    for (i = 3; i < 100; i++) registered.onRxData(&irq);
    assert(g_sensitivityWindows == 100 && g_sensitivityLost == 99);
    assert(g_sensitivityWindowLost == 0);
    writes = 0; failWrite = 1; started = 0;
    assert(SatPayloadStartSensitivity(&params) == SAT_PAYLOAD_ERR_DRIVER);
    assert(writes == 1 && started == 0);
    resetFail = 1;
    assert(SatPayloadStopHal() == SAT_PAYLOAD_ERR_DRIVER);
    assert(g_sensitivityActive == 1);
    resetFail = 0;
    assert(SatPayloadStopHal() == SAT_PAYLOAD_OK);
    assert(g_sensitivityActive == 0 && g_satPayload.halActive == 0);
    assert(registered.onRxData == NULL);
    params.rateCount = 2;
    assert(SatPayloadStartSensitivity(&params) == SAT_PAYLOAD_ERR_PARAM);
    puts("rx_sensitivity_test: PASS");
    return 0;
}
