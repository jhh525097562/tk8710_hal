/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File          : Cls_RX_SYNC_.h (TX Data Generation)
**  Created By    : Darren
**  Created On    : 06/08/2020
**
**  History     :
**  Programmer    Date         Rev          Description
**  Darren        20/01/2021   v1.0
**
**  Notice:
**
****************************************************************************/

#pragma once
/*#define  MATLAB_DUMP_SYNC*/
#include "Lib_complex.h"
#include "Lib_utility.h"
#include "common_type.h"
#include "Lib_dump.h"
#include "Cls_sync_DDC.h"
#include "Cls_cordic.h"
//#define PHY2_1_SYNC_PRINT
//#define RTL_DUMP_SYNC

using namespace std;
int PskPreamble[] = {-1,-1, 1, -1, 1, 1, -1, 1, 1, -1, -1, -1 };
int DSeq_1[] = {1};
int DSeq_2[] = {1, 1};
int DSeq_4[] = {1, 1, -1, -1};
int DSeq_8[] = {1, 1,-1, -1, 1, 1, -1, -1};
int DSeq_16[] = {1, 1, 1, -1, -1, -1, -1, 1, -1, 1, -1, -1, 1, 1, -1, 1};
int DSeq_32[] = {1, 1, 1, 1, -1, 1, -1, 1, -1, -1, -1, 1, -1, -1, 1, 1, 1, -1, -1, -1, -1, -1, 1, 1, -1, -1, 1, -1, 1, 1, -1, 1};

typedef struct RX_PHY2_1_SYNC_CFG
{
    int len_FreqComp;
    int TOScope;
    double Fs;
    int sync_len;
    int len_freq;
    float freq_start;
    float freq_inter;
    int dsss_num;
    double freq_drift_s;
    double freq_drift_i;
    int freq_drift_len;
    int Sym_Num_Data;
    int seg_num;
}RX_PHY2_1_SYNC_CFG;

template <class T>
class Cls_Rx_Sync_2_1
{
    public:
        Cls_Rx_Sync_2_1() {
            //m_FskFreqsLoc = NULL;
            m_sync = NULL;
            m_dseq = NULL;
            //m_sigFltFreqComp = NULL;
            m_tmpS = NULL;
            m_symRx_tmp_diff = NULL;
            m_correctionTiming = NULL;
            m_PskPreamble = NULL;
            m_symRx = NULL;
            //m_out = NULL;
            m_out_temp = NULL;
            m_out_in_seg = NULL;
            m_out_n = NULL;
            m_sym_dss = NULL;
            m_DriftComp = NULL;
            m_sync_ddc_in = NULL;
            m_sync_n = NULL;
        }
        ~Cls_Rx_Sync_2_1() {
            //free(m_FskFreqsLoc);

            //free(m_sigFltFreqComp);
            free(m_out_temp);
            free(m_out_n);
            free(m_PskPreamble);
            free(m_tmpS);
            free(m_symRx_tmp_diff);
            free(m_correctionTiming);
            free(m_symRx);
            //free(m_out);
            free(m_sync);
            free(m_dseq);
            free(m_sym_dss);
            free(m_DriftComp);
            free(m_sync_ddc_in);
            free(m_sync_n);
#ifdef MATLAB_DUMP_SYNC
            file_o_corrmat_nocoh.close();
            file_o_corrMat_coh.close();
            file_o_sync_in.close();
            file_o_data_o.close();
#endif

        }
        void                    init(char* module_name)        ;     //osrTx means over sampling rate , fs means sampling freq, IF means intermedia Freq
        void                    go(Complex<double> in)              ;
        void                    reset()                             ;
        void                    configure(RX_PHY2_1_SYNC_CFG param)   ;
        bool                    m_out_valid                         ;
        int                     m_sync_out_len                      ;
        bool                    m_finished                          ;
        Complex<T>*             m_correctionTiming;
        Complex<T>              m_out                               ;
        Complex<T>              m_out_sync_in                               ;
        Complex<T>              m_in                                ;
        T                       m_avg_power;
        unsigned int            m_seg_num;
    private:
        RX_PHY2_1_SYNC_CFG      m_cfg                       ;
        bool                    m_out_en                    ;
        bool                    m_out_init                  ;
        int                     m_dsss_num                  ;
        unsigned int            m_SymNumPreamble            ;
        unsigned int            m_LenSym                    ;
       // unsigned int            m_Mfsk                      ;
        unsigned int            m_cnt                       ;
        unsigned int            m_sync_len                  ;
        unsigned int            m_sync_len_set              ;
        unsigned int            m_len_FreqComp              ;
        unsigned int            m_TOScope                   ;
        unsigned int            m_SyncCombinePower          ;
        unsigned int            m_out_cnt                   ;

        unsigned int            m_symdata_len               ;
        unsigned int            m_cnt_out_total             ;
        unsigned int            m_cnt_out_total_n           ;
        unsigned int            m_cnt_seg                   ;

        double                  m_Fs                        ;
        T                       m_Fs_frac                   ;
        double                  m_val                       ;
        double                  m_timing_phs_print          ;
        T                       m_timing_phs                ;
        double                  m_EstFreqOffset             ;
        int*                    m_PskPreamble               ;
        T                       m_FSK_Setting[2]            ;
        int                     m_pos                       ;
        int                     m_pos_ddc                   ;
        int                     m_estTimingOffset           ;
        int                     m_p_out                     ;
        int                     m_p_out_n                   ;
        int                     m_p_in                      ;
        int                     m_p_in_n                    ;
        int                     m_sync_cnt                  ;
        bool                    m_en                        ;
        void                    GenFskFreqs()               ;
        //Complex<double>**       m_FskFreqsLoc               ;
        //Complex<double>*        m_out                       ;
        Complex<T>              m_timing_phs_tmp            ;
        Complex<T>              m_temp_for_debug            ;
        Complex<T>              m_temp_new                  ;
        T                       m_corrMat_nocoh             ;
        Complex<T>              m_tmpCorr_power             ;
        T                       m_FreqComp[64]              ;
        double                  m_val1                      ;
        double                  m_val2                      ;
        T                       m_corrMat_coh               ;
        T                       m_angle_tmp                 ;
        T                       m_angle_tmp_o               ;
        T                       m_angle_tmp_n               ;
        T                       m_angle                     ;
        T                       m_ddc_out                   ;
        T                       m_mult                      ;
        T                       m_freq_start                ;
        T                       m_freq_inter                ;
        T                       m_freq_drift_s              ;
        T                       m_freq_drift_i              ;
        T                       m_FreqOffsetDriftTabletmp   ;
        T                       m_drift_fix                 ;
        Complex<double>*        m_sync                      ;
        Complex<double>*        m_sync_n                    ;
        //Complex<double>*        m_sigFltFreqComp            ;
        Complex<double>*        m_tmpS                      ;
        Complex<double>*        m_out_temp                  ;
        Complex<double>*        m_out_in_seg                ;
        Complex<double>*        m_out_n                     ;
        Complex<T>*             m_symRx                     ;
        Complex<T>              m_symRx_tmp                 ;
        Complex<T>*             m_symRx_tmp_diff            ;
        Complex<T>*             m_sym_dss                   ;
        Complex<T>*             m_sync_ddc_in               ;

