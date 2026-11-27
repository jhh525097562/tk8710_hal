/****************************************************************************
**
**  Copyright  (c)  2006,  Amlogic, Inc.
**          All Rights Reserved.
**
**  Subsystem    :
**  File         : Cls_RX_DEMOD_PHY2_1.h
**  Created By   :
**  Created On   :
**
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**  Darren      24/04/2022      v1.0    Initial
**
****************************************************************************/
#pragma once

#define MATLAB_DUMP_RX_DATA_2_1
#include "Lib_io.h"
#include "common_type.h"
#include "Cls_CC.h"
#include "Cls_RS.h"
#include "Cls_Rx_Sync_2_1.h"
#include "Cls_rx_data_phy2_1.h"

typedef struct DEMOD_2_1_Cfg
{
    IO_config           io_config;
    Module_config       module_config;
    RX_PHY2_1_SYNC_CFG  sync_cfg;
    RX_DATA_2_1_CFG     demod_cfg;
    ModeParameter       m_cfg;
    int                 phy_ver;
}DEMOD_2_1_Cfg;


template <class T>
class Cls_RX_DEMOD_PHY2_1
{
public:
    Cls_RX_DEMOD_PHY2_1() {
        m_data = NULL;
        FskDecodedOut = NULL;
        DqpskDecodedOut = NULL;
        FskSNREst = NULL;
        DqpskSNREst = NULL;
    }
    ~Cls_RX_DEMOD_PHY2_1() {
        free(m_data);
#ifdef  MATLAB_DUMP_RX_DATA_2_1
        file_o_matlab_1.close();
        file_o_matlab_2.close();
        file_o_matlab_3.close();
        file_o_matlab_4.close();
        file_i_matlab.close();
        file_i_matlab_2.close();
#endif
    }
    void                    init(char* module_name);
    void                    reset();
    void                    config(DEMOD_2_1_Cfg param);
    void                    go(Complex<double> in);
    Complex<double>         m_out;
    bool                    m_out_en;// = false;
    bool                    m_tx_reset;// = false;
    rx_result               m_result;
private:
    DEMOD_Cfg               m_cfg;
    Cls_Rx_Sync_2_1<fxp>      m_sync;
    Cls_rx_data_phy_2_1<double>  m_demod;
    Complex<double>*        m_data;
    int                     m_mode;
    unsigned int            m_data_cnt;
    bool                    m_rx_demod_en;
    int                    *FskDecodedOut;
    int                    *DqpskDecodedOut;
    double                 *FskSNREst;
    double                 *DqpskSNREst;
    int                     m_slot_cnt;
    int                     m_slot_cur;

#ifdef MATLAB_DUMP_RX_DATA_2_1
    ofstream                file_o_matlab_1;
    ofstream                file_o_matlab_2;
    ofstream                file_o_matlab_3;
    ofstream                file_o_matlab_4;
    ofstream                file_i_matlab;
    ofstream                file_i_matlab_2;
#endif // MATLAB_DUMP
};

template <class T>
void Cls_RX_DEMOD_PHY2_1<T>::reset()
{
    m_data_cnt = 0;
    m_rx_demod_en = 0;
    m_result.ber = 0;
    m_result.per = 0;
    m_sync.reset();
    m_demod.reset();
    zeros(m_data, m_sync.m_sync_out_len);
};

template <class T>
void Cls_RX_DEMOD_PHY2_1<T>::config(DEMOD_2_1_Cfg param)
{
    m_sync.configure(param.sync_cfg);
    m_demod.config(param.demod_cfg);
};

