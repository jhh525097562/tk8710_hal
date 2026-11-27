/****************************************************************************
**
**  Copyright  (c)  2023~2026,  Amlogic, Inc.
**          All Rights Reserved.
**
**  File         : Cls_module_phy_top.h
**  Created By   : Darren.Ren
**  Created On   : 18/10/2023
**
**
**  History     :
**  Programmer      Date        Rev     Description
**  --------------- ----------  ------- ------------------------------
**  Darren.Ren      18/10/2023  V1.0    Initial
**
****************************************************************************/
#pragma once
#include "common_type.h"
#include "Lib_io.h"

#include "Cls_module_rx_ud.h"
#include "Cls_module_rx_is.h"
#include "Cls_module_rx_bf.h"
#include "Cls_module_tx_bf.h"

typedef struct InterF_phy_top
{
    Cls_io<bool>    p_reset_n           ;
    Cls_io<bool>    i_enable            ;
    Cls_io<double>  i_dig_iq_rx_1_i     ;
    Cls_io<double>  i_dig_iq_rx_1_q     ;
    Cls_io<double>  i_dig_iq_rx_2_i     ;
    Cls_io<double>  i_dig_iq_rx_2_q     ;
    Cls_io<double>  i_dig_iq_rx_3_i     ;
    Cls_io<double>  i_dig_iq_rx_3_q     ;
    Cls_io<double>  i_dig_iq_rx_4_i     ;
    Cls_io<double>  i_dig_iq_rx_4_q     ;
    Cls_io<double>  i_dig_iq_rx_5_i     ;
    Cls_io<double>  i_dig_iq_rx_5_q     ;
    Cls_io<double>  i_dig_iq_rx_6_i     ;
    Cls_io<double>  i_dig_iq_rx_6_q     ;
    Cls_io<double>  i_dig_iq_rx_7_i     ;
    Cls_io<double>  i_dig_iq_rx_7_q     ;
    Cls_io<double>  i_dig_iq_rx_8_i     ;
    Cls_io<double>  i_dig_iq_rx_8_q     ;
    Cls_io<bool>    i_dig_iq_rx_v       ;
    Cls_io<int>     i_fft_len           ;
    Cls_io<int>     i_ant_num           ;
    Cls_io<double>  o_dig_iq_tx_1_i     ;
    Cls_io<double>  o_dig_iq_tx_1_q     ;
    Cls_io<bool>    o_dig_iq_tx_data_v  ;
    Cls_io<double>  i_tx_symbol_i       ;
    Cls_io<double>  i_tx_symbol_q       ;
    Cls_io<bool>    i_tx_symbol_v       ;
    Cls_io<bool>    i_tx_symbol_f       ;
    Cls_io<int>     i_tx_symbol_active  ;
    Cls_io<bool>    i_rx_beacon_active  ;
    Cls_io<bool>    i_tx_up_flag        ;
    Cls_io<bool>    o_rx_beacon_done    ;
    Cls_io<bool>    i_rx_data_active    ;
    Cls_io<bool>    o_rx_data_done      ;
    Cls_io<bool>    o_power_down_prepare;
    //rx_result       o_rx_result         ;
}InterF_phy_top;

typedef struct phy_top_config
{
    RX_UD_Cfg           ud_cfg;
    RX_IS_Cfg           is_cfg;
    TX_RX               tx_rx_flag;// = OTHER;
}phy_top_config;


class Module_phy_top
{
    public:
        Module_phy_top(){
            m_io_config.io_fileOut_flag = 0;
            //m_init = 1;
        }
        ~Module_phy_top()
        {
        }

        void init(char *module_name);
        void configure(phy_top_config cfg);
        void reset();
        void go();

        InterF_phy_top          m_IO;
        Cls_module_ud           m_rx_ud;
        Cls_module_rx_is        m_rx_is;
        Cls_module_rx_bf        m_rx_bf;
        Cls_module_tx_bf        m_tx_bf;
        /*
        Cls_module_RX_ADC       m_rx_adc_if;
        Cls_module_RX_FE        m_rx_fe;
        Cls_module_RX_DS_FLT    m_rx_ds_flt;
        Cls_module_RX_AGC       m_rx_agc;
        Cls_module_RX_BEACON    m_rx_beacon;
        Cls_module_RX_DATA      m_rx_data;
        Cls_module_TX_US        m_tx_us_flt;
        Cls_module_TX_DAC       m_tx_dac_if;
        Cls_module_RX_IQIM      m_rx_iqim;
        */
        //Cls_module_RX_DEMOD     m_rx_demod;
        //bool                    m_init;
private:
        phy_top_config          m_cfg;
        IO_config               m_io_config;
        bool                    m_ts_us_st;// = 0;
};

