/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File          : Cls_TX_US.h (TX Upsample filter)
**  Created By    : Darren
**  Created On    : 06/08/2020
**
**  History     :
**  Programmer    Date         Rev          Description
**  Darren        07/08/2020   v1.0
**
**  Notice:
**
****************************************************************************/

#pragma once
//#define  PRINT_OUTPUT

#include "Lib_complex.h"
#include "Lib_utility.h"
#include "Cls_FIR.h"
#include "Cls_DDC.h"

typedef struct TX_US_2_Cfg
{
    IO_config       io_config       ;
    Module_config   module_config   ;
    DDC_Cfg         tx_ddc_config   ;
    unsigned int    tx_len_cfg      ;
    unsigned int    osr_cfg         ;
    unsigned int    osr             ;
}TX_US_2_Cfg;

template <class T>
class Cls_TX_US
{
    public:
        Cls_TX_US() {
            m_tx_base = NULL;
            m_tx_up = NULL;
            m_tx_half = NULL;
            m_tx_ddc_o = NULL;
            m_tx_hf_1x_o = NULL;
            m_tx_hf_2x_o = NULL;
            m_tx_hf_4x_o = NULL;
            m_tx_hf_8x_o = NULL;
            m_tx_hf_16x_o = NULL;
            m_tx_hf_32x_o = NULL;
            m_tx_hf_64x_o = NULL;
            m_tx_hf_128x_o = NULL;
            m_tx_hf_256x_o = NULL;
            m_out_tmp = NULL;
            m_in_buffer = NULL;
        }
        ~Cls_TX_US() {
            free(m_tx_base);
            free(m_tx_up);
            free(m_tx_half);
            free(m_tx_ddc_o);
            free(m_tx_hf_1x_o);
            free(m_tx_hf_2x_o);
            free(m_tx_hf_4x_o);
            free(m_tx_hf_8x_o);
            free(m_tx_hf_16x_o);
            free(m_tx_hf_32x_o);
            free(m_tx_hf_64x_o);
            free(m_tx_hf_128x_o);
            free(m_tx_hf_256x_o);
            //free(m_out_tmp);
            free(m_in_buffer);
#ifdef PRINT_OUTPUT
            output.close();
#endif // PRINT_OUTPUT
        }

        bool                    m_out_en;// = false            ;
        bool                    m_finished_i;// = false        ;
        bool                    m_finished_o;// = false        ;
        bool                    m_i_in_en;// = false           ;
        unsigned int            m_tx_cnt;// = 0                ;
        Complex<double>         m_out;// = 0.0                 ;

