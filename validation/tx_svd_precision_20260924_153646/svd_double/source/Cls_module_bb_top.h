/****************************************************************************
**
**  Copyright  (c)  2006,  Amlogic, Inc.
**          All Rights Reserved.
**
**  Subsystem    :
**  File         : Cls_module_bb_top.h
**  Created By   :
**  Created On   :
**
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**
****************************************************************************/

#ifndef __Module_BB_TOP_H
#define __Module_BB_TOP_H

#include "common_type.h"
#include "Lib_io.h"
#include "Cls_module_phy_top.h"

typedef struct InterF_bb_top
{
    Cls_io<bool>        p_reset_n                   ;
    Cls_io<bool>        i_enable                    ;
    Cls_io<double>      i_dig_iq_rx_1_i             ;
    Cls_io<double>      i_dig_iq_rx_1_q             ;
    Cls_io<double>      i_dig_iq_rx_2_i             ;
    Cls_io<double>      i_dig_iq_rx_2_q             ;
    Cls_io<double>      i_dig_iq_rx_3_i             ;
    Cls_io<double>      i_dig_iq_rx_3_q             ;
    Cls_io<double>      i_dig_iq_rx_4_i             ;
    Cls_io<double>      i_dig_iq_rx_4_q             ;
    Cls_io<double>      i_dig_iq_rx_5_i             ;
    Cls_io<double>      i_dig_iq_rx_5_q             ;
    Cls_io<double>      i_dig_iq_rx_6_i             ;
    Cls_io<double>      i_dig_iq_rx_6_q             ;
    Cls_io<double>      i_dig_iq_rx_7_i             ;
    Cls_io<double>      i_dig_iq_rx_7_q             ;
    Cls_io<double>      i_dig_iq_rx_8_i             ;
    Cls_io<double>      i_dig_iq_rx_8_q             ;
    Cls_io<bool>        i_dig_iq_rx_v               ;
    Cls_io<bool>        i_dig_iq_rx_data_active     ;
    Cls_io<bool>        i_power_down_prepare        ;
    Cls_io<double>      o_dig_iq_tx_data_i          ;
    Cls_io<double>      o_dig_iq_tx_data_q          ;
    Cls_io<bool>        o_dig_iq_tx_data_v          ;
    Cls_io<bool>        o_dig_iq_tx_data_active     ;
    Cls_io<bool>        o_interrupt                 ;
    Cls_io<double>      o_interrupt_type            ;
    Cls_io<bool>        o_idle                      ;
    Cls_io<bool>        o_rx_data_done              ;
    //rx_result           o_rx_result                 ;
}InterF_bb_top;

typedef struct bb_top_config
{
    phy_top_config      phy_top_cfg;
    int                 rx_mode;
    //mac_top_config      mac_top_cfg;
}bb_top_config;


class Cls_module_bb_top
{
    public:
        Cls_module_bb_top(){}
        ~Cls_module_bb_top(){
        }

        void init(char *module_name);
        void configure(bb_top_config cfg);
        void reset();
        void go();

        InterF_bb_top       m_IO;
        CONF                CONF_cfg;
        Data_In_Mode        m_source_mode;// = Data_OTHER;
        //Module_mac_top      m_mac_top;
        Module_phy_top      m_phy_top;

private:
        bb_top_config           m_cfg;
        IO_config               m_io_config;
        TX_RX                   m_cfg_flag;
        //Cls_INITIAL_phy2<double>m_init;
        bool                    m_power_flag;
        bool                    m_preset;
        int                     m_rx_mode;
};