        double                  m_corrMaxInT                ;
        int                     m_iTMax                     ;
        int*                    m_dseq                      ;
        T*                      m_DriftComp                 ;
        T                       m_avg_power_tmp             ;
        Complex<double>         m_timing_phs_freq           ;
        Complex<T>              m_sum_odd                   ;
        Complex<T>              m_sum_even                  ;
        Complex<T>              m_FSK_LOC[16]               ;
        Complex<T>              m_fsk_loc_t                 ;
        T                       m_tmpCorr2                  ;
        T                       m_symRx_power               ;
        int                     m_freq_drift_len            ;
        int                     m_out_reset                 ;
        int                     m_pos_drift                 ;
        int                     m_sync_cnt_en               ;
        int                     m_TOScope_set               ;
        int                     m_SymNumPreamble_set        ;
        int                     m_cnt_drift                 ;
        int                     m_cnt_drift_n               ;
        int                     m_cnt_in                    ;
        int                     m_cnt_out_offset            ;
        int                     m_cnt_out_offset_n          ;
        int                     m_out_en_new                ;
        Complex<T>              m_tmpCorr                   ;
        Complex<double>         rot(double input)           ;
        void                    sync_fifo(Complex<double> in);
        Cls_sync_DDC<T>         m_ddc                       ;
        Cls_sync_DDC<T>         m_ddc_n                     ;
        ClsCORDIC<T>            m_cordic                    ;
        SYNC_DDC_Cfg            m_cfg_ddc                   ;
        void                    Sync_Cor()                  ;
        void                    Sync_out_new()              ;
        //void                    DDC(int times)              ;
        void                    dem_timing_offset(int times, int ddc_pos, int drift_pos);
        void                    diff()                      ;
        void                    Sync_out()                  ;
        void                    Sync_new()                  ;
        void                    Sync_PeakSrh(double val1, double val2, int times, int DDC_times, int drift_times);
#ifdef MATLAB_DUMP_SYNC
        std::ofstream           file_o_corrmat_nocoh;
        std::ofstream           file_o_corrMat_coh;
        std::ofstream           file_o_sync_in;
        std::ofstream           file_o_data_o;
#endif
#ifdef RTL_DUMP_SYNC
        std::ofstream           file_in, f_reg, f_ddc_in, f_ddc_out, f_angle_tmp, f_debug, f_corr_out, f_corr_timing, f_corr_coh, f_pk_srch, f_out, f_test;
#endif
};

template <class T>
void Cls_Rx_Sync_2_1<T>::configure(RX_PHY2_1_SYNC_CFG param)
{
    m_cfg = param;
    //dump_double = m_cfg.SymNumPreamble;
    m_SymNumPreamble = 12;
    m_LenSym = 16;
    m_len_FreqComp = m_cfg.len_freq;
    m_TOScope = m_cfg.TOScope;
    //m_Mfsk = m_cfg.Mfsk;
    m_Fs = m_cfg.Fs;
    m_dsss_num = m_cfg.dsss_num;
    m_SyncCombinePower = 0;//m_cfg.SyncCombinePower;
    m_seg_num = m_cfg.seg_num;
    m_freq_drift_len = m_cfg.freq_drift_len;
    m_symdata_len = m_cfg.Sym_Num_Data;
    m_FSK_Setting[0] = 1.5;
    m_FSK_Setting[1] = -1.5;

    m_dseq = safe_new(m_dseq, m_dsss_num);

    if(m_dsss_num == 1)
        mem_copy(m_dseq, DSeq_1, m_dsss_num);
    else if(m_dsss_num == 2)
        mem_copy(m_dseq, DSeq_2, m_dsss_num);
    else if(m_dsss_num == 4)
        mem_copy(m_dseq, DSeq_4, m_dsss_num);
    else if(m_dsss_num == 8)
        mem_copy(m_dseq, DSeq_8, m_dsss_num);
    else if(m_dsss_num == 16)
        mem_copy(m_dseq, DSeq_16, m_dsss_num);
    else if(m_dsss_num == 32)
        mem_copy(m_dseq, DSeq_32, m_dsss_num);

    m_PskPreamble = safe_new(m_PskPreamble, m_SymNumPreamble);
    for (unsigned int ck0 = 0; ck0 < m_SymNumPreamble; ck0++)
    {
        m_PskPreamble[ck0] = PskPreamble[ck0 % 12];
    }
    for (unsigned int ck0 = 0; ck0 < m_LenSym; ck0++)
    {
        m_FSK_LOC[ck0].I = cos(2 * PI * (4 * ck0 + 2) * -1.5 / m_LenSym / 4.0);
        m_FSK_LOC[ck0].Q = sin(2 * PI * (4 * ck0 + 2) * -1.5 / m_LenSym / 4.0);
        //LOG2DEV << m_FSK_LOC[ck0].I << "\t" <<m_FSK_LOC[ck0].Q<<std::endl;
    }
    m_sync_len = m_SymNumPreamble * m_LenSym * m_dsss_num + m_TOScope;
    m_sync_len_set = m_sync_len;//m_SymNumPreamble * m_LenSym * m_dsss_num + m_TOScope;
    m_sync_out_len = (m_symdata_len + 1) * m_LenSym * m_dsss_num;
}


