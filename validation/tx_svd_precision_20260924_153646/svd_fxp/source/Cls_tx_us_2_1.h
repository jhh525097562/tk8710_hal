/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File          : Cls_tx_us_2_1.h (TX Upsample filter)
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
//#define  DUMP_MATLAB_tx_us
//#define  DUMP_RTL
#include "Lib_complex.h"
#include "Lib_utility.h"
#include "Cls_DDC.h"
#include "common_type.h"
#include <type_traits>

typedef struct TX_US_2_1_Cfg
{
    IO_config       io_config       ;
    Module_config   module_config   ;
    long long       tx_data_len_cfg ;
    unsigned int    tx_bcn_len_cfg  ;
    int             tx_data_agc_cfg ;
    unsigned int    bcn_times       ;
    unsigned int    data_times      ;
    unsigned int    times           ;
    unsigned int    reg_rep_times   ;
	unsigned int    BlockNum        ;
    unsigned int    tx_tone_en      ;
    int             tx_tone_freq    ;
    int             tx_ddc          ;
    int             tx_samplingrate ;
}TX_US_2_1_Cfg;

template <class T>
class Cls_tx_us_2_1
{
    public:
        Cls_tx_us_2_1() {
            m_tx_us_1x_o = NULL;
            m_tx_us_2x_o = NULL;
            m_tx_us_4x_o = NULL;
            m_tx_us_8x_o = NULL;
            m_tx_us_16x_o = NULL;
            m_tx_us_32x_o = NULL;
            m_tx_us_64x_o = NULL;
            m_tx_us_128x_o = NULL;
            m_tx_us_256x_o = NULL;
            m_tx_us_512x_o = NULL;
            m_tx_us_1024x_o = NULL;
            m_tx_us_2048x_o = NULL;
            m_tx_us_4096x_o = NULL;
            m_in_buffer = NULL;
            m_out_buffer = NULL;
        }
        ~Cls_tx_us_2_1() {
            free(m_tx_us_1x_o);
            free(m_tx_us_2x_o);
            free(m_tx_us_4x_o);
            free(m_tx_us_8x_o);
            free(m_tx_us_16x_o);
            free(m_tx_us_32x_o);
            free(m_tx_us_64x_o);
            free(m_tx_us_128x_o);
            free(m_tx_us_256x_o);
            free(m_tx_us_512x_o);
            free(m_tx_us_1024x_o);
            free(m_tx_us_2048x_o);
            free(m_tx_us_4096x_o);
            free(m_in_buffer);
            free(m_out_buffer);
#ifdef DUMP_MATLAB_tx_us
            file_in.close();
            file_in1.close();
            file_in2.close();
            file_o.close();
            file_o1.close();
            file_o2.close();
            file_o_phase_diff.close();
            output_iq.close();
#endif // DUMP_MATLAB
#ifdef DUMP_RTL
            fp_input.close();
            fp_output.close();
            fp_phase_diff.close();
	    fp_phase2iq_i.close();
	    fp_phase2iq_q.close();
	    fp_reg_output.close();
            fp_test.close();
#endif // DUMP_RTL
        }

        bool                    m_out_en;// = false            ;
        bool                    m_compensate_en;
        bool                    m_finished_i;// = false        ;
        bool                    m_finished_o;// = false        ;
        bool                    m_i_in_en;// = false           ;
        unsigned int            m_tx_cnt;// = 0                ;
        double                       m_out;// = 0.0
        T                       t_out;// = 0.0                ;
        double                  last_package_last ;
        double                  next_package_first;
        T                       m_phase_diff                ;
        int                     m_upsampling_len            ;
        T                       t_tx_times                  ;
        T                       t_tx_data_len               ;
        T                       t_tx_agc_len                ;
        T                       t_rep_times                 ;
        T                       t_tx_tone_en                ;
        T                       t_tx_tone_freq              ;
	    T                       t_tx_ddc                    ;
        T                       m_test                      ;

        void                    init(char* foldername)      ;  //osrTx means over sampling rate , fs means sampling freq, IF means intermedia Freq
        void                    go(double in)               ;
        void                    reset(int times)            ;
        void                    tx_us_out()                 ;
        void                    config(TX_US_2_1_Cfg param) ;
        void                    compensate()                ;
        Complex<T>              m_out_iq                    ;
        int                     m_iblock                    ;
        int                     m_freq_in                   ;
        int                     m_tx_data_agc_cfg           ;
        unsigned int            m_output_len                ;
        TX_US_2_1_Cfg           m_cfg                       ;
        double                  m_ddc_o_init                ;
        T                       m_ddc_o                     ;
        double                  m_ddc_output                ;
    private:
        bool                    m_en                        ;//= false                ;
        bool                    tx_up_match_filter_en       ;
        bool                    m_tx_us_1x_en               ;
        bool                    m_tx_us_2x_en               ;
        bool                    m_tx_us_4x_en               ;
        bool                    m_tx_us_8x_en               ;
        bool                    m_tx_us_16x_en              ;
        bool                    m_tx_us_32x_en              ;
        bool                    m_tx_us_64x_en              ;
        bool                    m_tx_us_128x_en             ;
        bool                    m_tx_us_256x_en             ;
        bool                    m_tx_us_512x_en             ;
        bool                    m_tx_us_1024x_en            ;
        bool                    m_tx_us_2048x_en            ;
        bool                    m_tx_us_4096x_en            ;
        bool                    m_filter_delay_en           ;
        bool                    m_finished_init_en          ;
        bool                    m_in_en                     ;
        bool                    m_output_init               ;
        int                     m_bcn_or_data               ;
        unsigned int            m_cnt_filter_delay          ;
        unsigned int            m_cnt_in                    ;
        unsigned int            m_out_total_len             ;
        unsigned int            m_out_total_cnt             ;
        unsigned int            m_pos                       ;
        unsigned int            m_out_cnt                   ;
        unsigned int            m_compensate_cnt            ;
        unsigned int            m_delay_hf                  ;
        unsigned int            m_tx_len;// = 0                ;
        unsigned int            m_tx_len_cfg;// = 0            ;
        unsigned int            m_osr_cfg;// = 0               ;
        long long               m_osr;// = 0                   ;
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
        unsigned int            m_cnt_512x;// = 0              ;
        unsigned int            m_cnt_1024x;// = 0              ;
        unsigned int            m_cnt_2048x;// = 0              ;
        unsigned int            m_cnt_4096x;// = 0              ;
        unsigned int            m_pos_2x;// = 0                ;
        unsigned int            m_pos_4x;// = 0                ;
        unsigned int            m_pos_8x;// = 0                ;
        unsigned int            m_pos_16x;// = 0               ;
        unsigned int            m_pos_32x;// = 0               ;
        unsigned int            m_pos_64x;// = 0               ;
        unsigned int            m_pos_128x;// = 0              ;
        unsigned int            m_pos_256x;// = 0              ;
        unsigned int            m_cnt_out;// = 0               ;
        T                       m_filter_times;// = 0          ;
        unsigned int            m_filter_len;// = 0            ;
        unsigned int            m_cnt_in_out;
        unsigned int            m_bcn_times;
        unsigned int            m_data_times;
        double                  m_tx_data_len_cfg;
        unsigned int            m_tx_bcn_len_cfg;