void Cls_module_bb_top::configure(bb_top_config cfg)
{
    Module_config       default_module_config;
    IO_config           default_IO_config;

    default_module_config.fx_mode = floatpoint;//fixpoint

    m_cfg.phy_top_cfg.ud_cfg.module_config = default_module_config;
    m_cfg.phy_top_cfg.ud_cfg.module_config.fx_mode = fixpoint;//fixpoint;

    m_rx_mode = cfg.rx_mode;
    if(m_rx_mode == 1)
    {
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the1 = 4;
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the2 = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxAllUser = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_en = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_num = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_set = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para1 = 0.48;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para2 = 0.52;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_the = 10;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p2_the = 45;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p3_the = 20;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p6_the = 10;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxShrink = 1;
        m_cfg.phy_top_cfg.ud_cfg.cfg.rx_sampling_rate = 15625;

        m_cfg.phy_top_cfg.is_cfg.module_config = default_module_config;
        m_cfg.phy_top_cfg.is_cfg.cfg.user_num = 5;
        m_cfg.phy_top_cfg.is_cfg.cfg.slid_num = 3;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.freqOffsetCh = -946.044921875;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.Antenna = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.FFTLen = 8192;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.mode = m_rx_mode;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.filterLen = 1280;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.ifftLen = 4096;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRxSampleRate = 15625;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRXBFMode = 0;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.HCE = 1;

        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slide_num_i = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ant_num = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.fft_len = 8192;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ifft_len = 4096;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.rx_d_win_i = 2;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.gw_rxbf_mode = 0;
        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.mfsk = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.DSSS = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_alpha = 0.9;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_th = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slidedatanum = 2;//0:1, 1:2, 2:4, 3:8, 4:16

        m_phy_top.m_IO.i_ant_num = 8;
        m_phy_top.m_IO.i_fft_len = 8192;
    }
    if(m_rx_mode == 2)
    {
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the1 = 4;
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the2 = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxAllUser = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_en = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_num = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_set = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para1 = 0.48;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para2 = 0.52;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_the = 12;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p2_the = 45;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p3_the = 20;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p6_the = 10;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxShrink = 1;
        m_cfg.phy_top_cfg.ud_cfg.cfg.rx_sampling_rate = 31250;

        m_cfg.phy_top_cfg.is_cfg.module_config = default_module_config;
        m_cfg.phy_top_cfg.is_cfg.cfg.user_num = 5;
        m_cfg.phy_top_cfg.is_cfg.cfg.slid_num = 3;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.freqOffsetCh = 48141.47949218750;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.Antenna = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.FFTLen = 8192;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.mode = m_rx_mode;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.filterLen = 640;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.ifftLen = 2048;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRxSampleRate = 31250;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRXBFMode = 0;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.HCE = 1;

        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slide_num_i = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ant_num = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.fft_len = 8192;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ifft_len = 2048;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.rx_d_win_i = 2;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.gw_rxbf_mode = 0;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_alpha = 0.9;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_th = 4;
        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.mfsk = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.DSSS = 2;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slidedatanum = 2;//0:1, 1:2, 2:4, 3:8, 4:16


        m_phy_top.m_IO.i_ant_num = 8;
        m_phy_top.m_IO.i_fft_len = 8192;
    }
    if(m_rx_mode == 3)
    {
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the1 = 4;
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the2 = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxAllUser = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_en = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_num = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_set = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para1 = 0.48;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para2 = 0.52;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_the = 12;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p2_the = 45;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p3_the = 20;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p6_the = 10;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxShrink = 1;
        m_cfg.phy_top_cfg.ud_cfg.cfg.rx_sampling_rate = 62500;

        m_cfg.phy_top_cfg.is_cfg.module_config = default_module_config;
        m_cfg.phy_top_cfg.is_cfg.cfg.user_num = 5;
        m_cfg.phy_top_cfg.is_cfg.cfg.slid_num = 3;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.freqOffsetCh = 48141.47949218750;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.Antenna = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.FFTLen = 8192;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.mode = m_rx_mode;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.filterLen = 320;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.ifftLen = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRxSampleRate = 62500;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRXBFMode = 0;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.HCE = 1;

        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slide_num_i = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ant_num = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.fft_len = 8192;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ifft_len = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.rx_d_win_i = 2;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.gw_rxbf_mode = 0;
        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.mfsk = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.DSSS = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_alpha = 0.9;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_th = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slidedatanum = 2;//0:1, 1:2, 2:4, 3:8, 4:16


        m_phy_top.m_IO.i_ant_num = 8;
        m_phy_top.m_IO.i_fft_len = 8192;
    }
    if(m_rx_mode == 4)
    {
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the1 = 4;
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the2 = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxAllUser = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_en = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_num = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_set = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para1 = 0.48;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para2 = 0.52;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_the = 12;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p2_the = 45;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p3_the = 20;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p6_the = 10;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxShrink = 1;
        m_cfg.phy_top_cfg.ud_cfg.cfg.rx_sampling_rate = 125000;

        m_cfg.phy_top_cfg.is_cfg.module_config = default_module_config;
        m_cfg.phy_top_cfg.is_cfg.cfg.user_num = 5;
        m_cfg.phy_top_cfg.is_cfg.cfg.slid_num = 3;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.freqOffsetCh = 48141.47949218750;//not use from user detect
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.Antenna = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.FFTLen = 8192;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.mode = m_rx_mode;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.filterLen = 320;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.ifftLen = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRxSampleRate = 125000;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRXBFMode = 0;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.HCE = 1;

        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slide_num_i = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ant_num = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.fft_len = 8192;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ifft_len = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.rx_d_win_i = 2;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.gw_rxbf_mode = 0;
        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.mfsk = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.DSSS = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_alpha = 0.9;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_th = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slidedatanum = 2;//0:1, 1:2, 2:4, 3:8, 4:16

        m_phy_top.m_IO.i_ant_num = 8;
        m_phy_top.m_IO.i_fft_len = 8192;
    }
    if(m_rx_mode == 5)
    {
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the1 = 4;
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the2 = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxAllUser = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_en = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_num = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_set = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para1 = 0.48;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para2 = 0.52;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_the = 12;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p2_the = 45;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p3_the = 20;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p6_the = 10;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxShrink = 1;
        m_cfg.phy_top_cfg.ud_cfg.cfg.rx_sampling_rate = 250000;

        m_cfg.phy_top_cfg.is_cfg.module_config = default_module_config;
        m_cfg.phy_top_cfg.is_cfg.cfg.user_num = 5;
        m_cfg.phy_top_cfg.is_cfg.cfg.slid_num = 3;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.freqOffsetCh = 48141.47949218750;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.Antenna = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.FFTLen = 8192;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.mode = m_rx_mode;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.filterLen = 320;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.ifftLen = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRxSampleRate = 250000;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRXBFMode = 0;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.HCE = 1;

        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slide_num_i = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ant_num = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.fft_len = 8192;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ifft_len = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.rx_d_win_i = 2;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.gw_rxbf_mode = 0;
        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.mfsk = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.DSSS = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_alpha = 0.9;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_th = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slidedatanum = 2;//0:1, 1:2, 2:4, 3:8, 4:16


        m_phy_top.m_IO.i_ant_num = 8;
        m_phy_top.m_IO.i_fft_len = 8192;
    }
    if(m_rx_mode == 6)
    {
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the1 = 4;
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the2 = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxAllUser = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_en = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_num = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_set = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para1 = 0.48;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para2 = 0.52;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_the = 12;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p2_the = 45;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p3_the = 20;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p6_the = 10;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxShrink = 1;
        m_cfg.phy_top_cfg.ud_cfg.cfg.rx_sampling_rate = 500000;

        m_cfg.phy_top_cfg.is_cfg.module_config = default_module_config;
        m_cfg.phy_top_cfg.is_cfg.cfg.user_num = 5;
        m_cfg.phy_top_cfg.is_cfg.cfg.slid_num = 3;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.freqOffsetCh = 48141.47949218750;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.Antenna = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.FFTLen = 8192;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.mode = m_rx_mode;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.filterLen = 320;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.ifftLen = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRxSampleRate = 500000;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRXBFMode = 0;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.HCE = 1;

        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slide_num_i = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ant_num = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.fft_len = 8192;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ifft_len = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.rx_d_win_i = 2;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.gw_rxbf_mode = 0;
        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.mfsk = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.DSSS = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_alpha = 0.9;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_th = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slidedatanum = 2;//0:1, 1:2, 2:4, 3:8, 4:16

        m_phy_top.m_IO.i_ant_num = 8;
        m_phy_top.m_IO.i_fft_len = 8192;
    }
    if(m_rx_mode == 7)
    {
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the1 = 4;
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the2 = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxAllUser = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_en = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_num = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_set = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para1 = 0.48;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para2 = 0.52;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_the = 12;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p2_the = 45;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p3_the = 20;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p6_the = 10;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxShrink = 1;
        m_cfg.phy_top_cfg.ud_cfg.cfg.rx_sampling_rate = 500000;

        m_cfg.phy_top_cfg.is_cfg.module_config = default_module_config;
        m_cfg.phy_top_cfg.is_cfg.cfg.user_num = 5;
        m_cfg.phy_top_cfg.is_cfg.cfg.slid_num = 3;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.freqOffsetCh = 48141.47949218750;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.Antenna = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.FFTLen = 4096;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.mode = m_rx_mode;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.filterLen = 320;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.ifftLen = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRxSampleRate = 500000;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRXBFMode = 0;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.HCE = 1;

        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slide_num_i = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ant_num = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.fft_len = 4096;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ifft_len = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.rx_d_win_i = 2;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.gw_rxbf_mode = 0;
        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.mfsk = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.DSSS = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_alpha = 0.9;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_th = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slidedatanum = 2;//0:1, 1:2, 2:4, 3:8, 4:16

        m_phy_top.m_IO.i_ant_num = 8;
        m_phy_top.m_IO.i_fft_len = 4096;
    }
    if(m_rx_mode == 8)
    {
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the1 = 4;
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the2 = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxAllUser = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_en = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_num = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_set = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para1 = 0.48;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para2 = 0.52;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_the = 12;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p2_the = 45;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p3_the = 20;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p6_the = 10;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxShrink = 1;
        m_cfg.phy_top_cfg.ud_cfg.cfg.rx_sampling_rate = 500000;

        m_cfg.phy_top_cfg.is_cfg.module_config = default_module_config;
        m_cfg.phy_top_cfg.is_cfg.cfg.user_num = 5;
        m_cfg.phy_top_cfg.is_cfg.cfg.slid_num = 3;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.freqOffsetCh = 48141.47949218750;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.Antenna = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.FFTLen = 2048;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.mode = m_rx_mode;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.filterLen = 320;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.ifftLen = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRxSampleRate = 500000;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRXBFMode = 0;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.HCE = 1;

        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slide_num_i = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ant_num = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.fft_len = 2048;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ifft_len = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.rx_d_win_i = 2;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.gw_rxbf_mode = 0;
        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.mfsk = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.DSSS = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_alpha = 0.9;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_th = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slidedatanum = 2;//0:1, 1:2, 2:4, 3:8, 4:16

        m_phy_top.m_IO.i_ant_num = 8;
        m_phy_top.m_IO.i_fft_len = 2048;
    }
    if(m_rx_mode == 9)
    {
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the1 = 4;
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the2 = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxAllUser = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_en = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_num = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_set = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para1 = 0.48;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para2 = 0.52;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_the = 12;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p2_the = 45;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p3_the = 20;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p6_the = 10;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxShrink = 1;
        m_cfg.phy_top_cfg.ud_cfg.cfg.rx_sampling_rate = 500000;

        m_cfg.phy_top_cfg.is_cfg.module_config = default_module_config;
        m_cfg.phy_top_cfg.is_cfg.cfg.user_num = 5;
        m_cfg.phy_top_cfg.is_cfg.cfg.slid_num = 3;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.freqOffsetCh = 48141.47949218750;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.Antenna = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.FFTLen = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.mode = m_rx_mode;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.filterLen = 320;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.ifftLen = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRxSampleRate = 500000;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRXBFMode = 0;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.HCE = 1;

        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slide_num_i = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ant_num = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.fft_len = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ifft_len = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.rx_d_win_i = 2;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.gw_rxbf_mode = 0;
        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.mfsk = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.DSSS = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_alpha = 0.9;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_th = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slidedatanum = 2;//0:1, 1:2, 2:4, 3:8, 4:16

        m_phy_top.m_IO.i_ant_num = 8;
        m_phy_top.m_IO.i_fft_len = 1024;
    }
    if(m_rx_mode == 10)
    {
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the1 = 4;
        m_cfg.phy_top_cfg.ud_cfg.cfg.anoise_the2 = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxAllUser = 5;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_en = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_num = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.freq_alloc_set = 0;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para1 = 0.48;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_para2 = 0.52;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p1_the = 12;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p2_the = 45;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p3_the = 20;
        m_cfg.phy_top_cfg.ud_cfg.cfg.p6_the = 10;
        m_cfg.phy_top_cfg.ud_cfg.cfg.GWRxShrink = 1;
        m_cfg.phy_top_cfg.ud_cfg.cfg.rx_sampling_rate = 500000;

        m_cfg.phy_top_cfg.is_cfg.module_config = default_module_config;
        m_cfg.phy_top_cfg.is_cfg.cfg.user_num = 5;
        m_cfg.phy_top_cfg.is_cfg.cfg.slid_num = 3;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.freqOffsetCh = 48141.47949218750;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.Antenna = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.FFTLen = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.mode = m_rx_mode;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.filterLen = 320;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.ifftLen = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRxSampleRate = 500000;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.GWRXBFMode = 0;
        m_cfg.phy_top_cfg.is_cfg.cfg.BF_cfg.HCE = 1;

        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slide_num_i = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ant_num = 8;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.fft_len = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.ifft_len = 1024;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.rx_d_win_i = 2;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.gw_rxbf_mode = 0;
        //m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.mfsk = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.DSSS = 1;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_alpha = 0.9;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.bf_th = 4;
        m_cfg.phy_top_cfg.is_cfg.cfg.beamf_cfg.slidedatanum = 2;//0:1, 1:2, 2:4, 3:8, 4:16


        m_phy_top.m_IO.i_ant_num = 8;
        m_phy_top.m_IO.i_fft_len = 1024;
    }
    m_cfg.phy_top_cfg.is_cfg.cfg.mode = m_rx_mode;
    m_cfg.phy_top_cfg.is_cfg.module_config.fx_mode = fixpoint;//fixpoint;
    m_phy_top.configure(m_cfg.phy_top_cfg);

    /*
    BW mode = (BW)CONF_cfg.mode;// BW::Mode7;
    PHY_VER phy_ver = PHY1;
    m_cfg_flag = tx_rx_flag;
    default_module_config.fx_mode = floatpoint;//fixpoint;//
    default_module_config.in_mode = data_in;   //file_in;
    default_module_config.tx_rx_flag = tx_rx_flag;
    default_module_config.by_pass_en = 0;
    default_module_config.source_type = CONF_cfg.source_type;
    default_IO_config.io_fileOut_flag = 0;
    default_IO_config.io_max_delay = 100;

    if (CONF_cfg.tx.ver == 2)
        phy_ver = PHY2;
    else
        phy_ver = PHY1;
    //PHY Configuration
    int    m = 0;
    //double if_mHz = 100000;
    double fs = 1e6;
    double f_symb = 125.0 / 128 * 1000 * (int)mode;
    int r = (int)floor(log2(1000000.0 / f_symb / 8));
    double f_symb_x8 = f_symb * 8;
    if (r < 2)  m = r;   else m = r - 2;

    //FEC RS Configuration
    unsigned int rs_n = 56;
    unsigned int rs_r = 12;
    unsigned int rs_k = rs_n - rs_r;
    unsigned int rs_D = 67;
    unsigned int rs_Nbit = 6;
    unsigned int time_slot_ms = 0;
    if(CONF_cfg.mode_phy2 == 1 )
        time_slot_ms = 35000000;
    else if(CONF_cfg.mode_phy2 == 2 )
        time_slot_ms = 16000000;
    else if(CONF_cfg.mode_phy2 == 3 )
        time_slot_ms = 8000000;
    else if(CONF_cfg.mode_phy2 == 4)
        time_slot_ms = 8000000;
    else if(CONF_cfg.mode_phy2 == 5)
        time_slot_ms = 5000000;
    else if(CONF_cfg.mode_phy2 == 6)
        time_slot_ms = 2000000;
    else if(CONF_cfg.mode_phy2 == 7)
        time_slot_ms = 2000000;
    else if(CONF_cfg.mode_phy2 == 8)
        time_slot_ms = 1000000;
    else if(CONF_cfg.mode_phy2 == 9)
        time_slot_ms = 1000000;
    else if(CONF_cfg.mode_phy2 == 10)
        time_slot_ms = 250000;
    else if(CONF_cfg.mode_phy2 == 11)
        time_slot_ms = 2500000;
    else if(CONF_cfg.mode_phy2 == 12)
        time_slot_ms = 2500000;
    //MAC Configuration
    m_cfg.mac_top_cfg.timer_cfg.module_config = default_module_config;
    m_cfg.mac_top_cfg.timer_cfg.io_config = default_IO_config;
    m_cfg.mac_top_cfg.timer_cfg.slot_length = time_slot_ms;  // 35000000
    m_cfg.mac_top_cfg.timer_cfg.slot_tx_beacon_start = 0;
    m_cfg.mac_top_cfg.timer_cfg.slot_tx_beacon_end = m_cfg.mac_top_cfg.timer_cfg.slot_tx_beacon_start + (word64)(2048 + 2048 + 2048 + 512) * 32;
    m_cfg.mac_top_cfg.timer_cfg.slot_tx_data_start = 0;
    m_cfg.mac_top_cfg.timer_cfg.slot_tx_data_end = time_slot_ms;
    m_cfg.mac_top_cfg.timer_cfg.slot_end = (word64)time_slot_ms * 4;// m_cfg.mac_top_cfg.timer_cfg.slot_tx_beacon_end;
    //m_cfg.mac_top_cfg.timer_cfg.slot_rx_data_start = 8000;
    //m_cfg.mac_top_cfg.timer_cfg.slot_rx_data_end = m_cfg.mac_top_cfg.timer_cfg.slot_rx_data_start + 2048 + (word64)(65 + 216) * 1024;
    //m_cfg.mac_top_cfg.timer_cfg.slot_rx_beacon_start = m_cfg.mac_top_cfg.timer_cfg.slot_rx_data_end + 8000;
    //m_cfg.mac_top_cfg.timer_cfg.slot_rx_beacon_end = m_cfg.mac_top_cfg.timer_cfg.slot_rx_beacon_start + (word64)(2048 + 2048 + 2048) * 32;
    //m_cfg.mac_top_cfg.timer_cfg.slot_tx_beacon_start = m_cfg.mac_top_cfg.timer_cfg.slot_tx_data_end + 8000;
    //m_cfg.mac_top_cfg.timer_cfg.slot_tx_beacon_end = m_cfg.mac_top_cfg.timer_cfg.slot_tx_beacon_start + (word64)(2048 + 2048) * 32;
    //m_cfg.mac_top_cfg.timer_cfg.slot_end = m_cfg.mac_top_cfg.timer_cfg.slot_tx_beacon_end;
    m_cfg.mac_top_cfg.timer_cfg.sw_intrrpt_window = 10;
    m_cfg.mac_top_cfg.timer_cfg.hw_clk_MHz = 1;

    m_cfg.mac_top_cfg.tx_fec_top_cfg.module_config = default_module_config;
    m_cfg.mac_top_cfg.tx_fec_top_cfg.io_config = default_IO_config;
    m_cfg.mac_top_cfg.tx_fec_top_cfg.m_phy_ver = phy_ver;
    m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.rs_k = rs_k;
    m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.rs_num = 2;
    m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.rs_Nbit = rs_Nbit;
    m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.rs_N = rs_n;
    m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.pn_mode = PN64;
    m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.mode_t = 4;
    m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.fb_Mhz = 125.0 / 128 * 1000 * m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.mode_t / 1e6;
    m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.agc_level = 1024.0 / 2048;
    m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.agc_len = (int)floor(m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.fb_Mhz * 1e6 * 16.384 / 1000);
    m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.bch_len = 2048 + 2048 + 2048;
    m_cfg.mac_top_cfg.tx_fec_top_cfg.mode = m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.rs_Nbit * m_cfg.mac_top_cfg.tx_fec_top_cfg.tx_gen_cfg.rs_N; // 216 is short, 360 is long
    m_cfg.mac_top_cfg.tx_fec_top_cfg.data_bw = 1;// 1 is 125/128KHz, 128 is 125KHz
    m_cfg.mac_top_cfg.tx_fec_top_cfg.beacon_bw = 32;// default is 32
    m_cfg.mac_top_cfg.tx_fec_top_cfg.simulation_clk_MHz = 1;// 1 is 1MHz, 16 is 16MHz

    //PHY Configuration
    m_cfg.phy_top_cfg.rx_iqim_cfg.module_config = default_module_config;
    m_cfg.phy_top_cfg.rx_iqim_cfg.io_config = default_IO_config;
    m_cfg.phy_top_cfg.rx_iqim_cfg.module_config.fx_mode = floatpoint;
    m_cfg.phy_top_cfg.rx_iqim_cfg.module_config.by_pass_en = 1;

    m_cfg.phy_top_cfg.rx_fe_cfg.module_config = default_module_config;
    m_cfg.phy_top_cfg.rx_fe_cfg.io_config = default_IO_config;
    m_cfg.phy_top_cfg.rx_fe_cfg.module_config.by_pass_en = 0;
    m_cfg.phy_top_cfg.rx_fe_cfg.src_bypass_en = 0;
    m_cfg.phy_top_cfg.rx_fe_cfg.acf_bypass_en = 0;
    m_cfg.phy_top_cfg.rx_fe_cfg.io_config.tx_rx_flag = tx_rx_flag;
    m_cfg.phy_top_cfg.rx_fe_cfg.rx_ddc_config.ang_delta = 0;
    m_cfg.phy_top_cfg.rx_fe_cfg.rx_ddc_config.ang_init = 0;
    m_cfg.phy_top_cfg.rx_fe_cfg.rx_src_config.outRate = (int)(f_symb_x8 * DOUBLE_POW2[64 + m]);
    m_cfg.phy_top_cfg.rx_fe_cfg.rx_src_config.alpha = 0.25;
    m_cfg.phy_top_cfg.rx_fe_cfg.rx_src_config.inRate = (int)(fs / (f_symb_x8 * DOUBLE_POW2[64 + m]) * DOUBLE_POW2[64 + 20]);
    m_cfg.phy_top_cfg.rx_fe_cfg.rx_src_config.counter = (int)((fs - f_symb_x8 * DOUBLE_POW2[64 + m]) / (f_symb_x8 * DOUBLE_POW2[64 + m]) * DOUBLE_POW2[64 + 20]);

    m_cfg.phy_top_cfg.rx_ds_flt_cfg.module_config = default_module_config;
    m_cfg.phy_top_cfg.rx_ds_flt_cfg.io_config = default_IO_config;
    m_cfg.phy_top_cfg.rx_ds_flt_cfg.module_config.by_pass_en = 0;
    m_cfg.phy_top_cfg.rx_ds_flt_cfg.io_config.tx_rx_flag = tx_rx_flag;
    m_cfg.phy_top_cfg.rx_ds_flt_cfg.mode = m;

    m_cfg.phy_top_cfg.rx_adc_if_cfg.module_config = default_module_config;
    m_cfg.phy_top_cfg.rx_adc_if_cfg.io_config = default_IO_config;
    m_cfg.phy_top_cfg.rx_adc_if_cfg.module_config.by_pass_en = 1;

    m_cfg.phy_top_cfg.rx_agc_cfg.module_config = default_module_config;
    m_cfg.phy_top_cfg.rx_agc_cfg.io_config = default_IO_config;
    m_cfg.phy_top_cfg.rx_agc_cfg.module_config.by_pass_en = 0;
    m_cfg.phy_top_cfg.rx_agc_cfg.agc_len = int(floor(f_symb * 16.384 / 1000)) * 8;

    m_cfg.phy_top_cfg.rx_beacon_cfg.module_config = default_module_config;
    m_cfg.phy_top_cfg.rx_beacon_cfg.io_config = default_IO_config;
    m_cfg.phy_top_cfg.rx_beacon_cfg.io_config.io_fileOut_flag = 0;

    m_cfg.phy_top_cfg.rx_data.demod_phy1.module_config = default_module_config;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.io_config = default_IO_config;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.f_symb = f_symb;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.phy_ver = CONF_cfg.tx.ver;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.dist = 4;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.symb_len = 64;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.mode_search_win = 1;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.fec_frame_len = rs_n * rs_Nbit;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.pn_len = 64;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.constel = QPSK;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.num_symb = (int)mode;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.rs_cfg.n = rs_n;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.rs_cfg.r = rs_r;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.rs_cfg.D = rs_D;
    m_cfg.phy_top_cfg.rx_data.demod_phy1.rs_cfg.Nbit = rs_Nbit;

    m_cfg.phy_top_cfg.tx_us_flt_cfg.module_config = default_module_config;
    m_cfg.phy_top_cfg.tx_us_flt_cfg.io_config = default_IO_config;
    m_cfg.phy_top_cfg.tx_us_flt_cfg.tx_ddc_config.ang_delta = 0;
    m_cfg.phy_top_cfg.tx_us_flt_cfg.tx_ddc_config.ang_init = 0;
    m_cfg.phy_top_cfg.tx_us_flt_cfg.tx_len_cfg = 1759;//need to be modified
    if (CONF_cfg.tx.ver == 2) {
        m_cfg.phy_top_cfg.rx_data.module_config = default_module_config;
        m_cfg.phy_top_cfg.tx_us_flt_cfg.osr = 1;
        m_cfg.phy_top_cfg.rx_agc_cfg.module_config.by_pass_en = 1;
        m_cfg.phy_top_cfg.rx_fe_cfg.module_config.by_pass_en = 1;
        m_cfg.phy_top_cfg.rx_fe_cfg.src_bypass_en = 1;
        m_cfg.phy_top_cfg.rx_fe_cfg.acf_bypass_en = 1;
        m_cfg.phy_top_cfg.rx_ds_flt_cfg.mode = 6; // mode 10
        m_cfg.phy_top_cfg.rx_ds_flt_cfg.phy_ver = CONF_cfg.tx.ver;
        m_cfg.phy_top_cfg.rx_data.phy_ver = 2;


        if(g_fix_or_float == 2){
           init_memory_cfg(&ModePara_C);
           gen_cfg_c(CONF_cfg.mode_phy2, CONF_cfg.rs_polor,&ModePara_C);
        }

        m_init.gen_cfg(CONF_cfg.mode_phy2, CONF_cfg.rs_polor);


        m_cfg.phy_top_cfg.rx_data.demod_phy2.m_cfg = m_init.m_cfg;
        m_cfg.mac_top_cfg.tx_fec_top_cfg.phy2_init = m_init.m_cfg;
        m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.DelayFilter_Sim = 0;
        m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.Fs = m_init.m_cfg.Fs;
        m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.Mfsk = m_init.m_cfg.Mfsk;
        m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.TOScope = m_init.m_cfg.TOScope;
        m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.seg_num = m_init.m_cfg.SegNum;
        m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.Sym_Num_Data = m_init.m_cfg.SymNumData;
        m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.SyncCombinePower = m_init.m_cfg.SyncCombineFlag;
        if (m_init.m_cfg.NumDsss == 1)
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.NumDss = 0;
        else if (m_init.m_cfg.NumDsss == 2)
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.NumDss = 1;
        else if (m_init.m_cfg.NumDsss == 4)
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.NumDss = 2;
        else if (m_init.m_cfg.NumDsss == 8)
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.NumDss = 3;
        else if (m_init.m_cfg.NumDsss == 16)
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.NumDss = 4;
        else if (m_init.m_cfg.NumDsss == 32)
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.NumDss = 5;

        if (m_init.m_cfg.LenSym == 16)
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.LenSym = 0;
        else
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.LenSym = 1;
        m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.len_prm = m_init.m_cfg.SymNumPreambleBase;

        if (m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.Mfsk == 4)
        {
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.FSK_Setting[0] = 1.5;
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.FSK_Setting[1] = -1.5;
        }
        else
        {
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.FSK_Setting[0] = 0.5;
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.FSK_Setting[1] = -0.5;
        }
        if (CONF_cfg.mode_phy2 == 12) {
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.len_FreqComp = 1;
            mem_copy(m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.FreqComp, FreqComp_1, m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.len_FreqComp);
            m_cfg.phy_top_cfg.tx_us_flt_cfg.osr_cfg = 16;
            m_cfg.phy_top_cfg.rx_ds_flt_cfg.mode = 4;
        }
        else if (CONF_cfg.mode_phy2 == 11)
        {
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.len_FreqComp = 2;
            mem_copy(m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.FreqComp, FreqComp_2, m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.len_FreqComp);
            m_cfg.phy_top_cfg.tx_us_flt_cfg.osr_cfg = 32;
            m_cfg.phy_top_cfg.rx_ds_flt_cfg.mode = 5;
        }
        else if (CONF_cfg.mode_phy2 == 10)
        {
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.len_FreqComp = 4;
            mem_copy(m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.FreqComp, FreqComp_4, m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.len_FreqComp);
            m_cfg.phy_top_cfg.tx_us_flt_cfg.osr_cfg = 64;
            m_cfg.phy_top_cfg.rx_ds_flt_cfg.mode = 6;
        }
        else if (CONF_cfg.mode_phy2 == 9)
        {
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.len_FreqComp = 8;
            mem_copy(m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.FreqComp, FreqComp_8, m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.len_FreqComp);
            m_cfg.phy_top_cfg.tx_us_flt_cfg.osr_cfg = 128;
            m_cfg.phy_top_cfg.rx_ds_flt_cfg.mode = 7;
        }
        else if (CONF_cfg.mode_phy2 == 1 || CONF_cfg.mode_phy2 == 2 || CONF_cfg.mode_phy2 == 3 || CONF_cfg.mode_phy2 == 4 || CONF_cfg.mode_phy2 == 5 || CONF_cfg.mode_phy2 == 6 || CONF_cfg.mode_phy2 == 7 || CONF_cfg.mode_phy2 == 8)
        {
            m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.len_FreqComp = 16;
            mem_copy(m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.FreqComp, FreqComp_16, m_cfg.phy_top_cfg.rx_data.demod_phy2.sync_cfg.len_FreqComp);
            m_cfg.phy_top_cfg.tx_us_flt_cfg.osr_cfg = 256;
            m_cfg.phy_top_cfg.rx_ds_flt_cfg.mode = 8;
        }
    }
    else
    {
        m_cfg.phy_top_cfg.tx_us_flt_cfg.osr = 4;
        m_cfg.phy_top_cfg.rx_data.phy_ver = 1;
    }



    m_cfg.phy_top_cfg.tx_dac_if_cfg.module_config = default_module_config;
    m_cfg.phy_top_cfg.tx_dac_if_cfg.io_config     = default_IO_config;

    m_cfg.phy_top_cfg.rx_data.module_config.fx_mode = (g_fix_or_float == 0.0) ? floatpoint:fixpoint;


    m_phy_top.configure(m_cfg.phy_top_cfg);
    m_mac_top.configure(m_cfg.mac_top_cfg);
    */
}