        void                    init(int osr)               ;  //osrTx means over sampling rate , fs means sampling freq, IF means intermedia Freq
        void                    go(Complex<double> in)      ;
        void                    reset(int osr)              ;
        void                    tx_us_out()                 ;
        void                    config(TX_US_2_Cfg param)     ;
    private:
        bool                    m_en ;//= false                ;
        bool                    tx_up_match_filter_en       ;
        bool                    m_tx_hf_1x_en               ;
        bool                    m_tx_hf_2x_en               ;
        bool                    m_tx_hf_4x_en               ;
        bool                    m_tx_hf_8x_en               ;
        bool                    m_tx_hf_16x_en              ;
        bool                    m_tx_hf_32x_en              ;
        bool                    m_tx_hf_64x_en              ;
        bool                    m_tx_hf_128x_en             ;
        bool                    m_tx_hf_256x_en             ;
        bool                    m_filter_delay_en           ;
        bool                    m_finished_init_en          ;
        bool                    m_in_en                     ;
        bool                    m_output_init               ;
        unsigned int            m_cnt_filter_delay          ;
        unsigned int            m_output_len                ;
        unsigned int            m_cnt_in                    ;
        unsigned int            m_out_total_len             ;
        unsigned int            m_out_total_cnt             ;
        unsigned int            m_pos                       ;
        unsigned int            m_out_cnt                   ;
        unsigned int            m_delay_hf                  ;
        unsigned int            m_tx_len;// = 0                ;
        unsigned int            m_tx_len_cfg;// = 0            ;
        unsigned int            m_osr_cfg;// = 0               ;
        unsigned int            m_osr;// = 0                   ;
        unsigned int            m_cnt;// = 0                   ;
        unsigned int            m_cnt_1x;// = 0                ;
        unsigned int            m_cnt_2x;// = 0                ;
        unsigned int            m_cnt_4x;// = 0                ;
        unsigned int            m_cnt_8x;// = 0                ;
        unsigned int            m_cnt_16x;// = 0               ;
        unsigned int            m_cnt_32x;// = 0               ;
        unsigned int            m_cnt_64x ;//= 0               ;
        unsigned int            m_cnt_128x;// = 0              ;
        unsigned int            m_cnt_256x;// = 0              ;
        unsigned int            m_pos_2x;// = 0                ;
        unsigned int            m_pos_4x;// = 0                ;
        unsigned int            m_pos_8x;// = 0                ;
        unsigned int            m_pos_16x;// = 0               ;
        unsigned int            m_pos_32x;// = 0               ;
        unsigned int            m_pos_64x;// = 0               ;
        unsigned int            m_pos_128x;// = 0              ;
        unsigned int            m_pos_256x;// = 0              ;
        unsigned int            m_cnt_out;// = 0               ;
        unsigned int            m_filter_times;// = 0          ;
        unsigned int            m_filter_len;// = 0            ;
        unsigned int            m_delay;// = 0                 ;
        unsigned int            m_osr_16;// = 0                ;
        unsigned int            m_osr_32;// = 0                ;
        unsigned int            m_osr_64;// = 0                ;
        unsigned int            m_osr_128;// = 0               ;
        unsigned int            m_osr_256;// = 0               ;
        unsigned int            m_cnt_in_out;                ;
        Complex<double>*        m_tx_base                   ;
        Complex<double>*        m_out_tmp;
        //Complex<double>*        m_tx_up                     ;
        Complex<double>*        m_tx_up                     ;
        Complex<double>*        m_tx_hf_1x_o                ;
        Complex<double>*        m_tx_hf_2x_o                ;
        Complex<double>*        m_tx_hf_4x_o                ;
        Complex<double>*        m_tx_hf_8x_o                ;
        Complex<double>*        m_tx_hf_16x_o                ;
        Complex<double>*        m_tx_hf_32x_o                ;
        Complex<double>*        m_tx_hf_64x_o                ;
        Complex<double>*        m_tx_hf_128x_o                ;
        Complex<double>*        m_tx_hf_256x_o                ;
        //Complex<double>         m_tx_hf_64x_o                ;
        //Complex<double>         m_tx_hf_128x_o                ;
        Complex<double>*        m_tx_half                   ;
        Complex<double>*        m_tx_ddc_o                  ;
        Cls_FIR<double>         m_tx_match_filter           ;
        Cls_FIR<double>         m_tx_hf_1x                  ;
        Cls_FIR<double>         m_tx_hf_2x                  ;
        Cls_FIR<double>         m_tx_hf_4x                  ;
        Cls_FIR<double>         m_tx_hf_8x                  ;
        Cls_FIR<double>         m_tx_hf_16x                 ;
        Cls_FIR<double>         m_tx_hf_32x                 ;
        Cls_FIR<double>         m_tx_hf_64x                 ;
        Cls_FIR<double>         m_tx_hf_128x                ;
        Cls_FIR<double>         m_tx_hf_256x                ;
        Complex<T>              m_in                        ;
        Complex<T>*             m_in_buffer                 ;
        Cls_DDC<T>              m_ddc                       ;
        TX_US_2_Cfg             m_cfg                       ;
        void                    tx_up_match_filter()        ;
        void                    tx_half_band_filter()       ;
        void                    tx_us_in(Complex<double> in);
        void                    tx_ddc()                    ;

#ifdef PRINT_OUTPUT
        std::ofstream           output                      ;
#endif // PRINT_OUTPUT
};

template <class T>
void Cls_TX_US<T>::config(TX_US_2_Cfg param)
{
    m_cfg = param;
    //m_ddc.config();
}

