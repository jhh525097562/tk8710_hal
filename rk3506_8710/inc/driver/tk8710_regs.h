/**
 * @file tk8710_regs.h
 * @brief TK8710 寄存器定义
 */

#ifndef TK8710_REGS_H
#define TK8710_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* SPI 命令定义 */
#define TK8710_CMD_RST          0x00
#define TK8710_CMD_WR_REG       0x01
#define TK8710_CMD_RD_REG       0x02
#define TK8710_CMD_WR_BUFF      0x03
#define TK8710_CMD_RD_BUFF      0x04

/* 复位参数定义 */
#define TK8710_RST_SM_ONLY      0x01    /* 仅复位状态机 */
#define TK8710_RST_SM_AND_REG   0x02    /* 复位状态机和寄存器 */

/* 寄存器类型定义 */
#define TK8710_REG_TYPE_GLOBAL  0x00    /* 全局/普通寄存器 */
#define TK8710_REG_TYPE_RF0     0x01    /* RF0寄存器 */
#define TK8710_REG_TYPE_RF1     0x02    /* RF1寄存器 */
#define TK8710_REG_TYPE_RF2     0x04    /* RF2寄存器 */
#define TK8710_REG_TYPE_RF3     0x08    /* RF3寄存器 */
#define TK8710_REG_TYPE_RF4     0x10    /* RF4寄存器 */
#define TK8710_REG_TYPE_RF5     0x20    /* RF5寄存器 */
#define TK8710_REG_TYPE_RF6     0x40    /* RF6寄存器 */
#define TK8710_REG_TYPE_RF7     0x80    /* RF7寄存器 */
#define TK8710_REG_TYPE_RF_ALL  0xFF    /* 全部RF寄存器 */

/* 中断位定义 */
#define TK8710_IRQ_BIT_RX_BCN       (1 << 0)
#define TK8710_IRQ_BIT_BRD_UD       (1 << 1)
#define TK8710_IRQ_BIT_BRD_DATA     (1 << 2)
#define TK8710_IRQ_BIT_MD_UD        (1 << 3)
#define TK8710_IRQ_BIT_MD_DATA      (1 << 4)
#define TK8710_IRQ_BIT_S0           (1 << 5)
#define TK8710_IRQ_BIT_S1           (1 << 6)
#define TK8710_IRQ_BIT_S2           (1 << 7)
#define TK8710_IRQ_BIT_S3           (1 << 8)
#define TK8710_IRQ_BIT_ACM          (1 << 9)
#define TK8710_IRQ_BIT_TO           (1 << 10)

/* 寄存器基地址 */
#define RX_FE_BASE  0x0000
#define TX_FE_BASE  0x0800
#define ACM_BASE    0x9000
#define RX_BCN_BASE 0x9400
#define RX_MUP_BASE 0x9800
#define TX_MOD_BASE 0xa000
#define MAC_BASE    0xc000

typedef union s_rx_ctrl1 
{
    uint32_t data;
    struct
    {
        uint32_t rx_bypass  : 12;
        uint32_t cap_cfg  : 4;
        uint32_t gc_bypass  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t afifo_rnco  : 8;
        uint32_t afifo_wgap  : 4;
    }b;
}s_rx_ctrl1;

typedef union s_rx_dfe0 
{
    uint32_t data;
    struct
    {
        uint32_t dfe_ddc_phase  : 25;
        uint32_t RESERVED_0 : 3;
        uint32_t dfe_ddc_bypass  : 1;
        uint32_t RESERVED_1 : 3;
    }b;
}s_rx_dfe0;

typedef union s_rx_dfe1 
{
    uint32_t data;
    struct
    {
        uint32_t rx_swap  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t rx_negate  : 2;
        uint32_t RESERVED_1 : 2;
        uint32_t rx_cfg  : 3;
        uint32_t RESERVED_2 : 1;
        uint32_t rx_cmp_alpha  : 8;
        uint32_t rx_cmp_bypass  : 1;
        uint32_t RESERVED_3 : 3;
        uint32_t rx_sft  : 8;
    }b;
}s_rx_dfe1;

typedef union s_rx_dfe2 
{
    uint32_t data;
    struct
    {
        uint32_t rx_agc_gain_q  : 16;
        uint32_t rx_agc_gain_i  : 16;
    }b;
}s_rx_dfe2;

typedef union s_rx_format 
{
    uint32_t data;
    struct
    {
        uint32_t q_dc_offset  : 12;
        uint32_t i_dc_offset  : 12;
        uint32_t iq_tc_or_us  : 1;
        uint32_t iq_swap  : 1;
        uint32_t msb_lsb_swap  : 1;
        uint32_t RESERVED_0 : 1;
        uint32_t loop  : 1;
        uint32_t RESERVED_1 : 3;
    }b;
}s_rx_format;

typedef union s_iqib_top 
{
    uint32_t data;
    struct
    {
        uint32_t iqib_set_b  : 12;
        uint32_t iqib_set_a  : 10;
        uint32_t iqib_set_val  : 1;
        uint32_t iqib_bypass  : 1;
        uint32_t iqib_step_b  : 2;
        uint32_t iqib_step_a  : 2;
        uint32_t iqib_period  : 3;
        uint32_t iqib_hold  : 1;
    }b;
}s_iqib_top;

typedef union s_dc_loop_0 
{
    uint32_t data;
    struct
    {
        uint32_t num_ct_14   : 8;
        uint32_t num_ct_6    : 8;
        uint32_t num_ct_2    : 8;
        uint32_t rx_en  : 1;
        uint32_t RESERVED_0 : 7;
    }b;
}s_dc_loop_0;

typedef union s_dc_loop_1 
{
    uint32_t data;
    struct
    {
        uint32_t num_ct_190  : 8;
        uint32_t num_ct_126  : 8;
        uint32_t num_ct_62   : 8;
        uint32_t num_ct_30   : 8;
    }b;
}s_dc_loop_1;

typedef union s_cw_remove_0 
{
    uint32_t data;
    struct
    {
        uint32_t cw1_angle_base  : 25;
        uint32_t RESERVED_0 : 3;
        uint32_t cw1_bypass  : 1;
        uint32_t RESERVED_1 : 3;
    }b;
}s_cw_remove_0;