void Cls_module_bb_top::init(char *module_name)
{
    m_io_config = default_IO_config;
    m_power_flag = 0;
    //char fileName[300] = {0};


    m_phy_top.init(module_name);
    /*
    strcpy(fileName, module_name);
    configure(tx_rx_flag);
    m_phy_top.init(strcat(fileName, "_phy"));
    m_mac_top.init(strcat(fileName, "_mac"));
    strcpy(fileName, module_name);m_IO.p_reset_n                .init(strcat(fileName, "_p_reset_n                "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_enable                 .init(strcat(fileName, "_i_enable                 "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_dig_iq_rx_data_i       .init(strcat(fileName, "_i_dig_iq_rx_data_i       "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_dig_iq_rx_data_q       .init(strcat(fileName, "_i_dig_iq_rx_data_q       "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_dig_iq_rx_data_v       .init(strcat(fileName, "_i_dig_iq_rx_data_v       "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_dig_iq_rx_data_active  .init(strcat(fileName, "_i_dig_iq_rx_data_active  "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_power_down_prepare     .init(strcat(fileName, "_i_power_down_prepare     "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.o_dig_iq_tx_data_i       .init(strcat(fileName, "_o_dig_iq_tx_data_i       "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.o_dig_iq_tx_data_q       .init(strcat(fileName, "_o_dig_iq_tx_data_q       "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.o_dig_iq_tx_data_v       .init(strcat(fileName, "_o_dig_iq_tx_data_v       "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.o_dig_iq_tx_data_active  .init(strcat(fileName, "_o_dig_iq_tx_data_active  "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.o_interrupt              .init(strcat(fileName, "_o_interrupt              "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.o_interrupt_type         .init(strcat(fileName, "_o_interrupt_type         "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.o_idle                   .init(strcat(fileName, "_o_idle                   "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);

    reset();
    */
}