void Module_phy_top::configure(phy_top_config cfg)
{
    m_cfg = cfg;
    m_rx_ud.m_IO.i_fft_len      =   m_IO.i_fft_len;
    m_rx_ud.m_IO.i_ant_num      =   m_IO.i_ant_num;
    m_rx_ud.configure(m_cfg.ud_cfg);

    m_rx_is.configure(m_cfg.is_cfg);
    /*
    m_rx_fe.config(m_cfg.rx_fe_cfg);
    m_rx_adc_if.config(m_cfg.rx_adc_if_cfg);
    m_rx_ds_flt.config(m_cfg.rx_ds_flt_cfg);
    m_rx_agc.config(m_cfg.rx_agc_cfg);
    m_rx_beacon.config(m_cfg.rx_beacon_cfg);
    m_rx_data.config(m_cfg.rx_data);
    m_tx_us_flt.config(m_cfg.tx_us_flt_cfg);
    m_tx_dac_if.config(m_cfg.tx_dac_if_cfg);
    m_rx_iqim.config(m_cfg.rx_iqim_cfg);
    */

}

void Module_phy_top::init(char *module_name)
{
    m_io_config = default_IO_config;
    m_rx_ud.init(module_name);

    m_rx_is.init(module_name);
    //char fileName[300];
    /*
    strcpy(fileName, module_name); m_rx_adc_if.init(strcat(fileName, "_rx_adc_if\\"));
    strcpy(fileName, module_name); m_rx_fe.init(strcat(fileName, "_rx_fe\\"));
    strcpy(fileName, module_name); m_rx_ds_flt.init(strcat(fileName, "_rx_ds_flt\\"));
    strcpy(fileName, module_name); m_rx_agc.init(strcat(fileName, "_rx_agc\\"));
    strcpy(fileName, module_name); m_rx_beacon.init(strcat(fileName, "_rx_beacon\\"));
    strcpy(fileName, module_name); m_rx_data.init(  strcat(fileName, "_rx_data\\"  ));
    strcpy(fileName, module_name); m_tx_us_flt.init(strcat(fileName, "_tx_us_flt\\"));
    strcpy(fileName, module_name); m_tx_dac_if.init(strcat(fileName, "_tx_dac_if\\"));
    strcpy(fileName, module_name); m_rx_iqim.init(strcat(fileName, "_rx_iqim\\"));

    strcpy(fileName, module_name);m_IO.p_reset_n         .init(strcat(fileName, "_p_reset_n           "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_enable          .init(strcat(fileName, "_i_enable            "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_dig_iq_rx_data_i.init(strcat(fileName, "_i_dig_iq_rx_data_i  "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_dig_iq_rx_data_q.init(strcat(fileName, "_i_dig_iq_rx_data_q  "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_dig_iq_rx_data_v.init(strcat(fileName, "_i_dig_iq_rx_data_v  "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.o_dig_iq_tx_data_i.init(strcat(fileName, "_o_dig_iq_tx_data_i  "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.o_dig_iq_tx_data_q.init(strcat(fileName, "_o_dig_iq_tx_data_q  "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.o_dig_iq_tx_data_v.init(strcat(fileName, "_o_dig_iq_tx_data_v  "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_tx_symbol_i     .init(strcat(fileName, "_i_tx_symbol_i       "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_tx_symbol_q     .init(strcat(fileName, "_i_tx_symbol_q       "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_tx_symbol_v     .init(strcat(fileName, "_i_tx_symbol_v       "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_tx_symbol_active.init(strcat(fileName, "_i_tx_symbol_active  "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_rx_beacon_active.init(strcat(fileName, "_i_rx_beacon_active  "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.o_rx_beacon_done  .init(strcat(fileName, "_o_rx_beacon_done    "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.i_rx_data_active  .init(strcat(fileName, "_i_rx_data_active    "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    strcpy(fileName, module_name);m_IO.o_rx_data_done    .init(strcat(fileName, "_o_rx_data_done      "),m_io_config.io_fileOut_flag,m_io_config.io_max_delay);
    */

    //configure(m_cfg);

    m_ts_us_st = false;
    reset();
}

