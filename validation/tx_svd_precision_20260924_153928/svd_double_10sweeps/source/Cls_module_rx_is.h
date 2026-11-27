/****************************************************************************
**
**  Copyright  (c)  2023~2026,  Amlogic, Inc.
**          All Rights Reserved.
**
**  File         : Cls_rx_module_is.h
**  Created By   : Darren.Ren
**  Created On   : 20/10/2023
**
**
**  History     :
**  Programmer      Date        Rev     Description
**  --------------- ----------  ------- ------------------------------
**  Darren.Ren      20/10/2023  V1.0    Initial
**
****************************************************************************/
#pragma once
#include "Lib_io.h"
#include "common_type.h"
#include "Cls_BF_beamf.h"

typedef struct InterF_RX_IS
{
    Cls_io<int     >        p_reset_n;
    Cls_io<int     >        i_enable;
    Cls_io<int     >        i_in_en;
    Cls_io<int     >        i_fft_len;
    Cls_io<int     >        i_ant_num;
    Cls_io<bool>            i_d_v;
    Cls_io<bool>            i_u_v;
    Cls_io<double>          i_in_1_I;
    Cls_io<double>          i_in_1_Q;
    Cls_io<double>          i_in_2_I;
    Cls_io<double>          i_in_2_Q;
    Cls_io<double>          i_in_3_I;
    Cls_io<double>          i_in_3_Q;
    Cls_io<double>          i_in_4_I;
    Cls_io<double>          i_in_4_Q;
    Cls_io<double>          i_in_5_I;
    Cls_io<double>          i_in_5_Q;
    Cls_io<double>          i_in_6_I;
    Cls_io<double>          i_in_6_Q;
    Cls_io<double>          i_in_7_I;
    Cls_io<double>          i_in_7_Q;
    Cls_io<double>          i_in_8_I;
    Cls_io<double>          i_in_8_Q;
    Cls_io<double>          i_freq;
    Cls_io<double>          i_ah_1_I;
    Cls_io<double>          i_ah_1_Q;
    Cls_io<double>          i_ah_2_I;
    Cls_io<double>          i_ah_2_Q;
    Cls_io<double>          i_ah_3_I;
    Cls_io<double>          i_ah_3_Q;
    Cls_io<double>          i_ah_4_I;
    Cls_io<double>          i_ah_4_Q;
    Cls_io<double>          i_ah_5_I;
    Cls_io<double>          i_ah_5_Q;
    Cls_io<double>          i_ah_6_I;
    Cls_io<double>          i_ah_6_Q;
    Cls_io<double>          i_ah_7_I;
    Cls_io<double>          i_ah_7_Q;
    Cls_io<double>          i_ah_8_I;
    Cls_io<double>          i_ah_8_Q;
    Cls_io<int     >        o_out_en;
}InterF_RX_IS;

typedef struct RX_IS_Cfg
{
    IO_config       io_config;
    Module_config   module_config;
    BF_beamf_CFG          cfg;
}RX_IS_Cfg;

class Cls_module_rx_is
{
public:
    Cls_module_rx_is() {
    }
    ~Cls_module_rx_is() {
    }

    void                        init(char* module_name);
    void                        reset();
    void                        go();
    void                        configure(RX_IS_Cfg cfg);
    InterF_RX_IS                m_IO;
private:
    Cls_BF_beamf<double>        is_bf_fl;
    Cls_BF_beamf<fxp>           is_bf_fx;
    RX_IS_Cfg                   m_cfg;
};

void Cls_module_rx_is::configure(RX_IS_Cfg cfg)
{
    m_cfg = cfg;
    if(m_cfg.module_config.fx_mode == floatpoint) {
        is_bf_fl.configure(m_cfg.cfg);
    } else if(m_cfg.module_config.fx_mode == fixpoint) {
        is_bf_fx.configure(m_cfg.cfg);
    }
}

void Cls_module_rx_is::init(char* module_name)
{
    char folder_name[300];
    strcpy(folder_name, module_name);
    if(m_cfg.module_config.fx_mode == floatpoint) {
        strcat(folder_name, "_fl/");
        is_bf_fl.init(module_name);
    }  else if(m_cfg.module_config.fx_mode == fixpoint) {
        strcat(folder_name, "_fx/");
        is_bf_fx.init(module_name);
    }
    reset();
}

void Cls_module_rx_is::reset()
{
    if(m_cfg.module_config.fx_mode == floatpoint)
        is_bf_fl.reset();
    else if(m_cfg.module_config.fx_mode == fixpoint)
        is_bf_fx.reset();
}