template <class T>
void Cls_Rx_Sync_2_1<T>::init(char* module_name)
{

    //configure(param);
    m_cnt_in = 0;
    m_tmpCorr2 = 0.0;
    m_tmpCorr = 0.0;
    m_cnt = 0;
    m_en = 0;
    m_avg_power = 0;
    m_out_reset = 0;
    m_out_en_new = 0;

    //m_FskFreqsLoc = safe_new(m_FskFreqsLoc, m_Mfsk, m_LenSym);

    m_correctionTiming = safe_new(m_correctionTiming, 4);
    m_sync = safe_new(m_sync, m_sync_len + 1);
    m_sync_n = safe_new(m_sync_n, m_sync_len + 1);
    //m_sigFltFreqComp = safe_new(m_sigFltFreqComp, m_sync_len);
    m_tmpS = safe_new(m_tmpS, m_LenSym);
    m_symRx = safe_new(m_symRx, m_SymNumPreamble);
    m_sym_dss = safe_new(m_sym_dss, m_LenSym);
    //m_out = safe_new(m_out, m_sync_len);
    m_symRx_tmp_diff = safe_new(m_symRx_tmp_diff, m_SymNumPreamble - 2);
    m_DriftComp = safe_new(m_DriftComp, m_freq_drift_len);
    m_sync_ddc_in = safe_new(m_sync_ddc_in, m_LenSym * m_dsss_num);
    m_out_temp = safe_new(m_out_temp, m_sync_len);
    m_out_in_seg = safe_new(m_out_in_seg, m_sync_len);
    m_out_n = safe_new(m_out_n, m_sync_len);

    fix_set(m_in, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);   //statistic_set(m_in, "m_in", 1, 1000);
    fix_set(m_temp_for_debug, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);   //statistic_set(m_in, "m_in", 1, 1000);
    fix_set(m_temp_new, 11, 2, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(m_temp_new, "m_temp_new", 1, 100);
    fix_set(m_symRx_tmp, 15, 6, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_symRx_tmp, "m_symRx_tmp", 1, 100);
    fix_set(m_symRx_power, 18, 2, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_symRx_power, "m_symRx_power", 1, 100);

    fix_set_group(m_symRx, m_SymNumPreamble, 11, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set_group(m_FreqComp, m_cfg.len_freq, 16, 12, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set_group(m_DriftComp, m_cfg.freq_drift_len, 16, 12, FXP_TC, FXP_RND, FXP_SAT_SYM);

    fix_set(m_freq_start, 16, 12, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_tmpCorr, "m_tmpCorr", 1, 100);
    fix_set(m_fsk_loc_t,  10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_freq_inter, 16, 12, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_tmpCorr, "m_tmpCorr", 1, 100);
    fix_set(m_freq_drift_s, 16, 12, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_tmpCorr, "m_tmpCorr", 1, 100);
    fix_set(m_freq_drift_i, 16, 12, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_tmpCorr, "m_tmpCorr", 1, 100);

    fix_set(m_tmpCorr, 19, 9, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_tmpCorr, "m_tmpCorr", 1, 100);
    fix_set(m_mult, 10, -4, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_mult, "m_mult", 1, 100);
    fix_set(m_tmpCorr_power, 15, 5, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_tmpCorr_power, "m_tmpCorr_power", 1, 100);

    fix_set(m_tmpCorr2,18, 5, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_tmpCorr2, "m_tmpCorr2", 1, 100);
    fix_set(m_corrMat_nocoh,18, 5, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_corrMat_nocoh, "m_corrMat_nocoh", 1, 100);
    fix_set(m_corrMat_coh,20, 7, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_corrMat_coh, "m_corrMat_coh", 1, 100);
    fix_set(m_sum_even, 18, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sum_even, "m_sum_even", 1, 100);
    fix_set(m_sum_odd, 18, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sum_odd, "m_sum_odd", 1, 100);

    fix_set(m_timing_phs_tmp, 26, 2, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_timing_phs_tmp, "m_timing_phs_tmp", 1, 100);
    fix_set(m_timing_phs, 10, 0, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_timing_phs, "m_timing_phs", 1, 1);
    fix_set(m_avg_power, 15, 8, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM);//statistic_set(m_avg_power, "m_avg_power", 1, 1);
    fix_set(m_avg_power_tmp, 15, 2, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_avg_power_tmp, "m_avg_power_tmp", 1, 1);

//    fix_set(m_angle_tmp, 8, 3, FXP_TC, FXP_RND, FXP_WRAP);//statistic_set(m_angle_tmp, "m_angle_tmp", 1, 100);
    fix_set(m_out, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_out, "m_out", 1, 100);
    fix_set(m_Fs_frac, 17, -9, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_Fs_frac, "m_Fs_frac", 1, 1);
    fix_set_group(m_FSK_Setting, 2, 5, 4, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_FSK_Setting, "m_FSK_Setting", 1, 100);
    fix_set_group(m_symRx_tmp_diff, m_SymNumPreamble - 2, 22, 2, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_symRx_tmp_diff, "m_symRx_tmp_diff", 1, 100);
    fix_set_group(m_correctionTiming, 4, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_angle, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);//statistic_set(m_angle, "m_angle", 1, 100);
    fix_set(m_FreqOffsetDriftTabletmp  , 20, -19, FXP_US, FXP_RND, FXP_SAT_SYM); //statistic_set(m_FreqOffsetDriftTabletmp,"m_FreqOffsetDriftTabletmp", 1, 1);//%%%%%%%%%debug
    fix_set(m_drift_fix  , 10, -18, FXP_TC, FXP_RND, FXP_SAT_SYM);// statistic_set(m_drift_fix,"m_drift_fix", 1, 1);//%%%%%%%%%debug

    fix_set(m_angle_tmp, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
    fix_set(m_angle_tmp_o, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
    fix_set(m_angle_tmp_n, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
#ifdef MATLAB_DUMP_SYNC
    char filename[300] = {};
    strcpy(filename, module_name); strcat(filename, "corrMat_nocoh_c.txt"); file_o_corrmat_nocoh.open(filename);
    strcpy(filename, module_name); strcat(filename, "corrMat_coh.txt"); file_o_corrMat_coh.open(filename);
    strcpy(filename, module_name); strcat(filename, "sync_in.txt"); file_o_sync_in.open(filename);
    strcpy(filename, module_name); strcat(filename, "data_o.txt"); file_o_data_o.open(filename);
    //strcpy(filename, module_name); strcat(filename, "in.dat"); file_in.open(filename);
#endif
/*
    m_drift_fix = 30 / 15625.0 /15625.0;
    m_drift_fix = 15 / 15625.0 /15625.0;
    m_drift_fix = 5 / 15625.0 /15625.0;
    m_drift_fix = -20 / 15625.0 /15625.0;
    m_drift_fix = -20 / 15625.0 /15625.0;
    m_drift_fix = 7.5 / 7812.5/7812.5;
    m_drift_fix = -2.5 / 7812.5/7812.5;
    m_drift_fix = -0.5 / 1953.125/1953.125;
    m_drift_fix = -5 / 1953.125/1953.125;
*/

#ifdef RTL_DUMP_SYNC
    char filename[300] = {};
    strcpy(filename, module_name); strcat(filename, "f_sync_in.dat"); file_in.open(filename);
    strcpy(filename, module_name); strcat(filename, "f_reg.dat"); f_reg.open(filename);
    strcpy(filename, module_name); strcat(filename, "f_ddc_in.dat"); f_ddc_in.open(filename);
    strcpy(filename, module_name); strcat(filename, "f_ddc_out.dat"); f_ddc_out.open(filename);
    strcpy(filename, module_name); strcat(filename, "f_debug.dat"); f_debug.open(filename);
    strcpy(filename, module_name); strcat(filename, "f_angle_tmp.dat"); f_angle_tmp.open(filename);
    strcpy(filename, module_name); strcat(filename, "f_corr_out.dat"); f_corr_out.open(filename);
    strcpy(filename, module_name); strcat(filename, "f_corr_timing.dat"); f_corr_timing.open(filename);
    strcpy(filename, module_name); strcat(filename, "f_corr_coh.dat"); f_corr_coh.open(filename);
    strcpy(filename, module_name); strcat(filename, "f_pk_srch.dat"); f_pk_srch.open(filename);
    strcpy(filename, module_name); strcat(filename, "f_out.dat"); f_out.open(filename);
    strcpy(filename, module_name); strcat(filename, "f_test.dat"); f_test.open(filename);
#endif

    m_cordic.init(33);
    reset();

#ifdef RTL_DUMP_SYNC
    fxp rtl_dump1, rtl_dump3;
    fix_set(rtl_dump1,   4,  4, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump3,  16, 16, FXP_US, FXP_RND, FXP_SAT_SYM);
    rtl_dump3=m_seg_num;
    dump(f_reg, &rtl_dump3, 1, rtl_dump3, 0);
    rtl_dump3=m_symdata_len;
    dump(f_reg, &rtl_dump3, 1, rtl_dump3, 0);
    rtl_dump1= m_len_FreqComp==1 ? 0 : m_len_FreqComp==2 ? 1 : m_len_FreqComp==4 ? 2 : m_len_FreqComp==8 ? 3 : m_len_FreqComp==16 ? 4 : m_len_FreqComp==32 ? 5 : 6;
    dump(f_reg, &rtl_dump1, 1, rtl_dump1, 0);
    rtl_dump1= m_freq_drift_len==1 ? 0 : m_freq_drift_len==2 ? 1 : 2;
    dump(f_reg, &rtl_dump1, 1, rtl_dump1, 0);
    rtl_dump1= m_TOScope==8 ? 0 : m_TOScope==16 ? 1 : m_TOScope==32 ? 2 : m_TOScope==64 ? 3 : m_TOScope==128 ? 4 : 5;
    dump(f_reg, &rtl_dump1, 1, rtl_dump1, 0);
    rtl_dump1= m_dsss_num==1 ? 0 : m_dsss_num==2 ? 1 : m_dsss_num==4 ? 2 : 3;
    dump(f_reg, &rtl_dump1, 1, rtl_dump1, 0);
    dump(f_reg, &m_freq_start, 1, m_freq_start, 0);
    dump(f_reg, &m_freq_inter, 1, m_freq_inter, 0);
    dump(f_reg, &m_freq_drift_s, 1, m_freq_drift_s, 0);
    dump(f_reg, &m_freq_drift_i, 1, m_freq_drift_i, 0);
    rtl_dump1= 0;
    dump(f_reg, &rtl_dump1, 1, rtl_dump1, 0);
    rtl_dump3 = m_Fs==1953.125 ? 16384 : m_Fs==3906.25 ? 8192 : m_Fs==7812.5 ? 4096 : m_Fs==15625 ? 2048 : m_Fs==31250 ? 1024 :  m_Fs==62500 ? 512 : m_Fs==125000 ? 256 : m_Fs==250000 ? 128 : 64;
    dump(f_reg, &rtl_dump3, 1, rtl_dump3);
#endif
}

template <class T>
void Cls_Rx_Sync_2_1<T>::go(Complex<double> in)
{
    sync_fifo(in); // Save data to RAM
    Sync_Cor();  // Input: From RAM ;Output: m_avg_power, m_timing_phs, m_estTimingOffset, m_EstFreqOffset,m_correctionTiming;
    Sync_out();
#ifdef RTL_DUMP_SYNC
    if(m_out_valid) dump(f_out, &m_out, 1, m_out.I);
#endif
}


template <class T>
void Cls_Rx_Sync_2_1<T>::reset()
{
    m_out_valid = 0;
    m_tmpCorr2 = 0.0;
    m_tmpCorr = 0.0;
    m_cnt = 0;
    m_en = 0;
    m_avg_power = 0;
    m_out_en = 0;
    m_out_cnt = 0;
    m_cnt_out_total = 0;
    m_finished = 0;
    m_cnt_seg = 0;


    m_sync_cnt_en = 0;
    m_Fs_frac = 2.0 / m_Fs;
    m_FreqOffsetDriftTabletmp = 2.0 / (m_Fs * m_Fs);
    m_freq_start = m_cfg.freq_start;
    m_freq_inter = m_cfg.freq_inter;
    m_freq_drift_s = m_cfg.freq_drift_s;
    m_freq_drift_i = m_cfg.freq_drift_i;
    m_TOScope_set = m_TOScope;
    m_SymNumPreamble_set = m_SymNumPreamble;
    m_sync_len_set = m_sync_len;
    m_ddc_out = 0;
    m_out_en_new = 0;

    //dump(f_test, &m_Fs_frac, 1, m_Fs_frac);
//    dump(f_test, &m_FreqOffsetDriftTabletmp, 1, m_FreqOffsetDriftTabletmp);

    for(unsigned int ck0 = 0; ck0 < m_cfg.len_freq; ck0++)
    {
        m_FreqComp[ck0] = (double)m_freq_start + (double)m_freq_inter * ck0;
        //LOG2DEV<<m_FreqComp[ck0]<<endl;
    }

    for(unsigned int ck0 = 0; ck0 < m_cfg.freq_drift_len; ck0++)
    {
        m_DriftComp[ck0] = (double)m_freq_drift_s + (double)m_freq_drift_i * ck0;
        //LOG2DEV<<m_cfg.freq_drift_s<<"\t"<<m_cfg.freq_drift_i<<"\t"<<ck0<<"\t"<<m_cfg.freq_drift_s + m_cfg.freq_drift_i * ck0<<"\t"<<m_DriftComp[ck0] <<endl;
    }
    //zeros(m_FskFreqsLoc, m_Mfsk, m_LenSym);
    zeros(m_correctionTiming, 4);
    zeros(m_sync, m_sync_len);
    zeros(m_sync_n, m_sync_len);
    //zeros(m_sigFltFreqComp, m_sync_len);
    zeros(m_tmpS, m_LenSym);
    zeros(m_symRx, m_SymNumPreamble);
    zeros(m_symRx_tmp_diff, m_SymNumPreamble - 2);
    zeros(m_sym_dss, m_LenSym);

    //GenFskFreqs();
}

template <class T>
Complex<double> Cls_Rx_Sync_2_1<T>::rot(double input)
{
    Complex<double> output;

    output.I = cos(input * 2 * PI);
    output.Q = sin(input * 2 * PI);

    return output;
}

template <class T>
void Cls_Rx_Sync_2_1<T>::sync_fifo(Complex<double> in)
{
    m_cnt_in++;
    //LOG2DEV << m_cnt_in<< "\t" << in.I <<"\t" << in.Q <<endl;
    //if(m_cnt_in == 1508)
        //int a= 1;
    if (m_cnt < m_sync_len)
    {
        m_in = in;
        //dump(file_in, &m_in, 1, m_in.I);
        m_sync[m_cnt] = m_in;
        //cout<<m_sync[m_cnt].I<<"\t"<<m_sync[m_cnt].Q<<endl;
        m_cnt++;
        if (m_cnt == m_sync_len)
        {
            m_en = 1;
            m_out_init = 1;
        }
    }
    else
    {
        m_in = in;
    }

    #ifdef RTL_DUMP_SYNC
        dump(file_in, &m_in, 1, m_in.I);
    #endif

    //file_o_matlab << m_in.I << "\t" << m_in.Q << endl;
}

template <class T>
void Cls_Rx_Sync_2_1<T>::Sync_Cor()
{
    if (m_en)
    {
        m_val = 0;
        m_pos = 0;
        m_pos_ddc = 0;
        for (unsigned int ck1 = 0; ck1 < m_TOScope_set; ck1++)
        {
            for(unsigned int ck2 = 0; ck2 < m_freq_drift_len; ck2++)
            {
                for (unsigned int ck0 = 0; ck0 < m_len_FreqComp; ck0++)
                {
                    dem_timing_offset(ck1, ck0, ck2);//Input:m_sigFltFreqComp,m_FskFreqsLoc, m_PskPreamble Output:m_timing_phs_tmp,m_corrMat_nocoh,m_corrMat_coh;
                    Sync_PeakSrh(m_corrMat_nocoh, m_corrMat_coh, ck1, ck0, ck2);//Input m_corrMat_nocoh, m_corrMat_coh; Output: m_avg_power, m_timing_phs, m_estTimingOffset, m_EstFreqOffset,m_correctionTiming;
                }
            }
        }

        m_en = 0;

#ifdef PHY2_1_SYNC_PRINT
        double PeakRatio = 0;
        PeakRatio = m_val ;
        cout << "SyncPeakRatio = " << PeakRatio << "\t";
        cout << "m_corrMat_nocoh = " << m_val1 << endl;
        cout << "m_corrMat_coh = " << m_val2 << endl;
        cout << "m_DDC = " << m_FreqComp[ m_pos_ddc] << "\t";
        if(m_freq_drift_len != 0)
            cout << "m_drift = " << m_DriftComp[ m_pos_drift] << "\t";
        else
            cout << "m_drift = " << 0 << "\t";
        cout << "m_pos = " << m_pos << "\t";
        cout << "m_timing_phs_freq = " << m_timing_phs_freq.I << "\t" << m_timing_phs_freq.Q << endl;
        cout << "m_avg_power = " << 1.0 / (double)m_avg_power << "\n";
        cout << "m_timing_phs = " << m_timing_phs_print << "\n";
        cout << "m_correctionTiming[0] = " << m_correctionTiming[0].I << "\t" << m_correctionTiming[0].Q << endl;
        cout << "m_correctionTiming[1] = " << m_correctionTiming[1].I << "\t" << m_correctionTiming[1].Q << endl;
        cout << "m_correctionTiming[2] = " << m_correctionTiming[2].I << "\t" << m_correctionTiming[2].Q << endl;
        cout << "m_correctionTiming[3] = " << m_correctionTiming[3].I << "\t" << m_correctionTiming[3].Q << endl;
#endif
        m_out_en = 1;
        //m_out_init = 1;
    }
}

template <class T>
void Cls_Rx_Sync_2_1<T>::dem_timing_offset(int times, int ddc_pos, int drift_pos)
{
    Complex<double> temp;
    Complex<double> temp_2;
    Complex<double> temp_old;
    Complex<double> temp_new;
    Complex<double> temp_for_debug;
    int loc = 0;
    int loc_new = 0;
    m_tmpCorr2 = 0.0;
    m_tmpCorr = 0.0;
    int cnt = 0;
    //double temp_output = 0;
    m_angle = 1.0 * (m_FreqComp[ddc_pos] ) * m_Fs_frac;
    m_cfg_ddc.ang_delta = m_angle;
    loc_new = (0)*m_LenSym + times;
    m_cfg_ddc.ang_init = loc_new * m_cfg_ddc.ang_delta;    //dump(f_debug, &m_cfg_ddc.ang_init, 1, m_angle);
    m_ddc.config(m_cfg_ddc);
    m_ddc.init();
    m_drift_fix =  m_DriftComp[drift_pos] * m_FreqOffsetDriftTabletmp;
    m_angle_tmp = times * (times + 1) / 2 * m_drift_fix;     //dump(f_debug, &m_angle_tmp, 1, m_angle);
    for (unsigned int ck1 = 0; ck1 < m_SymNumPreamble_set; ck1++)
    {
        m_symRx_tmp = 0.0;
        cnt = 0;
        for (unsigned int ck2 = 0; ck2 < m_dsss_num; ck2++)
        {
            for (unsigned int ck0 = 0; ck0 < m_LenSym ; ck0++)
            {
                //m_tmpS[ck0] = m_sigFltFreqComp[(ck1) * m_LenSym + m_DelayFilter_Sim + times + ck0];
                //temp_old = m_FskFreqsLoc[m_FskPreamble[ck1 % 2] - 1][ck0] * m_tmpS[ck0] * m_PskPreamble[ck1] * 1.0;
                //cout<<"phase_old = "<<m_FreqComp[ddc_pos] * -1.0 * loc / m_Fs + m_FSK_Setting[ck1%2] * (ck0 % m_LenSym) / m_LenSym * -1.0<<endl;
                //temp_old = m_sync[loc] * rot(m_FreqComp[ddc_pos] * -1.0 * loc / m_Fs + m_FSK_Setting[ck1%2] * (ck0 % m_LenSym) / m_LenSym * -1.0)* m_PskPreamble[ck1];

                loc = (ck1) * m_LenSym * m_dsss_num + ck2 * m_LenSym + times + ck0;

                //m_ddc.ang_delta(2 * m_FSK_Setting[ck1%2] * (ck0 % m_LenSym) / m_LenSym * -1.0 + loc_new * m_cfg_ddc.ang_delta, 4);
                //m_angle_tmp = 2 * m_FSK_Setting[ck1 % 2]  / m_LenSym / 4 * -1.0 * (4 * ck0 + 2);
                m_angle_tmp = m_angle_tmp + loc * m_drift_fix;// + 2 * m_FSK_Setting[ck1 % 2]  / m_LenSym / 4 * -1.0 * (4 * ck0 + 2);
				//	dump(f_debug, &m_drift_fix, 1, m_drift_fix);
				//	dump(f_debug, &m_angle_tmp, 1, m_angle_tmp);
                m_ddc.ang_delta(m_angle_tmp , 4);
                m_sync_ddc_in[cnt++] = m_ddc.rot(m_sync[loc]);// * m_dseq[ck2];

                //m_sym_dss[ck0] =  m_sym_dss[ck0] + m_temp_for_debug;


#ifdef RTL_DUMP_SYNC
	        dump(f_angle_tmp, &m_angle_tmp, 1, m_angle);
	        dump(f_ddc_in, &m_sync[loc], 1, m_in.I);
			m_temp_for_debug = m_sync_ddc_in[cnt-1];
	        dump(f_ddc_out, &m_temp_for_debug, 1, m_temp_for_debug.I);
#endif

                //LOG2DEV<<m_sync[loc].I<<"\t"<<m_sync[loc].Q<<endl;
                //if(times == 1)
                //{
                //LOG2DEV <<cnt - 1<<"\t"<< m_sync_ddc_in[loc % (m_dsss_num * m_LenSym)].I << "\t" << m_sync_ddc_in[loc % (m_dsss_num * m_LenSym)].Q<<endl;
                //    LOG2DEV<<temp_for_debug.I<<"\t"<<temp_for_debug.Q<<endl;
               // }

                //LOG2DEV<<"next"<<endl;
                //cout<<m_sync[loc].I<<"\t"<<m_sync[loc].Q<<endl;
                //if (abs(m_symRx_tmp.I) > 3)
                //    int a = 1;
                //cout << temp_old.I << "\t" << temp_old.Q << endl;
                //cout << m_temp_new.I << "\t" << m_temp_new.Q << endl;
                //cout << m_sigFltFreqComp[(ck1)*m_LenSym + m_DelayFilter_Sim + times + ck0].I << "\t" << m_sigFltFreqComp[(ck1)*m_LenSym + m_DelayFilter_Sim + times + ck0].Q << endl;
            }
            //m_sym_dss[ck0] = m_sym_dss[ck0] / m_dsss_num ;
        }
        zeros(m_sym_dss, m_LenSym);
        Complex<double> temp;
        for (unsigned int ck0 = 0; ck0 < m_LenSym ; ck0++)
        {
            for (unsigned int ck1 = 0; ck1 < m_dsss_num ; ck1++)
            {
                m_sym_dss[ck0] = m_sym_dss[ck0] + m_sync_ddc_in[m_LenSym * ck1 + ck0] * m_dseq[ck1];
                temp = m_sync_ddc_in[m_LenSym * ck1 + ck0] * m_dseq[ck1];
            }
            m_sym_dss[ck0] = m_sym_dss[ck0] / m_dsss_num;
            //LOG2DEV << m_sym_dss[ck0].I <<"\t"<<m_sym_dss[ck0].Q<<endl;
        }
        //LOG2DEV<<"sym"<<std::endl;
        for (unsigned int ck0 = 0; ck0 < m_LenSym ; ck0++)
        {
	    m_fsk_loc_t = m_FSK_LOC[ck0] ;
            if(ck1 % 2 == 0)
                m_temp_new = m_sym_dss[ck0] * m_PskPreamble[ck1] * m_fsk_loc_t;
            else
                m_temp_new = m_sym_dss[ck0] * m_PskPreamble[ck1] * m_fsk_loc_t.conjug();
            //LOG2DEV<<m_temp_new.I<<"\t"<<m_temp_new.Q<<"\t"<<m_FSK_LOC[ck0].I<<"\t"<<m_FSK_LOC[ck0].Q <<"\t"<< m_temp_new.I <<"\t"<<m_temp_new.Q<<endl;
            m_symRx_tmp = m_symRx_tmp.todouble() + m_temp_new.todouble();// +m_FskFreqsLoc[m_FskPreamble[ck1 % 2] - 1][ck0] * m_tmpS[ck0] * m_PskPreamble[ck1] * 1.0;
	    fxp rtl_dump;
	    fix_set(rtl_dump, 13, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
//	    dump(f_test, &m_sym_dss[ck0], 1, rtl_dump);
//	    dump(f_test, &m_temp_new, 1, m_temp_new.I);
//	    dump(f_test, &m_symRx_tmp, 1, m_symRx_tmp.I);
        }
        m_symRx[ck1] = m_symRx_tmp /m_LenSym * 1.0;
        //LOG2DEV<<m_symRx[ck1].I<<"\t"<<m_symRx[ck1].Q<<endl;
        m_symRx_power = m_symRx[ck1].power();
        m_tmpCorr2 = m_tmpCorr2 + m_symRx_power;

        m_tmpCorr = m_tmpCorr.todouble() + m_symRx[ck1].todouble();

#ifdef RTL_DUMP_SYNC
	dump(f_corr_out, &m_symRx[ck1], 1, m_symRx[ck1].I, 0);
	dump(f_corr_out, &m_symRx_power, 1, m_symRx_power);
#endif
        //cout << m_symRx[ck1].I << "\t" << m_symRx[ck1].Q << "\t" << endl;
    }
    diff();
    m_timing_phs_tmp = m_sum_odd.todouble() * m_sum_even.todouble().conjug();
    m_corrMat_nocoh = m_tmpCorr2;
    m_tmpCorr_power = m_tmpCorr.todouble();
    //cout<<m_tmpCorr_power.I<<"\t"<<m_tmpCorr_power.Q<<endl;
    //dump(file_o_matlab, &m_tmpCorr_power, 1);
    m_corrMat_coh = m_tmpCorr_power.power();
    //LOG2DEV<<m_corrMat_coh<<endl;

#ifdef RTL_DUMP_SYNC
    dump(f_corr_timing, &m_sum_odd, 1, m_sum_odd.I, 0);
    dump(f_corr_timing, &m_sum_even, 1, m_sum_even.I);
    dump(f_corr_coh, &m_timing_phs_tmp, 1, m_timing_phs_tmp.I,0);
    dump(f_corr_coh, &m_corrMat_nocoh, 1, m_corrMat_nocoh, 0);
    dump(f_corr_coh, &m_corrMat_coh, 1, m_corrMat_coh);
#endif
#ifdef MATLAB_DUMP_SYNC
    file_o_corrmat_nocoh << m_corrMat_nocoh<< endl;
    file_o_corrMat_coh << m_corrMat_coh<< endl;
    //LOG2DEV<<m_corrMat_coh<<endl;
    //cout << m_corrMat_nocoh<< endl;
#endif
}

template <class T>
void Cls_Rx_Sync_2_1<T>::diff()
{
    m_sum_odd = 0.0;
    m_sum_even = 0.0;
    for (unsigned int ck0 = 0; ck0 < m_SymNumPreamble_set - 2; ck0++)
    {
        m_symRx_tmp_diff[ck0] = m_symRx[ck0 + 1] * m_symRx[ck0].conjug();
        //LOG2DEV<<m_symRx_tmp_diff[ck0].I<<"\t"<<m_symRx_tmp_diff[ck0].Q<<endl;
        if (ck0 % 2 == 0)
            m_sum_odd = m_sum_odd.todouble() + m_symRx_tmp_diff[ck0].todouble();
        else
            m_sum_even = m_sum_even.todouble() + m_symRx_tmp_diff[ck0].todouble();
    }
}

template <class T>
void Cls_Rx_Sync_2_1<T>::Sync_PeakSrh(double val1, double val2, int times, int DDC_times, int drift_times)
{
    double temp = 0;
    double m_avg_power_tmp_double = 0;
    double cordic_o =0;
    if (m_SyncCombinePower == 0)
        temp = val2;
    else
        temp = val1;
    m_val1 = val1;
    m_val2 = val2;
    //cout<<temp <<"\t"<<val1<<endl;
    if (temp > m_val)
    {
        m_val = temp;
        m_pos = times;
        m_pos_ddc = DDC_times;
        m_pos_drift = drift_times;
        m_timing_phs_freq = m_timing_phs_tmp;
        m_avg_power_tmp = val1;
        m_corrMaxInT = m_val;
        m_iTMax = m_pos;
        m_avg_power_tmp_double = m_avg_power_tmp;
        m_avg_power = m_SymNumPreamble_set / m_avg_power_tmp_double * 1.0;
        m_timing_phs = atan2(m_timing_phs_freq.Q, m_timing_phs_freq.I) / 2.0;
        m_timing_phs_print = (double)m_timing_phs * 2;
        //LOG2DEV<<m_timing_phs * 2<<endl;
        //m_cordic.vector(cordic_o, m_timing_phs_freq, 33);

        cordic_o = m_cordic.vector(m_timing_phs_freq, 16);
        m_mult = 1/ 24.0;

        m_timing_phs = cordic_o  /   8.0 ; m_correctionTiming[0] = rot(m_timing_phs);
        //LOG2DEV << m_correctionTiming[0].I <<"\t"<< m_correctionTiming[0].Q << endl;
        m_timing_phs = cordic_o  * m_mult ; m_correctionTiming[1] = rot(m_timing_phs);
        //LOG2DEV << m_correctionTiming[1].I <<"\t"<< m_correctionTiming[0].Q << endl;
        m_timing_phs = cordic_o  * m_mult * -1 ; m_correctionTiming[2] = rot(m_timing_phs);
        //LOG2DEV << m_correctionTiming[2].I <<"\t"<< m_correctionTiming[0].Q << endl;
        m_timing_phs = cordic_o  /  -8.0 ; m_correctionTiming[3] = rot(m_timing_phs);
        //LOG2DEV << m_correctionTiming[3].I <<"\t"<< m_correctionTiming[0].Q << endl;

        m_estTimingOffset = m_iTMax;
        m_EstFreqOffset = m_FreqComp[m_pos_ddc];

#ifdef RTL_DUMP_SYNC
        fxp rtl_dump4, rtl_dump5, rtl_dump6;
    	fix_set(rtl_dump4, 8, 8, FXP_US, FXP_RND, FXP_SAT_SYM);
    	fix_set(rtl_dump5, 23,  1, FXP_TC, FXP_RND, FXP_SAT_SYM);
        fix_set(rtl_dump6, 23, 11, FXP_US, FXP_RND, FXP_SAT_SYM);
    	rtl_dump4=m_pos;
    	dump(f_pk_srch, &rtl_dump4, 1, rtl_dump4, 0);
//    	rtl_dump4=m_pos_ddc;
//    	dump(f_pk_srch, &rtl_dump4, 1, rtl_dump4, 0);
    	dump(f_pk_srch, &m_corrMaxInT,  1, m_corrMat_coh, 0);
    	dump(f_pk_srch, &m_avg_power,  1, m_avg_power,0);
        dump(f_pk_srch, &cordic_o,  1, rtl_dump5,0);
        dump(f_pk_srch, &m_correctionTiming[0],  1, m_correctionTiming[0].I,0);
        dump(f_pk_srch, &m_correctionTiming[1],  1, m_correctionTiming[1].I,0);
        dump(f_pk_srch, &m_correctionTiming[2],  1, m_correctionTiming[2].I,0);
        dump(f_pk_srch, &m_correctionTiming[3],  1, m_correctionTiming[3].I);
//	dump(f_pk_srch, &m_EstFreqOffset,  1, m_correctionTiming[3],0);

        //for(int ck=0; ck<129; ck=ck+1){
	//  m_timing_phs = ck/512.0;
	//  m_correctionTiming[0] = rot(m_timing_phs);
	//  dump(f_debug, &m_correctionTiming[0], 1, m_correctionTiming[0].I);
	//}
#endif

    }

}

template <class T>
void Cls_Rx_Sync_2_1<T>::Sync_new()
{

    if(m_sync_cnt_en)
    {
        m_sync_n[m_sync_cnt] = m_out_sync_in;
        m_sync[m_sync_cnt++] = m_out_sync_in;
//#ifdef MATLAB_DUMP_SYNC
//      file_o_sync_in<< m_out_sync_in.I  << "\t" <<m_out_sync_in.Q<<endl;
//#endif
        if (m_sync_cnt == m_sync_len_set)
        {
            m_en = 1;
            m_out_reset = 1;
            m_TOScope_set  = 1;
            m_SymNumPreamble_set = 10;
            m_sync_cnt_en = 0;
            m_pos = 0;
            Sync_Cor();  // Input: From RAM ;Output: m_avg_power, m_timing_phs, m_estTimingOffset, m_EstFreqOffset,m_correctionTiming;
            //Sync_out_new();
            m_out_en_new = 1;
        }
    }
}

template <class T>
void Cls_Rx_Sync_2_1<T>::Sync_out_new()
{
    if(m_out_en_new)
    {
        if(m_out_reset == 1)
        {
            m_out_reset = 0;
            if(m_freq_drift_len != 0)
                m_angle = 1.0 * (m_FreqComp[m_pos_ddc])* m_Fs_frac;
            else
                m_angle = 1.0 * (m_FreqComp[m_pos_ddc])* m_Fs_frac;
            m_ddc_out = m_angle;   //dump(f_test, &m_angle, 1, m_angle);
            m_cfg_ddc.ang_delta = m_ddc_out;
            m_cfg_ddc.ang_init = 0;//(m_pos + (m_SymNumPreamble - 1) * m_dsss_num  * m_LenSym) * m_cfg_ddc.ang_delta;//loc_new * m_cfg_ddc.ang_delta;
            m_ddc_n.config(m_cfg_ddc);
            m_ddc_n.init();
            m_cnt_drift_n = 0 ;
            m_angle_tmp_n = 0;
            m_p_out_n = 0;
            m_p_in_n = -1;
            for(unsigned int ck0 = ((m_SymNumPreamble - 1) * m_LenSym * m_dsss_num ); ck0 < m_sync_len_set;ck0++)
            {
	        	m_drift_fix = m_DriftComp[m_pos_drift] * m_FreqOffsetDriftTabletmp;
                m_angle_tmp_n = m_angle_tmp_n + m_cnt_drift_n*  m_drift_fix;// + 2 * m_FSK_Setting[ck1 % 2]  / m_LenSym / 4 * -1.0 * (4 * ck0 + 2);
                m_ddc_n.ang_delta(m_angle_tmp_n , 4);
                    //LOG2DEV<<m_angle_tmp<<endl;
                    //LOG2DEV<<m_sync[ck0].I <<"\t" <<m_sync[ck0].Q<<endl;
                m_out_n[m_cnt_drift_n]= m_ddc_n.rot(m_sync[ck0]);// =  m_ddc.rot( m_sync[ck0] );
                //LOG2DEV<<m_out_n[m_cnt_drift_n].I <<"\t" <<m_out_n[m_cnt_drift_n].Q<<endl;
//  		dump(f_test, &m_sync[ck0], 1, m_in.I);
//    		dump(f_test, &m_angle_tmp_n, 1, m_angle_tmp_n);
//    		dump(f_test, &m_out_n[m_cnt_drift_n], 1, m_in.I);
                m_cnt_drift_n++;
            }
            m_cnt_out_offset_n = m_cnt_drift_n ;

            m_out = m_out_n[m_p_out_n];
            m_p_in_n = m_p_out_n;
            m_p_out_n++;
            if(m_p_out_n == m_cnt_out_offset_n)
                m_p_out_n = 0;
            m_cnt_out_total_n = 0;//(m_SymNumPreamble - 1) * m_LenSym * m_dsss_num + m_cnt_drift_n - 1;
            m_out_valid = 1;
        }
        else
        {
	    	m_drift_fix = m_DriftComp[m_pos_drift] * m_FreqOffsetDriftTabletmp;
            m_angle_tmp_n = m_angle_tmp_n + m_cnt_drift_n * m_drift_fix;//m_DriftComp[m_pos_drift] * m_FreqOffsetDriftTabletmp;//
            m_ddc_n.ang_delta(m_angle_tmp_n , 4);

            //m_angle_tmp = m_angle_tmp + m_cnt_drift_n * m_DriftComp[m_pos_drift] * m_FreqOffsetDriftTabletmp;// + 2 * m_FSK_Setting[ck1 % 2]  / m_LenSym / 4 * -1.0 * (4 * ck0 + 2);
            m_out_n[m_p_in_n] = m_ddc_n.rot(m_out_sync_in.todouble());//m_ddc.rot(m_out.todouble());

			//dump(f_test, &m_out_sync_in, 1, m_in.I);
			//dump(f_test, &m_angle_tmp_n, 1, m_angle_tmp_o);
			//dump(f_test, &m_out_n[m_p_in_n], 1, m_in.I);

            m_cnt_drift_n++;
            m_out = m_out_n[m_p_out_n];
            m_p_in_n = m_p_out_n;
            m_p_out_n++;
            if(m_p_out_n ==  m_cnt_out_offset_n)
                m_p_out_n = 0;
        }
        m_cnt_out_total_n++;
//        file_o_data_o<<m_out.I<<"\t"<<m_out.Q<<endl;
        if(m_cnt_out_total_n <= (1 + m_symdata_len) * m_LenSym * m_dsss_num )
            m_out_valid = 1;
        else
            m_out_valid = 0;
        if(m_cnt_out_total_n == (1 + m_symdata_len) * m_LenSym * m_dsss_num - m_cnt_out_offset_n + 1)
        {
                //m_cnt_out_total = 0;
            m_cnt_seg++;
            m_sync_cnt_en = 1;
            m_sync_cnt = 0;
            m_sync_len_set = m_SymNumPreamble * m_LenSym * m_dsss_num;
        }
    }
}


template <class T>
void Cls_Rx_Sync_2_1<T>::Sync_out()
{
    if(m_out_en)
    {

        if(m_cnt_seg == m_seg_num){
	    	m_drift_fix =  m_DriftComp[m_pos_drift] * m_FreqOffsetDriftTabletmp;
            m_angle_tmp_n = m_angle_tmp_n + m_cnt_drift_n * m_drift_fix;//m_DriftComp[m_pos_drift] * m_FreqOffsetDriftTabletmp;//
            m_cnt_drift_n++;
            m_ddc_n.ang_delta(m_angle_tmp_n , 4);
            m_out_n[m_p_in_n] = m_ddc_n.rot(m_out_sync_in.todouble());//m_ddc.rot(m_out.todouble());
            m_out = m_out_n[m_p_out_n];
            m_p_in_n = m_p_out_n;
            m_p_out_n++;

            if(m_p_out ==  m_cnt_out_offset + 1)
                m_p_out = 0;
            m_cnt_out_total_n++;
            if(m_cnt_out_total_n <= (1 + m_symdata_len) * m_LenSym * m_dsss_num + 1)
                m_out_valid = 1;
            else
                m_finished = 1;
            if(m_cnt_seg == 1)
            {
                m_cnt_out_total++;
                if(m_cnt_out_total <= (1 + m_symdata_len) * m_LenSym * m_dsss_num + 1)
                    m_out_valid = 1;
                else
                    m_finished = 1;
            }
            return;
        }
        if(m_finished){
            m_out_en = 0;
            m_out_valid = 0;
            m_en = 0;
            //m_finished = 1;
        }

        //temp =  m_sync_len - (m_pos + (m_SymNumPreamble - 1) * m_dsss_num  * m_LenSym);
        if(m_out_init)
        {
            //m_out_temp = safe_new(m_out_temp,temp);
            if(m_freq_drift_len != 0)
                m_angle = 1.0 * (m_FreqComp[m_pos_ddc])* m_Fs_frac;
            else
                m_angle = 1.0 * (m_FreqComp[m_pos_ddc])* m_Fs_frac;
            m_ddc_out = m_angle;   //dump(f_test, &m_angle, 1, m_angle);
            m_cfg_ddc.ang_delta = m_ddc_out;
            m_cfg_ddc.ang_init = 0;//(m_pos + (m_SymNumPreamble - 1) * m_dsss_num  * m_LenSym) * m_cfg_ddc.ang_delta;//loc_new * m_cfg_ddc.ang_delta;
            m_ddc.config(m_cfg_ddc);
            m_ddc.init();
            m_cnt_drift = 0 ;
            m_angle_tmp_o = 0;
            m_p_out = 0;

	    fxp rtl_dbg;
	    fix_set(rtl_dbg, 8, 8, FXP_US, FXP_RND, FXP_SAT_SYM);
	    rtl_dbg = m_pos;

//	    dump(f_test, &rtl_dbg, 1, rtl_dbg);
//
//	    dump(f_test, &m_sync[0], 1, m_in.I);

            if(m_cnt_seg != 0)
                m_pos = 0;
            for(unsigned int ck0 = (m_pos + (m_SymNumPreamble - 1) * m_LenSym * m_dsss_num ); ck0 < m_sync_len_set;ck0++)
            {
	        	m_drift_fix =  m_DriftComp[m_pos_drift] * m_FreqOffsetDriftTabletmp;    //dump(f_test, &m_drift_fix, 1, m_drift_fix);
                m_angle_tmp_o = m_angle_tmp_o + m_cnt_drift*  m_drift_fix;//m_DriftComp[m_pos_drift] * m_FreqOffsetDriftTabletmp;//
                m_ddc.ang_delta(m_angle_tmp_o , 4);
                //LOG2DEV<<m_angle_tmp<<endl;
                //LOG2DEV<<m_sync[ck0].I <<"\t" <<m_sync[ck0].Q<<endl;


                m_out_temp[m_cnt_drift]= m_ddc.rot(m_sync[ck0]);// =  m_ddc.rot( m_sync[ck0] );
                m_out_in_seg[m_cnt_drift] = m_sync[ck0];
//				dump(f_test, &m_sync[ck0], 1, m_in.I);
//				dump(f_test, &m_angle_tmp_o, 1, m_angle_tmp_o);
//				dump(f_test, &m_out_temp[m_cnt_drift], 1, m_in.I);
                m_cnt_drift++;
            }
            m_cnt_out_offset = m_cnt_drift;
            //if(m_cnt_seg != 0)
            //     m_cnt_out_offset = m_cnt_out_offset + 1;
            m_out_init = 0;
            m_p_out = 0;//(m_pos + (m_SymNumPreamble - 1) * m_LenSym);
            m_out = m_out_temp[m_p_out];
            m_out_sync_in = m_out_in_seg[m_p_out];
            m_p_in = m_p_out;
            m_p_out++;
            if(m_p_out == m_cnt_out_offset)
                m_p_out = 0;
            m_cnt_out_total = 0;//(m_SymNumPreamble - 1) * m_LenSym * m_dsss_num + m_cnt_drift - 1;
        }
        else
        {
	    	m_drift_fix =  m_DriftComp[m_pos_drift] * m_FreqOffsetDriftTabletmp;    //dump(f_test, &m_drift_fix, 1, m_drift_fix);
            m_angle_tmp_o = m_angle_tmp_o + m_cnt_drift * m_drift_fix;//m_DriftComp[m_pos_drift] * m_FreqOffsetDriftTabletmp;//
            m_ddc.ang_delta(m_angle_tmp_o , 4);
            m_out_temp[m_p_in] = m_ddc.rot(m_in.todouble());
            m_out_in_seg[m_p_in] = m_in;

			//dump(f_test, &m_in, 1, m_in.I);
			//dump(f_test, &m_angle_tmp_o, 1, m_angle_tmp_o);
			//dump(f_test, &m_out_temp[m_p_in], 1, m_in.I);

            m_cnt_drift++;
            m_out = m_out_temp[m_p_out];
            m_out_sync_in = m_out_in_seg[m_p_out];
            m_p_in = m_p_out;
            m_p_out++;
            if(m_p_out ==  m_cnt_out_offset)
                m_p_out = 0;
        }
        m_cnt_out_total++;
//		file_o_data_o<<m_out.I<<"\t"<<m_out.Q<<endl;

        Sync_new();

        if(m_cnt_out_total <= (1 + m_symdata_len) * m_LenSym * m_dsss_num )
            m_out_valid = 1;
        else
            m_out_valid = 0;
        if(m_cnt_out_total == (1 + m_symdata_len) * m_LenSym * m_dsss_num )
        {
            //m_cnt_out_total = 0;
            m_cnt_seg++;
            m_sync_cnt_en = 1;
            m_sync_cnt = 0;
            m_sync_len_set = m_SymNumPreamble * m_LenSym * m_dsss_num;
        }
        Sync_out_new();
    }
}