template <class T>
void Cls_RX_DEMOD_PHY2_1<T>::init(char* module_name)
{
    m_sync.init(module_name);
    m_demod.init(module_name);
    m_data = safe_new(m_data, m_sync.m_sync_out_len);
    FskDecodedOut = safe_new(FskDecodedOut, 4096);
    DqpskDecodedOut = safe_new(DqpskDecodedOut, 4096);
    m_slot_cnt = 1;
    m_slot_cur = 0;
#ifdef MATLAB_DUMP_RX_DATA_2_1
    file_o_matlab_1.open("rx_data_1.txt");
    file_o_matlab_2.open("rx_data_2.txt");
    file_o_matlab_3.open("rx_data_3.txt");
    file_o_matlab_4.open("rx_data_4.txt");
    file_i_matlab.open("sync_2_1_in.txt");
    file_i_matlab_2.open("sync_2_1_in_1.txt");
#endif
};
//Cls_Dpfsk_Demod<int> RX;

template <class T>
void Cls_RX_DEMOD_PHY2_1<T>::go(Complex<double> in)
{
    //int CRCError;//[1] = { 0 };
    //LOG2DEV<<in.I<<"\t"<<in.Q<<endl;
#ifdef MATLAB_DUMP_RX_DATA_2_1
//    if(m_slot_cnt == 1)
        file_i_matlab << in.I <<"\t"<<in.Q<<endl;
//    else if(m_slot_cnt == 2)
//        file_i_matlab_2 << in.I <<"\t"<<in.Q<<endl;
#endif
    m_sync.go(in);
    Complex<double> correctionTiming[4];
    double avg_power = 0;
    int byte_rxB[4096];
    int Sbit;
    double DriftPerSec;
    double snr_est_fsk;
    double snr_est_dqpsk;
    double RSSI;

    if (m_sync.m_out_valid == 1)
    {
#ifdef MATLAB_DUMP_RX_DATA_2_1
//        if(m_slot_cnt == 1)
            file_o_matlab_1 << m_sync.m_out.I <<"\t"<<m_sync.m_out.Q<<endl;
//        else if(m_slot_cnt == 2)
//        {
//             file_o_matlab_2 << m_sync.m_out.I <<"\t"<<m_sync.m_out.Q<<endl;
//             //LOG2DEV << m_sync.m_out.I <<"\t"<<m_sync.m_out.Q<<endl;
//        }
//        else if(m_slot_cnt == 3)
//        {
//            file_o_matlab_3 << m_sync.m_out.I <<"\t"<<m_sync.m_out.Q<<endl;
//        }
//        else if(m_slot_cnt == 4)
//        {
//            file_o_matlab_4 << m_sync.m_out.I <<"\t"<<m_sync.m_out.Q<<endl;
//        }
#endif
        m_data[m_data_cnt++] = m_sync.m_out;
//        LOG2DEV<<m_sync.m_out.I <<"\t"<<m_sync.m_out.Q<<endl;
        if (m_data_cnt == m_sync.m_sync_out_len)
        {
            m_data_cnt = 0;
            m_rx_demod_en = 1;
        }
    }
    if (m_rx_demod_en)
    {
        m_demod.m_avg_power = m_sync.m_avg_power;
        mem_copy(correctionTiming, m_sync.m_correctionTiming, 4);
        //m_demod.go(m_data, correctionTiming, FskDecodedOut, DqpskDecodedOut, FskSNREst, DqpskSNREst, CRCError);
        m_demod.go(m_data, correctionTiming);
        //phy2_1_rx_dmod.go(m_data_in1,m_data_in2,m_data_out1,m_data_out2,m_data_out3,&errorCRC,&Sbit,&FreqDriftComp,&snr_est_fsk,&snr_est_dqpsk,&RSSI);
        //m_demod.init();
        m_rx_demod_en = 0;
        m_result.per += m_demod.m_crc_error;
        cout<<m_demod.m_crc_error<<endl;
        m_slot_cnt = m_slot_cnt + 1;
//        LOG2DEV << "m_slot_cnt = " <<m_slot_cnt<<endl;
    }
    m_tx_reset = m_sync.m_finished;
    if (m_tx_reset == 1)
    {
        m_result.per = m_result.per / m_sync.m_seg_num * 1.0;
//        LOG2DEV<<"done"<<endl;
    }

};