void Cls_module_rx_is::go()
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
                is_bf_fl.m_in_1.I = m_IO.i_in_1_I;
                is_bf_fl.m_in_1.Q = m_IO.i_in_1_Q;
                is_bf_fl.m_in_2.I = m_IO.i_in_2_I;
                is_bf_fl.m_in_2.Q = m_IO.i_in_2_Q;
                is_bf_fl.m_in_3.I = m_IO.i_in_3_I;
                is_bf_fl.m_in_3.Q = m_IO.i_in_3_Q;
                is_bf_fl.m_in_4.I = m_IO.i_in_4_I;
                is_bf_fl.m_in_4.Q = m_IO.i_in_4_Q;
                is_bf_fl.m_in_5.I = m_IO.i_in_5_I;
                is_bf_fl.m_in_5.Q = m_IO.i_in_5_Q;
                is_bf_fl.m_in_6.I = m_IO.i_in_6_I;
                is_bf_fl.m_in_6.Q = m_IO.i_in_6_Q;
                is_bf_fl.m_in_7.I = m_IO.i_in_7_I;
                is_bf_fl.m_in_7.Q = m_IO.i_in_7_Q;
                is_bf_fl.m_in_8.I = m_IO.i_in_8_I;
                is_bf_fl.m_in_8.Q = m_IO.i_in_8_Q;
                is_bf_fl.m_i_d_v  = m_IO.i_d_v;
                is_bf_fl.m_i_ah_1.I = m_IO.i_ah_1_I;
                is_bf_fl.m_i_ah_1.Q = m_IO.i_ah_1_Q;
                is_bf_fl.m_i_ah_2.I = m_IO.i_ah_2_I;
                is_bf_fl.m_i_ah_2.Q = m_IO.i_ah_2_Q;
                is_bf_fl.m_i_ah_3.I = m_IO.i_ah_3_I;
                is_bf_fl.m_i_ah_3.Q = m_IO.i_ah_3_Q;
                is_bf_fl.m_i_ah_4.I = m_IO.i_ah_4_I;
                is_bf_fl.m_i_ah_4.Q = m_IO.i_ah_4_Q;
                is_bf_fl.m_i_ah_5.I = m_IO.i_ah_5_I;
                is_bf_fl.m_i_ah_5.Q = m_IO.i_ah_5_Q;
                is_bf_fl.m_i_ah_6.I = m_IO.i_ah_6_I;
                is_bf_fl.m_i_ah_6.Q = m_IO.i_ah_6_Q;
                is_bf_fl.m_i_ah_7.I = m_IO.i_ah_7_I;
                is_bf_fl.m_i_ah_7.Q = m_IO.i_ah_7_Q;
                is_bf_fl.m_i_ah_8.I = m_IO.i_ah_8_I;
                is_bf_fl.m_i_ah_8.Q = m_IO.i_ah_8_Q;
                is_bf_fl.m_i_freq = m_IO.i_freq;
                is_bf_fl.m_i_u_v = m_IO.i_u_v;
                is_bf_fl.go();
                break;
            case (fixpoint):
                is_bf_fx.m_in_1.I = m_IO.i_in_1_I;
                is_bf_fx.m_in_1.Q = m_IO.i_in_1_Q;
                is_bf_fx.m_in_2.I = m_IO.i_in_2_I;
                is_bf_fx.m_in_2.Q = m_IO.i_in_2_Q;
                is_bf_fx.m_in_3.I = m_IO.i_in_3_I;
                is_bf_fx.m_in_3.Q = m_IO.i_in_3_Q;
                is_bf_fx.m_in_4.I = m_IO.i_in_4_I;
                is_bf_fx.m_in_4.Q = m_IO.i_in_4_Q;
                is_bf_fx.m_in_5.I = m_IO.i_in_5_I;
                is_bf_fx.m_in_5.Q = m_IO.i_in_5_Q;
                is_bf_fx.m_in_6.I = m_IO.i_in_6_I;
                is_bf_fx.m_in_6.Q = m_IO.i_in_6_Q;
                is_bf_fx.m_in_7.I = m_IO.i_in_7_I;
                is_bf_fx.m_in_7.Q = m_IO.i_in_7_Q;
                is_bf_fx.m_in_8.I = m_IO.i_in_8_I;
                is_bf_fx.m_in_8.Q = m_IO.i_in_8_Q;
                is_bf_fx.m_i_d_v  = m_IO.i_d_v;
                is_bf_fx.m_i_ah_1.I = m_IO.i_ah_1_I;
                is_bf_fx.m_i_ah_1.Q = m_IO.i_ah_1_Q;
                is_bf_fx.m_i_ah_2.I = m_IO.i_ah_2_I;
                is_bf_fx.m_i_ah_2.Q = m_IO.i_ah_2_Q;
                is_bf_fx.m_i_ah_3.I = m_IO.i_ah_3_I;
                is_bf_fx.m_i_ah_3.Q = m_IO.i_ah_3_Q;
                is_bf_fx.m_i_ah_4.I = m_IO.i_ah_4_I;
                is_bf_fx.m_i_ah_4.Q = m_IO.i_ah_4_Q;
                is_bf_fx.m_i_ah_5.I = m_IO.i_ah_5_I;
                is_bf_fx.m_i_ah_5.Q = m_IO.i_ah_5_Q;
                is_bf_fx.m_i_ah_6.I = m_IO.i_ah_6_I;
                is_bf_fx.m_i_ah_6.Q = m_IO.i_ah_6_Q;
                is_bf_fx.m_i_ah_7.I = m_IO.i_ah_7_I;
                is_bf_fx.m_i_ah_7.Q = m_IO.i_ah_7_Q;
                is_bf_fx.m_i_ah_8.I = m_IO.i_ah_8_I;
                is_bf_fx.m_i_ah_8.Q = m_IO.i_ah_8_Q;
                is_bf_fx.m_i_freq = m_IO.i_freq;
                is_bf_fx.m_i_u_v = m_IO.i_u_v;
                is_bf_fx.go();
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