void Module_phy_top::reset()
{
    m_rx_is.reset();
    /*
    m_rx_adc_if.reset();
    m_rx_fe.reset();
    m_rx_ds_flt.reset();
    m_rx_agc.reset();
    m_rx_beacon.reset();
    m_rx_data.reset();
    m_tx_us_flt.reset();
    m_tx_dac_if.reset();
    m_IO.p_reset_n  = 0;
    m_IO.i_enable   = 1;
    */

    //m_init = 1;
}

void Module_phy_top::go()
{
    m_rx_ud.m_IO.p_reset_n      =   m_IO.p_reset_n;
    m_rx_ud.m_IO.i_enable       =   m_IO.i_dig_iq_rx_v;
    m_rx_ud.m_IO.i_in_1_I       =   m_IO.i_dig_iq_rx_1_i;
    m_rx_ud.m_IO.i_in_1_Q       =   m_IO.i_dig_iq_rx_1_q;
    m_rx_ud.m_IO.i_in_2_I       =   m_IO.i_dig_iq_rx_2_i;
    m_rx_ud.m_IO.i_in_2_Q       =   m_IO.i_dig_iq_rx_2_q;
    m_rx_ud.m_IO.i_in_3_I       =   m_IO.i_dig_iq_rx_3_i;
    m_rx_ud.m_IO.i_in_3_Q       =   m_IO.i_dig_iq_rx_3_q;
    m_rx_ud.m_IO.i_in_4_I       =   m_IO.i_dig_iq_rx_4_i;
    m_rx_ud.m_IO.i_in_4_Q       =   m_IO.i_dig_iq_rx_4_q;
    m_rx_ud.m_IO.i_in_5_I       =   m_IO.i_dig_iq_rx_5_i;
    m_rx_ud.m_IO.i_in_5_Q       =   m_IO.i_dig_iq_rx_5_q;
    m_rx_ud.m_IO.i_in_6_I       =   m_IO.i_dig_iq_rx_6_i;
    m_rx_ud.m_IO.i_in_6_Q       =   m_IO.i_dig_iq_rx_6_q;
    m_rx_ud.m_IO.i_in_7_I       =   m_IO.i_dig_iq_rx_7_i;
    m_rx_ud.m_IO.i_in_7_Q       =   m_IO.i_dig_iq_rx_7_q;
    m_rx_ud.m_IO.i_in_8_I       =   m_IO.i_dig_iq_rx_8_i;
    m_rx_ud.m_IO.i_in_8_Q       =   m_IO.i_dig_iq_rx_8_q;
    m_rx_ud.m_IO.i_in_en        =   m_IO.i_dig_iq_rx_v;

    m_rx_is.m_IO.p_reset_n      =   m_IO.p_reset_n;
    m_rx_is.m_IO.i_enable       =   m_IO.i_dig_iq_rx_v;
    m_rx_is.m_IO.i_in_en        =   m_rx_ud.m_IO.o_d_v || m_rx_ud.m_IO.o_u_v;
    m_rx_is.m_IO.i_in_1_I       =   m_rx_ud.m_IO.o_out_1_I;
    m_rx_is.m_IO.i_in_1_Q       =   m_rx_ud.m_IO.o_out_1_Q;
    m_rx_is.m_IO.i_in_2_I       =   m_rx_ud.m_IO.o_out_2_I;
    m_rx_is.m_IO.i_in_2_Q       =   m_rx_ud.m_IO.o_out_2_Q;
    m_rx_is.m_IO.i_in_3_I       =   m_rx_ud.m_IO.o_out_3_I;
    m_rx_is.m_IO.i_in_3_Q       =   m_rx_ud.m_IO.o_out_3_Q;
    m_rx_is.m_IO.i_in_4_I       =   m_rx_ud.m_IO.o_out_4_I;
    m_rx_is.m_IO.i_in_4_Q       =   m_rx_ud.m_IO.o_out_4_Q;
    m_rx_is.m_IO.i_in_5_I       =   m_rx_ud.m_IO.o_out_5_I;
    m_rx_is.m_IO.i_in_5_Q       =   m_rx_ud.m_IO.o_out_5_Q;
    m_rx_is.m_IO.i_in_6_I       =   m_rx_ud.m_IO.o_out_6_I;
    m_rx_is.m_IO.i_in_6_Q       =   m_rx_ud.m_IO.o_out_6_Q;
    m_rx_is.m_IO.i_in_7_I       =   m_rx_ud.m_IO.o_out_7_I;
    m_rx_is.m_IO.i_in_7_Q       =   m_rx_ud.m_IO.o_out_7_Q;
    m_rx_is.m_IO.i_in_8_I       =   m_rx_ud.m_IO.o_out_8_I;
    m_rx_is.m_IO.i_in_8_Q       =   m_rx_ud.m_IO.o_out_8_Q;
    m_rx_is.m_IO.i_d_v          =   m_rx_ud.m_IO.o_d_v;
    m_rx_is.m_IO.i_u_v          =   m_rx_ud.m_IO.o_u_v;
    m_rx_is.m_IO.i_ah_1_I       =   m_rx_ud.m_IO.o_ah_1_I;
    m_rx_is.m_IO.i_ah_1_Q       =   m_rx_ud.m_IO.o_ah_1_Q;
    m_rx_is.m_IO.i_ah_2_I       =   m_rx_ud.m_IO.o_ah_2_I;
    m_rx_is.m_IO.i_ah_2_Q       =   m_rx_ud.m_IO.o_ah_2_Q;
    m_rx_is.m_IO.i_ah_3_I       =   m_rx_ud.m_IO.o_ah_3_I;
    m_rx_is.m_IO.i_ah_3_Q       =   m_rx_ud.m_IO.o_ah_3_Q;
    m_rx_is.m_IO.i_ah_4_I       =   m_rx_ud.m_IO.o_ah_4_I;
    m_rx_is.m_IO.i_ah_4_Q       =   m_rx_ud.m_IO.o_ah_4_Q;
    m_rx_is.m_IO.i_ah_5_I       =   m_rx_ud.m_IO.o_ah_5_I;
    m_rx_is.m_IO.i_ah_5_Q       =   m_rx_ud.m_IO.o_ah_5_Q;
    m_rx_is.m_IO.i_ah_6_I       =   m_rx_ud.m_IO.o_ah_6_I;
    m_rx_is.m_IO.i_ah_6_Q       =   m_rx_ud.m_IO.o_ah_6_Q;
    m_rx_is.m_IO.i_ah_7_I       =   m_rx_ud.m_IO.o_ah_7_I;
    m_rx_is.m_IO.i_ah_7_Q       =   m_rx_ud.m_IO.o_ah_7_Q;
    m_rx_is.m_IO.i_ah_8_I       =   m_rx_ud.m_IO.o_ah_8_I;
    m_rx_is.m_IO.i_ah_8_Q       =   m_rx_ud.m_IO.o_ah_8_Q;
    m_rx_is.m_IO.i_freq         =   m_rx_ud.m_IO.o_freq;

    m_rx_ud.go();
    m_rx_is.go();
    //m_rx_bf.go();
    m_tx_bf.go();
    /*
    m_rx_adc_if.m_IO.p_reset_n  =   m_IO.p_reset_n;
    m_rx_adc_if.m_IO.i_enable   =   m_IO.i_rx_beacon_active||m_IO.i_rx_data_active;
    m_rx_adc_if.m_IO.i_in_real  =   m_IO.i_dig_iq_rx_data_i;
    m_rx_adc_if.m_IO.i_in_imag  =   m_IO.i_dig_iq_rx_data_q;
    m_rx_adc_if.m_IO.i_in_en    =   m_IO.i_rx_beacon_active || m_IO.i_rx_data_active;

    m_rx_iqim.m_IO.p_reset_n    =   m_IO.p_reset_n;
    m_rx_iqim.m_IO.i_enable     =   m_IO.i_enable;
    m_rx_iqim.m_IO.i_b          =   1;
    m_rx_iqim.m_IO.i_a          =   0;
    m_rx_iqim.m_IO.i_in_real    =   m_rx_adc_if.m_IO.o_out_real;
    m_rx_iqim.m_IO.i_in_imag    =   m_rx_adc_if.m_IO.o_out_imag;
    m_rx_iqim.m_IO.i_in_en      =   m_rx_adc_if.m_IO.o_out_en;

    m_rx_fe.m_IO.p_reset_n      =   m_IO.p_reset_n;
    m_rx_fe.m_IO.i_enable       =   m_IO.i_enable;
    m_rx_fe.m_IO.i_in_real      =   m_rx_iqim.m_IO.i_in_real;
    m_rx_fe.m_IO.i_in_imag      =   m_rx_iqim.m_IO.i_in_imag;
    m_rx_fe.m_IO.i_in_en        =   m_rx_iqim.m_IO.i_in_en;

    m_rx_ds_flt.m_IO.p_reset_n  =   m_IO.p_reset_n;
    m_rx_ds_flt.m_IO.i_enable   =   m_IO.i_enable;
    m_rx_ds_flt.m_IO.i_in_real  =   m_rx_fe.m_IO.o_out_real;
    m_rx_ds_flt.m_IO.i_in_imag  =   m_rx_fe.m_IO.o_out_imag;
    m_rx_ds_flt.m_IO.i_in_en    =   m_rx_fe.m_IO.o_out_en;

    m_rx_agc.m_IO.p_reset_n     =   m_IO.p_reset_n;
    m_rx_agc.m_IO.i_enable      =   m_IO.i_enable;
    m_rx_agc.m_IO.i_in_real     =   m_rx_ds_flt.m_IO.o_out_real;
    m_rx_agc.m_IO.i_in_imag     =   m_rx_ds_flt.m_IO.o_out_imag;
    m_rx_agc.m_IO.i_in_en       =   m_rx_ds_flt.m_IO.o_out_en;

    m_rx_beacon.m_IO.p_reset_n  =   m_IO.p_reset_n;
    m_rx_beacon.m_IO.i_enable   =   m_IO.i_enable;
    m_rx_beacon.m_IO.i_in_real  =   m_rx_agc.m_IO.o_out_real;
    m_rx_beacon.m_IO.i_in_imag  =   m_rx_agc.m_IO.o_out_imag;
    m_rx_beacon.m_IO.i_in_en = 0;// m_rx_agc.m_IO.o_out_en;

    m_rx_data.m_IO.p_reset_n    =   m_IO.p_reset_n;
    m_rx_data.m_IO.i_enable     =   m_IO.i_enable;
    m_rx_data.m_IO.i_in_real    =   m_rx_agc.m_IO.o_out_real;
    m_rx_data.m_IO.i_in_imag    =   m_rx_agc.m_IO.o_out_imag;
    m_rx_data.m_IO.i_in_en      =   m_rx_agc.m_IO.o_out_en;
    //m_rx_data.m_IO.i_tx_reset   =   m_tx_us_flt.m_IO.o_finished;
    m_rx_data.m_IO.i_data_active=   m_IO.i_rx_data_active;

    m_tx_us_flt.m_IO.p_reset_n  =   m_IO.p_reset_n;
    m_tx_us_flt.m_IO.i_enable   =   m_IO.i_enable;
    m_tx_us_flt.m_IO.i_in_real  =   m_IO.i_tx_symbol_i;
    m_tx_us_flt.m_IO.i_in_imag  =   m_IO.i_tx_symbol_q;
    m_tx_us_flt.m_IO.i_in_en    =   m_IO.i_tx_symbol_v;
    m_tx_us_flt.m_IO.i_finish   =   m_IO.i_tx_symbol_f;
    m_tx_us_flt.m_IO.i_up_flag  =   m_IO.i_tx_symbol_active;

    m_tx_dac_if.m_IO.p_reset_n  =   m_IO.p_reset_n;
    m_tx_dac_if.m_IO.i_enable   =   m_IO.i_enable;
    m_tx_dac_if.m_IO.i_in_real  =   m_tx_us_flt.m_IO.o_out_real;
    m_tx_dac_if.m_IO.i_in_imag  =   m_tx_us_flt.m_IO.o_out_imag;
    m_tx_dac_if.m_IO.i_in_en    =   m_tx_us_flt.m_IO.o_out_en;


    m_IO.o_dig_iq_tx_data_i     =   m_tx_dac_if.m_IO.o_out_real;
    m_IO.o_dig_iq_tx_data_q     =   m_tx_dac_if.m_IO.o_out_imag;
    m_IO.o_dig_iq_tx_data_v     =   m_tx_dac_if.m_IO.o_out_en;

    m_rx_adc_if.go();
    m_rx_iqim.go();
    m_rx_fe.go();
    m_rx_ds_flt.go();
    m_rx_agc.go();
    m_rx_beacon.go();
    m_rx_data.go();
    m_tx_us_flt.go();
    m_tx_dac_if.go();
    m_IO.o_rx_data_done = m_rx_data.m_IO.o_tx_reset;
    m_tx_us_flt.m_IO.p_reset_n = m_rx_data.m_IO.o_tx_reset;
    m_IO.o_rx_result = m_rx_data.m_IO.o_rx_result;
    m_IO.o_power_down_prepare = m_rx_data.m_IO.o_tx_reset;
    */
}