void Cls_module_bb_top::reset()
{
    //m_mac_top.reset();
    m_phy_top.reset();
    m_preset = false;
}

void Cls_module_bb_top::go()
{
    // Interface transcation
    // User Detection
    m_phy_top.m_IO.p_reset_n            =   m_preset;
    m_phy_top.m_IO.i_dig_iq_rx_1_i      =   m_IO.i_dig_iq_rx_1_i;
    m_phy_top.m_IO.i_dig_iq_rx_1_q      =   m_IO.i_dig_iq_rx_1_q;
    m_phy_top.m_IO.i_dig_iq_rx_2_i      =   m_IO.i_dig_iq_rx_2_i;
    m_phy_top.m_IO.i_dig_iq_rx_2_q      =   m_IO.i_dig_iq_rx_2_q;
    m_phy_top.m_IO.i_dig_iq_rx_3_i      =   m_IO.i_dig_iq_rx_3_i;
    m_phy_top.m_IO.i_dig_iq_rx_3_q      =   m_IO.i_dig_iq_rx_3_q;
    m_phy_top.m_IO.i_dig_iq_rx_4_i      =   m_IO.i_dig_iq_rx_4_i;
    m_phy_top.m_IO.i_dig_iq_rx_4_q      =   m_IO.i_dig_iq_rx_4_q;
    m_phy_top.m_IO.i_dig_iq_rx_5_i      =   m_IO.i_dig_iq_rx_5_i;
    m_phy_top.m_IO.i_dig_iq_rx_5_q      =   m_IO.i_dig_iq_rx_5_q;
    m_phy_top.m_IO.i_dig_iq_rx_6_i      =   m_IO.i_dig_iq_rx_6_i;
    m_phy_top.m_IO.i_dig_iq_rx_6_q      =   m_IO.i_dig_iq_rx_6_q;
    m_phy_top.m_IO.i_dig_iq_rx_7_i      =   m_IO.i_dig_iq_rx_7_i;
    m_phy_top.m_IO.i_dig_iq_rx_7_q      =   m_IO.i_dig_iq_rx_7_q;
    m_phy_top.m_IO.i_dig_iq_rx_8_i      =   m_IO.i_dig_iq_rx_8_i;
    m_phy_top.m_IO.i_dig_iq_rx_8_q      =   m_IO.i_dig_iq_rx_8_q;
    m_phy_top.m_IO.i_dig_iq_rx_v        =   m_IO.i_dig_iq_rx_v;

    // Interference Suppression


    m_phy_top.go();
    //m_
    m_preset = true;
    /*
    // Interface transcation
    if ((bool)m_phy_top.m_IO.o_power_down_prepare == true)
        m_phy_top.m_IO.p_reset_n = false;
    else
        m_phy_top.m_IO.p_reset_n = true;
    //m_phy_top.m_IO.p_reset_n            =   m_IO.p_reset_n                      ;
    m_phy_top.m_IO.i_enable             =   m_IO.i_enable                       ;
    m_phy_top.m_IO.i_dig_iq_rx_data_i   =   m_IO.i_dig_iq_rx_data_i             ;
    m_phy_top.m_IO.i_dig_iq_rx_data_q   =   m_IO.i_dig_iq_rx_data_q             ;
    m_phy_top.m_IO.i_dig_iq_rx_data_v   =   m_IO.i_dig_iq_rx_data_v             ;
    m_phy_top.m_IO.i_tx_symbol_i        =   m_mac_top.m_IO.o_tx_symbol_i        ;
    m_phy_top.m_IO.i_tx_symbol_q        =   m_mac_top.m_IO.o_tx_symbol_q        ;
    m_phy_top.m_IO.i_tx_symbol_v        =   m_mac_top.m_IO.o_tx_symbol_v        ;
    m_phy_top.m_IO.i_tx_symbol_f        =   m_mac_top.m_IO.o_tx_symbol_f        ;
    m_phy_top.m_IO.i_tx_symbol_active   =   m_mac_top.m_IO.o_tx_symbol_active   ;
    m_phy_top.m_IO.i_rx_beacon_active   =   m_mac_top.m_IO.o_rx_bcn_enable      ;
    m_phy_top.m_IO.i_rx_data_active     =   m_mac_top.m_IO.o_rx_data_enable     ;

    m_mac_top.m_IO.p_reset_n            =   m_IO.p_reset_n                      ;
    m_mac_top.m_IO.i_enable             =   m_IO.i_enable                       ;
    m_mac_top.m_IO.i_cnt_hold           =   m_IO.i_power_down_prepare           ;
    m_mac_top.m_IO.o_master_cnt         =   m_mac_top.m_IO.o_master_cnt         ;
    m_mac_top.m_IO.o_slave_cnt          =   m_mac_top.m_IO.o_slave_cnt          ;
    m_mac_top.m_IO.i_phy_out_v          =   m_phy_top.m_IO.o_dig_iq_tx_data_v   ;

    m_IO.o_dig_iq_tx_data_i             =   m_phy_top.m_IO.o_dig_iq_tx_data_i   ;
    m_IO.o_dig_iq_tx_data_q             =   m_phy_top.m_IO.o_dig_iq_tx_data_q   ;
    m_IO.o_dig_iq_tx_data_v             =   m_phy_top.m_IO.o_dig_iq_tx_data_v   ;
    m_IO.o_dig_iq_tx_data_active        =   m_mac_top.m_IO.o_tx_symbol_active   ;
    m_IO.o_interrupt                    =   m_mac_top.m_IO.o_interrupt          ;
    m_IO.o_interrupt_type               =   m_mac_top.m_IO.o_intrrpt_type       ;
    m_IO.i_power_down_prepare           =   m_phy_top.m_IO.o_power_down_prepare ;
    m_IO.o_idle                         =   (!m_mac_top.m_IO.o_tx_symbol_active)&&m_phy_top.m_IO.o_rx_data_done&&m_phy_top.m_IO.o_rx_beacon_done ;

    m_phy_top.go();
    m_mac_top.go();
    //m_IO.p_reset_n = true;
    m_IO.i_enable = true;
    m_IO.o_rx_data_done                 =   m_phy_top.m_IO.o_rx_data_done;
    m_IO.o_rx_result                    =   m_phy_top.m_IO.o_rx_result;
    */
}
#endif