        T                       m_tx_tone_freq;
        unsigned int            m_cnt_o;
        unsigned int            m_cnt_i;
        T                       m_ddc_freq                  ;
        T                       m_ddc_phase                 ;
        //Complex<double>*        m_tx_up                     ;
        double*                 m_tx_us_1x_o                ;
        double*                 m_tx_us_2x_o                ;
        double*                 m_tx_us_4x_o                ;
        double*                 m_tx_us_8x_o                ;
        double*                 m_tx_us_16x_o               ;
        double*                 m_tx_us_32x_o               ;
        double*                 m_tx_us_64x_o               ;
        double*                 m_tx_us_128x_o              ;
        double*                 m_tx_us_256x_o              ;
        double*                 m_tx_us_512x_o              ;
        double*                 m_tx_us_1024x_o             ;
        double*                 m_tx_us_2048x_o             ;
        double*                 m_tx_us_4096x_o             ;
        double*                 out_tmp1                    ;
        double*                 out_tmp2                    ;
        T*                      m_out_buffer                ;
        int                     m_cnt_fre                   ;
        Complex<T>              m_cossin                    ;
        //Complex<double>         m_tx_hf_64x_o                ;
        //Complex<double>         m_tx_hf_128x_o                ;
        T                       m_phase                         ;
        T                       m_in_1                          ;
        T                       m_in_2                          ;
        T                       m_fxp                           ;
        T                       m_fxp_add                       ;
        T                       m_phase_f                       ;
        T                       m_phase_l                       ;
        T                       m_tx_phase                      ;
        T                       m_tx_tone_o                     ;
        T                       m_phase_fxp                     ;
        int                     m_phase_cnt                     ;
        int                     m_rep_times                     ;
        int                     m_rep_cnt                       ;
		int                     m_tx_tone_en                    ;
        int                     m_Blocknum                      ;

        T*                      m_in_buffer                     ;

        void                    tx_half_band_filter()           ;
        void                    tx_us_in(double in)             ;
        void                    tx_us()                         ;
        double                  add(double in_1, double in_2)   ;
        Complex<double>         phase2iq(double phase)          ;
        double                  phase2diff()                    ;
        int                     m_phase_init                    ;
        int                     m_cnt_test                      ;
        double                  minus_c(double in_1, double in_2);
#ifdef DUMP_MATLAB_tx_us
        std::ofstream           file_in                          ;
        std::ofstream           file_in1                          ;
        std::ofstream           file_in2                          ;
        std::ofstream           file_o                          ;
        std::ofstream           file_o1                          ;
        std::ofstream           file_o2                          ;
        std::ofstream           output_iq                       ;
        std::ofstream           file_o_phase_diff               ;
#endif // DUMP_MATLAB
#ifdef DUMP_RTL
        std::ofstream           fp_input                    ;
        std::ofstream           fp_output                   ;
	std::ofstream           fp_phase_diff               ;
	std::ofstream           fp_phase2iq_i               ;
        std::ofstream           fp_phase2iq_q               ;
       	std::ofstream           fp_reg_output               ;
	std::ofstream           fp_test                     ;
#endif // DUMP_RTL

};

template <class T>
void Cls_tx_us_2_1<T>::config(TX_US_2_1_Cfg param)
{
    m_cfg = param;
    //m_ddc.config();
}

