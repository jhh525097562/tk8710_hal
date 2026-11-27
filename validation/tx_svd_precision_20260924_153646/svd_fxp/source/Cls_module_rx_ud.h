/****************************************************************************
**
**  Copyright  (c)  2023~2026,  Amlogic, Inc.
**          All Rights Reserved.
**
**  File         : Cls_rx_module_ud.h
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
#include "Lib_io.h"
#include "common_type.h"
#include "Cls_ud.h"

typedef struct InterF_RX_UD
{
    Cls_io<int     >        p_reset_n;
    Cls_io<int     >        i_enable;
    Cls_io<int     >        i_in_en;
    Cls_io<int     >        i_fft_len;
    Cls_io<int     >        i_ant_num;
    Cls_io<double>          o_out_1_I;
    Cls_io<double>          o_out_1_Q;
    Cls_io<double>          o_out_2_I;
    Cls_io<double>          o_out_2_Q;
    Cls_io<double>          o_out_3_I;
    Cls_io<double>          o_out_3_Q;
    Cls_io<double>          o_out_4_I;
    Cls_io<double>          o_out_4_Q;
    Cls_io<double>          o_out_5_I;
    Cls_io<double>          o_out_5_Q;
    Cls_io<double>          o_out_6_I;
    Cls_io<double>          o_out_6_Q;
    Cls_io<double>          o_out_7_I;
    Cls_io<double>          o_out_7_Q;
    Cls_io<double>          o_out_8_I;
    Cls_io<double>          o_out_8_Q;
    Cls_io<double  >        i_in_1_I;
    Cls_io<double  >        i_in_1_Q;
    Cls_io<double  >        i_in_2_I;
    Cls_io<double  >        i_in_2_Q;
    Cls_io<double  >        i_in_3_I;
    Cls_io<double  >        i_in_3_Q;
    Cls_io<double  >        i_in_4_I;
    Cls_io<double  >        i_in_4_Q;
    Cls_io<double  >        i_in_5_I;
    Cls_io<double  >        i_in_5_Q;
    Cls_io<double  >        i_in_6_I;
    Cls_io<double  >        i_in_6_Q;
    Cls_io<double  >        i_in_7_I;
    Cls_io<double  >        i_in_7_Q;
    Cls_io<double  >        i_in_8_I;
    Cls_io<double  >        i_in_8_Q;
    Cls_io<int     >        o_out_en;
    Cls_io<bool>            o_d_v;
    Cls_io<bool>            o_u_v;
    Cls_io<double  >        o_freq;
    Cls_io<double>          o_ah_1_I;
    Cls_io<double>          o_ah_1_Q;
    Cls_io<double>          o_ah_2_I;
    Cls_io<double>          o_ah_2_Q;
    Cls_io<double>          o_ah_3_I;
    Cls_io<double>          o_ah_3_Q;
    Cls_io<double>          o_ah_4_I;
    Cls_io<double>          o_ah_4_Q;
    Cls_io<double>          o_ah_5_I;
    Cls_io<double>          o_ah_5_Q;
    Cls_io<double>          o_ah_6_I;
    Cls_io<double>          o_ah_6_Q;
    Cls_io<double>          o_ah_7_I;
    Cls_io<double>          o_ah_7_Q;
    Cls_io<double>          o_ah_8_I;
    Cls_io<double>          o_ah_8_Q;
}InterF_RX_UD;

typedef struct RX_UD_Cfg
{
    IO_config       io_config;
    Module_config   module_config;
    UD_CFG          cfg;
}RX_UD_Cfg;

class Cls_module_ud
{
public:
    Cls_module_ud() {
    }
    ~Cls_module_ud() {
    }

    void                        init(char* module_name);
    void                        reset();
    void                        go();
    void                        configure(RX_UD_Cfg cfg);
    InterF_RX_UD                m_IO;
private:
    Cls_ud<double>              ud_fl;
    Cls_ud<fxp>                 ud_fx;
    Complex<double>             m_out_Fl;
    Complex<double>             m_out_Fx;
    bool                        m_out_en;
    RX_UD_Cfg                   m_cfg;
};

void Cls_module_ud::configure(RX_UD_Cfg cfg)
{
    m_cfg = cfg;
    if(m_cfg.module_config.fx_mode == floatpoint) {
        ud_fl.m_fft_len = m_IO.i_fft_len;
        ud_fl.m_ant_num = m_IO.i_ant_num;
        ud_fl.configure(m_cfg.cfg);
    } else if(m_cfg.module_config.fx_mode == fixpoint) {
        ud_fx.m_fft_len = m_IO.i_fft_len;
        ud_fx.m_ant_num = m_IO.i_ant_num;
        ud_fx.configure(m_cfg.cfg);
    }
}

void Cls_module_ud::init(char* module_name)
{
    char folder_name[300];
    strcpy(folder_name, module_name);
    if(m_cfg.module_config.fx_mode == floatpoint) {
        strcat(folder_name, "_fl/");
        ud_fl.init(module_name);
    }  else if(m_cfg.module_config.fx_mode == fixpoint) {
        ud_fx.m_fft_len = m_IO.i_fft_len;
        ud_fx.m_ant_num = m_IO.i_ant_num;
        strcat(folder_name, "_fx/");
        ud_fx.init(module_name);
    }
    reset();
}

void Cls_module_ud::reset()
{
    if(m_cfg.module_config.fx_mode == floatpoint)
        ud_fl.reset();
    else if(m_cfg.module_config.fx_mode == fixpoint)
        ud_fx.reset();

    m_out_Fl = 0.0;
    m_out_Fx = 0.0;
    m_out_en = 0;
}


void Cls_module_ud::go()
{
    ////////////////////////////////////////////////////////////////////////
    // Module Reset & Enable
    ////////////////////////////////////////////////////////////////////////
    if (!bool(m_IO.p_reset_n))
    {
        reset();
        m_IO.p_reset_n = true;
        m_IO.o_out_en = false;
        //return;
    }

    if (!bool(m_IO.i_enable))
    {
        return;
    }

    Complex<double> out_temp;

    ////////////////////////////////////////////////////////////////////////
    // Module Control Variable
    ////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////
    // Module Data Process
    ////////////////////////////////////////////////////////////////////////
    if (m_IO.i_in_en)
    {
        switch (m_cfg.module_config.fx_mode)
        {
            case (floatpoint):
                ud_fl.m_in_1.I = m_IO.i_in_1_I;
                ud_fl.m_in_1.Q = m_IO.i_in_1_Q;
                ud_fl.m_in_2.I = m_IO.i_in_2_I;
                ud_fl.m_in_2.Q = m_IO.i_in_2_Q;
                ud_fl.m_in_3.I = m_IO.i_in_3_I;
                ud_fl.m_in_3.Q = m_IO.i_in_3_Q;
                ud_fl.m_in_4.I = m_IO.i_in_4_I;
                ud_fl.m_in_4.Q = m_IO.i_in_4_Q;
                ud_fl.m_in_5.I = m_IO.i_in_5_I;
                ud_fl.m_in_5.Q = m_IO.i_in_5_Q;
                ud_fl.m_in_6.I = m_IO.i_in_6_I;
                ud_fl.m_in_6.Q = m_IO.i_in_6_Q;
                ud_fl.m_in_7.I = m_IO.i_in_7_I;
                ud_fl.m_in_7.Q = m_IO.i_in_7_Q;
                ud_fl.m_in_8.I = m_IO.i_in_8_I;
                ud_fl.m_in_8.Q = m_IO.i_in_8_Q;
                ud_fl.go();
                m_IO.o_out_1_I = ud_fl.m_out_1.I;
                m_IO.o_out_1_Q = ud_fl.m_out_1.Q;
                m_IO.o_out_2_I = ud_fl.m_out_2.I;
                m_IO.o_out_2_Q = ud_fl.m_out_2.Q;
                m_IO.o_out_3_I = ud_fl.m_out_3.I;
                m_IO.o_out_3_Q = ud_fl.m_out_3.Q;
                m_IO.o_out_4_I = ud_fl.m_out_4.I;
                m_IO.o_out_4_Q = ud_fl.m_out_4.Q;
                m_IO.o_out_5_I = ud_fl.m_out_5.I;
                m_IO.o_out_5_Q = ud_fl.m_out_5.Q;
                m_IO.o_out_6_I = ud_fl.m_out_6.I;
                m_IO.o_out_6_Q = ud_fl.m_out_6.Q;
                m_IO.o_out_7_I = ud_fl.m_out_7.I;
                m_IO.o_out_7_Q = ud_fl.m_out_7.Q;
                m_IO.o_out_8_I = ud_fl.m_out_8.I;
                m_IO.o_out_8_Q = ud_fl.m_out_8.Q;
                m_IO.o_ah_1_I = ud_fl.m_ah_o_1.I;
                m_IO.o_ah_1_Q = ud_fl.m_ah_o_1.Q;
                m_IO.o_ah_2_I = ud_fl.m_ah_o_2.I;
                m_IO.o_ah_2_Q = ud_fl.m_ah_o_2.Q;
                m_IO.o_ah_3_I = ud_fl.m_ah_o_3.I;
                m_IO.o_ah_3_Q = ud_fl.m_ah_o_3.Q;
                m_IO.o_ah_4_I = ud_fl.m_ah_o_4.I;
                m_IO.o_ah_4_Q = ud_fl.m_ah_o_4.Q;
                m_IO.o_ah_5_I = ud_fl.m_ah_o_5.I;
                m_IO.o_ah_5_Q = ud_fl.m_ah_o_5.Q;
                m_IO.o_ah_6_I = ud_fl.m_ah_o_6.I;
                m_IO.o_ah_6_Q = ud_fl.m_ah_o_6.Q;
                m_IO.o_ah_7_I = ud_fl.m_ah_o_7.I;
                m_IO.o_ah_7_Q = ud_fl.m_ah_o_7.Q;
                m_IO.o_ah_8_I = ud_fl.m_ah_o_8.I;
                m_IO.o_ah_8_Q = ud_fl.m_ah_o_8.Q;
                m_IO.o_freq = ud_fl.m_freq_o;
                m_IO.o_d_v = ud_fl.m_data_o_v;
                m_IO.o_u_v = ud_fl.m_data_u_o_v;
                break;
            case (fixpoint):
                ud_fx.m_in_1.I = m_IO.i_in_1_I;
                ud_fx.m_in_1.Q = m_IO.i_in_1_Q;
                ud_fx.m_in_2.I = m_IO.i_in_2_I;
                ud_fx.m_in_2.Q = m_IO.i_in_2_Q;
                ud_fx.m_in_3.I = m_IO.i_in_3_I;
                ud_fx.m_in_3.Q = m_IO.i_in_3_Q;
                ud_fx.m_in_4.I = m_IO.i_in_4_I;
                ud_fx.m_in_4.Q = m_IO.i_in_4_Q;
                ud_fx.m_in_5.I = m_IO.i_in_5_I;
                ud_fx.m_in_5.Q = m_IO.i_in_5_Q;
                ud_fx.m_in_6.I = m_IO.i_in_6_I;
                ud_fx.m_in_6.Q = m_IO.i_in_6_Q;
                ud_fx.m_in_7.I = m_IO.i_in_7_I;
                ud_fx.m_in_7.Q = m_IO.i_in_7_Q;
                ud_fx.m_in_8.I = m_IO.i_in_8_I;
                ud_fx.m_in_8.Q = m_IO.i_in_8_Q;
                ud_fx.go();
                m_IO.o_out_1_I = ud_fx.m_out_1.I;
                m_IO.o_out_1_Q = ud_fx.m_out_1.Q;
                m_IO.o_out_2_I = ud_fx.m_out_2.I;
                m_IO.o_out_2_Q = ud_fx.m_out_2.Q;
                m_IO.o_out_3_I = ud_fx.m_out_3.I;
                m_IO.o_out_3_Q = ud_fx.m_out_3.Q;
                m_IO.o_out_4_I = ud_fx.m_out_4.I;
                m_IO.o_out_4_Q = ud_fx.m_out_4.Q;
                m_IO.o_out_5_I = ud_fx.m_out_5.I;
                m_IO.o_out_5_Q = ud_fx.m_out_5.Q;
                m_IO.o_out_6_I = ud_fx.m_out_6.I;
                m_IO.o_out_6_Q = ud_fx.m_out_6.Q;
                m_IO.o_out_7_I = ud_fx.m_out_7.I;
                m_IO.o_out_7_Q = ud_fx.m_out_7.Q;
                m_IO.o_out_8_I = ud_fx.m_out_8.I;
                m_IO.o_out_8_Q = ud_fx.m_out_8.Q;
                m_IO.o_ah_1_I = ud_fx.m_ah_o_1.I;
                m_IO.o_ah_1_Q = ud_fx.m_ah_o_1.Q;
                m_IO.o_ah_2_I = ud_fx.m_ah_o_2.I;
                m_IO.o_ah_2_Q = ud_fx.m_ah_o_2.Q;
                m_IO.o_ah_3_I = ud_fx.m_ah_o_3.I;
                m_IO.o_ah_3_Q = ud_fx.m_ah_o_3.Q;
                m_IO.o_ah_4_I = ud_fx.m_ah_o_4.I;
                m_IO.o_ah_4_Q = ud_fx.m_ah_o_4.Q;
                m_IO.o_ah_5_I = ud_fx.m_ah_o_5.I;
                m_IO.o_ah_5_Q = ud_fx.m_ah_o_5.Q;
                m_IO.o_ah_6_I = ud_fx.m_ah_o_6.I;
                m_IO.o_ah_6_Q = ud_fx.m_ah_o_6.Q;
                m_IO.o_ah_7_I = ud_fx.m_ah_o_7.I;
                m_IO.o_ah_7_Q = ud_fx.m_ah_o_7.Q;
                m_IO.o_ah_8_I = ud_fx.m_ah_o_8.I;
                m_IO.o_ah_8_Q = ud_fx.m_ah_o_8.Q;
                m_IO.o_freq = ud_fx.m_freq_o;
                m_IO.o_d_v = ud_fx.m_data_o_v;
                m_IO.o_u_v = ud_fx.m_data_u_o_v;
                break;
            default:
                std::cout << "base.m_FX_mode error! Out of range of value.\n";
                break;
        }
    }
    else
    {
        m_IO.o_out_en = false;
    }
}