template <class T>
void Cls_TX_US<T>::init(int osr)
{
    m_osr           =   m_cfg.osr;
    m_tx_len_cfg    =   m_cfg.tx_len_cfg;
    m_osr_cfg       =   m_cfg.osr_cfg;
    m_filter_times  =   (int)log2(m_osr_cfg * 1.0 / m_osr);
    m_tx_base       =   safe_new(m_tx_base, 500 * m_tx_len_cfg);
    m_tx_up         =   safe_new(m_tx_up, m_osr);
    m_tx_hf_1x_o    =   safe_new(m_tx_hf_1x_o, m_osr * 2);
    m_tx_hf_2x_o    =   safe_new(m_tx_hf_2x_o, m_osr * 4);
    m_tx_hf_4x_o    =   safe_new(m_tx_hf_4x_o, m_osr * 8);
    m_tx_hf_8x_o    =   safe_new(m_tx_hf_8x_o, m_osr * 16);
    m_tx_hf_16x_o   =   safe_new(m_tx_hf_16x_o, m_osr * 32);
    m_tx_hf_32x_o   =   safe_new(m_tx_hf_32x_o, m_osr * 64);
    m_tx_hf_64x_o   =   safe_new(m_tx_hf_64x_o, m_osr * 128);
    m_tx_hf_128x_o  =   safe_new(m_tx_hf_128x_o, m_osr * 256);
    m_tx_hf_256x_o  =   safe_new(m_tx_hf_256x_o, m_osr * 512);
    m_tx_half       =   safe_new(m_tx_half, 500 * m_tx_len_cfg);
    m_tx_ddc_o      =   safe_new(m_tx_ddc_o, 500 * m_tx_len_cfg);
    m_in_buffer     =   safe_new(m_in_buffer, 118848 * 2);
    m_out_tmp       =   safe_new(m_out_tmp, 1000000);
    m_output_len    =   0;
    m_out_total_cnt = 0;
    m_output_init = 1;
    m_finished_init_en = 1;
    m_filter_len    =   64;
    m_cnt_in_out    =   0;
    m_cnt_in        =   0;
    m_in_en         =   1;
    m_out_total_len =  (m_tx_len_cfg + 1) * 2;
    if (m_osr == 1) {
        m_tx_match_filter.init(TX_2_MFIR);
        m_delay = 32;
        m_delay_hf = 13;
    } else if(m_osr == 2){
        m_delay_hf = 13;
    } else {
        m_tx_match_filter.init(TX_MFIR);
        m_delay_hf = 13;
    }
    m_tx_hf_1x.init(TX_HFIR);
    m_tx_hf_2x.init(TX_HFIR);
    m_tx_hf_4x.init(TX_HFIR);
    m_tx_hf_8x.init(TX_HFIR);
    m_tx_hf_16x.init(TX_HFIR);
    m_tx_hf_32x.init(TX_HFIR);
    m_tx_hf_64x.init(TX_HFIR);
    m_tx_hf_128x.init(TX_HFIR);
    m_tx_hf_256x.init(TX_HFIR);
    m_ddc.init("_ddc");

    reset(osr);

#ifdef PRINT_OUTPUT
    output.open("tx_us_out.txt");
#endif
}

template <class T>
void Cls_TX_US<T>::tx_us_out()
{
    if (m_out_en) {
        m_out = m_out_tmp[m_out_cnt];
#ifdef PRINT_OUTPUT
        output << m_out.I << "\t" << m_out.Q << endl;
#endif
        m_out_cnt++;
        if (m_out_cnt == m_output_len)
        {
            m_in_en = 1;
            m_out_cnt = 0;
            //m_out_en = 0;
        }
        m_out_total_cnt++;
        if (m_out_total_cnt == m_out_total_len)
        {
            reset(m_osr_cfg);
            m_finished_o = 1;
            m_out = 0;
        }
    }
}

template <class T>
void Cls_TX_US<T>::tx_us_in(Complex<double> in)
{
    if (m_finished_i == 1 && m_finished_init_en)
    {
        m_cnt_in = m_cnt_in;
        m_finished_init_en = 0;
        m_out_total_len = (m_cnt_in + 1) * (unsigned int)(UINT64_POW2[m_filter_times]);// pow(2, m_filter_times);
        m_filter_delay_en = 1;
    }
    if (!m_filter_delay_en)
    {
        m_in_buffer[m_cnt_in++] = in;
        if (m_in_en)
            m_in = m_in_buffer[m_cnt_in_out++];
    }
    else
    {
        if (m_in_en)
        {
            m_in = m_in_buffer[m_cnt_in_out++];
        }

    }
}

template <class T>
void Cls_TX_US<T>::tx_ddc()
{
    for (unsigned int ck0 = 0; ck0 < m_tx_len; ck0++)
    {
        m_tx_ddc_o[ck0] = m_ddc.rot(m_tx_half[ck0]);
    }
}

template <class T>
void Cls_TX_US<T>::go(Complex<double> in)
{
    if (m_i_in_en || m_filter_delay_en)
    {
        tx_us_in(in);
        if (m_in_en)
        {
            //tx_up_match_filter();
            tx_half_band_filter();
            //tx_ddc();
        }
    }
    tx_us_out();
}