template <class T>
void Cls_tx_us_2_1<T>::init(char* foldername)
{
    m_cnt_fre = 0;
    m_tx_data_len_cfg   =   m_cfg.tx_data_len_cfg;
    m_tx_bcn_len_cfg    =   m_cfg.tx_bcn_len_cfg;
    if((unsigned int)m_tx_data_len_cfg > m_tx_bcn_len_cfg)
        m_tx_len_cfg    =   m_tx_data_len_cfg;
    else
        m_tx_len_cfg    =   m_tx_bcn_len_cfg;

    m_tx_data_agc_cfg   =   m_cfg.tx_data_agc_cfg;
    m_Blocknum          =   m_cfg.BlockNum;

    if(m_cfg.reg_rep_times == 0)
        m_rep_times = 1 ;
    else if(m_cfg.reg_rep_times == 1)
        m_rep_times = 2 ;
    else if(m_cfg.reg_rep_times == 3)
        m_rep_times = 4 ;
    m_tx_tone_en        =   m_cfg.tx_tone_en;
    if(m_cfg.tx_tone_freq > 1024)
        m_tx_tone_freq = m_cfg.tx_tone_freq - 2048;
    else
        m_tx_tone_freq = m_cfg.tx_tone_freq;
    t_tx_tone_freq      = m_tx_tone_freq; //dump RTL

    if(m_cfg.tx_ddc > 16777216)
        m_ddc_freq = m_cfg.tx_ddc - 33554432;
    else
        m_ddc_freq = m_cfg.tx_ddc;
    t_tx_ddc  = m_ddc_freq;//dump RTL

    m_tx_tone_freq      =   (double)m_tx_tone_freq * 100;
    m_ddc_freq          =   (double)m_ddc_freq;

    m_tx_phase          =   2 * m_tx_tone_freq * 1074 * DOUBLE_POW2[64 -35] * m_rep_times;// / 32e6;
    if(m_cfg.tx_samplingrate == 15625)
        m_ddc_phase         =   2 * m_ddc_freq * 2199023 * DOUBLE_POW2[64 -35] * m_rep_times;// / 15625;
    else if(m_cfg.tx_samplingrate == 31250)
        m_ddc_phase         =   2 * m_ddc_freq * 1099512 * DOUBLE_POW2[64 -35] * m_rep_times;// / 31250;
    else if(m_cfg.tx_samplingrate == 62500)
        m_ddc_phase         =   2 * m_ddc_freq * 54976 * DOUBLE_POW2[64 -35] * m_rep_times;// / 62500;
    else if(m_cfg.tx_samplingrate == 125000)
        m_ddc_phase         =   2 * m_ddc_freq * 274878 * DOUBLE_POW2[64 -35] * m_rep_times;// / 125000;
    else if(m_cfg.tx_samplingrate == 250000)
        m_ddc_phase         =   2 * m_ddc_freq * 137439 * DOUBLE_POW2[64 -35] * m_rep_times;// / 250000;
    else if(m_cfg.tx_samplingrate == 500000)
        m_ddc_phase         =   2 * m_ddc_freq * 68719 * DOUBLE_POW2[64 -35] * m_rep_times;// / 500000;
//    m_filter_times      =   m_cfg.times;

    m_ddc_o_init        =   0;
    m_bcn_times         =   m_cfg.bcn_times;
    m_data_times        =   m_cfg.data_times;
    m_osr               =   m_tx_len_cfg + 1;
    m_tx_us_1x_o        =   safe_new(m_tx_us_1x_o, m_osr * 2  );
    m_tx_us_2x_o        =   safe_new(m_tx_us_2x_o, m_osr * 4  );
    m_tx_us_4x_o        =   safe_new(m_tx_us_4x_o, m_osr * 8  );
    m_tx_us_8x_o        =   safe_new(m_tx_us_8x_o, m_osr * 16 );
    m_tx_us_16x_o       =   safe_new(m_tx_us_16x_o, m_osr * 32 );
    m_tx_us_32x_o       =   safe_new(m_tx_us_32x_o, m_osr * 64 );
    m_tx_us_64x_o       =   safe_new(m_tx_us_64x_o, m_osr * 128 );
    m_tx_us_128x_o      =   safe_new(m_tx_us_128x_o, m_osr * 256 );
    m_tx_us_256x_o      =   safe_new(m_tx_us_256x_o, m_osr * 512 );
    m_tx_us_512x_o      =   safe_new(m_tx_us_512x_o, m_osr * 1024 );
    m_tx_us_1024x_o     =   safe_new(m_tx_us_1024x_o, m_osr * 2048 );
    m_tx_us_2048x_o     =   safe_new(m_tx_us_2048x_o, m_osr * 4096 );
    m_tx_us_4096x_o     =   safe_new(m_tx_us_4096x_o, m_osr * 8192 );
    out_tmp1            =   safe_new(out_tmp1, 4*DOUBLE_POW2[64 + m_data_times]+1);
    out_tmp2            =   safe_new(out_tmp2, 4*DOUBLE_POW2[64 + m_data_times]+1 );
    m_in_buffer         =   safe_new(m_in_buffer, m_osr);
    m_out_buffer        =   safe_new(m_out_buffer, m_tx_data_agc_cfg);

    fix_set(m_fxp, 10, 1, FXP_TC, FXP_RND, FXP_WRAP);//statistic_set(m_fxp, "m_fxp", 1, 1);
    fix_set(m_phase, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);//statistic_set(m_phase, "m_phase", 1, 1);
    fix_set(m_phase_fxp, 10, 1, FXP_TC, FXP_RND, FXP_WRAP);//statistic_set(m_phase, "m_phase", 1, 1);
    fix_set(m_in_1, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
    fix_set(m_in_2, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
    fix_set(m_tx_tone_o, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
    fix_set(m_ddc_o, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
    fix_set(t_out, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
    fix_set(m_phase_diff, 25, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_phase_diff, "m_phase_diff", 1, 1000);
    fix_set(m_tx_phase, 25, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_phase_diff, "m_phase_diff", 1, 1000);
    fix_set(m_ddc_phase, 25, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_phase_diff, "m_phase_diff", 1, 1000);
    fix_set(m_tx_tone_freq, 20, 20, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_phase_diff, "m_phase_diff", 1, 1000);
    fix_set(m_ddc_freq, 25, 25, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_phase_diff, "m_phase_diff", 1, 1000);
    //fix_set(m_out_iq, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);

    //reg dump
    fix_set(t_tx_times, 4, 4, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(t_tx_data_len, 32, 32, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(t_tx_agc_len, 32, 32, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(t_rep_times , 2, 2, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(t_tx_tone_en, 1, 1, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(t_tx_tone_freq , 11, 11, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(t_tx_ddc , 25, 25, FXP_TC, FXP_RND, FXP_SAT_SYM);

    fix_set(m_test, 20, 3, FXP_TC, FXP_RND, FXP_SAT_SYM);

    //statistic_set(m_fxp, "m_fxp", 1, 1);
    //fix_set(m_fxp_add, 35, 3, FXP_TC, FXP_WRAP, FXP_WRAP);//statistic_set(m_fxp, "m_fxp", 1, 1);
    m_out_total_cnt = 0;
    m_output_init = 1;
    m_finished_init_en = 1;
    m_filter_len    =   64;
    m_cnt_in_out    =   0;
    m_cnt_in        =   0;
    m_in_en         =   0;

    reset(2);
    m_phase_init=   1;
#ifdef DUMP_MATLAB_tx_us
//char filename[200] = {};
file_in.open("tx_us_in.txt");
file_in1.open("tx_us_in1.txt");
file_in2.open("tx_us_in2.txt");
file_o.open("tx_us_out.txt");
file_o1.open("tx_us_out1.txt");
file_o2.open("tx_us_out2.txt");
output_iq.open("tx_us_iq.txt");
file_o_phase_diff.open("phase_diff.txt");
//    strcpy(filename, foldername); strcat(filename, "tx_us_out.txt");        file_o.open(filename);
//    strcpy(filename, foldername); strcat(filename, "tx_us_iq.txt");         output_iq.open(filename);
//    strcpy(filename, foldername); strcat(filename, "phase_diff.txt");       file_o_phase_diff.open(filename);
#endif
#ifdef DUMP_RTL
     char filename_rtl[300] = {};
     strcpy(filename_rtl, foldername); strcat(filename_rtl, "upsam_in_i.dat");        fp_input.open(filename_rtl);
     strcpy(filename_rtl, foldername); strcat(filename_rtl, "tx_us_out.dat");         fp_output.open(filename_rtl);
     strcpy(filename_rtl, foldername); strcat(filename_rtl, "phase_diff_out.dat");    fp_phase_diff.open(filename_rtl);
     strcpy(filename_rtl, foldername); strcat(filename_rtl, "phase_i_out.dat");       fp_phase2iq_i.open(filename_rtl);
     strcpy(filename_rtl, foldername); strcat(filename_rtl, "phase_q_out.dat");       fp_phase2iq_q.open(filename_rtl);
     strcpy(filename_rtl, foldername); strcat(filename_rtl, "upsam_reg_o.dat");       fp_reg_output.open(filename_rtl);
     strcpy(filename_rtl, foldername); strcat(filename_rtl, "test.dat");              fp_test.open(filename_rtl);
#endif

     t_tx_times = m_data_times;
     t_tx_data_len = m_tx_data_len_cfg;
     t_tx_agc_len = m_tx_data_agc_cfg;
     t_rep_times         =  m_cfg.reg_rep_times;
     t_tx_tone_en        =  m_cfg.tx_tone_en;
     //t_tx_tone_freq      =  m_cfg.tx_tone_freq;
     //t_tx_ddc            =  m_cfg.tx_ddc;
     //LOG2DEV <<t_tx_times<<"\t"<<t_tx_data_len<<"\t"<<t_tx_agc_len<<endl;
#ifdef DUMP_RTL //reg dump
     dump(fp_reg_output, &t_tx_times, 1, t_tx_times);
     dump(fp_reg_output, &t_tx_data_len, 1, t_tx_data_len);
     dump(fp_reg_output, &t_tx_agc_len, 1, t_tx_agc_len);
     dump(fp_reg_output, &t_rep_times,1,t_rep_times);
     dump(fp_reg_output, &t_tx_tone_en,1,t_tx_tone_en);
     dump(fp_reg_output, &t_tx_tone_freq,1,t_tx_tone_freq);
     dump(fp_reg_output, &t_tx_ddc,1,t_tx_ddc);

     dump(fp_test , &m_tx_phase ,1,m_tx_phase );
     dump(fp_test , &m_ddc_phase ,1,m_ddc_phase );


#endif

}

template <class T>
void Cls_tx_us_2_1<T>::tx_us_out()
{
    if (m_out_en & !m_compensate_en)
    {
        //m_output_len = (m_cfg.tx_data_len_cfg - 1) * DOUBLE_POW2[64 + m_data_times] + 1;
        //if(m_iblock == m_Blocknum-1)
        //{
        //    m_output_len = m_cfg.tx_data_len_cfg * DOUBLE_POW2[64 + m_data_times] + 1;
        //}

        if(m_filter_times == 0)
            m_out = m_in_buffer[m_out_cnt];
        if(m_filter_times == 1)
            m_out = m_tx_us_1x_o[m_out_cnt];
        if(m_filter_times == 2)
            m_out = m_tx_us_2x_o[m_out_cnt];
        if(m_filter_times == 3)
            m_out = m_tx_us_4x_o[m_out_cnt];
        if(m_filter_times == 4)
            m_out = m_tx_us_8x_o[m_out_cnt];
        if(m_filter_times == 5)
            m_out = m_tx_us_16x_o[m_out_cnt];
        if(m_filter_times == 6)
            m_out = m_tx_us_32x_o[m_out_cnt];
        if(m_filter_times == 7)
            m_out = m_tx_us_64x_o[m_out_cnt];
        if(m_filter_times == 8)
            m_out = m_tx_us_128x_o[m_out_cnt];
        if(m_filter_times == 9)
            m_out = m_tx_us_256x_o[m_out_cnt];
        if(m_filter_times == 10)
            m_out = m_tx_us_512x_o[m_out_cnt];
        if(m_filter_times == 11)
            m_out = m_tx_us_1024x_o[m_out_cnt];
        if(m_filter_times == 12)
            m_out = m_tx_us_2048x_o[m_out_cnt];
        if(m_filter_times == 13)
            m_out = m_tx_us_4096x_o[m_out_cnt];

        double out_temp = 0;
        if(m_tx_data_agc_cfg != 0)
        {
            if(m_out_cnt < (unsigned int)m_tx_data_agc_cfg)
                out_temp = -1.0;
            else {
                m_cnt_o = m_cnt_o % (unsigned int)m_tx_data_agc_cfg;
                out_temp = m_out_buffer[m_cnt_o++];
            }
            m_out_buffer[m_cnt_i++] = m_out;
            m_cnt_i = m_cnt_i % (unsigned int)m_tx_data_agc_cfg;
            m_out = out_temp + m_ddc_o;
        }
        else
        {
            m_out = m_out + m_ddc_o;
        }
        if(m_tx_tone_en)
        {
            m_out = m_tx_tone_o + m_ddc_o;
        }
        m_phase_diff = phase2diff();
        m_out_iq = phase2iq(m_out);
        //LOG2DEV << m_phase_diff <<endl;

        //LOG2DEV << m_out_cnt << out_temp.I<<endl;
        m_rep_cnt++;
        if(m_rep_cnt == m_rep_times)
        {
            m_out_cnt++;
            m_rep_cnt = 0;
            m_tx_tone_o = m_tx_tone_o + m_tx_phase;
            m_ddc_o = m_ddc_o + m_ddc_phase;
#ifdef DUMP_RTL
            //dump(fp_test , & m_tx_tone_o ,1,m_tx_tone_o );
#endif
        }

        //m_out_phase = phase2diff();
        if (m_out_cnt == m_output_len)
        {
            m_cnt_fre = m_cnt_fre + 1;
            last_package_last = m_out;
            m_ddc_output = m_ddc_o;
            reset(2);
            m_finished_o = 1;
//            m_out = 0;
        }
        t_out = m_out;
        m_out = t_out;
if(m_cnt_fre ==0)
{
#ifdef DUMP_MATLAB_tx_us
        file_o<<setprecision(20)<<m_out<<endl;
        output_iq<<setprecision(20)<<m_out_iq.I <<"\t"<<m_out_iq.Q<<endl;
        file_o_phase_diff<<setprecision(20)<<m_phase_diff<<endl;
#endif
}
if(m_cnt_fre ==1)
{
#ifdef DUMP_MATLAB_tx_us
file_o1<<setprecision(20)<<m_out<<endl;
#endif
}
if(m_cnt_fre ==1)
{
#ifdef DUMP_MATLAB_tx_us
file_o2<<setprecision(20)<<m_out<<endl;
#endif
}
#ifdef DUMP_RTL
	//dump(fp_phase2iq_i,&m_out_iq.I,1,m_out_iq.I);
      //  dump(fp_phase2iq_q,&m_out_iq.Q,1,m_out_iq.Q);
        dump(fp_output, &m_out, 1, m_phase);
        if((m_out_cnt >= 94203 && m_out <= 94207) && m_phase_diff == 0)
           int a = 1;
        dump(fp_phase_diff, &m_phase_diff, 1, m_phase_diff);
#endif
    }
    else
        m_finished_o = 0;
}

template <class T>
double Cls_tx_us_2_1<T>::phase2diff()
{
    double out = 0;
    m_phase_cnt = m_phase_cnt % 2;
    if(m_phase_init)
    {
        out = 0;
        m_phase_l = m_out;
        m_phase_init = 0;
        m_phase_cnt = m_phase_cnt + 1;
        return out;
    }
    if(m_phase_cnt == 0)
    {
        m_phase_l = m_out;
        out = minus_c(m_phase_l, m_phase_f);
    } else {
        m_phase_f = m_out;
        out = minus_c(m_phase_f, m_phase_l);
    }
    m_phase_cnt = m_phase_cnt + 1;
    return out;
    //LOG2DEV<<out<<endl;
}

template <class T>
Complex<double> Cls_tx_us_2_1<T>::phase2iq(double phase)
{
    Complex<double> output;
    m_phase_fxp = phase;
    phase = m_phase_fxp;
    if(m_bcn_or_data == 1) {
        m_cossin.I = cos(phase * PI);
        m_cossin.Q = sin(phase * PI);
    } else {
        m_cossin.I = cos(phase * PI);
        m_cossin.Q = sin(phase * PI);
    }

    output.I = (double)m_cossin.I;
    output.Q = (double)m_cossin.Q;

    return output;
}

template <class T>
void Cls_tx_us_2_1<T>::tx_us_in(double in)
{
    if(m_i_in_en) {
        if(m_cnt_in < m_tx_len_cfg) {
            // FXP_WRAP used to supply this phase-domain contract implicitly.
            // Floating-point assignment needs explicit modulo-2 normalization.
            if (std::is_floating_point<T>::value)
                in -= 2.0 * std::floor((in + 1.0) / 2.0);
            m_fxp = in;
            //LOG2DEV<<in<<endl;
            m_in_buffer[m_cnt_in++] = m_fxp;
        if(m_cnt_fre ==0)
        {
        #ifdef DUMP_MATLAB_tx_us
        file_in<<setprecision(20)<<m_fxp<<endl;
        #endif
        }
        if(m_cnt_fre ==1)
        {
        #ifdef DUMP_MATLAB_tx_us
        file_in1<<setprecision(20)<<m_fxp<<endl;
        #endif
        }
        if(m_cnt_fre ==2)
        {
        #ifdef DUMP_MATLAB_tx_us
        file_in2<<setprecision(20)<<m_fxp<<endl;
        #endif
        }
            if(m_cnt_in == m_tx_len_cfg){
                m_en = 1;
                m_tx_us_1x_en = 1;
            }
#ifdef DUMP_RTL
            dump(fp_input, &m_fxp, 1, m_fxp);
#endif
        }

    }
}

template <class T>
void Cls_tx_us_2_1<T>::go(double in)
{
    tx_us_in(in);
    if(m_en)
        tx_us();
    compensate();
    tx_us_out();
}

template <class T>
double Cls_tx_us_2_1<T>::add(double in_1, double in_2)
{
    double out;
    if((in_1 - in_2 > 1))
        out = in_1 + in_2 + 2.0 ;
    else if((in_2 - in_1 > 1))
        out = in_1 + in_2 - 2.0 ;
    else
        out = in_1 + in_2;

    out *= 0.5;
    if (std::is_floating_point<T>::value) {
        if (out > 1.0) out -= 2.0;
        else if (out < -1.0) out += 2.0;
    }
    m_phase = out;
    out = m_phase;
    return out;
}

template <class T>
double Cls_tx_us_2_1<T>::minus_c(double in_1, double in_2)
{
    double out;
    if((in_1 - in_2 > 1))
        out = in_1 - (in_2 + 2.0) ;
    else if((in_2 - in_1 > 1))
        out = in_1 - (in_2 - 2.0) ;
    else
        out = in_1 - in_2;

    //m_phase = out * 1.0 / 2;
    //out = m_phase;
    return out;
}

template <class T>
void Cls_tx_us_2_1<T>::tx_us()
{
    unsigned int ck0 = 0;
//    int  len_tmp = 0;
    m_osr = m_tx_len_cfg ;
    m_upsampling_len = (m_tx_len_cfg - 1) * DOUBLE_POW2[64 + m_data_times] +1;
    if(m_iblock == m_Blocknum-1)
    {
        m_osr = m_tx_len_cfg +1;
        m_upsampling_len = m_tx_len_cfg  * DOUBLE_POW2[64 + m_data_times] +1;
        m_fxp = m_in_buffer[m_tx_len_cfg - 1] + m_in_buffer[m_tx_len_cfg - 1] - m_in_buffer[m_tx_len_cfg - 2];
        m_in_buffer[m_tx_len_cfg] = m_fxp;
    }

    if(m_tx_us_1x_en)
    {
        for (ck0 = 0; ck0 < m_osr; ck0++)
        {
            m_tx_us_1x_o[2 * ck0]       =  m_in_buffer[ck0];
            m_tx_us_1x_o[2 * ck0 + 1]   =  add(m_in_buffer[ck0], m_in_buffer[ck0 + 1]);//m_fxp;
            //LOG2DEV<<m_tx_us_1x_o[2 * ck0]<<"\t"<<m_tx_us_1x_o[2 * ck0 + 1] <<endl;
            m_cnt_1x = m_cnt_1x + 2;
        }
    }
    if(m_cnt_1x > m_upsampling_len && !m_out_en)
        m_out_en = 1;
    else
        m_tx_us_2x_en = 1;
    if((m_out_en && m_cnt_1x == 0))
        m_tx_us_2x_en = 0;

    if(m_tx_us_2x_en)
    {
        for (ck0 = 0; ck0 < m_osr * 2; ck0++)
        {
            m_tx_us_2x_o[2 * ck0]       =   m_tx_us_1x_o[ck0];
            m_tx_us_2x_o[2 * ck0 + 1]   =   add(m_tx_us_1x_o[ck0], m_tx_us_1x_o[ck0 + 1]);//m_fxp;
            m_cnt_2x = m_cnt_2x + 2;
        }
    }
    if(m_cnt_2x > m_upsampling_len && !m_out_en)
        m_out_en = 1;
    else
        m_tx_us_4x_en = 1;
    if((m_out_en && m_cnt_2x == 0))
        m_tx_us_4x_en = 0;

    if(m_tx_us_4x_en)
    {
        for (ck0 = 0; ck0 < m_osr * 4; ck0++)
        {
            m_tx_us_4x_o[2 * ck0]       =   m_tx_us_2x_o[ck0];
            m_tx_us_4x_o[2 * ck0 + 1]   =   add(m_tx_us_2x_o[ck0], m_tx_us_2x_o[ck0 + 1]);//m_fxp;
            m_cnt_4x = m_cnt_4x + 2;
        }
    }
    if(m_cnt_4x > m_upsampling_len && !m_out_en)
        m_out_en = 1;
    else
        m_tx_us_8x_en = 1;
    if((m_out_en && m_cnt_4x == 0))
        m_tx_us_8x_en = 0;

    if(m_tx_us_8x_en)
    {
        for (ck0 = 0; ck0 < m_osr * 8; ck0++)
        {
            m_tx_us_8x_o[2 * ck0]       =   m_tx_us_4x_o[ck0];
            m_tx_us_8x_o[2 * ck0 + 1]   =   add(m_tx_us_4x_o[ck0], m_tx_us_4x_o[ck0 + 1]);//m_fxp;
            m_cnt_8x = m_cnt_8x + 2;
        }
    }
    if(m_cnt_8x > m_upsampling_len)
        m_out_en = 1;
    else
        m_tx_us_16x_en = 1;
    if((m_out_en && m_cnt_8x == 0))
        m_tx_us_16x_en = 0;

    if(m_tx_us_16x_en)
    {
        for (ck0 = 0; ck0 < m_osr * 16; ck0++)
        {
            m_tx_us_16x_o[2 * ck0]       =  m_tx_us_8x_o[ck0];
            m_tx_us_16x_o[2 * ck0 + 1]   =  add(m_tx_us_8x_o[ck0], m_tx_us_8x_o[ck0 + 1]);//m_fxp;
            m_cnt_16x = m_cnt_16x + 2;
        }
    }
    if(m_cnt_16x > m_upsampling_len)
        m_out_en = 1;
    else
        m_tx_us_32x_en = 1;
    if((m_out_en && m_cnt_16x == 0))
        m_tx_us_32x_en = 0;

    if(m_tx_us_32x_en)
    {
        for (ck0 = 0; ck0 < m_osr * 32; ck0++)
        {
            m_tx_us_32x_o[2 * ck0]      =  m_tx_us_16x_o[ck0];
            m_tx_us_32x_o[2 * ck0 + 1]  =  add(m_tx_us_16x_o[ck0], m_tx_us_16x_o[ck0 + 1]) ;//m_fxp;
            m_cnt_32x = m_cnt_32x + 2;
        }
    }
    if(m_cnt_32x > m_upsampling_len)
        m_out_en = 1;
    else
        m_tx_us_64x_en = 1;
    if((m_out_en && m_cnt_32x == 0))
        m_tx_us_64x_en = 0;

    if(m_tx_us_64x_en )
    {
        for (ck0 = 0; ck0 < m_osr * 64; ck0++)
        {
            m_tx_us_64x_o[2 * ck0]       =  m_tx_us_32x_o[ck0];
            m_tx_us_64x_o[2 * ck0 + 1]   =  add(m_tx_us_32x_o[ck0], m_tx_us_32x_o[ck0 + 1]);//m_fxp;
            m_cnt_64x = m_cnt_64x + 2;
        }
    }
    if(m_cnt_64x > m_upsampling_len)
        m_out_en = 1;
    else
        m_tx_us_128x_en = 1;

    if((m_out_en && m_cnt_64x == 0))
        m_tx_us_128x_en = 0;

    if(m_tx_us_128x_en)
    {
        for (ck0 = 0; ck0 < m_osr * 128; ck0++)
        {
            m_tx_us_128x_o[2 * ck0]       =  m_tx_us_64x_o[ck0];
            m_tx_us_128x_o[2 * ck0 + 1]   =  add(m_tx_us_64x_o[ck0], m_tx_us_64x_o[ck0 + 1]);//m_fxp;
            m_cnt_128x = m_cnt_128x + 2;
        }
    }
    if(m_cnt_128x > m_upsampling_len )
        m_out_en = 1;
    else
        m_tx_us_256x_en = 1;
    if((m_out_en && m_cnt_128x == 0))
        m_tx_us_256x_en = 0;

    if(m_tx_us_256x_en)
    {
        for (ck0 = 0; ck0 < m_osr * 256; ck0++)
        {
            m_tx_us_256x_o[2 * ck0]         =  m_tx_us_128x_o[ck0];
            m_tx_us_256x_o[2 * ck0 + 1]     =   add(m_tx_us_128x_o[ck0], m_tx_us_128x_o[ck0 + 1]) ;//m_fxp;
            m_cnt_256x = m_cnt_256x + 2;
        }
    }
    if(m_cnt_256x > m_upsampling_len )
        m_out_en = 1;
    else
        m_tx_us_512x_en = 1;
    if(m_out_en && m_cnt_256x == 0)
        m_tx_us_512x_en = 0;

    if(m_tx_us_512x_en)
    {
        for (ck0 = 0; ck0 < m_osr * 512; ck0++)
        {
            m_tx_us_512x_o[2 * ck0]         =  m_tx_us_256x_o[ck0];
            m_tx_us_512x_o[2 * ck0 + 1]     =  add(m_tx_us_256x_o[ck0], m_tx_us_256x_o[ck0 + 1]) ;//m_fxp;
            m_cnt_512x = m_cnt_512x + 2;
        }
    }
    if(m_cnt_512x > m_upsampling_len)
        m_out_en = 1;
    else
        m_tx_us_1024x_en = 1;
    if((m_out_en && m_cnt_512x == 0))
        m_tx_us_1024x_en = 0;

    if(m_tx_us_1024x_en)
    {
        for (ck0 = 0; ck0 < m_osr * 1024; ck0++)
        {
            m_tx_us_1024x_o[2 * ck0]         =  m_tx_us_512x_o[ck0];
            m_tx_us_1024x_o[2 * ck0 + 1]     =  add(m_tx_us_512x_o[ck0], m_tx_us_512x_o[ck0 + 1]) ;//m_fxp;
            m_cnt_1024x = m_cnt_1024x + 2;
        }
    }
    if(m_cnt_1024x > m_upsampling_len)
        m_out_en = 1;
    else
        m_tx_us_2048x_en = 1;
    if((m_out_en && m_cnt_1024x == 0))
        m_tx_us_2048x_en = 0;

    if(m_tx_us_2048x_en)
    {
        for (ck0 = 0; ck0 < m_osr * 2048; ck0++)
        {
            m_tx_us_2048x_o[2 * ck0]         =  m_tx_us_1024x_o[ck0];
            m_tx_us_2048x_o[2 * ck0 + 1]     =  add(m_tx_us_1024x_o[ck0], m_tx_us_1024x_o[ck0 + 1]) ;//m_fxp;
            m_cnt_2048x = m_cnt_2048x + 2;
        }
    }
    if(m_cnt_2048x > m_upsampling_len)
        m_out_en = 1;
    else
        m_tx_us_4096x_en = 1;
    if(m_out_en && m_cnt_2048x == 0)
        m_tx_us_4096x_en = 0;

    if(m_tx_us_4096x_en)
    {
        for (ck0 = 0; ck0 < m_osr * 4096; ck0++)
        {
            m_tx_us_4096x_o[2 * ck0]         =  m_tx_us_2048x_o[ck0];
            m_tx_us_4096x_o[2 * ck0 + 1]     =  add(m_tx_us_2048x_o[ck0], m_tx_us_2048x_o[ck0 + 1]) ;//m_fxp;
            m_cnt_4096x = m_cnt_4096x + 2;
        }
    }
    if(m_cnt_4096x > m_upsampling_len)
        m_out_en = 1;
    m_en = 0;
    m_compensate_en = 1;
}

template <class T>
void Cls_tx_us_2_1<T>::reset(int bcn_or_data)
{
	m_tx_cnt    =   0;
    m_in_en     =   0;
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
    m_cnt_512x  =   0;
    m_cnt_512x  =   0;
    m_cnt_1024x =   0;
    m_cnt_2048x =   0;
    m_cnt_4096x =   0;
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
    m_output_init = 1;
    m_out_total_cnt = 0;
    m_out_total_len = 1000000;
    m_finished_init_en = 1;
    m_pos = 0;
    m_out_cnt = 0;
    m_compensate_cnt = 0;
    tx_up_match_filter_en = 0;
    m_finished_i=   0;
    m_cnt_filter_delay = 0;
    m_finished_o=   0;
    m_out_en    =   0;
    m_compensate_en = 0;
    m_cnt_in_out = 0;
    m_tx_us_1x_en = 0;
    m_tx_us_2x_en = 0;
    m_tx_us_4x_en = 0;
    m_tx_us_8x_en = 0;
    m_tx_us_16x_en = 0;
    m_tx_us_32x_en = 0;
    m_tx_us_64x_en = 0;
    m_tx_us_128x_en = 0;
    m_tx_us_256x_en = 0;
    m_tx_us_512x_en = 0;
    m_tx_us_1024x_en = 0;
    m_tx_us_2048x_en = 0;
    m_tx_us_4096x_en = 0;
    m_cnt_o = 0;
    m_cnt_i = 0;
    m_cnt_test = 0;

    if(m_iblock == m_Blocknum-1)
    {
    m_phase_init=   1;
    m_phase_f = 0;
    m_phase_l = 0;
    m_phase_cnt = 0;
    }
    m_tx_tone_o = 0;
    m_ddc_o = 0;
    m_rep_cnt = 0;
    //m_filter_times = times;

    m_bcn_or_data = bcn_or_data;
    if(bcn_or_data == 1) {
        m_upsampling_len=   m_cfg.tx_bcn_len_cfg * DOUBLE_POW2[64 + m_bcn_times] + 1 ;
        m_output_len    =   m_cfg.tx_bcn_len_cfg * DOUBLE_POW2[64 + m_bcn_times] + 1 ;
        m_tx_len_cfg    =   m_cfg.tx_bcn_len_cfg;
        m_filter_times  =   m_bcn_times;
        if(m_filter_times == 0)
            m_output_len    = m_cfg.tx_bcn_len_cfg;
    } else if(bcn_or_data == 2 || bcn_or_data == 3) {// || bcn_or_data == 0){
        m_upsampling_len=   m_cfg.tx_data_len_cfg * DOUBLE_POW2[64 + m_data_times] + 1;
        m_output_len    =   m_cfg.tx_data_len_cfg * DOUBLE_POW2[64 + m_data_times] + 1 + m_tx_data_agc_cfg;
        m_tx_len_cfg    =   m_cfg.tx_data_len_cfg;
        m_filter_times  =   m_data_times;
        if(m_filter_times == 0)
            m_output_len    = m_cfg.tx_data_len_cfg;
        if(bcn_or_data == 3) {
            m_out_cnt = 1;
            m_ddc_o = m_ddc_o_init;
        }
    }

    m_osr               =   m_tx_len_cfg + 1;

    m_ddc_freq          =   (double)m_freq_in;

    if(m_cfg.tx_samplingrate == 15625)
        m_ddc_phase         =   2 * m_ddc_freq * 2199023 * DOUBLE_POW2[64 -35] * m_rep_times;// / 15625;
    else if(m_cfg.tx_samplingrate == 31250)
        m_ddc_phase         =   2 * m_ddc_freq * 1099512 * DOUBLE_POW2[64 -35] * m_rep_times;// / 31250;
    else if(m_cfg.tx_samplingrate == 62500)
        m_ddc_phase         =   2 * m_ddc_freq * 549756 * DOUBLE_POW2[64 -35] * m_rep_times;// / 62500;
    else if(m_cfg.tx_samplingrate == 125000)
        m_ddc_phase         =   2 * m_ddc_freq * 274878 * DOUBLE_POW2[64 -35] * m_rep_times;// / 125000;
    else if(m_cfg.tx_samplingrate == 250000)
        m_ddc_phase         =   2 * m_ddc_freq * 137439 * DOUBLE_POW2[64 -35] * m_rep_times;// / 250000;
    else if(m_cfg.tx_samplingrate == 500000)
        m_ddc_phase         =   2 * m_ddc_freq * 68719 * DOUBLE_POW2[64 -35] * m_rep_times;// / 500000;
}

template <class T>
void Cls_tx_us_2_1<T>::compensate()
{
    if(m_iblock == 0)
        m_compensate_en = 0;
    if (m_out_en & m_compensate_en)
    {
        if(m_filter_times == 0)
            m_out = m_in_buffer[0];
        if(m_filter_times == 1)
            m_out = m_tx_us_1x_o[0];
        if(m_filter_times == 2)
            m_out = m_tx_us_2x_o[0];
        if(m_filter_times == 3)
            m_out = m_tx_us_4x_o[0];
        if(m_filter_times == 4)
            m_out = m_tx_us_8x_o[0];
        if(m_filter_times == 5)
            m_out = m_tx_us_16x_o[0];
        if(m_filter_times == 6)
            m_out = m_tx_us_32x_o[0];
        if(m_filter_times == 7)
            m_out = m_tx_us_64x_o[0];
        if(m_filter_times == 8)
            m_out = m_tx_us_128x_o[0];
        if(m_filter_times == 9)
            m_out = m_tx_us_256x_o[0];
        if(m_filter_times == 10)
            m_out = m_tx_us_512x_o[0];
        if(m_filter_times == 11)
            m_out = m_tx_us_1024x_o[0];
        if(m_filter_times == 12)
            m_out = m_tx_us_2048x_o[0];
        if(m_filter_times == 13)
            m_out = m_tx_us_4096x_o[0];

/* inter package  compensation*/
        int i,j;
        if(m_compensate_cnt == 0)
        {
            next_package_first = m_out;
            out_tmp1[0] = last_package_last;
            out_tmp1[1] = next_package_first;
            for(i=0;i<m_data_times;i++)
            {
                for(j=0;j<DOUBLE_POW2[64+i+1];j++)
                {
                    out_tmp2[2*j] = out_tmp1[j];
                    out_tmp2[2*j+1] = add(out_tmp1[j],out_tmp1[j+1]);
                }
                for(j=0;j<DOUBLE_POW2[64+i+2];j++)
                {
                    out_tmp1[j] = out_tmp2[j];
                }
            }
        }

        m_out = out_tmp1[1+m_compensate_cnt];

        m_phase_diff = phase2diff();
        m_out_iq = phase2iq(m_out);
        //LOG2DEV << m_phase_diff <<endl;
        t_out  = m_out;
        m_out = t_out;
#ifdef DUMP_MATLAB_tx_us
        file_o<<setprecision(20)<<m_out<<endl;
        output_iq<<setprecision(20)<<m_out_iq.I <<"\t"<<m_out_iq.Q<<endl;
        file_o_phase_diff<<setprecision(20)<<m_phase_diff<<endl;
#endif
#ifdef DUMP_RTL
        //dump(fp_output, &m_out_iq, 1, m_phase);
        dump(fp_output, &m_out, 1, m_phase);
        dump(fp_phase_diff, &m_phase_diff, 1, m_phase_diff);
#endif
        m_compensate_cnt = m_compensate_cnt + 1;
        if(m_compensate_cnt == DOUBLE_POW2[64 + m_data_times]-1)
            m_compensate_en = 0;
    }
}