typedef union s_cw_remove_1 
{
    uint32_t data;
    struct
    {
        uint32_t cw2_cnt_max  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t cw1_cnt_max  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_cw_remove_1;

typedef union s_cw_remove_2 
{
    uint32_t data;
    struct
    {
        uint32_t cw2_angle_base  : 25;
        uint32_t RESERVED_0 : 3;
        uint32_t cw2_bypass  : 1;
        uint32_t RESERVED_1 : 3;
    }b;
}s_cw_remove_2;

typedef union s_ddc 
{
    uint32_t data;
    struct
    {
        uint32_t ddc_freq_ini  : 25;
        uint32_t RESERVED_0 : 7;
    }b;
}s_ddc;

typedef union s_acf 
{
    uint32_t data;
    struct
    {
        uint32_t src_norm_inrate   : 24;
        uint32_t acf_cyc_num       : 4;
        uint32_t acf_bypass        : 1;
        uint32_t RESERVED_0 : 3;
    }b;
}s_acf;

typedef union s_src 
{
    uint32_t data;
    struct
    {
        uint32_t src_init_phase        : 24;
        uint32_t src_phase_ini_val     : 1;
        uint32_t RESERVED_0 : 7;
    }b;
}s_src;

typedef union s_rx_ds_flt_0 
{
    uint32_t data;
    struct
    {
        uint32_t rx_ds_next_interval_1  : 16;
        uint32_t rx_ds_next_interval_0  : 16;
    }b;
}s_rx_ds_flt_0;

typedef union s_rx_ds_flt_1 
{
    uint32_t data;
    struct
    {
        uint32_t rx_ds_next_interval_3  : 16;
        uint32_t rx_ds_next_interval_2  : 16;
    }b;
}s_rx_ds_flt_1;

typedef union s_rx_ds_flt_2 
{
    uint32_t data;
    struct
    {
        uint32_t rx_ds_next_interval_4  : 16;
        uint32_t ds_flt_bypass  : 6;
        uint32_t RESERVED_0 : 2;
        uint32_t rx_match_bypass  : 1;
        uint32_t RESERVED_1 : 3;
        uint32_t ds_sel  : 1;
        uint32_t RESERVED_2 : 3;
    }b;
}s_rx_ds_flt_2;

typedef union s_rx_dc
{
    uint32_t data;
    struct
    {
        uint32_t q_dc_offset  : 16;
        uint32_t i_dc_offset  : 16;
    }b;
}s_rx_dc;


typedef union s_dagc3_1 
{
    uint32_t data;
    struct
    {
        uint32_t dagc3_manual_sft  : 4;
        uint32_t dagc3_manual_en  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t dagc3_bypass  : 1;
        uint32_t RESERVED_1 : 3;
        uint32_t src_norm_inrate_bcn  : 4;
        uint32_t ds_flt_bypass_bcn  : 6;
        uint32_t RESERVED_2 : 2;
        uint32_t p2s_cyc_len  : 4;
        uint32_t RESERVED_3 : 4;
    }b;
}s_dagc3_1;

typedef union s_dagc2 
{
    uint32_t data;
    struct
    {
        uint32_t dagc2_alpha  : 4;
        uint32_t dagc2_auto  : 5;
        uint32_t RESERVED_0 : 3;
        uint32_t dagc2_manual_sft  : 4;
        uint32_t dagc2_manual_en  : 1;
        uint32_t RESERVED_1 : 3;
        uint32_t dagc2_lpf_sft_max  : 4;
        uint32_t dagc2_bypass  : 1;
        uint32_t RESERVED_2 : 3;
        uint32_t stop_en  : 2;
        uint32_t RESERVED_3 : 2;
    }b;
}s_dagc2;

typedef union s_rx_sync_0 
{
    uint32_t data;
    struct
    {
        uint32_t seg_num  : 8;
        uint32_t len_freqcomp  : 3;
        uint32_t RESERVED_0 : 1;
        uint32_t len_dsss  : 2;
        uint32_t drift_len  : 2;
        uint32_t toscope  : 3;
        uint32_t RESERVED_1 : 1;
        uint32_t sym_num  : 9;
        uint32_t RESERVED_2 : 3;
    }b;
}s_rx_sync_0;

typedef union s_rx_sync_1 
{
    uint32_t data;
    struct
    {
        uint32_t freq_inter  : 16;
        uint32_t freq_start  : 16;
    }b;
}s_rx_sync_1;

typedef union s_rx_sync_2 
{
    uint32_t data;
    struct
    {
        uint32_t drift_i  : 16;
        uint32_t drift_s  : 16;
    }b;
}s_rx_sync_2;

typedef union s_spi_cfg3 
{
    uint32_t data;
    struct
    {
        uint32_t spi_data1  : 32;
    }b;
}s_spi_cfg3;

typedef union s_spi_cfg0 
{
    uint32_t data;
    struct
    {
        uint32_t spi_cfg  : 32;
    }b;
}s_spi_cfg0;

typedef union s_spi_cfg1 
{
    uint32_t data;
    struct
    {
        uint32_t spi_data  : 32;
    }b;
}s_spi_cfg1;

typedef union s_spi_cfg2 
{
    uint32_t data;
    struct
    {
        uint32_t spi_st  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t spi_clr  : 1;
        uint32_t RESERVED_1 : 3;
        uint32_t rfgain_addr  : 16;
        uint32_t RESERVED_2 : 8;
    }b;
}s_spi_cfg2;

typedef union s_sync_ob 
{
    uint32_t data;
    struct
    {
        uint32_t sync_phs  : 10;
        uint32_t RESERVED_0 : 2;
        uint32_t sync_max_pos  : 11;
        uint32_t RESERVED_1 : 9;
    }b;
}s_sync_ob;

typedef union s_adc_if 
{
    uint32_t data;
    struct
    {
        uint32_t adc_q  : 11;
        uint32_t RESERVED_0 : 1;
        uint32_t adc_i  : 11;
        uint32_t RESERVED_1 : 1;
        uint32_t adc_v  : 1;
        uint32_t RESERVED_2 : 7;
    }b;
}s_adc_if;

typedef union s_afifo_tb_0 
{
    uint32_t data;
    struct
    {
        uint32_t afifo_dataq  : 11;
        uint32_t RESERVED_0 : 1;
        uint32_t afifo_datai  : 11;
        uint32_t RESERVED_1 : 1;
        uint32_t afifo_rvld  : 1;
        uint32_t tb_afifo_rempty  : 1;
        uint32_t tb_afifo_wfull  : 1;
        uint32_t RESERVED_2 : 5;
    }b;
}s_afifo_tb_0;

typedef union s_dig_format_tb_0 
{
    uint32_t data;
    struct
    {
        uint32_t rx_dig_format_dataq  : 13;
        uint32_t RESERVED_0 : 3;
        uint32_t rx_dig_format_datai  : 13;
        uint32_t RESERVED_1 : 2;
        uint32_t rx_dig_format_rvld  : 1;
    }b;
}s_dig_format_tb_0;

typedef union s_iqib_top_r 
{
    uint32_t data;
    struct
    {
        uint32_t tb_iqib_est_b  : 12;
        uint32_t tb_iqib_est_a  : 10;
        uint32_t RESERVED_0 : 10;
    }b;
}s_iqib_top_r;

typedef union s_dcloop_tb_0 
{
    uint32_t data;
    struct
    {
        uint32_t dc_loop_rx_q_dco  : 13;
        uint32_t RESERVED_0 : 3;
        uint32_t dc_loop_rx_i_dco  : 13;
        uint32_t RESERVED_1 : 2;
        uint32_t dc_loop_rx_v_dco  : 1;
    }b;
}s_dcloop_tb_0;

typedef union s_dcloop_tb_1 
{
    uint32_t data;
    struct
    {
        uint32_t tb_dc_dloop_est80_q  : 13;
        uint32_t RESERVED_0 : 3;
        uint32_t tb_dc_dloop_est80_i  : 13;
        uint32_t RESERVED_1 : 2;
        uint32_t tb_dc_dloop_est80_v  : 1;
    }b;
}s_dcloop_tb_1;

typedef union s_cw1_tb 
{
    uint32_t data;
    struct
    {
        uint32_t cw2_data_out_i  : 13;
        uint32_t RESERVED_0 : 3;
        uint32_t cw1_data_out_i  : 13;
        uint32_t RESERVED_1 : 3;
    }b;
}s_cw1_tb;

typedef union s_rd_obv_0 
{
    uint32_t data;
    struct
    {
        uint32_t psk_snr  : 9;
        uint32_t RESERVED_0 : 3;
        uint32_t fsk_snr  : 9;
        uint32_t RESERVED_1 : 3;
        uint32_t crc_err  : 1;
        uint32_t demod_fsm  : 3;
        uint32_t sf_bit  : 2;
        uint32_t RESERVED_2 : 1;
        uint32_t pkt_done  : 1;
    }b;
}s_rd_obv_0;

typedef union s_rd_obv_1 
{
    uint32_t data;
    struct
    {
        uint32_t rx_da  : 32;
    }b;
}s_rd_obv_1;

typedef union s_sync_obv_0 
{
    uint32_t data;
    struct
    {
        uint32_t avg_pow  : 15;
        uint32_t RESERVED_0 : 1;
        uint32_t est_cfo  : 16;
    }b;
}s_sync_obv_0;

typedef union s_sync_obv_1 
{
    uint32_t data;
    struct
    {
        uint32_t max_val  : 20;
        uint32_t dagc2_shift_l  : 4;
        uint32_t dagc3_shift  : 4;
        uint32_t dagc2_shift  : 4;
    }b;
}s_sync_obv_1;

typedef union s_rx_fd_comp 
{
    uint32_t data;
    struct
    {
        uint32_t fd_comp_l  : 11;
        uint32_t RESERVED_0 : 1;
        uint32_t drift_fs  : 10;
        uint32_t RESERVED_1 : 10;
    }b;
}s_rx_fd_comp;

typedef union s_spi_res0 
{
    uint32_t data;
    struct
    {
        uint32_t spi_rd_data  : 31;
        uint32_t spi_busy_op  : 1;
    }b;
}s_spi_res0;

typedef union s_spi_res1 
{
    uint32_t data;
    struct
    {
        uint32_t sw_hw_conf  : 1;
        uint32_t RESERVED_0 : 31;
    }b;
}s_spi_res1;

struct rx_top
{
    volatile uint32_t  reserved_0[1];	//0x0
    volatile union      s_rx_ctrl1 rx_ctrl1;	//0x4
    volatile union      s_rx_dfe0 rx_dfe0;	//0x8
    volatile union      s_rx_dfe1 rx_dfe1;	//0xc
    volatile union      s_rx_dfe2 rx_dfe2;	//0x10
    volatile union      s_rx_format rx_format;	//0x14
    volatile union      s_iqib_top iqib_top;	//0x18
    volatile union      s_dc_loop_0 dc_loop_0;	//0x1c
    volatile union      s_dc_loop_1 dc_loop_1;	//0x20
    volatile union      s_cw_remove_0 cw_remove_0;	//0x24
    volatile union      s_cw_remove_1 cw_remove_1;	//0x28
    volatile union      s_cw_remove_2 cw_remove_2;	//0x2c
    volatile union      s_ddc ddc;	//0x30
    volatile union      s_acf acf;	//0x34
    volatile union      s_src src;	//0x38
    volatile union      s_rx_ds_flt_0 rx_ds_flt_0;	//0x3c
    volatile union      s_rx_ds_flt_1 rx_ds_flt_1;	//0x40
    volatile union      s_rx_ds_flt_2 rx_ds_flt_2;	//0x44
    volatile union   	s_rx_dc rx_dc;	//0x48
    volatile union      s_dagc3_1 dagc3_1;	//0x4c
    volatile uint32_t   reserved_2[15];	//0x50
    volatile union      s_dagc2 dagc2;	//0x8c
    volatile union      s_rx_sync_0 rx_sync_0;	//0x90
    volatile union      s_rx_sync_1 rx_sync_1;	//0x94
    volatile union      s_rx_sync_2 rx_sync_2;	//0x98
    volatile union      s_spi_cfg3 spi_cfg3;	//0x9c
    volatile union      s_spi_cfg0 spi_cfg0;	//0xa0
    volatile union      s_spi_cfg1 spi_cfg1;	//0xa4
    volatile union      s_spi_cfg2 spi_cfg2;	//0xa8
    volatile union      s_sync_ob sync_ob;	//0xac
    volatile union      s_adc_if adc_if;	//0xb0
    volatile union      s_afifo_tb_0 afifo_tb_0;	//0xb4
    volatile union      s_dig_format_tb_0 dig_format_tb_0;	//0xb8
    volatile union      s_iqib_top_r iqib_top_r;	//0xbc
    volatile union      s_dcloop_tb_0 dcloop_tb_0;	//0xc0
    volatile union      s_dcloop_tb_1 dcloop_tb_1;	//0xc4
    volatile union      s_cw1_tb cw1_tb;	//0xc8
    volatile uint32_t   reserved_3[8];	//0xcc
    volatile union      s_rd_obv_0 rd_obv_0;	//0xec
    volatile union      s_rd_obv_1 rd_obv_1;	//0xf0
    volatile union      s_sync_obv_0 sync_obv_0;	//0xf4
    volatile union      s_sync_obv_1 sync_obv_1;	//0xf8
    volatile union      s_rx_fd_comp rx_fd_comp;	//0xfc
    volatile union      s_spi_res0 spi_res0;	//0x100
    volatile union      s_spi_res1 spi_res1;	//0x104
};
typedef union s_mac_config_0 
{
    uint32_t data;
    struct
    {
        uint32_t s0_cfg  : 8;
        uint32_t s1_cfg  : 10;
        uint32_t RESERVED_0 : 2;
        uint32_t mode  : 8;
        uint32_t RESERVED_1 : 4;
    }b;
}s_mac_config_0;

typedef union s_mac_config_1 
{
    uint32_t data;
    struct
    {
        uint32_t s2_cfg  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t s3_cfg  : 10;
        uint32_t RESERVED_1 : 2;
        uint32_t pl_crc_en  : 1;
        uint32_t RESERVED_2 : 3;
    }b;
}s_mac_config_1;

typedef union s_init_0 
{
    uint32_t data;
    struct
    {
        uint32_t bcn_agc  : 12;
        uint32_t interval  : 8;
        uint32_t tx_freq_dly  : 3;
        uint32_t RESERVED_0 : 9;
    }b;
}s_init_0;

typedef union s_init_1 
{
    uint32_t data;
    struct
    {
        uint32_t md_agc  : 12;
        uint32_t rx_delay  : 20;
    }b;
}s_init_1;

typedef union s_init_2 
{
    uint32_t data;
    struct
    {
        uint32_t da1_m  : 24;
        uint32_t RESERVED_0  : 8;
    }b;
}s_init_2;

typedef union s_init_3 
{
    uint32_t data;
    struct
    {
        uint32_t da2_m  : 24;
        uint32_t tx_fix_freq  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t rx_fix_freq  : 1;
        uint32_t RESERVED_1 : 3;
    }b;
}s_init_3;

typedef union s_init_4 
{
    uint32_t data;
    struct
    {
        uint32_t da3_m  : 24;
        uint32_t RESERVED_0 : 8;
    }b;
}s_init_4;

typedef union s_init_5 
{
    uint32_t data;
    struct
    {
        uint32_t offset_adj  : 13;
        uint32_t RESERVED_0 : 3;
        uint32_t tx_pre  : 12;
        uint32_t conti_mode  : 1;
        uint32_t conti_scan  : 1;
        uint32_t RESERVED_1 : 2;
    }b;
}s_init_5;

typedef union s_init_6 
{
    uint32_t data;
    struct
    {
        uint32_t bit_arr_h  : 32;
    }b;
}s_init_6;

typedef union s_init_7 
{
    uint32_t data;
    struct
    {
        uint32_t bit_arr_l  : 32;
    }b;
}s_init_7;

typedef union s_init_8 
{
    uint32_t data;
    struct
    {
        uint32_t to_thd  : 32;
    }b;
}s_init_8;

typedef union s_init_9 
{
    uint32_t data;
    struct
    {
        uint32_t ant_en  : 8;
        uint32_t rf_sel  : 8;
        uint32_t tx_bcn_ant_en  : 8;
//        uint32_t rx_bcn_en : 8;
        uint32_t RESERVED_0 : 8;
    }b;
}s_init_9;

typedef union s_init_10 
{
    uint32_t data;
    struct
    {
        uint32_t dio_ctrl  : 8;
        uint32_t RESERVED_0 : 24;
    }b;
}s_init_10;

typedef union s_init_11
{
    uint32_t data;
    struct
    {
        uint32_t pa_setup   : 12;
        uint32_t rf_type    : 2;
        uint32_t RESERVED_0 : 18;
    }b;
}s_init_11;

typedef union s_init_12
{
    uint32_t data;
    struct
    {
        uint32_t ls_val     : 16;
        uint32_t ls_master  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t ls_en      : 1;
        uint32_t RESERVED_1 : 3;
        uint32_t loop : 1;
        uint32_t RESERVED_2 : 7;
    }b;
}s_init_12;

typedef union s_init_13
{
    uint32_t data;
    struct
    {
        uint32_t user_val0  : 32;
    }b;
}s_init_13;

typedef union s_init_14
{
    uint32_t data;
    struct
    {
    	uint32_t user_val1  : 32;
    }b;
}s_init_14;

typedef union s_init_15
{
    uint32_t data;
    struct
    {
    	uint32_t user_val2  : 32;
    }b;
}s_init_15;

typedef union s_init_16
{
    uint32_t data;
    struct
    {
    	uint32_t user_val3  : 32;
    }b;
}s_init_16;

typedef union s_init_17
{
    uint32_t data;
    struct
    {
    	uint32_t brd_user_val  : 16;
    	uint32_t RESERVED_0    : 16;
    }b;
}s_init_17;

typedef union s_init_18
{
    uint32_t data;
    struct
    {
        uint32_t da0_m  : 24;
        uint32_t RESERVED_0 : 8;
    }b;
}s_init_18;

typedef union s_efuse_0
{
    uint32_t data;
    struct
    {
        uint32_t efuse_adr  : 9;
        uint32_t RESERVED_0 : 3;
        uint32_t efuse_wr  : 1;
        uint32_t RESERVED_1 : 19;
    }b;
}s_efuse_0;

typedef union s_irq_ctrl0 
{
    uint32_t data;
    struct
    {
        uint32_t rxbcn_irq_mask  : 1;
        uint32_t brd_ud_irq_mask  : 1;
        uint32_t brd_irq_mask  : 1;
        uint32_t md_ud_irq_mask  : 1;
        uint32_t md_irq_mask  : 1;
        uint32_t s0_irq_mask  : 1;
        uint32_t s1_irq_mask  : 1;
        uint32_t s2_irq_mask  : 1;
        uint32_t s3_irq_mask  : 1;
        uint32_t acm_irq_mask  : 1;
        uint32_t to_irq_mask  : 1;
        uint32_t RESERVED_0 : 21;
    }b;
}s_irq_ctrl0;

typedef union s_irq_ctrl1 
{
    uint32_t data;
    struct
    {
        uint32_t rxbcn_intr_clr  : 1;
        uint32_t brd_ud_intr_clr  : 1;
        uint32_t brd_irq_clr  : 1;
        uint32_t data_ud_irq_clr  : 1;
        uint32_t data_irq_clr  : 1;
        uint32_t s0_irq_clr  : 1;
        uint32_t s1_irq_clr  : 1;
        uint32_t s2_irq_clr  : 1;
        uint32_t s3_irq_clr  : 1;
        uint32_t acm_irq_clr  : 1;
        uint32_t to_irq_clr  : 1;
        uint32_t RESERVED_0 : 21;
    }b;
}s_irq_ctrl1;

typedef union s_tx_pow_ctrl
{
    uint32_t data;
    struct
    {
        uint32_t power  : 8;
        uint32_t UserIndex : 8;
        uint32_t RESERVED_0 : 16;
    }b;
}s_tx_pow_ctrl;

typedef union s_trx_trig0
{
    uint32_t data;
    struct
    {
        uint32_t active_trans  : 1;
        uint32_t RESERVED_0 : 31;
    }b;
}s_trx_trig0;

typedef union s_trx_trig1
{
    uint32_t data;
    struct
    {
        uint32_t passive_trans  : 1;
        uint32_t RESERVED_0 : 31;
    }b;
}s_trx_trig1;

typedef union s_acm_ctrl
{
    uint32_t data;
    struct
    {
        uint32_t acm_trig  : 1;
        uint32_t RESERVED_0 : 31;
    }b;
}s_acm_ctrl;

typedef union s_wakeup_ctrl
{
    uint32_t data;
    struct
    {
        uint32_t wakeup_len  : 24;
        uint32_t wakeup_mode : 4;
        uint32_t RESERVED_0  : 4;
    }b;
}s_wakeup_ctrl;

typedef union s_brd_crc_res 
{
    uint32_t data;
    struct
    {
        uint32_t brd_crc  : 16;
        uint32_t RESERVED_0 : 16;
    }b;
}s_brd_crc_res;

typedef union s_rx_crc_res0 
{
    uint32_t data;
    struct
    {
        uint32_t rx_crc0  : 32;
    }b;
}s_rx_crc_res0;

typedef union s_rx_crc_res1 
{
    uint32_t data;
    struct
    {
        uint32_t rx_crc1  : 32;
    }b;
}s_rx_crc_res1;

typedef union s_rx_crc_res2 
{
    uint32_t data;
    struct
    {
        uint32_t rx_crc2  : 32;
    }b;
}s_rx_crc_res2;

typedef union s_rx_crc_res3 
{
    uint32_t data;
    struct
    {
        uint32_t rx_crc3  : 32;
    }b;
}s_rx_crc_res3;

typedef union s_irq_res 
{
    uint32_t data;
    struct
    {
        uint32_t irq_type  : 11;
        uint32_t RESERVED_0 : 21;
    }b;
}s_irq_res;

typedef union s_ts_res 
{
    uint32_t data;
    struct
    {
        uint32_t ts_timer  : 16;
        uint32_t golden_timer  : 16;
    }b;
}s_ts_res;

typedef union s_obv_0 
{
    uint32_t data;
    struct
    {
        uint32_t timer_cnt  : 24;
        uint32_t RESERVED_0 : 8;
    }b;
}s_obv_0;

typedef union s_obv_1 
{
    uint32_t data;
    struct
    {
        uint32_t s0_len  : 17;
        uint32_t RESERVED_0 : 15;
    }b;
}s_obv_1;

typedef union s_obv_2 
{
    uint32_t data;
    struct
    {
        uint32_t s1_len  : 24;
        uint32_t RESERVED_0 : 8;
    }b;
}s_obv_2;

typedef union s_obv_3 
{
    uint32_t data;
    struct
    {
        uint32_t s2_len  : 24;
        uint32_t RESERVED_0 : 8;
    }b;
}s_obv_3;

typedef union s_obv_4 
{
    uint32_t data;
    struct
    {
        uint32_t s3_len  : 24;
        uint32_t RESERVED_0 : 8;
    }b;
}s_obv_4;

typedef union s_obv_5 
{
    uint32_t data;
    struct
    {
        uint32_t to_cnt  : 24;
        uint32_t RESERVED_0 : 8;
    }b;
}s_obv_5;

typedef union s_obv_6
{
    uint32_t data;
    struct
    {
        uint32_t efuse_out  : 8;
        uint32_t efuse_busy  : 1;
        uint32_t RESERVED_0 : 23;
    }b;
}s_obv_6;

typedef union s_obv_7
{
    uint32_t data;
    struct
    {
        uint32_t timer_1  : 24;
        uint32_t RESERVED_0 : 8;
    }b;
}s_obv_7;

typedef union s_obv_8
{
    uint32_t data;
    struct
    {
        uint32_t version  : 32;
    }b;
}s_obv_8;

typedef union s_obv_9
{
    uint32_t data;
    struct
    {
        uint32_t sys_info   : 16;
        uint32_t RESERVED_0 : 16;
    }b;
}s_obv_9;

struct mac
{
    volatile union      s_mac_config_0 mac_config_0;	//0x0
    volatile union      s_mac_config_1 mac_config_1;	//0x4
    volatile union      s_init_0 init_0;	//0x8
    volatile union      s_init_1 init_1;	//0xc
    volatile union      s_init_2 init_2;	//0x10
    volatile union      s_init_3 init_3;	//0x14
    volatile union      s_init_4 init_4;	//0x18
    volatile union      s_init_5 init_5;	//0x1c
    volatile union      s_init_6 init_6;	//0x20
    volatile union      s_init_7 init_7;	//0x24
    volatile union      s_init_8 init_8;	//0x28
    volatile union      s_init_9 init_9;	//0x2c
    volatile union      s_init_10 init_10;	//0x30
    volatile union      s_init_11 init_11;	//0x34
    volatile union      s_init_12 init_12;	//0x38
    volatile union      s_init_13 init_13;	//0x3c
    volatile union      s_init_14 init_14;	//0x40
    volatile union      s_init_15 init_15;	//0x44
    volatile union      s_init_16 init_16;	//0x48
	volatile union      s_init_17 init_17;	//0x4c
    volatile union      s_efuse_0 efuse_0;	//0x50
    volatile union      s_init_18 init_18;	//0x54
    volatile uint32_t   reserved_1[2];	//0x58
    volatile union      s_irq_ctrl0 irq_ctrl0;	//0x60
    volatile union      s_irq_ctrl1 irq_ctrl1;	//0x64
    volatile uint32_t   reserved_2[2];	//0x68
    volatile union      s_tx_pow_ctrl tx_pow_ctrl;	//0x70
    volatile union 		s_trx_trig0 trx_trig0;//0x74
    volatile union		s_trx_trig1 trx_trig1;//0x78
    volatile union		s_acm_ctrl acm_ctrl;//0x7c
    volatile union 		s_wakeup_ctrl wakeup_ctrl;//0x80
    volatile uint32_t   reserved_3[6];	//0x84
    volatile union      s_brd_crc_res brd_crc_res;	//0x9c
    volatile union      s_rx_crc_res0 rx_crc_res0;	//0xa0
    volatile union      s_rx_crc_res1 rx_crc_res1;	//0xa4
    volatile union      s_rx_crc_res2 rx_crc_res2;	//0xa8
    volatile union      s_rx_crc_res3 rx_crc_res3;	//0xac
    volatile union      s_irq_res irq_res;	//0xb0
    volatile union      s_ts_res ts_res;	//0xb4
    volatile uint32_t   reserved_4[14];	//0xb8
    volatile union      s_obv_0 obv_0;	//0xf0
    volatile union      s_obv_1 obv_1;	//0xf4
    volatile union      s_obv_2 obv_2;	//0xf8
    volatile union      s_obv_3 obv_3;	//0xfc
    volatile union      s_obv_4 obv_4;	//0x100
    volatile union      s_obv_5 obv_5;	//0x104
    volatile union      s_obv_6 obv_6;	//0x108
    volatile union      s_obv_7 obv_7;	//0x10c
    volatile union      s_obv_8 obv_8;	//0x110
    volatile union      s_obv_9 obv_9;	//0x108
};
typedef union s_tx_config_0 
{
    uint32_t data;
    struct
    {
        uint32_t tx_bypass  : 6;
        uint32_t RESERVED_0 : 2;
        uint32_t tx_enable  : 6;
        uint32_t RESERVED_1 : 2;
        uint32_t tx_ws_rev  : 1;
        uint32_t RESERVED_2 : 15;
    }b;
}s_tx_config_0;

typedef union s_tx_bcn_0
{
    uint32_t data;
    struct
    {
        uint32_t bcn_bits_mode  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t bcn_bits  : 8;
        uint32_t add_offseten  : 1;
        uint32_t RESERVED_1 : 3;
        uint32_t work_mode  : 2;
        uint32_t RESERVED_2 : 2;
        uint32_t bcn_manual  : 1;
        uint32_t RESERVED_3 : 11;
    }b;
}s_tx_bcn_0;

typedef union s_tx_bcn_1
{
    uint32_t data;
    struct
    {
        uint32_t bin_diff_start  : 16;
        uint32_t bin_diff_step  : 16;
    }b;
}s_tx_bcn_1;

typedef union s_tx_bcn_2
{
    uint32_t data;
    struct
    {
        uint32_t add_offset1  : 13;
        uint32_t RESERVED_0 : 3;
        uint32_t add_offset0  : 13;
        uint32_t RESERVED_1 : 3;
    }b;
}s_tx_bcn_2;

typedef union s_tx_bcn_3
{
    uint32_t data;
    struct
    {
        uint32_t add_offset3  : 13;
        uint32_t RESERVED_0 : 3;
        uint32_t add_offset2  : 13;
        uint32_t RESERVED_1 : 3;
    }b;
}s_tx_bcn_3;

typedef union s_tx_bcn_4
{
    uint32_t data;
    struct
    {
        uint32_t freq_bin_diff  : 16;
        uint32_t time_bin_diff  : 16;
    }b;
}s_tx_bcn_4;

typedef union s_tx_bcn_5
{
    uint32_t data;
    struct
    {
        uint32_t wakeup_freq_bin_offset  : 16;
        uint32_t RESERVED_0 : 8;
        uint32_t bcn_tx_conti  : 1;
        uint32_t RESERVED_1 : 7;
    }b;
}s_tx_bcn_5;

typedef union s_tx_bcn_6
{
    uint32_t data;
    struct
    {
        uint32_t bcn_agc_len  : 16;
        uint32_t wakeup_rep  : 8;
        uint32_t RESERVED_0 : 8;
    }b;
}s_tx_bcn_6;

typedef union s_tx_bcn_7
{
    uint32_t data;
    struct
    {
        uint32_t bcn_ddc_ang  : 25;
        uint32_t RESERVED_0 : 7;
    }b;
}s_tx_bcn_7;

typedef union s_tx_config_18
{
    uint32_t data;
    struct
    {
        uint32_t tx_up_flt_bypass  : 8;
        uint32_t RESERVED_0 : 24;
    }b;
}s_tx_config_18;

typedef union s_tx_config_27 
{
    uint32_t data;
    struct
    {
        uint32_t tx_ddc_angle  : 25;
        uint32_t RESERVED_0 : 3;
        uint32_t tx_ddc_bypass  : 1;
        uint32_t RESERVED_1 : 3;
    }b;
}s_tx_config_27;

typedef union s_tx_config_28 
{
    uint32_t data;
    struct
    {
        uint32_t sc_da  : 8;
        uint32_t RESERVED_0 : 4;
        uint32_t sc_bcn  : 8;
        uint32_t tx_iq_swap  : 1;
        uint32_t RESERVED_1 : 3;
        uint32_t tx_msb_neg  : 1;
        uint32_t RESERVED_2 : 3;
        uint32_t tx_iq_zero  : 2;
        uint32_t RESERVED_3 : 2;
    }b;
}s_tx_config_28;

typedef union s_tx_config_29 
{
    uint32_t data;
    struct
    {
        uint32_t tx_dcq  : 16;
        uint32_t tx_dci  : 16;
    }b;
}s_tx_config_29;

typedef union s_tx_config_30 
{
    uint32_t data;
    struct
    {
        uint32_t tx_iqim_b  : 13;
        uint32_t RESERVED_0 : 3;
        uint32_t tx_iqim_a  : 13;
        uint32_t tx_iqim_bypass  : 1;
        uint32_t RESERVED_1 : 2;
    }b;
}s_tx_config_30;

typedef union s_tx_config_31 
{
    uint32_t data;
    struct
    {
        uint32_t tx_tone_angle0  : 25;
        uint32_t RESERVED_0 : 7;
    }b;
}s_tx_config_31;

typedef union s_tx_config_32 
{
    uint32_t data;
    struct
    {
        uint32_t tx_tone_angle1  : 25;
        uint32_t RESERVED_0 : 7;
    }b;
}s_tx_config_32;

typedef union s_tx_config_33 
{
    uint32_t data;
    struct
    {
        uint32_t tx_tone_amp0  : 8;
        uint32_t tx_tone_amp1  : 8;
        uint32_t tx_agc_len  : 16;
    }b;
}s_tx_config_33;

typedef union s_tx_config_34
{
    uint32_t data;
    struct
    {
        uint32_t tx_gain_q  : 16;
        uint32_t tx_gain_i  : 6;
        uint32_t RESERVED_0 : 10;
    }b;
}s_tx_config_34;

typedef union s_tx_config_35
{
    uint32_t data;
    struct
    {
        uint32_t tx_cmp_alpha  : 8;
        uint32_t tx_cmp_bypass  : 1;
        uint32_t RESERVED_0 : 23;
    }b;
}s_tx_config_35;

struct tx_dac_if
{
    volatile union      s_tx_config_0 tx_config_0;	//0x0
    volatile uint32_t   reserved_0[4];	//0x4
    volatile union      s_tx_bcn_0 tx_bcn_0;	//0x14
    volatile union      s_tx_bcn_1 tx_bcn_1;	//0x18
    volatile union      s_tx_bcn_2 tx_bcn_2;	//0x1c
    volatile union      s_tx_bcn_3 tx_bcn_3;	//0x20
    volatile union      s_tx_bcn_4 tx_bcn_4;	//0x24
    volatile union      s_tx_bcn_5 tx_bcn_5;	//0x28
    volatile union      s_tx_bcn_6 tx_bcn_6;	//0x2c
    volatile union      s_tx_bcn_7 tx_bcn_7;	//0x30
    volatile uint32_t   reserved_2[5];	//0x34
    volatile union      s_tx_config_18 tx_config_18;	//0x48
    volatile uint32_t   reserved_3[10];	//0x4c
    volatile union      s_tx_config_27 tx_config_27;	//0x74
    volatile union      s_tx_config_28 tx_config_28;	//0x78
    volatile uint32_t   reserved_4[19];	//0x7c
    volatile union      s_tx_config_29 tx_config_29;	//0xc8
    volatile union      s_tx_config_30 tx_config_30;	//0xcc
    volatile union      s_tx_config_31 tx_config_31;	//0xd0
    volatile union      s_tx_config_32 tx_config_32;	//0xd4
    volatile union      s_tx_config_33 tx_config_33;	//0xd8
    volatile union      s_tx_config_34 tx_config_34;	//0xdc
    volatile union      s_tx_config_35 tx_config_35;	//0xe0
};

typedef union s_bcn_0
{
    uint32_t data;
    struct
    {
        uint32_t osr  : 16;
        uint32_t bcn_mode  : 4;
        uint32_t work_mode  : 2;
        uint32_t RESERVED_0 : 2;
        uint32_t delay_out  : 8;
    }b;
}s_bcn_0;

typedef union s_bcn_2
{
    uint32_t data;
    struct
    {
        uint32_t norm_paral  : 8;
        uint32_t bcn_bit_mode  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t wakeup_th  : 10;
        uint32_t RESERVED_1 : 10;
    }b;
}s_bcn_2;

typedef union s_bcn_3
{
    uint32_t data;
    struct
    {
        uint32_t time_bin_diff_hi  : 16;
        uint32_t time_bin_diff_lo  : 16;
    }b;
}s_bcn_3;

typedef union s_bcn_4
{
    uint32_t data;
    struct
    {
        uint32_t freq_bin_diff_hi  : 16;
        uint32_t freq_bin_diff_lo  : 16;
    }b;
}s_bcn_4;

typedef union s_bcn_5
{
    uint32_t data;
    struct
    {
        uint32_t bin_diff_start  : 16;
        uint32_t bin_diff_step  : 16;
    }b;
}s_bcn_5;

typedef union s_bcn_6
{
    uint32_t data;
    struct
    {
        uint32_t min_energy_diff  : 8;
        uint32_t min_energy_hold  : 8;
        uint32_t bcn_bit_mask  : 16;
    }b;
}s_bcn_6;

typedef union s_bcn_7
{
    uint32_t data;
    struct
    {
        uint32_t min_energy_1  : 28;
        uint32_t RESERVED_0 : 4;
    }b;
}s_bcn_7;

typedef union s_bcn_9
{
    uint32_t data;
    struct
    {
        uint32_t min_energy_3  : 28;
        uint32_t bcn_src_sel  : 3;
        uint32_t RESERVED_0 : 1;
    }b;
}s_bcn_9;

typedef union s_bcn_10
{
    uint32_t data;
    struct
    {
        uint32_t add_offset_en  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t threshold  : 10;
        uint32_t RESERVED_1 : 10;
        uint32_t energy_hold_step  : 8;
    }b;
}s_bcn_10;

typedef union s_bcn_11
{
    uint32_t data;
    struct
    {
        uint32_t add_offset1  : 13;
        uint32_t RESERVED_0 : 3;
        uint32_t add_offset0  : 13;
        uint32_t RESERVED_1 : 3;
    }b;
}s_bcn_11;

typedef union s_bcn_12
{
    uint32_t data;
    struct
    {
        uint32_t add_offset3  : 13;
        uint32_t RESERVED_0 : 3;
        uint32_t add_offset2  : 13;
        uint32_t RESERVED_1 : 3;
    }b;
}s_bcn_12;

typedef union s_bcn_13
{
    uint32_t data;
    struct
    {
        uint32_t wakeup_freq_bin_lo  : 16;
        uint32_t wakeup_freq_bin_hi  : 16;
    }b;
}s_bcn_13;

typedef union s_bcn_14
{
    uint32_t data;
    struct
    {
        uint32_t wakeup_verify  : 16;
        uint32_t max_scope  : 16;
    }b;
}s_bcn_14;

typedef union s_bcn_obv1
{
    uint32_t data;
    struct
    {
        uint32_t bcn_rssi  : 12;
        uint32_t bcn_q  : 10;
        uint32_t RESERVED_0 : 2;
        uint32_t sync_on  : 2;
        uint32_t RESERVED_1 : 6;
    }b;
}s_bcn_obv1;

typedef union s_bcn_obv2
{
    uint32_t data;
    struct
    {
        uint32_t bcn_bits_out  : 16;
        uint32_t freq_offset  : 16;
    }b;
}s_bcn_obv2;

typedef union s_bcn_obv3
{
    uint32_t data;
    struct
    {
        uint32_t bcn_time_offset  : 8;
        uint32_t RESERVED_0 : 24;
    }b;
}s_bcn_obv3;

struct rx_bcn
{
    volatile uint32_t  reserved_0[40];	//0x0
    volatile union      s_bcn_0 bcn_0;	//0xa0
    volatile union      s_bcn_2 bcn_2;	//0xa4
    volatile union      s_bcn_3 bcn_3;	//0xa8
    volatile union      s_bcn_4 bcn_4;	//0xac
    volatile union      s_bcn_5 bcn_5;	//0xb0
    volatile union      s_bcn_6 bcn_6;	//0xb4
    volatile union      s_bcn_7 bcn_7;	//0xb8
    volatile union      s_bcn_9 bcn_9;	//0xbc
    volatile union      s_bcn_10 bcn_10;	//0xc0
    volatile union      s_bcn_11 bcn_11;	//0xc4
    volatile union      s_bcn_12 bcn_12;	//0xc8
    volatile union      s_bcn_13 bcn_13;	//0xcc
    volatile union      s_bcn_14 bcn_14;	//0xd0
    volatile uint32_t   reserved_1[11];	//0xd4
    volatile union      s_bcn_obv1 bcn_obv1;	//0x100
    volatile union      s_bcn_obv2 bcn_obv2;	//0x104
    volatile union      s_bcn_obv3 bcn_obv3;	//0x108
};
typedef union s_tm_config_0 
{
    uint32_t data;
    struct
    {
        uint32_t crc_en  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t len_infobit  : 10;
        uint32_t RESERVED_1 : 2;
        uint32_t gc_bypass  : 1;
        uint32_t RESERVED_2 : 15;
    }b;
}s_tm_config_0;

typedef union s_tm_config_1 
{
    uint32_t data;
    struct
    {
        uint32_t lensymphase  : 10;
        uint32_t RESERVED_0 : 2;
        uint32_t symnum_data  : 9;
        uint32_t RESERVED_1 : 11;
    }b;
}s_tm_config_1;

typedef union s_tm_config_2 
{
    uint32_t data;
    struct
    {
        uint32_t dsss_num  : 4;
        uint32_t trans_ratio  : 3;
        uint32_t RESERVED_0 : 1;
        uint32_t block_num  : 8;
        uint32_t RESERVED_1 : 16;
    }b;
}s_tm_config_2;

typedef union s_tm_config_3 
{
    uint32_t data;
    struct
    {
        uint32_t data_len_i  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t tone_freq  : 11;
        uint32_t RESERVED_1 : 1;
        uint32_t tone_en  : 1;
        uint32_t rep_time  : 2;
        uint32_t RESERVED_2 : 1;
    }b;
}s_tm_config_3;

typedef union s_tm_config_4 
{
    uint32_t data;
    struct
    {
        uint32_t times  : 4;
        uint32_t times_bcn  : 4;
        uint32_t agc_len  : 17;
        uint32_t RESERVED_0 : 7;
    }b;
}s_tm_config_4;

typedef union s_tm_config_5 
{
    uint32_t data;
    struct
    {
        uint32_t fref_mode  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t model  : 1;
        uint32_t RESERVED_1 : 3;
        uint32_t dither_shift  : 3;
        uint32_t RESERVED_2 : 1;
        uint32_t tx_data  : 8;
        uint32_t RESERVED_3 : 12;
    }b;
}s_tm_config_5;

typedef union s_tm_config_6 
{
    uint32_t data;
    struct
    {
        uint32_t sdm_en  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t tx_data_sel  : 1;
        uint32_t RESERVED_1 : 3;
        uint32_t tx_twos  : 1;
        uint32_t RESERVED_2 : 3;
        uint32_t nint  : 8;
        uint32_t RESERVED_3 : 12;
    }b;
}s_tm_config_6;

typedef union s_tm_config_7 
{
    uint32_t data;
    struct
    {
        uint32_t dither_en  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t mode_en  : 1;
        uint32_t RESERVED_1 : 3;
        uint32_t tx_data_gain  : 8;
        uint32_t RESERVED_2 : 16;
    }b;
}s_tm_config_7;

typedef union s_tm_config_8 
{
    uint32_t data;
    struct
    {
        uint32_t delay  : 9;
        uint32_t RESERVED_0 : 23;
    }b;
}s_tm_config_8;

typedef union s_tm_config_9 
{
    uint32_t data;
    struct
    {
        uint32_t sdm_frac  : 25;
        uint32_t RESERVED_0 : 7;
    }b;
}s_tm_config_9;

typedef union s_tm_bcn_0 
{
    uint32_t data;
    struct
    {
        uint32_t bcn_bits_mode  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t bcn_bits  : 8;
        uint32_t add_offseten  : 1;
        uint32_t RESERVED_1 : 3;
        uint32_t work_mode  : 2;
        uint32_t RESERVED_2 : 2;
        uint32_t bcn_mode  : 4;
        uint32_t RESERVED_3 : 8;
    }b;
}s_tm_bcn_0;

typedef union s_tm_bcn_1 
{
    uint32_t data;
    struct
    {
        uint32_t bin_diff_start  : 16;
        uint32_t bin_diff_step  : 16;
    }b;
}s_tm_bcn_1;

typedef union s_tm_bcn_2 
{
    uint32_t data;
    struct
    {
        uint32_t add_offset1  : 13;
        uint32_t RESERVED_0 : 3;
        uint32_t add_offset0  : 13;
        uint32_t RESERVED_1 : 3;
    }b;
}s_tm_bcn_2;

typedef union s_tm_bcn_3 
{
    uint32_t data;
    struct
    {
        uint32_t add_offset3  : 13;
        uint32_t RESERVED_0 : 3;
        uint32_t add_offset2  : 13;
        uint32_t RESERVED_1 : 3;
    }b;
}s_tm_bcn_3;

typedef union s_tm_bcn_4 
{
    uint32_t data;
    struct
    {
        uint32_t freq_bin_diff  : 16;
        uint32_t time_bin_diff  : 16;
    }b;
}s_tm_bcn_4;

typedef union s_tm_bcn_5 
{
    uint32_t data;
    struct
    {
        uint32_t wakeup_freq_bin_offset  : 16;
        uint32_t tx_p2s_en  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t trx_sel  : 1;
        uint32_t RESERVED_1 : 3;
        uint32_t bcn_tx_conti  : 1;
        uint32_t RESERVED_2 : 7;
    }b;
}s_tm_bcn_5;

typedef union s_tm_bcn_share_0 
{
    uint32_t data;
    struct
    {
        uint32_t bcn_sh_clr  : 1;
        uint32_t RESERVED_0 : 31;
    }b;
}s_tm_bcn_share_0;

typedef union s_trx_bcn_share_1 
{
    uint32_t data;
    struct
    {
        uint32_t bcn_sh_d  : 32;
    }b;
}s_trx_bcn_share_1;

typedef union s_tm_ddc_0 
{
    uint32_t data;
    struct
    {
        uint32_t ddc_freq  : 25;
        uint32_t RESERVED_0 : 7;
    }b;
}s_tm_ddc_0;

typedef union s_tm_ddc_1 
{
    uint32_t data;
    struct
    {
        uint32_t ddc_freq_clr  : 1;
        uint32_t RESERVED_0 : 31;
    }b;
}s_tm_ddc_1;

typedef union s_tm_tx_bf_1 
{
    uint32_t data;
    struct
    {
        uint32_t tx_samplingrate  : 25;
        uint32_t RESERVED_0 : 7;
    }b;
}s_tm_tx_bf_1;

typedef union s_tm_tx_bf_2 
{
    uint32_t data;
    struct
    {
        uint32_t recipracalcaliflag  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t gwtxpowerflag  : 2;
        uint32_t RESERVED_1 : 2;
        uint32_t user_num  : 8;
        uint32_t ant_num  : 1;
        uint32_t RESERVED_2 : 7;
        uint32_t tx_mode  : 8;
    }b;
}s_tm_tx_bf_2;

typedef union s_tm_tx_bf_3 
{
    uint32_t data;
    struct
    {
        uint32_t total_power  : 10;
        uint32_t RESERVED_0 : 2;
        uint32_t bw_overlop  : 4;
        uint32_t mmse  : 9;
        uint32_t RESERVED_1 : 7;
    }b;
}s_tm_tx_bf_3;

typedef union s_tm_tx_bf_4 
{
    uint32_t data;
    struct
    {
        uint32_t power2_diff  : 5;
        uint32_t RESERVED_0 : 11;
        uint32_t power3_diff  : 16;
    }b;
}s_tm_tx_bf_4;

typedef union s_tm_tx_bf_5 
{
    uint32_t data;
    struct
    {
        uint32_t ah_i  : 19;
        uint32_t RESERVED_0 : 13;
    }b;
}s_tm_tx_bf_5;

typedef union s_tm_tx_bf_6 
{
    uint32_t data;
    struct
    {
        uint32_t ah_q  : 19;
        uint32_t RESERVED_0 : 13;
    }b;
}s_tm_tx_bf_6;

typedef union s_tm_tx_bf_7 
{
    uint32_t data;
    struct
    {
        uint32_t anoise_d  : 32;
    }b;
}s_tm_tx_bf_7;

typedef union s_tm_tx_bf_8 
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor_i  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_tm_tx_bf_8;

typedef union s_tm_tx_bf_9 
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor_q  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_tm_tx_bf_9;

typedef union s_tm_tx_bf_10 
{
    uint32_t data;
    struct
    {
        uint32_t freq_d  : 25;
        uint32_t RESERVED_0 : 7;
    }b;
}s_tm_tx_bf_10;

typedef union s_tm_tx_bf_11 
{
    uint32_t data;
    struct
    {
        uint32_t txpower_d  : 25;
        uint32_t RESERVED_0 : 7;
    }b;
}s_tm_tx_bf_11;

typedef union s_tm_tx_bf_12 
{
    uint32_t data;
    struct
    {
        uint32_t gwrxpilot_d  : 28;
        uint32_t RESERVED_0 : 4;
    }b;
}s_tm_tx_bf_12;

typedef union s_tm_tx_bf_13 
{
    uint32_t data;
    struct
    {
        uint32_t bf_ram_clr  : 1;
        uint32_t RESERVED_0 : 31;
    }b;
}s_tm_tx_bf_13;

typedef union s_tm_bcn_6 
{
    uint32_t data;
    struct
    {
        uint32_t bcn_agc_len  : 16;
        uint32_t wakeup_rep  : 8;
        uint32_t RESERVED_0 : 8;
    }b;
}s_tm_bcn_6;

typedef union s_tm_ob_0 
{
    uint32_t data;
    struct
    {
        uint32_t data_mod  : 10;
        uint32_t RESERVED_0 : 22;
    }b;
}s_tm_ob_0;

typedef union s_tm_ob_1 
{
    uint32_t data;
    struct
    {
        uint32_t phase_diff  : 25;
        uint32_t RESERVED_0 : 7;
    }b;
}s_tm_ob_1;

struct tx_top
{
    volatile union      s_tm_config_0 tm_config_0;	//0x0
    volatile union      s_tm_config_1 tm_config_1;	//0x4
    volatile union      s_tm_config_2 tm_config_2;	//0x8
    volatile union      s_tm_config_3 tm_config_3;	//0xc
    volatile union      s_tm_config_4 tm_config_4;	//0x10
    volatile union      s_tm_config_5 tm_config_5;	//0x14
    volatile union      s_tm_config_6 tm_config_6;	//0x18
    volatile union      s_tm_config_7 tm_config_7;	//0x1c
    volatile union      s_tm_config_8 tm_config_8;	//0x20
    volatile uint32_t   reserved_0[3];	//0x24
    volatile union      s_tm_config_9 tm_config_9;	//0x30
    volatile union      s_tm_bcn_0 tm_bcn_0;	//0x34
    volatile union      s_tm_bcn_1 tm_bcn_1;	//0x38
    volatile union      s_tm_bcn_2 tm_bcn_2;	//0x3c
    volatile union      s_tm_bcn_3 tm_bcn_3;	//0x40
    volatile union      s_tm_bcn_4 tm_bcn_4;	//0x44
    volatile union      s_tm_bcn_5 tm_bcn_5;	//0x48
    volatile union      s_tm_bcn_share_0 tm_bcn_share_0;	//0x4c
    volatile union      s_trx_bcn_share_1 trx_bcn_share_1;	//0x50
    volatile union      s_tm_ddc_0 tm_ddc_0;	//0x54
    volatile union      s_tm_ddc_1 tm_ddc_1;	//0x58
    volatile union      s_tm_tx_bf_1 tm_tx_bf_1;	//0x5c
    volatile union      s_tm_tx_bf_2 tm_tx_bf_2;	//0x60
    volatile union      s_tm_tx_bf_3 tm_tx_bf_3;	//0x64
    volatile union      s_tm_tx_bf_4 tm_tx_bf_4;	//0x68
    volatile union      s_tm_tx_bf_5 tm_tx_bf_5;	//0x6c
    volatile union      s_tm_tx_bf_6 tm_tx_bf_6;	//0x70
    volatile union      s_tm_tx_bf_7 tm_tx_bf_7;	//0x74
    volatile union      s_tm_tx_bf_8 tm_tx_bf_8;	//0x78
    volatile union      s_tm_tx_bf_9 tm_tx_bf_9;	//0x7c
    volatile union      s_tm_tx_bf_10 tm_tx_bf_10;	//0x80
    volatile union      s_tm_tx_bf_11 tm_tx_bf_11;	//0x84
    volatile union      s_tm_tx_bf_12 tm_tx_bf_12;	//0x88
    volatile union      s_tm_tx_bf_13 tm_tx_bf_13;	//0x8c
    volatile union      s_tm_bcn_6 tm_bcn_6;	//0x90
    volatile union      s_tm_ob_0 tm_ob_0;	//0x94
    volatile union      s_tm_ob_1 tm_ob_1;	//0x98
};
typedef union s_rx_acm_ctrl0 
{
    uint32_t data;
    struct
    {
        uint32_t acm_bypass  : 8;
        uint32_t fft_len  : 3;
        uint32_t RESERVED_0 : 1;
        uint32_t ant_num  : 3;
        uint32_t RESERVED_1 : 1;
        uint32_t rx_acm  : 4;
        uint32_t acm_rx_en  : 1;
        uint32_t RESERVED_2 : 11;
    }b;
}s_rx_acm_ctrl0;

typedef union s_ram_rd0 
{
    uint32_t data;
    struct
    {
        uint32_t st_adr  : 13;
        uint32_t RESERVED_0 : 3;
        uint32_t rx_ram_idx  : 4;
        uint32_t cap_en : 1;
        uint32_t RESERVED_1 : 11;
    }b;
}s_ram_rd0;

typedef union s_ud_ctrl0 
{
    uint32_t data;
    struct
    {
        uint32_t anoise_the1_frac  : 11;
        uint32_t RESERVED_0 : 1;
        uint32_t anoise_the2  : 8;
        uint32_t anoise_the1  : 8;
        uint32_t RESERVED_1 : 4;
    }b;
}s_ud_ctrl0;

typedef union s_ud_ctrl1 
{
    uint32_t data;
    struct
    {
        uint32_t the_fxp  : 13;
        uint32_t RESERVED_0 : 19;
    }b;
}s_ud_ctrl1;

typedef union s_ud_ctrl2 
{
    uint32_t data;
    struct
    {
        uint32_t p6_the  : 7;
        uint32_t RESERVED_0 : 1;
        uint32_t p3_the  : 7;
        uint32_t RESERVED_1 : 1;
        uint32_t p2_the  : 7;
        uint32_t RESERVED_2 : 1;
        uint32_t p1_the  : 7;
        uint32_t RESERVED_3 : 1;
    }b;
}s_ud_ctrl2;

typedef union s_ud_ctrl3 
{
    uint32_t data;
    struct
    {
        uint32_t GWRX_P1_para2  : 12;
        uint32_t GWRX_P1_para1  : 12;
        uint32_t RESERVED_0 : 8;
    }b;
}s_ud_ctrl3;

typedef union s_ud_ctrl4 
{
    uint32_t data;
    struct
    {
        uint32_t Max_out_len  : 11;
        uint32_t RESERVED_0 : 1;
        uint32_t GWRXShrink  : 16;
        uint32_t RESERVED_1 : 4;
    }b;
}s_ud_ctrl4;

typedef union s_ud_ctrl5 
{
    uint32_t data;
    struct
    {
        uint32_t GWRXSampleRate  : 3;
        uint32_t RESERVED_0 : 1;
        uint32_t GWRXAllUser  : 8;
        uint32_t freq_alloc_num  : 8;
        uint32_t freq_alloc_en  : 1;
        uint32_t RESERVED_1 : 11;
    }b;
}s_ud_ctrl5;

typedef union s_ud_ctrl6 
{
    uint32_t data;
    struct
    {
        uint32_t window_num  : 8;
        uint32_t TermainalTxMode  : 8;
        uint32_t alpha  : 8;
        uint32_t RESERVED_0 : 8;
    }b;
}s_ud_ctrl6;

typedef union s_ud_ctrl7 
{
    uint32_t data;
    struct
    {
        uint32_t user_freq  : 20;
        uint32_t user_freq_rst  : 1;
        uint32_t RESERVED_0 : 11;
    }b;
}s_ud_ctrl7;

typedef union s_hc_ctrl0 
{
    uint32_t data;
    struct
    {
        uint32_t filter_len  : 12;
        uint32_t hce  : 10;
        uint32_t RESERVED_0 : 10;
    }b;
}s_hc_ctrl0;

typedef union s_is_bf0
{
    uint32_t data;
    struct
    {
        uint32_t bf_the  : 2;
        uint32_t slide_dn  : 2;
        uint32_t dsss  : 2;
        uint32_t RESERVED_0 : 2;
        uint32_t bf_mode  : 2;
        uint32_t RESERVED_1 : 2;
        uint32_t user_num  : 8;
        uint32_t RESERVED_2 : 4;
        uint32_t blk_num  : 5;
        uint32_t RESERVED_3 : 3;
    }b;
}s_is_bf0;

typedef union s_demod_c0 
{
    uint32_t data;
    struct
    {
        uint32_t toscope  : 3;
        uint32_t RESERVED_0 : 1;
        uint32_t drift_len  : 2;
        uint32_t RESERVED_1 : 2;
        uint32_t len_freqcomp  : 3;
        uint32_t RESERVED_2 : 21;
    }b;
}s_demod_c0;

typedef union s_demod_c1 
{
    uint32_t data;
    struct
    {
        uint32_t freq_inter  : 16;
        uint32_t freq_start  : 16;
    }b;
}s_demod_c1;

typedef union s_demod_c2
{
    uint32_t data;
    struct
    {
        uint32_t drift_i  : 16;
        uint32_t drift_s  : 16;
    }b;
}s_demod_c2;

typedef union s_demod_c3
{
    uint32_t data;
    struct
    {
        uint32_t fdc_num  : 3;
        uint32_t RESERVED_0 : 1;
        uint32_t sym_num  : 9;
        uint32_t RESERVED_1 : 3;
        uint32_t len_dsss  : 2;
        uint32_t RESERVED_2 : 2;
        uint32_t seg_num  : 8;
        uint32_t RESERVED_3 : 4;
    }b;
}s_demod_c3;

typedef union s_demod_c4
{
    uint32_t data;
    struct
    {
        uint32_t fs  : 4;
        uint32_t add_len  : 8;
        uint32_t fdc_delta  : 13;
        uint32_t RESERVED_0 : 7;
    }b;
}s_demod_c4;

typedef union s_ram_clr 
{
    uint32_t data;
    struct
    {
        uint32_t da0_clr  : 1;
        uint32_t RESERVED_0 : 3;
        uint32_t da1_clr  : 1;
        uint32_t RESERVED_1 : 3;
        uint32_t ud_clr  : 1;
        uint32_t RESERVED_2 : 23;
    }b;
}s_ram_clr;

typedef union s_rf_cal_trig
{
    uint32_t data;
    struct
    {
        uint32_t rx_trig    : 1;
        uint32_t RESERVED_0 : 31;
    }b;
}s_rf_cal_trig;


typedef union s_rx_ram_obv0 
{
    uint32_t data;
    struct
    {
        uint32_t rx_ram_i  : 32;
    }b;
}s_rx_ram_obv0;

typedef union s_rx_ram_obv1 
{
    uint32_t data;
    struct
    {
        uint32_t rx_ram_q  : 32;
    }b;
}s_rx_ram_obv1;

typedef union s_ud_obv0 
{
    uint32_t data;
    struct
    {
        uint32_t anoise_d0  : 32;
    }b;
}s_ud_obv0;

typedef union s_ud_obv1 
{
    uint32_t data;
    struct
    {
        uint32_t anoise_d1  : 32;
    }b;
}s_ud_obv1;

typedef union s_ud_obv2 
{
    uint32_t data;
    struct
    {
        uint32_t anoise_d2  : 32;
    }b;
}s_ud_obv2;

typedef union s_ud_obv3 
{
    uint32_t data;
    struct
    {
        uint32_t anoise_d3  : 32;
    }b;
}s_ud_obv3;

typedef union s_ud_obv4 
{
    uint32_t data;
    struct
    {
        uint32_t anoise_d4  : 32;
    }b;
}s_ud_obv4;

typedef union s_ud_obv5 
{
    uint32_t data;
    struct
    {
        uint32_t anoise_d5  : 32;
    }b;
}s_ud_obv5;

typedef union s_ud_obv6 
{
    uint32_t data;
    struct
    {
        uint32_t anoise_d6  : 32;
    }b;
}s_ud_obv6;

typedef union s_ud_obv7 
{
    uint32_t data;
    struct
    {
        uint32_t anoise_d7  : 32;
    }b;
}s_ud_obv7;

typedef union s_intr_obv 
{
    uint32_t data;
    struct
    {
        uint32_t intr_status  : 5;
        uint32_t RESERVED_0 : 27;
    }b;
}s_intr_obv;

typedef union s_rf_cal_en
{
    uint32_t data;
    struct
    {
        uint32_t rf_cal_en  : 1;
        uint32_t RESERVED_0 : 31;
    }b;
}s_rf_cal_en;


struct rx_mup
{
    volatile union      s_rx_acm_ctrl0 rx_acm_ctrl0;	//0x0
    volatile union      s_ram_rd0 ram_rd0;	//0x4
    volatile uint32_t   reserved_0[1];	//0x8
    volatile union      s_ud_ctrl0 ud_ctrl0;	//0xc
    volatile union      s_ud_ctrl1 ud_ctrl1;	//0x10
    volatile union      s_ud_ctrl2 ud_ctrl2;	//0x14
    volatile union      s_ud_ctrl3 ud_ctrl3;	//0x18
    volatile union      s_ud_ctrl4 ud_ctrl4;	//0x1c
    volatile union      s_ud_ctrl5 ud_ctrl5;	//0x20
    volatile union      s_ud_ctrl6 ud_ctrl6;	//0x24
    volatile union      s_ud_ctrl7 ud_ctrl7;	//0x28
    volatile union      s_hc_ctrl0 hc_ctrl0;	//0x2c
    volatile union      s_is_bf0 is_bf0;	//0x30
    volatile union      s_demod_c0 demod_c0;	//0x34
    volatile union      s_demod_c1 demod_c1;	//0x38
    volatile union      s_demod_c2 demod_c2;	//0x3c
    volatile union      s_demod_c3 demod_c3;	//0x40
    volatile union      s_demod_c4 demod_c4;	//0x44
    volatile union      s_ram_clr ram_clr;	//0x48
    volatile uint32_t   reserved_1[1];	//0x4c
    volatile union      s_rf_cal_trig rf_cal_trig;	//0x50
    volatile uint32_t   reserved_2[11];	//0x4c
    volatile union      s_rx_ram_obv0 rx_ram_obv0;	//0x80
    volatile union      s_rx_ram_obv1 rx_ram_obv1;	//0x84
    volatile union      s_ud_obv0 ud_obv0;	//0x88
    volatile union      s_ud_obv1 ud_obv1;	//0x8c
    volatile union      s_ud_obv2 ud_obv2;	//0x90
    volatile union      s_ud_obv3 ud_obv3;	//0x94
    volatile union      s_ud_obv4 ud_obv4;	//0x98
    volatile union      s_ud_obv5 ud_obv5;	//0x9c
    volatile union      s_ud_obv6 ud_obv6;	//0xa0
    volatile union      s_ud_obv7 ud_obv7;	//0xa4
    volatile union      s_intr_obv intr_obv;	//0xa8
    volatile union      s_rf_cal_en rf_cal_en;	//0xac
};
typedef union s_acm_ctrl0
{
    uint32_t data;
    struct
    {
        uint32_t acm_fft_len  : 2;
        uint32_t RESERVED_0 : 2;
        uint32_t acm_sr  : 2;
        uint32_t RESERVED_1 : 2;
        uint32_t acm_p0  : 4;
        uint32_t acm_p1  : 4;
        uint32_t acm_en  : 1;
        uint32_t RESERVED_2 : 15;
    }b;
}s_acm_ctrl0;

typedef union s_acm_ctrl1
{
    uint32_t data;
    struct
    {
        uint32_t freq1  : 11;
        uint32_t RESERVED_0 : 5;
        uint32_t freq0  : 11;
        uint32_t RESERVED_1 : 5;
    }b;
}s_acm_ctrl1;

typedef union s_acm_ctrl2
{
    uint32_t data;
    struct
    {
        uint32_t freq3  : 11;
        uint32_t RESERVED_0 : 5;
        uint32_t freq2  : 11;
        uint32_t RESERVED_1 : 5;
    }b;
}s_acm_ctrl2;

typedef union s_acm_ctrl3
{
    uint32_t data;
    struct
    {
        uint32_t freq5  : 11;
        uint32_t RESERVED_0 : 5;
        uint32_t freq4  : 11;
        uint32_t RESERVED_1 : 5;
    }b;
}s_acm_ctrl3;

typedef union s_acm_ctrl4
{
    uint32_t data;
    struct
    {
        uint32_t freq7  : 11;
        uint32_t RESERVED_0 : 5;
        uint32_t freq6  : 11;
        uint32_t RESERVED_1 : 5;
    }b;
}s_acm_ctrl4;

typedef union s_acm_ctrl5
{
    uint32_t data;
    struct
    {
        uint32_t freq9  : 11;
        uint32_t RESERVED_0 : 5;
        uint32_t freq8  : 11;
        uint32_t RESERVED_1 : 5;
    }b;
}s_acm_ctrl5;

typedef union s_acm_ctrl6
{
    uint32_t data;
    struct
    {
        uint32_t freq11  : 11;
        uint32_t RESERVED_0 : 5;
        uint32_t freq10  : 11;
        uint32_t RESERVED_1 : 5;
    }b;
}s_acm_ctrl6;

typedef union s_acm_ctrl7
{
    uint32_t data;
    struct
    {
        uint32_t freq13  : 11;
        uint32_t RESERVED_0 : 5;
        uint32_t freq12  : 11;
        uint32_t RESERVED_1 : 5;
    }b;
}s_acm_ctrl7;

typedef union s_acm_ctrl8
{
    uint32_t data;
    struct
    {
        uint32_t freq15  : 11;
        uint32_t RESERVED_0 : 5;
        uint32_t freq14  : 11;
        uint32_t RESERVED_1 : 5;
    }b;
}s_acm_ctrl8;

typedef union s_acm_ctrl9
{
    uint32_t data;
    struct
    {
        uint32_t dgain1  : 8;
        uint32_t dgain1_1 : 8;
        uint32_t dgain0  : 8;
        uint32_t dgain0_1 : 8;
    }b;
}s_acm_ctrl9;

typedef union s_acm_ctrl10
{
    uint32_t data;
    struct
    {
        uint32_t dgain3  : 8;
        uint32_t dgain3_1 : 8;
        uint32_t dgain2  : 8;
        uint32_t dgain2_1 : 8;
    }b;
}s_acm_ctrl10;

typedef union s_acm_ctrl11
{
    uint32_t data;
    struct
    {
        uint32_t dgain5  : 8;
        uint32_t dgain5_1 : 8;
        uint32_t dgain4  : 8;
        uint32_t dgain4_1 : 8;
    }b;
}s_acm_ctrl11;

typedef union s_acm_ctrl12
{
    uint32_t data;
    struct
    {
        uint32_t dgain7  : 8;
        uint32_t dgain7_1 : 8;
        uint32_t dgain6  : 8;
        uint32_t dgain6_1 : 8;
    }b;
}s_acm_ctrl12;

typedef union s_acm_ctrl13
{
    uint32_t data;
    struct
    {
        uint32_t dgain9  : 8;
        uint32_t dgain9_1 : 8;
        uint32_t dgain8  : 8;
        uint32_t dgain8_1 : 8;
    }b;
}s_acm_ctrl13;

typedef union s_acm_ctrl14
{
    uint32_t data;
    struct
    {
        uint32_t dgain11  : 8;
        uint32_t dgain11_1 : 8;
        uint32_t dgain10  : 8;
        uint32_t dgain10_1 : 8;
    }b;
}s_acm_ctrl14;

typedef union s_acm_ctrl15
{
    uint32_t data;
    struct
    {
        uint32_t dgain13  : 8;
        uint32_t dgain13_1 : 8;
        uint32_t dgain12  : 8;
        uint32_t dgain12_1 : 8;
    }b;
}s_acm_ctrl15;

typedef union s_acm_ctrl16
{
    uint32_t data;
    struct
    {
        uint32_t dgain15  : 8;
        uint32_t dgain15_1 : 8;
        uint32_t dgain14  : 8;
        uint32_t dgain14_1 : 8;
    }b;
}s_acm_ctrl16;

typedef union s_acm_ctrl17
{
    uint32_t data;
    struct
    {
        uint32_t again1  : 8;
        uint32_t again1_1 : 8;
        uint32_t again0  : 8;
        uint32_t again0_1 : 8;
    }b;
}s_acm_ctrl17;

typedef union s_acm_ctrl18
{
    uint32_t data;
    struct
    {
        uint32_t again3  : 8;
        uint32_t again3_1 : 8;
        uint32_t again2  : 8;
        uint32_t again2_1 : 8;
    }b;
}s_acm_ctrl18;

typedef union s_acm_ctrl19
{
    uint32_t data;
    struct
    {
        uint32_t again5  : 8;
        uint32_t again5_1 : 8;
        uint32_t again4  : 8;
        uint32_t again4_1 : 8;
    }b;
}s_acm_ctrl19;

typedef union s_acm_ctrl20
{
    uint32_t data;
    struct
    {
        uint32_t again7  : 8;
        uint32_t again7_1 : 8;
        uint32_t again6  : 8;
        uint32_t again6_1 : 8;
    }b;
}s_acm_ctrl20;

typedef union s_acm_ctrl21
{
    uint32_t data;
    struct
    {
        uint32_t again9  : 8;
        uint32_t again9_1 : 8;
        uint32_t again8  : 8;
        uint32_t again8_1 : 8;
    }b;
}s_acm_ctrl21;

typedef union s_acm_ctrl22
{
    uint32_t data;
    struct
    {
        uint32_t again11  : 8;
        uint32_t again11_1 : 8;
        uint32_t again10  : 8;
        uint32_t again10_1 : 8;
    }b;
}s_acm_ctrl22;

typedef union s_acm_ctrl23
{
    uint32_t data;
    struct
    {
        uint32_t again13  : 8;
        uint32_t again13_1 : 8;
        uint32_t again12  : 8;
        uint32_t again12_1 : 8;
    }b;
}s_acm_ctrl23;

typedef union s_acm_ctrl24
{
    uint32_t data;
    struct
    {
        uint32_t again15  : 8;
        uint32_t again15_1 : 8;
        uint32_t again14  : 8;
        uint32_t again14_1 : 8;
    }b;
}s_acm_ctrl24;

typedef union s_acm_ctrl25
{
    uint32_t data;
    struct
    {
        uint32_t dphase1  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t dphase0  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl25;

typedef union s_acm_ctrl26
{
    uint32_t data;
    struct
    {
        uint32_t dphase3  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t dphase2  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl26;

typedef union s_acm_ctrl27
{
    uint32_t data;
    struct
    {
        uint32_t dphase5  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t dphase4  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl27;

typedef union s_acm_ctrl28
{
    uint32_t data;
    struct
    {
        uint32_t dphase7  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t dphase6  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl28;

typedef union s_acm_ctrl29
{
    uint32_t data;
    struct
    {
        uint32_t dphase9  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t dphase8  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl29;

typedef union s_acm_ctrl30
{
    uint32_t data;
    struct
    {
        uint32_t dphase11  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t dphase10  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl30;

typedef union s_acm_ctrl31
{
    uint32_t data;
    struct
    {
        uint32_t dphase13  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t dphase12  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl31;

typedef union s_acm_ctrl32
{
    uint32_t data;
    struct
    {
        uint32_t dphase15  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t dphase14  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl32;

typedef union s_acm_ctrl33
{
    uint32_t data;
    struct
    {
        uint32_t aphase1  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t aphase0  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl33;

typedef union s_acm_ctrl34
{
    uint32_t data;
    struct
    {
        uint32_t aphase3  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t aphase2  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl34;

typedef union s_acm_ctrl35
{
    uint32_t data;
    struct
    {
        uint32_t aphase5  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t aphase4  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl35;

typedef union s_acm_ctrl36
{
    uint32_t data;
    struct
    {
        uint32_t aphase7  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t aphase6  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl36;

typedef union s_acm_ctrl37
{
    uint32_t data;
    struct
    {
        uint32_t aphase9  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t aphase8  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl37;

typedef union s_acm_ctrl38
{
    uint32_t data;
    struct
    {
        uint32_t aphase11  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t aphase10  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl38;

typedef union s_acm_ctrl39
{
    uint32_t data;
    struct
    {
        uint32_t aphase13  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t aphase12  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl39;

typedef union s_acm_ctrl40
{
    uint32_t data;
    struct
    {
        uint32_t aphase15  : 10;
        uint32_t RESERVED_0 : 6;
        uint32_t aphase14  : 10;
        uint32_t RESERVED_1 : 6;
    }b;
}s_acm_ctrl40;

typedef union s_acm_ctrl41
{
    uint32_t data;
    struct
    {
        uint32_t d1_tone_en  : 16;
        uint32_t d0_tone_en  : 16;
    }b;
}s_acm_ctrl41;

typedef union s_acm_ctrl42
{
    uint32_t data;
    struct
    {
        uint32_t d3_tone_en  : 16;
        uint32_t d2_tone_en  : 16;
    }b;
}s_acm_ctrl42;

typedef union s_acm_ctrl43
{
    uint32_t data;
    struct
    {
        uint32_t d5_tone_en  : 16;
        uint32_t d4_tone_en  : 16;
    }b;
}s_acm_ctrl43;

typedef union s_acm_ctrl44
{
    uint32_t data;
    struct
    {
        uint32_t d7_tone_en  : 16;
        uint32_t d6_tone_en  : 16;
    }b;
}s_acm_ctrl44;

typedef union s_acm_ctrl45
{
    uint32_t data;
    struct
    {
        uint32_t a_tone_en  : 16;
        uint32_t RESERVED_0 : 16;
    }b;
}s_acm_ctrl45;

typedef union s_acm_ctrl46
{
    uint32_t data;
    struct
    {
        uint32_t test_phase  : 25;
        uint32_t RESERVED_0 : 3;
        uint32_t test_acm_en  : 1;
        uint32_t RESERVED_1 : 3;
    }b;
}s_acm_ctrl46;

typedef union s_acm_ctrl47
{
    uint32_t data;
    struct
    {
        uint32_t Cali_factor  : 18;
        uint32_t RESERVED_0 : 10;
        uint32_t Cali_sel   : 4;
    }b;
}s_acm_ctrl47;

typedef union s_acm_obv0
{
    uint32_t data;
    struct
    {
        uint32_t txacm_fsm  : 3;
        uint32_t RESERVED_0 : 1;
        uint32_t rxacm_fsm  : 3;
        uint32_t RESERVED_1 : 25;
    }b;
}s_acm_obv0;

typedef union s_acm_obv1
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor0_i  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv1;

typedef union s_acm_obv2
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor0_q  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv2;

typedef union s_acm_obv3
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor1_i  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv3;

typedef union s_acm_obv4
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor1_q  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv4;

typedef union s_acm_obv5
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor2_i  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv5;

typedef union s_acm_obv6
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor2_q  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv6;

typedef union s_acm_obv7
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor3_i  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv7;

typedef union s_acm_obv8
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor3_q  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv8;

typedef union s_acm_obv9
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor4_i  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv9;

typedef union s_acm_obv10
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor4_q  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv10;

typedef union s_acm_obv11
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor5_i  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv11;

typedef union s_acm_obv12
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor5_q  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv12;

typedef union s_acm_obv13
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor6_i  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv13;

typedef union s_acm_obv14
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor6_q  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv14;

typedef union s_acm_obv15
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor7_i  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv15;

typedef union s_acm_obv16
{
    uint32_t data;
    struct
    {
        uint32_t cali_factor7_q  : 18;
        uint32_t RESERVED_0 : 14;
    }b;
}s_acm_obv16;

typedef union s_acm_obv17
{
    uint32_t data;
    struct
    {
        uint32_t ant4_snr0  : 8;
        uint32_t ant5_snr0  : 8;
        uint32_t ant6_snr0  : 8;
        uint32_t ant7_snr0  : 8;
    }b;
}s_acm_obv17;

typedef union s_acm_obv18
{
    uint32_t data;
    struct
    {
        uint32_t ant0_snr0  : 8;
        uint32_t ant1_snr0  : 8;
        uint32_t ant2_snr0  : 8;
        uint32_t ant3_snr0  : 8;
    }b;
}s_acm_obv18;

typedef union s_acm_obv19
{
    uint32_t data;
    struct
    {
        uint32_t ant4_snr1  : 8;
        uint32_t ant5_snr1  : 8;
        uint32_t ant6_snr1  : 8;
        uint32_t ant7_snr1  : 8;
    }b;
}s_acm_obv19;

typedef union s_acm_obv20
{
    uint32_t data;
    struct
    {
        uint32_t ant0_snr1  : 8;
        uint32_t ant1_snr1  : 8;
        uint32_t ant2_snr1  : 8;
        uint32_t ant3_snr1  : 8;
    }b;
}s_acm_obv20;

typedef union s_acm_obv21
{
    uint32_t data;
    struct
    {
        uint32_t ant4_snr2  : 8;
        uint32_t ant5_snr2  : 8;
        uint32_t ant6_snr2  : 8;
        uint32_t ant7_snr2  : 8;
    }b;
}s_acm_obv21;

typedef union s_acm_obv22
{
    uint32_t data;
    struct
    {
        uint32_t ant0_snr2  : 8;
        uint32_t ant1_snr2  : 8;
        uint32_t ant2_snr2  : 8;
        uint32_t ant3_snr2  : 8;
    }b;
}s_acm_obv22;

typedef union s_acm_obv23
{
    uint32_t data;
    struct
    {
        uint32_t ant4_snr3  : 8;
        uint32_t ant5_snr3  : 8;
        uint32_t ant6_snr3  : 8;
        uint32_t ant7_snr3  : 8;
    }b;
}s_acm_obv23;

typedef union s_acm_obv24
{
    uint32_t data;
    struct
    {
        uint32_t ant0_snr3  : 8;
        uint32_t ant1_snr3  : 8;
        uint32_t ant2_snr3  : 8;
        uint32_t ant3_snr3  : 8;
    }b;
}s_acm_obv24;


struct acm
{
    volatile union      s_acm_ctrl0 acm_ctrl0;	//0x0
    volatile union      s_acm_ctrl1 acm_ctrl1;	//0x4
    volatile union      s_acm_ctrl2 acm_ctrl2;	//0x8
    volatile union      s_acm_ctrl3 acm_ctrl3;	//0xc
    volatile union      s_acm_ctrl4 acm_ctrl4;	//0x10
    volatile union      s_acm_ctrl5 acm_ctrl5;	//0x14
    volatile union      s_acm_ctrl6 acm_ctrl6;	//0x18
    volatile union      s_acm_ctrl7 acm_ctrl7;	//0x1c
    volatile union      s_acm_ctrl8 acm_ctrl8;	//0x20
    volatile union      s_acm_ctrl9 acm_ctrl9;	//0x24
    volatile union      s_acm_ctrl10 acm_ctrl10;	//0x28
    volatile union      s_acm_ctrl11 acm_ctrl11;	//0x2c
    volatile union      s_acm_ctrl12 acm_ctrl12;	//0x30
    volatile union      s_acm_ctrl13 acm_ctrl13;	//0x34
    volatile union      s_acm_ctrl14 acm_ctrl14;	//0x38
    volatile union      s_acm_ctrl15 acm_ctrl15;	//0x3c
    volatile union      s_acm_ctrl16 acm_ctrl16;	//0x40
    volatile union      s_acm_ctrl17 acm_ctrl17;	//0x44
    volatile union      s_acm_ctrl18 acm_ctrl18;	//0x48
    volatile union      s_acm_ctrl19 acm_ctrl19;	//0x4c
    volatile union      s_acm_ctrl20 acm_ctrl20;	//0x50
    volatile union      s_acm_ctrl21 acm_ctrl21;	//0x54
    volatile union      s_acm_ctrl22 acm_ctrl22;	//0x58
    volatile union      s_acm_ctrl23 acm_ctrl23;	//0x5c
    volatile union      s_acm_ctrl24 acm_ctrl24;	//0x60
    volatile union      s_acm_ctrl25 acm_ctrl25;	//0x64
    volatile union      s_acm_ctrl26 acm_ctrl26;	//0x68
    volatile union      s_acm_ctrl27 acm_ctrl27;	//0x6c
    volatile union      s_acm_ctrl28 acm_ctrl28;	//0x70
    volatile union      s_acm_ctrl29 acm_ctrl29;	//0x74
    volatile union      s_acm_ctrl30 acm_ctrl30;	//0x78
    volatile union      s_acm_ctrl31 acm_ctrl31;	//0x7c
    volatile union      s_acm_ctrl32 acm_ctrl32;	//0x80
    volatile union      s_acm_ctrl33 acm_ctrl33;	//0x84
    volatile union      s_acm_ctrl34 acm_ctrl34;	//0x88
    volatile union      s_acm_ctrl35 acm_ctrl35;	//0x8c
    volatile union      s_acm_ctrl36 acm_ctrl36;	//0x90
    volatile union      s_acm_ctrl37 acm_ctrl37;	//0x94
    volatile union      s_acm_ctrl38 acm_ctrl38;	//0x98
    volatile union      s_acm_ctrl39 acm_ctrl39;	//0x9c
    volatile union      s_acm_ctrl40 acm_ctrl40;	//0xa0
    volatile union      s_acm_ctrl41 acm_ctrl41;	//0xa4
    volatile union      s_acm_ctrl42 acm_ctrl42;	//0xa8
    volatile union      s_acm_ctrl43 acm_ctrl43;	//0xac
    volatile union      s_acm_ctrl44 acm_ctrl44;	//0xb0
    volatile union      s_acm_ctrl45 acm_ctrl45;	//0xb4
    volatile union      s_acm_ctrl46 acm_ctrl46;	//0xb8
    volatile union      s_acm_ctrl47 acm_ctrl47;	//0xbc
    volatile uint32_t   reserved_0[4];	//0xc0
    volatile union      s_acm_obv0 acm_obv0;	//0xd0
    volatile union      s_acm_obv1 acm_obv1;	//0xd4
    volatile union      s_acm_obv2 acm_obv2;	//0xd8
    volatile union      s_acm_obv3 acm_obv3;	//0xdc
    volatile union      s_acm_obv4 acm_obv4;	//0xe0
    volatile union      s_acm_obv5 acm_obv5;	//0xe4
    volatile union      s_acm_obv6 acm_obv6;	//0xe8
    volatile union      s_acm_obv7 acm_obv7;	//0xec
    volatile union      s_acm_obv8 acm_obv8;	//0xf0
    volatile union      s_acm_obv9 acm_obv9;	//0xf4
    volatile union      s_acm_obv10 acm_obv10;	//0xf8
    volatile union      s_acm_obv11 acm_obv11;	//0xfc
    volatile union      s_acm_obv12 acm_obv12;	//0x100
    volatile union      s_acm_obv13 acm_obv13;	//0x104
    volatile union      s_acm_obv14 acm_obv14;	//0x108
    volatile union      s_acm_obv15 acm_obv15;	//0x10c
    volatile union      s_acm_obv16 acm_obv16;	//0x110
    volatile union      s_acm_obv17 acm_obv17;	//0x114
    volatile union      s_acm_obv18 acm_obv18;	//0x118
    volatile union      s_acm_obv19 acm_obv19;	//0x11c
    volatile union      s_acm_obv20 acm_obv20;	//0x120
    volatile union      s_acm_obv21 acm_obv21;	//0x124
    volatile union      s_acm_obv22 acm_obv22;	//0x128
    volatile union      s_acm_obv23 acm_obv23;	//0x12c
    volatile union      s_acm_obv24 acm_obv24;	//0x130
};
struct total
{
    volatile struct  rx_top rx_top_cfg;
    volatile struct  mac mac_cfg;
    volatile struct  tx_dac_if tx_dac_if_cfg;
    volatile struct  rx_bcn rx_bcn_cfg;
    volatile struct  tx_top tx_top_cfg;
    volatile struct  rx_mup rx_mup_cfg;
    volatile struct  acm acm_cfg;
};

#ifdef __cplusplus
}
#endif

#endif /* TK8710_REGS_H */