template <class T>
void Cls_TX_US<T>::tx_up_match_filter()
{
    if (m_osr == 1)
    {
        m_tx_up[0] = m_tx_match_filter.dFilt(m_in.todouble());
        m_cnt++;
    }
    else
    {
        for (unsigned int ck0 = 0; ck0 < m_osr; ck0++)
        {
            if (m_cnt % m_osr == 0)
                m_tx_up[ck0] = m_tx_match_filter.dFilt(m_in.todouble());
            else
                m_tx_up[ck0] = m_tx_match_filter.dFilt(0);
            m_cnt++;
        }
    }

    if (m_cnt > m_delay) {
        tx_up_match_filter_en = 1;
    }
    /*for (unsigned int i = 0; i < m_tx_len * 4 ; i++)
    {
        if (i % m_osr == 0)
            m_tx_up[i] = m_tx_match_filter.dFilt(m_tx_base[i / m_osr]);
        else
            m_tx_up[i] = m_tx_match_filter.dFilt(0);
    }
    for (unsigned int i = m_tx_len * 4; i < m_tx_len * 4 + m_filter_len; i++)
        m_tx_up[i] = m_tx_match_filter.dFilt(0);

    m_tx_len = m_tx_len * 4;
    m_delay = 64 / 2;*/
}

template <class T>
void Cls_TX_US<T>::tx_half_band_filter()
{
    unsigned int ck1 = 0;
    for (unsigned int ck0 = 0; ck0 < m_osr; ck0++) {
        m_tx_hf_1x_o[2 * ck0]     =  m_tx_hf_1x.dFilt(2 * m_in.todouble());
        m_tx_hf_1x_o[2 * ck0 + 1] =  m_tx_hf_1x.dFilt(0);

    }
    m_out_tmp[m_cnt_1x + 0] = m_tx_hf_1x_o[0];
    m_out_tmp[m_cnt_1x + 1] = m_tx_hf_1x_o[1];
    m_cnt_1x = m_cnt_1x + 2;
    m_out_en = 1;
}

template <class T>
void Cls_TX_US<T>::reset(int osr)
{
	m_tx_cnt    =   0;
    m_in_en     =   1;
    m_cnt_in    =   0;
    m_tx_len    =   0;
    m_cnt       =   0;
    m_cnt_1x    =   0;
    m_cnt_2x    =   0;
    m_cnt_4x    =   0;
    m_cnt_8x    =   0;
    m_cnt_16x   =   0;
    m_cnt_32x   =   0;
    m_cnt_64x   =   0;
    m_cnt_128x  =   0;
    m_cnt_256x  =   0;
    m_pos_2x    =   0;
    m_pos_4x    =   0;
    m_pos_8x    =   0;
    m_pos_16x   =   0;
    m_pos_32x   =   0;
    m_pos_64x   =   0;
    m_pos_128x  =   0;
    m_pos_256x  =   0;
    m_cnt_out   =   0;
    m_en        =   0;
    m_tx_hf_1x_en = 0;
    m_tx_hf_2x_en = 0;
    m_tx_hf_4x_en = 0;
    m_tx_hf_8x_en = 0;
    m_tx_hf_16x_en = 0;
    m_tx_hf_32x_en = 0;
    m_tx_hf_64x_en = 0;
    m_tx_hf_128x_en = 0;
    m_tx_hf_256x_en = 0;
    m_output_init = 1;
    m_output_len = 0;
    m_filter_delay_en = 0;
    m_out_total_cnt = 0;
    m_finished_init_en = 1;
    m_pos = 0;
    m_out_cnt = 0;
    m_osr_16 = 0;
    m_osr_32 = 0;
    m_osr_64 = 0;
    m_osr_128 = 0;
    m_osr_256 = 0;
    tx_up_match_filter_en = 0;
    m_finished_i=   0;
    m_cnt_filter_delay = 0;
    m_finished_o=   0;
    m_out_en    =   0;
    m_cnt_in_out = 0;
    m_osr_cfg = m_cfg.osr_cfg;
    m_filter_times = (int)log2(m_osr_cfg * 1.0 / m_osr);
    m_tx_match_filter.reset();
    m_tx_hf_1x.reset();
    m_tx_hf_2x.reset();
    m_tx_hf_4x.reset();
    m_tx_hf_8x.reset();
    m_tx_hf_16x.reset();
    m_tx_hf_32x.reset();
    m_tx_hf_64x.reset();
    m_tx_hf_128x.reset();
    m_tx_hf_256x.reset();
    zeros(m_tx_base, 500 * m_tx_len_cfg);
    //zeros(m_tx_up, 500 * m_tx_len_cfg);
    zeros(m_tx_half, 500 * m_tx_len_cfg);
    zeros(m_tx_ddc_o, 500 * m_tx_len_cfg);
    m_tx_hf_1x.reset();
    m_ddc.reset();
}
