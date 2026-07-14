/**
 * @file tk8710_rf_regs.h
 * @brief TK8710 RF寄存器定义 (SX1255/SX1257)
 */

#ifndef TK8710_RF_REGS_H
#define TK8710_RF_REGS_H

#include <stdint.h>

/*============================================================================
 * 频率转换常量
 *============================================================================*/
#define RF_SX1255_FREQ_STEP     30.517578125    /* SX1255: Hz/step */
#define RF_SX1257_FREQ_STEP     61.03515625     /* SX1257: Hz/step */

/*============================================================================
 * RF寄存器地址定义
 *============================================================================*/
/* RX频率寄存器 */
#define RF_REG_FRF_RX_MSB       0x01    /* RegFrfRxMsb */
#define RF_REG_FRF_RX_MID       0x02    /* RegFrfRxMid */
#define RF_REG_FRF_RX_LSB       0x03    /* RegFrfRxLsb */

/* TX频率寄存器 */
#define RF_REG_FRF_TX_MSB       0x04    /* RegFrfTxMsb */
#define RF_REG_FRF_TX_MID       0x05    /* RegFrfTxMid */
#define RF_REG_FRF_TX_LSB       0x06    /* RegFrfTxLsb */

/* 其他配置寄存器 */
#define RF_REG_RX_FILTER        0x12    /* RX滤波器配置 */
#define RF_REG_RX_BW            0x13    /* RX带宽配置 */
#define RF_REG_TX_DAC_BW        0x0B    /* RegTxDacBw */
#define RF_REG_RX_GAIN          0x0C    /* 接收增益 */

/*============================================================================
 * RF SPI命令定义 (地址 | 数据)
 *============================================================================*/
/* RX频率命令 (默认值) */
#define RF_CMD_FRF_RX_MSB       0x81E1  /* RegFrfRxMsb: E10000=450MHz, EB0000=470MHz, F00000=480MHz */
#define RF_CMD_FRF_RX_MID       0x8200  /* RegFrfRxMid */
#define RF_CMD_FRF_RX_LSB       0x8300  /* RegFrfRxLsb */

/* RX滤波器命令 */
#define RF_CMD_RX_FILTER        0x9200  /* 15 2C(125KHz), 12 1C(500KHz), 11 14(1MHz) for 32M */
#define RF_CMD_RX_FILTER_1M     0x9211  /* 15 2C(125KHz), 12 1C(500KHz), 11 14(1MHz) for 32M */
#define RF_CMD_RX_BW            0x9314  /* RX带宽配置 */

/* TX频率命令 (默认值) */
#define RF_CMD_FRF_TX_MSB       0x84E1  /* RegFrfTxMsb */
#define RF_CMD_FRF_TX_MID       0x8500  /* RegFrfTxMid */
#define RF_CMD_FRF_TX_LSB       0x8600  /* RegFrfTxLsb */

/* TX/RX增益命令 */
#define RF_CMD_TX_DAC_BW        0x8B05  /* RegTxDacBw: 05(32M), 07(1M) */
#define RF_CMD_RX_GAIN          0x8C7e  /* 接收增益: 3E, 7E, 36 */
#define RF_CMD_TX_GAIN  		0x882a //Tx Gain  //2a  2c

/* RF关闭/复位命令 */
#define RF_CMD_CLOSE_0          0x8003  /* rf_close 0x0 */
#define RF_CMD_CLOSE_1          0x8001  /* rf_close 0x1 */
#define RF_CMD_CLOSE_F          0x800F  /* rf_close 0xf */

/* RX PGA带宽命令 */
#define RF_CMD_RX_PGA_BW        0x9700  /* rx pga bw */

/* CLK设置命令 */
#define RF_CMD_CLK_SETTING      0x9002  /* CLK setting: bit[1] output clock enabled on pad CLK_OUT */

#endif /* TK8710_RF_REGS_H */
