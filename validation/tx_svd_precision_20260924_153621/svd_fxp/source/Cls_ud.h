/****************************************************************************
**
**  Copyright  (c)  2023-2028,  All Rights Reserved.
**
**  File          : Cls_ud.h
**  Created By    :
**  Created On    :
**
**  History     :
**  Programmer    	Date        Rev       	Description
**  --------------- ---------- 	-------- ------------------------------
**
**  Notice: FFT & User Detection
**
****************************************************************************/
#ifndef __Cls_ud
#define __Cls_ud

/*#define DUMP_MATLAB_UD
#define DUMP_RTL_UD
#define DUMP_MATLAB_UD_HJH*/
#include "common_type.h"
#include "Lib_dump.h"
#include "Cls_fft.h"
#include "Cls_GWRxBFModule.h"
#include "Cls_sqrt.h"

typedef struct UD_CFG
{
    //Read Write
    double anoise_the1;
    double anoise_the2;
    int u_mode;
    int UDSwitch;
    int p1_the;
    double p1_para1;
    double p1_para2;
    int p2_the;
    int p3_the;
    int  p6_the;
    int freq_alloc_en;
    int freq_alloc_num;
    int freq_alloc_set[128];
    int GWRxAllUser;
    double rx_sampling_rate;
    int FilterLen;
    int IFFTLen;
    double  HCE;
}UD_CFG;

template <class T>
class Cls_ud
{
	public:
		Cls_ud() {
            m_in                    = NULL;
            m_fft_o                 = NULL;
            m_preamble_pow          = NULL;
            m_preamble_pow_o        = NULL;
            m_ant_anoise            = NULL;
            m_ant_DC_o              = NULL;
            m_out                   = NULL;
            m_fft_in                = NULL;
            m_APreambleFft2         = NULL;
            m_APreambleFft3         = NULL;
            m_APreambleFftESum2     = NULL;
            m_APreambleFftESum3     = NULL;
            m_P1                    = NULL;
            m_P2                    = NULL;
            m_P3                    = NULL;
            m_P4                    = NULL;
            m_P5                    = NULL;
            m_P6                    = NULL;
            m_P7                    = NULL;
            m_AH                    = NULL;
            m_AppointTChID          = NULL;
            m_AFreqBinDMax          = NULL;
            m_UMode                 = NULL;
            m_AFreqBinD             = NULL;
            m_AFreqBinDtmp          = NULL;
            m_FreqPos               = NULL;
            m_FreqPosIndex0         = NULL;
            m_FreqPosIndex1         = NULL;
            m_AFreqBinD0            = NULL;
            m_AFreqBinDMax0         = NULL;
            m_APreambleFftESum2tmp  = NULL;
            m_ant_anoise_frac       = NULL;
            m_HC_tmp1               = NULL;
            m_HC_tmp                = NULL;
            m_h                     = NULL;
            m_HC                    = NULL;
            m_htmp1                 = NULL;
		}
		~Cls_ud() {
            free(m_in);
            free(m_fft_o);
            free(m_preamble_pow);
            free(m_preamble_pow_o);
            free(m_ant_anoise);
            free(m_ant_DC_o);
            free(m_out);
            free(m_fft_in);
            free(m_APreambleFft2);
            free(m_APreambleFft3);
            free(m_APreambleFftESum2);
            free(m_APreambleFftESum3);
            free(m_P1);
            free(m_P2);
            free(m_P3);
            free(m_P4);
            free(m_P5);
            free(m_P6);
            free(m_P7);
            free(m_AH);
            free(m_AppointTChID);
            free(m_AFreqBinDMax);
            free(m_UMode);
            free(m_AFreqBinD);
            free(m_AFreqBinDtmp);
            free(m_FreqPos);
            free(m_FreqPosIndex0);
            free(m_FreqPosIndex1);
            free(m_AFreqBinD0);
            free(m_AFreqBinDMax0);
            free(m_APreambleFftESum2tmp);
            free(m_ant_anoise_frac);
            free(m_HC_tmp1);
            free(m_HC_tmp);
            free(m_h);
            free(m_HC);
            free(m_htmp1);
#ifdef DUMP_MATLAB_UD
            m_file_in_1.close();
            m_file_in_2.close();
            m_file_in_3.close();
            m_file_in_4.close();
            m_file_in_5.close();
            m_file_in_6.close();
            m_file_in_7.close();
            m_file_in_8.close();
            m_file_fft_o_1.close();
            m_file_fft_o_2.close();
            m_file_fft_o_3.close();
            m_file_fft_o_4.close();
            m_file_fft_o_5.close();
            m_file_fft_o_6.close();
            m_file_fft_o_7.close();
            m_file_fft_o_8.close();
            m_file_fft_p_o_1.close();
            m_file_fft_p_o_2.close();
            m_file_fft_p_o_3.close();
            m_file_fft_p_o_4.close();
            m_file_fft_p_o_5.close();
            m_file_fft_p_o_6.close();
            m_file_fft_p_o_7.close();
            m_file_fft_p_o_8.close();
            m_file_anoise.close();
            m_file_out_1.close();
            m_file_out_2.close();
            m_file_out_3.close();
            m_file_out_4.close();
            m_file_out_5.close();
            m_file_out_6.close();
            m_file_out_7.close();
            m_file_out_8.close();
            m_file_preamable_o.close();
            AFreqBinDMax.close();
            p4.close();
            p5.close();
            p6.close();
            p7.close();
#endif

#ifdef DUMP_MATLAB_UD_HJH
    file_o_in_Tmp1.close();
    file_UD_FFT.close();
    file_UD_Out.close();
    file_UD_P7.close();
#endif

#ifdef DUMP_RTL_UD
            m_fp_fft_i.close();
            m_fp_in_1.close();
            m_fp_in_2.close();
            m_fp_in_3.close();
            m_fp_in_4.close();
            m_fp_in_5.close();
            m_fp_in_6.close();
            m_fp_in_7.close();
            m_fp_in_8.close();
            m_fp_fft_o.close();
            m_fp_fft_o_1.close();
            m_fp_fft_o_2.close();
            m_fp_fft_o_3.close();
            m_fp_fft_o_4.close();
            m_fp_fft_o_5.close();
            m_fp_fft_o_6.close();
            m_fp_fft_o_7.close();
            m_fp_fft_o_8.close();
            m_fp_fft_dc.close();
            m_fp_combine.close();
            m_fp_getThreadValue.close();
            m_fp_get_pow.close();
            m_fp_K_the.close();
            m_fp_reg.close();
            m_fp_anoise.close();
            m_fp_rsqrt_anoise.close();
            m_fp_anoise_maxmin.close();
            m_fp_P1_HLP.close();
            m_fp_diffVal.close();
            m_fp_P1_d.close();
            m_fp_P2_d.close();
            m_fp_P3_d.close();
            m_fp_P4_d.close();
            m_fp_P5_d.close();
            m_fp_P6_d.close();
            m_fp_P7_d.close();
            m_fp_AFreqBinD.close();
            m_fp_AFreqBinDMax.close();
            m_fp_freq_offset.close();
            m_fp_pilot_power.close();
            m_fp_ah_i.close();
            m_fp_ah_q.close();
            m_fp_umode.close();
            m_fp_SetFreqRx.close();
#endif
		}
		void			init(char* module_name);
		void			go(int FreamIndex, Complex<double> **m_APreambleFft3_Last, double *m_APreambleFftESum3_Last, double *ANoiseLast);
		void 			reset();
		void			configure(UD_CFG cfg);
        int             m_fft_len;
        int             m_ant_num;
        Complex<double> m_in_1;
        Complex<double> m_in_2;
        Complex<double> m_in_3;
        Complex<double> m_in_4;
        Complex<double> m_in_5;
        Complex<double> m_in_6;
        Complex<double> m_in_7;
        Complex<double> m_in_8;
        Complex<double> m_ah_o_1;
        Complex<double> m_ah_o_2;
        Complex<double> m_ah_o_3;
        Complex<double> m_ah_o_4;
        Complex<double> m_ah_o_5;
        Complex<double> m_ah_o_6;
        Complex<double> m_ah_o_7;
        Complex<double> m_ah_o_8;
        Complex<double> m_out_1;
        Complex<double> m_out_2;
        Complex<double> m_out_3;
        Complex<double> m_out_4;
        Complex<double> m_out_5;
        Complex<double> m_out_6;
        Complex<double> m_out_7;
        Complex<double> m_out_8;
        double          m_freq_o;
        int             m_data_u_o_v;
        double          m_pilot_power_o[128];
        double          *m_preamble_pow_o;
        int             m_data_o_v;
        Complex<double> **m_out;
        Complex<double> *m_HC;
        int             m_slide_num;
        int             m_GWRxAllUser;
        Complex<T>      m_ah_o[8][128];
        Complex<T>      m_AHNorm_o[8][128];
        double          m_freq_offset_o[128];
        double          m_GWRxSampleRate;
        int             m_BFStartFlag;
        Complex<T>      **m_APreambleFft2;
        double          *m_APreambleFftESum2;
        T               *m_ant_anoise;
	private:
        int             m_addr_reversed[8192];
        int             bitrev(int in , int bitwidth);
        T               m_test_fxp;
        T               m_test_fxp1;
        T               m_test_fxp2;
        T               fxp_1_GWRxSampleRate;
        T               fxp_fft_rate;
        T               m_test_fxp3;
        T               fxp_p_the;
        T               fxp_P1_HLpos;
        T               fxp_diffval;
        T               fxp_SetFreqRx;
        int             p_corr_flg;
        T               t_GWRX_P1_Para;
        T               t_freq_offset_o;
        T               m_AH_sum;
        T               m_AH_sum_abs;
        T               m_AH_sum_abs_frac;
        Complex<T>      m_h_norm_fxp;

        UD_CFG          m_cfg;
        Complex<double> **m_in;
        Cls_fft<T>      m_fft;
        Cls_GWRxBFModule<fxp>   m_tx;
        FFT_CFG         m_fft_cfg;
        Complex<double> **m_fft_o;
        Cls_sqrt<T>     m_sqrt;
        SQRT_CFG        m_sqrt_cfg;
        T               m_preamble_pow_fxp;
        T               **m_preamble_pow;
        Complex<T>      *m_ant_DC_o;
        T               *m_ant_anoise_frac;
        T               m_threshold;
        T               m_anoise_the1_frac;
        T               m_anoise_frac_rsqrt;
        T               m_ant_noise_rsqrt;
        T               m_pre_pow_comb_fxp;
        T               m_K_fxp;
        T               m_fa_fb_frac;
        T               m_y_fxp;
        T               m_y_sqrt_fxp;
        Complex<T>      m_in_dc_fxp;
        Complex<T>      m_dc_fxp;
        Complex<T>      m_out_fxp;
        double          m_anoise_the1;
        double          m_anoise_the2;
        int             m_cnt_in;
        bool            m_ud_en;
        bool            m_data_o_en;
        int             m_UDSwitch;
        bool            *m_P1;
        bool            *m_P2;
        T               *m_P3;
        T               *m_P4;
        T               *m_P5;
        T               *m_P6;
        T               *m_P7;
        int             m_freq_alloc_en;
        int             m_freq_alloc_num;
        int             m_freq_alloc_set[128];
        int             m_UMode_o[128];
        Complex<T>      m_in_fxp;
        Complex<T>      **m_fft_in;
        Complex<T>      **m_AH;
        Complex<T>      m_APreambleFft3_fxp;
        Complex<T>      m_v1v2_fxp;
        Complex<double>     *m_HC_tmp1;
        Complex<double>     *m_HC_tmp;
        Complex<double>     *m_h;
        int             m_halfBL;
        int             m_HCE;
        int             m_filterLen;
        Complex<T>          m_HC_fxp;
        double              *m_htmp1;
        Complex<double> **m_APreambleFft3;
        double          *m_APreambleFftESum3;

        double          K[64];
        T               m_APreambleFftESum3_fxp;
        int             freq_alloc_num;
        int             freq_alloc_set[128];
        int             TempFFTLen;
        int             DataRateNum;
        int             GWRxUserNum[5];
        int             TerminalTxMode[5];
        int             GWRxAllUser;
        int             *m_AppointTChID;
        int             *m_AFreqBinDMax;
        int             *m_UMode;
        int             m_p1_the;
        int             m_p2_the;
        int             m_p3_the;
        int             m_p6_the;
        double          * m_AFreqBinD;
        double          * m_AFreqBinDtmp;
        int             *m_FreqPos;
        int             *m_FreqPosIndex0;
        int             *m_FreqPosIndex1;
        int             *m_AFreqBinD0;
        int             *m_AFreqBinDMax0;
        int             m_RealFreqAllocationNum;
        int             m_HighLightPos[1024];
        int             m_HLPosLen;
        int             m_Idx;
        int             m_u_mode;
        int             m_TerminalTxMode[5];
        int             m_cnt_out;
        double          *m_APreambleFftESum2tmp;
        int             m_AllocationFreqSet[128];
        double          m_GWRX_P1_Para1;
        double          m_GWRX_P1_Para2;
        double          m_GWRX_ANoiseThe1;
        double          m_GWRX_ANoiseThe2;
        double          m_GWFreqSet[10];
        int             m_user_info_o_en;
        int             m_cnt_u_out;
        T               m_anoise_the_fxp;
        T               m_ant_anoise_threshold_fxp;
        T               m_midval_fxp;
        T               m_val_rate_fxp;
        T               m_p2_the_fxp;
        T               m_v2_norm_fxp;
        T               m_v1_norm_fxp;
        T               m_v1_norm_tmp_fxp;
        T               m_v2_norm_tmp_fxp;
        T               m_v3_norm_tmp_fxp;
        T               m_abs_fxp;
        T               m_cor_fxp;
        T               m_Q_fxp;
        T               m_p5_Q_fxp;
        T               m_p6_Q_fxp;
        T               m_Maxval_fxp;
        T               m_Minval_fxp;

        int             m_MaxOutLen;
        void            input();
        void            fft_module(int FreamIndex,double *ANoiseLast);
        void            dump_ant(ofstream &file_name1, ofstream &file_name2, ofstream &file_name3, ofstream &file_name4, Complex<double> **in_1, Complex<double> **in_2, T **in_3, Complex<double> **in_4, int ant_num);
        void            max_min(int ant_num, double *max_val, double *min_val);
        void            anoise_cal(int FreamIndex,double *ANoiseLast);
        void            gain_adjust();
        int             ant_anoise_num_cal(int ant_num, double threshold);
        void            pow_combine();
        void            sig_adjust();
        void            UserDetection(int FreamIndex, Complex<double> **m_APreambleFft3_Last, double *m_APreambleFftESum3_Last);
        void            P1_Rule();
        void            P2_Rule();
        void            P3_Rule();
        void            P4_Rule();
        void            P5_Rule();
        void            P6_Rule();
        void            P7_Rule();
        void            getThreadValue(double * in ,
                                       int Len_in  ,
                                       double * out);
        void            InterfrencePeriodicIdxCalcuNew(int *tmpP1);
        void            correlation(int a      ,
                                    int *b     ,
                                    int Len    ,
                                    int Size   ,
                                    int fft0   ,
                                    int flag   ,
                                    double *out);
        void            getmaxsort();
        void            ud_out_module();
        void            output();
        void            dc_remove();
        double          sqrt_bisection(double a, double epsilon);
        void            generate_HCfilter();
        void            generate_para();
        void            upsample(int time);
        void            quicksort(int a[8], int first,int end);
#ifdef DUMP_MATLAB_UD
        ofstream        m_file_in_1;
        ofstream        m_file_in_2;
        ofstream        m_file_in_3;
        ofstream        m_file_in_4;
        ofstream        m_file_in_5;
        ofstream        m_file_in_6;
        ofstream        m_file_in_7;
        ofstream        m_file_in_8;
        ofstream        m_file_fft_o_1;
        ofstream        m_file_fft_o_2;
        ofstream        m_file_fft_o_3;
        ofstream        m_file_fft_o_4;
        ofstream        m_file_fft_o_5;
        ofstream        m_file_fft_o_6;
        ofstream        m_file_fft_o_7;
        ofstream        m_file_fft_o_8;
        ofstream        m_file_fft_p_o_1;
        ofstream        m_file_fft_p_o_2;
        ofstream        m_file_fft_p_o_3;
        ofstream        m_file_fft_p_o_4;
        ofstream        m_file_fft_p_o_5;
        ofstream        m_file_fft_p_o_6;
        ofstream        m_file_fft_p_o_7;
        ofstream        m_file_fft_p_o_8;
        ofstream        m_file_anoise;
        ofstream        m_file_out_1;
        ofstream        m_file_out_2;
        ofstream        m_file_out_3;
        ofstream        m_file_out_4;
        ofstream        m_file_out_5;
        ofstream        m_file_out_6;
        ofstream        m_file_out_7;
        ofstream        m_file_out_8;
        ofstream        m_file_preamable_o;
        ofstream        AFreqBinDMax;
        ofstream        ud_mat_out1;
        ofstream        ud_mat_out2;
        ofstream        ud_mat_out3;
        ofstream        ud_mat_out4;
        ofstream        p4;
        ofstream        p5;
        ofstream        p6;
        ofstream        p7;
#endif

#ifdef DUMP_MATLAB_UD_HJH
        ofstream        file_o_in_Tmp1;
        ofstream        file_UD_FFT;
        ofstream        file_UD_Out;
        ofstream        file_UD_P7;
#endif

#ifdef DUMP_RTL_UD
        ofstream        m_fp_fft_i;
        ofstream        m_fp_in_1;
        ofstream        m_fp_in_2;
        ofstream        m_fp_in_3;
        ofstream        m_fp_in_4;
        ofstream        m_fp_in_5;
        ofstream        m_fp_in_6;
        ofstream        m_fp_in_7;
        ofstream        m_fp_in_8;
        ofstream        m_fp_fft_o;
        ofstream        m_fp_fft_o_1;
        ofstream        m_fp_fft_o_2;
        ofstream        m_fp_fft_o_3;
        ofstream        m_fp_fft_o_4;
        ofstream        m_fp_fft_o_5;
        ofstream        m_fp_fft_o_6;
        ofstream        m_fp_fft_o_7;
        ofstream        m_fp_fft_o_8;
        ofstream        m_fp_fft_dc;
        ofstream        m_fp_anoise;
        ofstream        m_fp_rsqrt_anoise;
        ofstream        m_fp_anoise_maxmin;
        ofstream        m_fp_combine;
        ofstream        m_fp_getThreadValue;
        ofstream        m_fp_get_pow ;
        ofstream        m_fp_K_the;
        ofstream        m_fp_reg;
        ofstream        m_fp_P1_HLP;
        ofstream        m_fp_diffVal;
        ofstream        m_fp_P1_d;
        ofstream        m_fp_P2_d;
        ofstream        m_fp_P3_d;
        ofstream        m_fp_P4_d;
        ofstream        m_fp_P5_d;
        ofstream        m_fp_P6_d;
        ofstream        m_fp_P7_d;
        ofstream        m_fp_AFreqBinD;
        ofstream        m_fp_AFreqBinDMax;
        ofstream        m_fp_freq_offset;
        ofstream        m_fp_pilot_power;
        ofstream        m_fp_ah_i;
        ofstream        m_fp_ah_q;
        ofstream        m_fp_umode;
        ofstream        m_fp_SetFreqRx;

#endif
};

template <class T>
void Cls_ud<T>::configure(UD_CFG cfg)
{
	m_cfg = cfg;
    m_fft_cfg.fft_log2_len = (int)log2(m_fft_len);
    m_anoise_the1 = m_cfg.anoise_the1;
    m_anoise_the2 = m_cfg.anoise_the2;
    m_p1_the  = m_cfg.p1_the;
    m_p2_the  = m_cfg.p2_the;
    m_p3_the  = m_cfg.p3_the;
    m_p6_the  = m_cfg.p6_the;
    m_freq_alloc_en  = m_cfg.freq_alloc_en;
    m_freq_alloc_num = m_cfg.freq_alloc_num;
    mem_copy(&m_freq_alloc_set[0],m_cfg.freq_alloc_set,128);
    m_GWRxAllUser = m_cfg.GWRxAllUser;
    m_GWRX_P1_Para1 = m_cfg.p1_para1;
    m_GWRX_P1_Para2 = m_cfg.p1_para2;
    m_GWRxSampleRate = m_cfg.rx_sampling_rate;
    m_TerminalTxMode[0] = m_cfg.u_mode;
    m_fft.configure(m_fft_cfg);
    m_UDSwitch = m_cfg.UDSwitch;

    m_filterLen = m_cfg.FilterLen;
    m_halfBL = m_filterLen / 2;
    m_HCE = m_cfg.HCE;

    m_h = safe_new(m_h, m_fft_len);
    zeros(m_h, m_fft_len);
    m_HC_tmp = safe_new(m_HC_tmp, m_fft_len);
    generate_para();

    m_MaxOutLen = 1024;
}

template <class T>
void Cls_ud<T>::init(char* module_name)
{
    m_fft.init(module_name);
    m_in                    = safe_new(m_in, 8, m_fft_len);
    m_fft_o                 = safe_new(m_fft_o, 8, m_fft_len);
    m_preamble_pow          = safe_new(m_preamble_pow, m_ant_num, m_fft_len);
    m_preamble_pow_o        = safe_new(m_preamble_pow_o, m_fft_len);
    m_ant_anoise            = safe_new(m_ant_anoise, m_ant_num);
    m_ant_anoise_frac       = safe_new(m_ant_anoise_frac, m_ant_num);
    m_ant_DC_o              = safe_new(m_ant_DC_o,m_ant_num);
    m_out                   = safe_new(m_out, m_ant_num, m_fft_len);
    m_fft_in                = safe_new(m_fft_in, m_ant_num, m_fft_len);
    m_APreambleFft2         = safe_new(m_APreambleFft2, m_ant_num, m_fft_len);
    m_APreambleFft3         = safe_new(m_APreambleFft3, m_ant_num, m_fft_len);
    m_APreambleFftESum2     = safe_new(m_APreambleFftESum2, m_fft_len);
    m_APreambleFftESum3     = safe_new(m_APreambleFftESum3, m_fft_len);
    m_P1                    = safe_new(m_P1, m_MaxOutLen);
    m_P2                    = safe_new(m_P2, m_MaxOutLen);
    m_P3                    = safe_new(m_P3, m_MaxOutLen);
    m_P4                    = safe_new(m_P4, m_MaxOutLen);
    m_P5                    = safe_new(m_P5, m_MaxOutLen);
    m_P6                    = safe_new(m_P6, m_MaxOutLen);
    m_P7                    = safe_new(m_P7, m_MaxOutLen);
    m_AH                    = safe_new(m_AH, m_ant_num, m_GWRxAllUser);
    m_AppointTChID          = safe_new(m_AppointTChID, m_GWRxAllUser);
    m_AFreqBinDMax          = safe_new(m_AFreqBinDMax, m_GWRxAllUser);
    m_UMode                 = safe_new(m_UMode, m_GWRxAllUser);
    m_AFreqBinD             = safe_new(m_AFreqBinD, m_fft_len);
    m_AFreqBinDtmp          = safe_new(m_AFreqBinDtmp, m_fft_len);
    m_FreqPos               = safe_new(m_FreqPos, m_freq_alloc_num);
    m_FreqPosIndex0         = safe_new(m_FreqPosIndex0, m_freq_alloc_num);
    m_FreqPosIndex1         = safe_new(m_FreqPosIndex1, m_freq_alloc_num);
    m_AFreqBinD0            = safe_new(m_AFreqBinD0, m_freq_alloc_num);
    m_AFreqBinDMax0         = safe_new(m_AFreqBinDMax0, m_freq_alloc_num);
    m_APreambleFftESum2tmp  = safe_new(m_APreambleFftESum2tmp, m_freq_alloc_num);

    m_HC = safe_new(m_HC, m_filterLen);
    m_HC_tmp1 = safe_new(m_HC_tmp1, m_fft_len);

    m_fft.fft_n(m_h, m_HC_tmp, 1, 0, 1);
    for (int ck0 = 0; ck0 < m_fft_len; ck0++)
    {
        m_HC_tmp1[ck0] = m_HC_tmp[ck0].conjug() ;
    }
    //FIX POINT
    fix_set(m_in_fxp, 14, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_dc_fxp, 14, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_in_dc_fxp, 14, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_in_dc_fxp,"m_in_dc_fxp", 1, 1000);
    fix_set(m_preamble_pow_fxp, 35, 24, FXP_US, FXP_RND, FXP_SAT_SYM);//32.24
    fix_set(m_threshold, 35, 24, FXP_US, FXP_TRN , FXP_SAT_SYM);//32.24
    fix_set(m_anoise_the1_frac, 11, 0, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set_group(m_ant_anoise, 8, 32, 21, FXP_US, FXP_TRN, FXP_SAT_SYM);
    fix_set_group(m_ant_anoise_frac, 8, 32, 22, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM);
    fix_set(m_anoise_frac_rsqrt, 16, 11, FXP_US, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_ant_noise_rsqrt, 11, 0, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_ant_noise_rsqrt,"m_ant_noise_rsqrt", 1, 1);
    fix_set(m_out_fxp, 20, 13, FXP_TC, FXP_RND, FXP_SAT_SYM);//(m_out_fxp,"m_out_fxp", 1, 1000);16.10
    fix_set(m_pre_pow_comb_fxp, 36, 30, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_pre_pow_comb_fxp,"m_pre_pow_comb_fxp", 1, 100);//36.30 30.27
    fix_set(m_APreambleFftESum3_fxp, 36,30, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_APreambleFftESum3_fxp,"m_APreambleFftESum3_fxp", 1, 1000);//36.30
    fix_set(m_APreambleFft3_fxp, 20, 13,FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_APreambleFft3_fxp,"m_APreambleFft3_fxp", 1, 1000);
    fix_set(m_K_fxp, 23, 21, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_K_fxp,"m_K_fxp", 1, 1);
    fix_set(m_anoise_the_fxp, 13, 1, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(m_ant_anoise_threshold_fxp, 14, 4, FXP_US, FXP_RND, FXP_SAT_SYM);//14 4
    fix_set(m_midval_fxp, 32, 24, FXP_US, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_val_rate_fxp, 14, 4, FXP_US, FXP_RND, FXP_SAT_SYM);//14 4
    fix_set(m_p2_the_fxp, 12, 8, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_pre_pow_comb_fxp,"m_pre_pow_comb_fxp", 1, 1);
    fix_set(m_v1_norm_fxp, 11, 8, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_pre_pow_comb_fxp,"m_pre_pow_comb_fxp", 1, 1);
    fix_set(m_v2_norm_fxp, 11, 8, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_pre_pow_comb_fxp,"m_pre_pow_comb_fxp", 1, 1);
    fix_set(m_v1_norm_tmp_fxp, 36, 32, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_v1_norm_tmp_fxp,"m_pre_pow_comb_fxp", 1, 1);//40.32
    fix_set(m_v2_norm_tmp_fxp, 22, 0, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_v2_norm_tmp_fxp,"m_v2_norm_tmp_fxp", 1, 1);30.10
    fix_set(m_v3_norm_tmp_fxp, 36, 32, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_v1_norm_tmp_fxp,"m_pre_pow_comb_fxp", 1, 1); 46.40
    fix_set(m_v1v2_fxp, 22, 16, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_v1v2_fxp,"m_v1v2_fxp", 1, 1);22 16
    fix_set(m_abs_fxp, 11, 0, FXP_US, FXP_TRN, FXP_SAT_SYM);//21 15
    fix_set(m_cor_fxp, 11, 0, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_cor_fxp,"m_cor_fxp", 1, 1);
    fix_set(m_p5_Q_fxp, 12, 1, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_cor_fxp,"m_cor_fxp", 1, 1);
    fix_set(m_fa_fb_frac, 22, 18, FXP_US, FXP_TRN_ZERO, FXP_SAT);//statistic_set(m_fa_fb_frac,"m_fa_fb_frac", 1, 1);
    fix_set_group(m_P3, m_MaxOutLen, 13, 1, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set_group(m_P4, m_MaxOutLen, 13, 1, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set_group(m_P5, m_MaxOutLen, 13, 1, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set_group(m_P6, m_MaxOutLen, 13, 1, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set_group(m_P7, m_MaxOutLen, 13, 1, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(m_y_fxp, 32, 10, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM);//(m_y_fxp,"m_y_fxp", 1, 1);//16 -6
    fix_set(m_y_sqrt_fxp, 16, 5, FXP_US, FXP_TRN, FXP_SAT_SYM);//(m_y_fxp,"m_y_fxp", 1, 1);//16 -6
    fix_set(m_Q_fxp, 12, -4, FXP_US, FXP_TRN, FXP_SAT_SYM);//statistic_set(m_Q_fxp,"m_Q_fxp", 1, 1);//12 -4

    fix_set(m_HC_fxp , 17, 6, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_HC_fxp,"m_HC_fxp", 1, 100);

    fix_set(t_freq_offset_o , 26, 19, FXP_TC, FXP_RND, FXP_SAT_SYM);//25.19
    fix_set(t_GWRX_P1_Para , 12, 1, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(fxp_1_GWRxSampleRate , 25, 0, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(fxp_fft_rate , 31, 14, FXP_TC, FXP_RND, FXP_SAT_SYM);

    fix_set(m_test_fxp, 45, 37, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(m_test_fxp1, 22, 19, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(m_test_fxp2, 25, 0, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(fxp_p_the, 7, 7, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(fxp_P1_HLpos, 14, 14, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(fxp_diffval, 12, 12, FXP_US, FXP_RND, FXP_SAT_SYM);// 12 ?
    fix_set(fxp_SetFreqRx,20, 20, FXP_TC, FXP_RND, FXP_SAT_SYM);

    fix_set(m_AH_sum, 34, 28, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_AH_sum,"m_AH_sum", 1, 1);
    fix_set(m_AH_sum_abs, 17, 14, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_AH_sum,"m_AH_sum", 1, 1);//21.16
    fix_set(m_AH_sum_abs_frac, 18, 1, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_AH_sum_abs_frac,"m_AH_sum_abs_frac", 1, 1);
    fix_set(m_h_norm_fxp, 18, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_h_norm_fxp,"m_h_norm_fxp", 1, 1);
    fix_set(m_Maxval_fxp, 35, 24, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_Maxval_fxp,"m_Maxval_fxp", 1, 1);//28.24
    fix_set(m_Minval_fxp, 21, 10, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_Minval_fxp,"m_Minval_fxp", 1, 1);//20.8

    m_anoise_the1_frac = 1.0 / m_anoise_the1;
    m_anoise_the_fxp = m_anoise_the2 * 1.0 / m_anoise_the1;

    for(int ck0 = 0; ck0 < 64; ck0++)
    {
        m_K_fxp = pow(10, ck0 *1.0 / 10);
        K[ck0] = m_K_fxp;
    }

    m_sqrt_cfg.bit_len = 22;
    m_sqrt.configure(m_sqrt_cfg);
    m_sqrt.init(module_name);
    reset();
    //mkdir Folder
#ifdef DUMP_MATLAB_UD
    /*struct stat statExist;
    int statMd = 0;
    if (stat(module_name, &statExist) != 0) {
            statMd = mkdir(module_name,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }*/
    char filename_mat[300] = {};
    strcpy(filename_mat, module_name); strcat(filename_mat, "in_1.txt");        m_file_in_1.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "in_2.txt");        m_file_in_2.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "in_3.txt");        m_file_in_3.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "in_4.txt");        m_file_in_4.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "in_5.txt");        m_file_in_5.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "in_6.txt");        m_file_in_6.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "in_7.txt");        m_file_in_7.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "in_8.txt");        m_file_in_8.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_o_1.txt");     m_file_fft_o_1.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_o_2.txt");     m_file_fft_o_2.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_o_3.txt");     m_file_fft_o_3.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_o_4.txt");     m_file_fft_o_4.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_o_5.txt");     m_file_fft_o_5.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_o_6.txt");     m_file_fft_o_6.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_o_7.txt");     m_file_fft_o_7.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_o_8.txt");     m_file_fft_o_8.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_p_o_1.txt");   m_file_fft_p_o_1.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_p_o_2.txt");   m_file_fft_p_o_2.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_p_o_3.txt");   m_file_fft_p_o_3.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_p_o_4.txt");   m_file_fft_p_o_4.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_p_o_5.txt");   m_file_fft_p_o_5.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_p_o_6.txt");   m_file_fft_p_o_6.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_p_o_7.txt");   m_file_fft_p_o_7.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_p_o_8.txt");   m_file_fft_p_o_8.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "anoise.txt");      m_file_anoise.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "out_1.txt");       m_file_out_1.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "out_2.txt");       m_file_out_2.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "out_3.txt");       m_file_out_3.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "out_4.txt");       m_file_out_4.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "out_5.txt");       m_file_out_5.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "out_6.txt");       m_file_out_6.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "out_7.txt");       m_file_out_7.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "out_8.txt");       m_file_out_8.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "p_out.txt");       m_file_preamable_o.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "AFreqBinDMax.txt");       AFreqBinDMax.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "p4.txt");       p4.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "p5.txt");       p5.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "p6.txt");       p6.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "p7.txt");       p7.open(filename_mat);
#endif

#ifdef DUMP_MATLAB_UD_HJH
    char filenameTmp[300] = {};
    strcpy(filenameTmp, module_name); strcat(filenameTmp, "TestCmodel.dat"); file_o_in_Tmp1.open(filenameTmp,std::ios::trunc);
    strcpy(filenameTmp, module_name); strcat(filenameTmp, "UD_FFT_Out.dat"); file_UD_FFT.open(filenameTmp,std::ios::trunc);
    strcpy(filenameTmp, module_name); strcat(filenameTmp, "UD_Out.dat"); file_UD_Out.open(filenameTmp,std::ios::trunc);
    strcpy(filenameTmp, module_name); strcat(filenameTmp, "UD_P7.dat"); file_UD_P7.open(filenameTmp,std::ios::trunc);
#endif


#ifdef DUMP_RTL_UD
char filename_rtl[300] = {};
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "in_1.dat");        m_fp_in_1.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "in_2.dat");        m_fp_in_2.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "in_3.dat");        m_fp_in_3.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "in_4.dat");        m_fp_in_4.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "in_5.dat");        m_fp_in_5.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "in_6.dat");        m_fp_in_6.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "in_7.dat");        m_fp_in_7.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "in_8.dat");        m_fp_in_8.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fft_i.dat");       m_fp_fft_i.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fft_o.dat");       m_fp_fft_o.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fft_o_1.dat");     m_fp_fft_o_1.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fft_o_2.dat");     m_fp_fft_o_2.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fft_o_3.dat");     m_fp_fft_o_3.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fft_o_4.dat");     m_fp_fft_o_4.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fft_o_5.dat");     m_fp_fft_o_5.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fft_o_6.dat");     m_fp_fft_o_6.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fft_o_7.dat");     m_fp_fft_o_7.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fft_o_8.dat");     m_fp_fft_o_8.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fft_dc.dat");      m_fp_fft_dc.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "anoise.dat");      m_fp_anoise.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "rsqrt_anoise.dat");    m_fp_rsqrt_anoise.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "anoise_maxmin.dat");   m_fp_anoise_maxmin.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "preamble_pow.dat");    m_fp_combine.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "getThreadValue.dat");  m_fp_getThreadValue.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "get_pow.dat");         m_fp_get_pow.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "K_the_table.dat");     m_fp_K_the.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "reg_ud_o.dat");        m_fp_reg.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "P1_HLpos.dat");        m_fp_P1_HLP.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "diffval.dat");         m_fp_diffVal.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "P1_d.dat");            m_fp_P1_d.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "P2_d.dat");            m_fp_P2_d.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "P3_d.dat");            m_fp_P3_d.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "P4_d.dat");            m_fp_P4_d.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "P5_d.dat");            m_fp_P5_d.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "P6_d.dat");            m_fp_P6_d.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "P7_d.dat");            m_fp_P7_d.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "AFreqBinD.dat");       m_fp_AFreqBinD.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "AFreqBinDMax.dat");    m_fp_AFreqBinDMax.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "freq_offset.dat");     m_fp_freq_offset.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "pilot_power.dat");     m_fp_pilot_power.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "ah_i.dat");            m_fp_ah_i.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "ah_q.dat");            m_fp_ah_q.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "umode.dat");           m_fp_umode.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "user_freq.dat");       m_fp_SetFreqRx.open(filename_rtl);

    //dump K_the table
    for(int ck0 = 0; ck0 < 64; ck0++) {
            dump(m_fp_K_the,&K[ck0] ,1,m_K_fxp);
     }

    int reg_ant_num = m_ant_num == 1 ? 0 :
                      m_ant_num == 2 ? 1 :
                      m_ant_num == 4 ? 2 :
                      m_ant_num == 8 ? 3 : 0;

    int reg_fft_len_cfg = m_fft_len == 256  ? 0 :
                          m_fft_len == 512  ? 1 :
                          m_fft_len == 1024 ? 2 :
                          m_fft_len == 2048 ? 3 :
                          m_fft_len == 4096 ? 4 :
                          m_fft_len == 8192 ? 5 : 2;
    fxp_p_the = reg_ant_num;
    dump(m_fp_reg,&fxp_p_the,1,fxp_p_the); //ant num
    fxp_p_the = reg_fft_len_cfg;
    dump(m_fp_reg,&fxp_p_the,1,fxp_p_the);//fft len
    fxp_p_the = m_p1_the;
    dump(m_fp_reg,&fxp_p_the,1,fxp_p_the);//p1_the
    fxp_p_the = m_p2_the;
    dump(m_fp_reg,&fxp_p_the,1,fxp_p_the);
    fxp_p_the = m_p3_the;
    dump(m_fp_reg,&fxp_p_the,1,fxp_p_the);
    fxp_p_the = m_p6_the;
    dump(m_fp_reg,&fxp_p_the,1,fxp_p_the);
    dump(m_fp_reg,&m_anoise_the1_frac,1,m_anoise_the1_frac);
    dump(m_fp_reg,&m_anoise_the_fxp,1,m_anoise_the_fxp);
    m_fp_reg << m_freq_alloc_en << endl;//m_freq_alloc_en
    fxp_p_the = m_freq_alloc_num;
    dump(m_fp_reg,&fxp_p_the,1,fxp_p_the); //freq_alloc_num
    fxp_p_the = m_GWRxAllUser;
    dump(m_fp_reg,&fxp_p_the,1,fxp_p_the); //m_GWRxAllUser
    t_GWRX_P1_Para = m_GWRX_P1_Para1;
    dump(m_fp_reg,&t_GWRX_P1_Para,1,t_GWRX_P1_Para); //m_GWRX_P1_Para1 u10.1
    t_GWRX_P1_Para = m_GWRX_P1_Para2;
    dump(m_fp_reg,&t_GWRX_P1_Para,1,t_GWRX_P1_Para); //m_GWRX_P1_Para2

    fxp_p_the = m_GWRxSampleRate == 15625.0  ? 0 :
                m_GWRxSampleRate == 31250.0  ? 1 :
                m_GWRxSampleRate == 62500.0  ? 2 :
                m_GWRxSampleRate == 125000.0 ? 3 :
                m_GWRxSampleRate == 250000.0 ? 4 : 5 ;
    dump(m_fp_reg,&fxp_p_the,1,fxp_p_the); //m_GWRxSampleRate
    fxp_p_the = m_TerminalTxMode[0];
    dump(m_fp_reg,&fxp_p_the,1,fxp_p_the); //m_TerminalTxMode
    cout << "rx_sampling_rate" << "\t" << m_GWRxSampleRate <<endl;
    for(int ck0 = 0; ck0 < 128; ck0++) {//128 user freq
            fxp_SetFreqRx = m_freq_alloc_set[ck0];
            dump(m_fp_SetFreqRx,&fxp_SetFreqRx,1,fxp_SetFreqRx);
    }
#endif
}

template <class T>
void Cls_ud<T>::reset()
{
    m_slide_num = 0;
    m_BFStartFlag = 0;
    m_ud_en = 0;
    m_cnt_in = 0;
    m_cnt_out = 0;
    m_data_o_en = 0;
    m_user_info_o_en = 0;
    m_cnt_u_out = 0;
    zeros(m_fft_o, 8, m_fft_len);
    zeros(m_in, 8, m_fft_len);
    zeros(m_preamble_pow, m_ant_num, m_fft_len);
    zeros(m_ant_anoise, m_ant_num);
    zeros(m_ant_DC_o,m_ant_num);
    zeros(m_preamble_pow_o, m_fft_len);
    zeros(m_out, m_ant_num, m_fft_len);

    generate_HCfilter();
}

template <class T>
void Cls_ud<T>::go(int FreamIndex, Complex<double> **m_APreambleFft3_Last, double *m_APreambleFftESum3_Last, double *ANoiseLast)
{
    //Input
    input();

    //FFT Module
    if(m_ud_en) {
        fft_module(FreamIndex,ANoiseLast);

    //User Detection
        if(m_slide_num == 1)
            UserDetection(FreamIndex,m_APreambleFft3_Last,m_APreambleFftESum3_Last);
        m_ud_en = 0;
    }
    //output
    output();
}

template <class T>
void Cls_ud<T>::output()
{
    if(m_data_o_en) {
        m_out_1 = m_out[0][m_cnt_out];
        m_out_2 = m_out[1][m_cnt_out];
        m_out_3 = m_out[2][m_cnt_out];
        m_out_4 = m_out[3][m_cnt_out];
        m_out_5 = m_out[4][m_cnt_out];
        m_out_6 = m_out[5][m_cnt_out];
        m_out_7 = m_out[6][m_cnt_out];
        m_out_8 = m_out[7][m_cnt_out];
        m_data_o_v = 1;
        m_cnt_out++;
        if(m_cnt_out == m_fft_len) {
            m_data_o_en = 0;
            m_cnt_out = 0;
        }
    } else {
        m_data_o_v = 0;
    }
    if(m_user_info_o_en) {
        m_ah_o_1 = m_ah_o[0][m_cnt_u_out];
        m_ah_o_2 = m_ah_o[1][m_cnt_u_out];
        m_ah_o_3 = m_ah_o[2][m_cnt_u_out];
        m_ah_o_4 = m_ah_o[3][m_cnt_u_out];
        m_ah_o_5 = m_ah_o[4][m_cnt_u_out];
        m_ah_o_6 = m_ah_o[5][m_cnt_u_out];
        m_ah_o_7 = m_ah_o[6][m_cnt_u_out];
        m_ah_o_8 = m_ah_o[7][m_cnt_u_out];
        m_freq_o = m_freq_offset_o[m_cnt_u_out];
        m_data_u_o_v = 1;
        m_cnt_u_out++;
        if(m_cnt_u_out == m_GWRxAllUser) {
            m_user_info_o_en = 0;
            m_cnt_u_out = 0;
        }
    } else {
        m_data_u_o_v = 0;
    }
}

template <class T>
void Cls_ud<T>::input()
{
    m_in_fxp = m_in_1;  m_in[0][m_cnt_in] = m_in_fxp;
    m_in_fxp = m_in_2;  m_in[1][m_cnt_in] = m_in_fxp;
    m_in_fxp = m_in_3;  m_in[2][m_cnt_in] = m_in_fxp;
    m_in_fxp = m_in_4;  m_in[3][m_cnt_in] = m_in_fxp;
    m_in_fxp = m_in_5;  m_in[4][m_cnt_in] = m_in_fxp;
    m_in_fxp = m_in_6;  m_in[5][m_cnt_in] = m_in_fxp;
    m_in_fxp = m_in_7;  m_in[6][m_cnt_in] = m_in_fxp;
    m_in_fxp = m_in_8;  m_in[7][m_cnt_in] = m_in_fxp;

#ifdef DUMP_RTL_UD
    fxp rtl_dump00;
    fix_set(rtl_dump00,14,1,FXP_TC,FXP_RND, FXP_SAT_SYM);
      //  cout  << "fft in"<<"\t" << m_in[0][m_cnt_in].I <<"\t" << m_cnt_in << endl;
        dump(m_fp_in_1,&m_in[0][m_cnt_in],1,rtl_dump00);
	dump(m_fp_in_2,&m_in[1][m_cnt_in],1,rtl_dump00);
	dump(m_fp_in_3,&m_in[2][m_cnt_in],1,rtl_dump00);
	dump(m_fp_in_4,&m_in[3][m_cnt_in],1,rtl_dump00);
	dump(m_fp_in_5,&m_in[4][m_cnt_in],1,rtl_dump00);
	dump(m_fp_in_6,&m_in[5][m_cnt_in],1,rtl_dump00);
	dump(m_fp_in_7,&m_in[6][m_cnt_in],1,rtl_dump00);
 	dump(m_fp_in_8,&m_in[7][m_cnt_in],1,rtl_dump00);
#endif
    m_cnt_in++;
    //LOG2DEV<< m_cnt_in<<endl;
    if(m_cnt_in == m_fft_len)
    {
        m_cnt_in = 0;
        m_ud_en = 1;
        dc_remove();
    }
    //mem_copy(m_in, in, m_ant_num, 0, m_fft_len);
}

template <class T>
void Cls_ud<T>::dc_remove()
{
    Complex<double> dc[8];
    zeros(dc, 8);

    for (int ck0 = 0; ck0 < 8; ck0++) {
        for (int ck1 = 0; ck1 < m_fft_len; ck1++) {
            dc[ck0] = m_in[ck0][ck1] + dc[ck0];
        }

        m_dc_fxp = dc[ck0] * 1.0 / m_fft_len;
        #ifdef DUMP_RTL_UD
            if(ck0 != 7) dump(m_fp_fft_dc,&m_dc_fxp,1,m_dc_fxp.I,0);
            else         dump(m_fp_fft_dc,&m_dc_fxp,1,m_dc_fxp.I);
        #endif
        m_ant_DC_o[ck0] = m_dc_fxp;
    }

    for (int ck0 = 0; ck0 < 8; ck0++) {
        for (int ck1 = 0; ck1 < m_fft_len; ck1++) {
            m_in_dc_fxp = m_in[ck0][ck1] - m_ant_DC_o[ck0].todouble();
            m_in[ck0][ck1] = m_in_dc_fxp;
        }
    }
}

#ifdef DUMP_RTL_UD
template <class T>
int Cls_ud<T>:: bitrev(int in , int bitwidth)
{
    int i   = 0;
    int tmp = 0;
    for (i = 0 ; i< bitwidth ;i ++){
        tmp <<= 1;
        tmp += in&0x1;
        in >>= 1;
    }
    return tmp;
}
#endif

template <class T>
void Cls_ud<T>::fft_module(int FreamIndex,double *ANoiseLast)
{
    #ifdef DUMP_RTL_UD
    const int  FFT_2N_WL_00[16]  = {14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14};
    const int  FFT_2N_IWL_00[16] = { 2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 14};
    int m_fft_log2_len = (int)log2(m_fft_len);
    fxp  rtl_dump11,rtl_dump12;
    fix_set(rtl_dump11,14, FFT_2N_IWL_00[m_fft_log2_len-1] ,FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump12,14, 1 ,FXP_TC,FXP_RND, FXP_SAT_SYM);
    for(int i=0;i<m_fft_len;i++){
        m_addr_reversed[i] = bitrev(i,(int)log2(m_fft_len));
    }
    #endif

    // FFT
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        m_fft.fft_n(m_in[ck0], m_fft_o[ck0], 1, 0, 1);
        #ifdef DUMP_RTL_UD
        for (int ck1 = 0; ck1 < m_fft_len; ck1++){
            dump(m_fp_fft_i,&m_in[ck0][m_addr_reversed[ck1]],1,rtl_dump12);
            dump(m_fp_fft_o,&m_fft_o[ck0][ck1],1,rtl_dump11);
        }
        #endif
        m_fft_o[ck0][0] = m_fft_o[ck0][1];
    }

    /*for(int i1 = 0; i1 < m_fft_len; i1++){
        for(int i2 = 0; i2 < 8; i2++){
            file_o_in_Tmp1 << fixed << setprecision(26) <<m_fft_o[i2][i1].I<<"\t"<<endl;
            file_o_in_Tmp1 << fixed << setprecision(26) <<m_fft_o[i2][i1].Q<<"\t"<<endl;
        }
    }*/
    // ANoise Calculation
    anoise_cal(FreamIndex,ANoiseLast);

    // Signal Adjust
    sig_adjust();

    m_slide_num = m_slide_num + 1;
    m_BFStartFlag = 1;
    m_data_o_en = 1;
#ifdef DUMP_MATLAB_UD
    for (int ck0 = 0; ck0 < m_ant_num; ck0++)
    {
        if(ck0 == 0) {
            dump_ant(m_file_in_1, m_file_fft_o_1, m_file_fft_p_o_1, m_file_out_1, m_in, m_fft_o, m_preamble_pow, m_out, 0);
        } else if(ck0 == 1) {
            dump_ant(m_file_in_2, m_file_fft_o_2, m_file_fft_p_o_2, m_file_out_2, m_in, m_fft_o, m_preamble_pow, m_out, 1);
        } else if(ck0 == 2) {
            dump_ant(m_file_in_3, m_file_fft_o_3, m_file_fft_p_o_3, m_file_out_3, m_in, m_fft_o, m_preamble_pow, m_out, 2);
        } else if(ck0 == 3) {
            dump_ant(m_file_in_4, m_file_fft_o_4, m_file_fft_p_o_4, m_file_out_4, m_in, m_fft_o, m_preamble_pow, m_out, 3);
        } else if(ck0 == 4) {
            dump_ant(m_file_in_5, m_file_fft_o_5, m_file_fft_p_o_5, m_file_out_5, m_in, m_fft_o, m_preamble_pow, m_out, 4);
        } else if(ck0 == 5) {
            dump_ant(m_file_in_6, m_file_fft_o_6, m_file_fft_p_o_6, m_file_out_6, m_in, m_fft_o, m_preamble_pow, m_out, 5);
        } else if(ck0 == 6) {
            dump_ant(m_file_in_7, m_file_fft_o_7, m_file_fft_p_o_7, m_file_out_7, m_in, m_fft_o, m_preamble_pow, m_out, 6);
        } else if(ck0 == 7) {
            dump_ant(m_file_in_8, m_file_fft_o_8, m_file_fft_p_o_8, m_file_out_8, m_in, m_fft_o, m_preamble_pow, m_out, 7);
        }
        if(m_slide_num == 1) {
            m_file_anoise << m_ant_anoise[ck0] << endl;
            if(ck0 == 0) {
                for (int ck1 = 0; ck1 < m_fft_len; ck1++)
                {
                    m_file_preamable_o << m_preamble_pow_o[ck1] << endl;
                }
            }
        }
    }

#endif

#ifdef DUMP_MATLAB_UD_HJH
    for(int i1 = 0; i1 < m_fft_len; i1++){
        for(int i2 = 0; i2 < m_ant_num; i2++){
            file_UD_FFT << fixed << setprecision(26) <<m_out[i2][i1].I<<"\t"<<endl;//m_fft_o  m_out
            file_UD_FFT << fixed << setprecision(26) <<m_out[i2][i1].Q<<"\t"<<endl;
        }
    }
#endif
}

template <class T>
void Cls_ud<T>::anoise_cal(int FreamIndex,double *ANoiseLast)
{
    if(m_slide_num == 0) {
        for (int ck0 = 0; ck0 < m_ant_num; ck0++){
            for (int ck1 = 0; ck1 < m_fft_len; ck1++)
            {
                m_preamble_pow_fxp = m_fft_o[ck0][ck1].power();
                if(m_preamble_pow_fxp == 0)
                    m_preamble_pow_fxp = 1.0 / DOUBLE_POW2[64 + 11];//32.24 64 + 8
                m_preamble_pow[ck0][ck1] = m_preamble_pow_fxp;
            }
        }
        /*for(int i1 = 0; i1 < m_fft_len; i1++){
            for(int i2 = 0; i2 < 8; i2++){
                file_o_in_Tmp1 << fixed << setprecision(26) <<m_preamble_pow[i2][i1]<<"\t"<<endl;
            }
        }*/
        gain_adjust();
        if(FreamIndex==0){

        }else{
            for(int it = 0;it<m_ant_num;it++){
                if(m_ant_anoise[it] > ANoiseLast[it]){
                    m_ant_anoise[it] = ANoiseLast[it] * (1 + 0.01);
                }else{
                    m_ant_anoise[it] = ANoiseLast[it] * (1 - 0.01);
                }
            }
        }
        pow_combine();
    }
}

template <class T>
void Cls_ud<T>::gain_adjust()
{
    double max_val, min_val;
    int ant_anoise_num;
    double ant_anoise_threshold;
    for (int ck0 = 0; ck0 < m_ant_num; ck0++)
    {
        max_min(ck0, &max_val, &min_val);//35.24
        m_Maxval_fxp = max_val;max_val = m_Maxval_fxp;
        m_Minval_fxp = min_val;min_val = m_Minval_fxp;

        //LOG2DEV << max_val << "\t" << min_val << endl;
        if(max_val == min_val)
            m_ant_anoise[ck0] = 1;
        else {
            for (int ck1 = 0; ck1 < 7; ck1++) {//10
                ant_anoise_num = 0;
                m_threshold = m_sqrt.sqrt(max_val * min_val);
                ant_anoise_num = ant_anoise_num_cal(ck0, m_threshold);
                #ifdef DUMP_RTL_UD
                    dump(m_fp_anoise_maxmin,&max_val,1,m_preamble_pow_fxp,0);
                    dump(m_fp_anoise_maxmin,&min_val,1,m_preamble_pow_fxp);
                #endif
                if(ant_anoise_num < (m_fft_len * m_anoise_the1_frac))
                    min_val = m_threshold;
                else
                    max_val = m_threshold;

                m_Maxval_fxp = max_val;max_val = m_Maxval_fxp;
                m_Minval_fxp = min_val;min_val = m_Minval_fxp;

                ant_anoise_threshold =  max_val * 1.0 / min_val;
                m_ant_anoise_threshold_fxp = ant_anoise_threshold;
                m_ant_anoise[ck0] = m_sqrt.sqrt(max_val * min_val);
                if(m_ant_anoise_threshold_fxp < m_anoise_the_fxp) {
                    #ifdef DUMP_RTL_UD
                        dump(m_fp_anoise_maxmin,&max_val,1,m_preamble_pow_fxp,0);
                        dump(m_fp_anoise_maxmin,&min_val,1,m_preamble_pow_fxp);
                    #endif
                    //LOG2DEV << m_ant_anoise[ck0] << endl;
                    break;
                }
            }
        }
        //LOG2DEV << m_ant_anoise[ck0] << endl;
    }
}

template <class T>
void Cls_ud<T>::dump_ant(ofstream &file_name1, ofstream &file_name2, ofstream &file_name3, ofstream &file_name4, Complex<double> **in_1, Complex<double> **in_2, T **in_3, Complex<double> **in_4, int ant_num)
{
    for (int ck1 = 0; ck1 < m_fft_len; ck1++) {
        file_name1 << in_1[ant_num][ck1].I << "\t" << in_1[ant_num][ck1].Q << endl;
        file_name2 << in_2[ant_num][ck1].I << "\t" << in_2[ant_num][ck1].Q << endl;
        file_name3 << in_3[ant_num][ck1] << endl;
        file_name4 << in_4[ant_num][ck1].I << "\t" << in_4[ant_num][ck1].Q << endl;
    }
}

template <class T>
void Cls_ud<T>::max_min(int ant_num, double *max_val, double *min_val)
{
    double max_val_tmp = 0;
    double min_val_tmp = 1e8;

    for (int ck0 = 0; ck0 < m_fft_len; ck0++)
    {
        if(m_preamble_pow[ant_num][ck0] > max_val_tmp)
            max_val_tmp = m_preamble_pow[ant_num][ck0];
        if(m_preamble_pow[ant_num][ck0] < min_val_tmp)
            min_val_tmp = m_preamble_pow[ant_num][ck0];
    }

    max_val[0] = max_val_tmp;
    min_val[0] = min_val_tmp;
}

template <class T>
int Cls_ud<T>::ant_anoise_num_cal(int ant_num, double threshold)
{
    int cnt = 0;
    for (int ck0 = 0; ck0 < m_fft_len; ck0++)
        if(m_preamble_pow[ant_num][ck0] < threshold)
            cnt = cnt + 1;
    return cnt;
}

template <class T>
void Cls_ud<T>::pow_combine()
{
    double *temp;
    double temp_sum;
    temp = NULL;
    temp = safe_new(temp, m_fft_len);
    /*for (int ck0 = 0; ck0 < m_ant_num; ck0++) {
        m_ant_anoise_frac[ck0] = 1.0 / m_ant_anoise[ck0];
    }*/
    double SumAnoise;
    SumAnoise = 0;
    for(int ck0 = 0; ck0 < m_ant_num; ck0++){
        SumAnoise = SumAnoise + m_ant_anoise[ck0];
    }
    for (int ck0 = 0; ck0 < m_ant_num; ck0++) {
        //m_ant_anoise_frac[ck0] = 1.0 / m_ant_anoise[ck0];
        m_ant_anoise_frac[ck0] = SumAnoise / m_ant_anoise[ck0];
    }

    for (int ck0 = 0; ck0 < m_fft_len; ck0++)
    {
        temp_sum = 0;
        for (int ck1 = 0; ck1 < m_ant_num; ck1++)
        {
            temp_sum = m_preamble_pow[ck1][ck0] * m_ant_anoise_frac[ck1] + temp_sum;
        }
        m_pre_pow_comb_fxp = temp_sum;
        temp[ck0] = m_pre_pow_comb_fxp;
        #ifdef DUMP_RTL_UD
            dump(m_fp_combine,&m_pre_pow_comb_fxp,1,m_pre_pow_comb_fxp);
        #endif
    }
    mem_copy(&m_preamble_pow_o[0], &temp[m_fft_len / 2], m_fft_len / 2);
    mem_copy(&m_preamble_pow_o[m_fft_len / 2], &temp[0], m_fft_len / 2);
    free(temp);
}

template <class T>
void Cls_ud<T>::sig_adjust()
{
    /*double SumAnoise;
    SumAnoise = 0;
    for(int ck0 = 0; ck0 < m_ant_num; ck0++){
        SumAnoise = SumAnoise + m_ant_anoise[ck0];
    }
    for (int ck0 = 0; ck0 < m_ant_num; ck0++) {
        //m_ant_anoise_frac[ck0] = 1.0 / m_ant_anoise[ck0];
        m_ant_anoise_frac[ck0] = SumAnoise / m_ant_anoise[ck0];
    }*/
    for (int ck0 = 0; ck0 < m_ant_num; ck0++)
    {
        //m_ant_noise_rsqrt = rsqrt(m_ant_anoise[ck0]);
        //m_ant_noise_rsqrt = sqrt_bisection(m_ant_anoise_frac[ck0], DOUBLE_POW2[64 - 5]);
        m_anoise_frac_rsqrt = m_sqrt.sqrt(m_ant_anoise_frac[ck0]);
        m_ant_noise_rsqrt = m_anoise_frac_rsqrt;
        #ifdef DUMP_RTL_UD
            if(ck0 != m_ant_num){
                dump(m_fp_anoise,&m_ant_anoise[ck0],1,m_ant_anoise[0],0);
                dump(m_fp_rsqrt_anoise,&m_ant_noise_rsqrt,1,m_ant_noise_rsqrt,0);
            }
            else {
                dump(m_fp_anoise,&m_ant_anoise[ck0],1,m_ant_anoise[0]);
                dump(m_fp_rsqrt_anoise,&m_ant_noise_rsqrt,1,m_ant_noise_rsqrt);
            }
        #endif
        for (int ck1 = 0; ck1 < m_fft_len; ck1++)
        {
            //m_out_fxp = m_fft_o[ck0][ck1] * m_ant_noise_rsqrt;//20.13
            m_out_fxp = m_fft_o[ck0][ck1] * m_anoise_frac_rsqrt;//20.13
            if(m_out_fxp.power()==0){
                m_out_fxp.I = DOUBLE_POW2[64 - 7];
                m_out_fxp.Q = DOUBLE_POW2[64 - 7];
            }
            m_out[ck0][ck1] = m_out_fxp;
        }
        //LOG2DEV << m_ant_noise_rsqrt << endl;
    }
}

template <class T>
void Cls_ud<T>::UserDetection(int FreamIndex, Complex<double> **m_APreambleFft3_Last, double *m_APreambleFftESum3_Last)
{
    int i, j;
    int m_TempAFreqBinDMin, m_TempAFreqBinDMax;

    m_RealFreqAllocationNum = 0;
    for(i = 0; i < m_ant_num; i++){
        for(j = 0; j < m_fft_len / 2; j++){
            m_APreambleFft2[i][j] = m_out[i][j + m_fft_len / 2];
            m_APreambleFft2[i][j + m_fft_len / 2] = m_out[i][j];
        }
    }

    for(i = 0; i < m_fft_len; i++){
        m_APreambleFftESum2[i] = m_preamble_pow_o[i];
    }
    if(FreamIndex==0){
        for(i = 0; i < m_ant_num; i++){
            for(j = 0; j < m_fft_len; j++){
                m_APreambleFft3_fxp.I = 1;
                m_APreambleFft3_fxp.Q = 0;
                m_APreambleFft3[i][j] = m_APreambleFft3_fxp;
            }
        }
        for(i = 0; i < m_fft_len; i++){
            m_APreambleFftESum3_fxp = 1;
            m_APreambleFftESum3[i] = m_APreambleFftESum3_fxp;
        }
    }else{
        for(i = 0; i < m_ant_num; i++){
            for(j = 0; j < m_fft_len; j++){
                m_APreambleFft3_fxp = m_APreambleFft3_Last[i][j];
                m_APreambleFft3[i][j] = m_APreambleFft3_fxp;
            }
        }
        for(i = 0; i < m_fft_len; i++){
            m_APreambleFftESum3_fxp = m_APreambleFftESum3_Last[i];
            m_APreambleFftESum3[i] = m_APreambleFftESum3_fxp;
        }
    }
    for(i = 0; i < m_GWRxAllUser; i++){
        m_AppointTChID[i] = -1;
        m_AFreqBinDMax[i] = 0;
        m_UMode[i] = m_TerminalTxMode[0];
    }

    if(m_freq_alloc_en == 0){
        P1_Rule();

        P2_Rule();

        P3_Rule();

        P4_Rule();

        P5_Rule();

        P6_Rule();

        P7_Rule();

        for(i = 0; i < m_MaxOutLen; i++){
            m_AFreqBinD[i] = m_P7[i];
        #ifdef DUMP_MATLAB_UD
            p4 << m_P4[i] << endl;
            p5 << m_P5[i] << endl;
            p6 << m_P6[i] << endl;
            p7 << m_P7[i] << endl;
        #endif
        #ifdef DUMP_RTL_UD
            dump(m_fp_AFreqBinD,&m_AFreqBinD[i],1,m_P7[0]);
        #endif
        }

        getmaxsort();
    }
    else if(m_freq_alloc_en == 1){

        P1_Rule();
        int TempFreqBinD;
        memcpy(m_AllocationFreqSet , m_freq_alloc_set , 128*sizeof(int));

        for(i = 0; i < m_freq_alloc_num; i++){
            //m_FreqPos[i] = (int)ceil(m_AllocationFreqSet[i] * m_fft_len / m_GWRxSampleRate);
            //m_FreqPos[i] = m_FreqPos[i] + m_fft_len / 2;
            //m_FreqPosIndex0[i] = (int)ceil(m_FreqPos[i] - 200 * m_fft_len / m_GWRxSampleRate);
            //m_FreqPosIndex1[i] = (int)ceil(m_FreqPos[i] + 200 * m_fft_len / m_GWRxSampleRate);
            fxp_1_GWRxSampleRate = 1.0 / m_GWRxSampleRate;//test u25.0
            fxp_fft_rate = m_fft_len * fxp_1_GWRxSampleRate;//s31.14
            m_FreqPos[i] = (int)ceil(m_AllocationFreqSet[i] * fxp_fft_rate );
            m_FreqPos[i] = m_FreqPos[i] + m_fft_len / 2;
            m_FreqPosIndex0[i] = (int)ceil(m_FreqPos[i] - 200 * fxp_fft_rate);
            m_FreqPosIndex1[i] = (int)ceil(m_FreqPos[i] + 200 * fxp_fft_rate);
            TempFreqBinD = m_FreqPosIndex0[i];
            for(int ck0 = m_FreqPosIndex0[i] + 1; ck0 <= m_FreqPosIndex1[i] ; ck0++)
            {
                if(m_APreambleFftESum2[TempFreqBinD] < m_APreambleFftESum2[ck0])
                    TempFreqBinD = ck0;
            }
            m_AFreqBinD0[i] = TempFreqBinD;
            //LOG2DEV << TempFreqBinD << endl;
        }
        m_RealFreqAllocationNum = 0;
        for(i = 0; i < m_freq_alloc_num; i++)
        {
            /*for(int ck0 = 0;ck0 < m_HLPosLen;ck0++)
            {
                if( m_HighLightPos[ck0] == m_AFreqBinD0[i])// need modify
                {
                    if(m_P1[ck0] == 1)
                    {
                        m_AFreqBinDMax[m_RealFreqAllocationNum] = m_AFreqBinD0[i];
                        //m_UMode_o[m_RealFreqAllocationNum] = (int)m_AllocationFreqSet[i];
                        m_RealFreqAllocationNum += 1;
                    }
                    break;
                }
            }*/
            m_AFreqBinDMax[m_RealFreqAllocationNum] = m_AFreqBinD0[i];
            m_RealFreqAllocationNum += 1;
        }
        if(m_GWRxAllUser - m_RealFreqAllocationNum > 0){
            P2_Rule();

            P3_Rule();

            P4_Rule();

            P5_Rule();

            P6_Rule();

            P7_Rule();

//            for(i = 0; i < m_TempAFreqBinDMax - m_TempAFreqBinDMin; i++){
//                m_P7[i + m_TempAFreqBinDMin] = 0;
//            }
            for(i = 0; i < m_MaxOutLen-m_RealFreqAllocationNum; i++){
                m_AFreqBinD[m_RealFreqAllocationNum+i] = m_P7[i];
            }
            m_TempAFreqBinDMin = m_AFreqBinDMax[0] - 64;
            m_TempAFreqBinDMax = m_AFreqBinDMax[m_RealFreqAllocationNum - 1] + 64;

            for(int ck0 = 0;ck0 < m_MaxOutLen;ck0++)
            {
                if(m_HighLightPos[ck0] >= m_TempAFreqBinDMin && m_HighLightPos[ck0] <= m_TempAFreqBinDMax)
                    m_AFreqBinD[ck0] = 0;
            }
            for(i = 0;i<m_RealFreqAllocationNum;i++){
                m_AFreqBinD[i] = 1;
            }
            #ifdef DUMP_RTL_UD
            for(i = 0; i < m_MaxOutLen-m_RealFreqAllocationNum; i++){
                dump(m_fp_AFreqBinD,&m_AFreqBinD[m_RealFreqAllocationNum+i],1,m_P7[0]);
            }
            #endif
            getmaxsort();
        }
    }

    ud_out_module();
    m_user_info_o_en = 1;
#ifdef DUMP_MATLAB_UD
    for(i = 0; i < m_fft_len; i++){
        ud_mat_out1 <<fixed<<setprecision(30)<< m_P7[i] <<endl;
        //ud_mat_out2 <<fixed<<setprecision(30)<< m_P5[i] <<endl;
        //ud_mat_out3 <<fixed<<setprecision(30)<< m_P6[i] <<endl;
        //ud_mat_out4 <<fixed<<setprecision(30)<< m_P7[i] <<endl;
    }
    for(i = 0; i < m_GWRxAllUser; i++){
        ud_mat_out2 <<fixed<<setprecision(30)<< m_freq_offset_o[i] <<endl;
        ud_mat_out3 <<fixed<<setprecision(30)<< m_pilot_power_o[i] <<endl;
        for(j = 0; j < m_ant_num; j++){
            ud_mat_out4 <<fixed<<setprecision(30)<< m_ah_o[j][i].I <<endl;
            ud_mat_out4 <<fixed<<setprecision(30)<< m_ah_o[j][i].Q <<endl;
        }
    }
#endif
}

template <class T>
void Cls_ud<T>::P1_Rule()
{
    int i, j;
    double THEtmp;
    int Tdiff;
    int m;

    int *index1;
    double *The;
    double *arr;
    int *tmpP1;

    index1 = NULL;
    The    = NULL;
    arr    = NULL;
    tmpP1  = NULL;

    index1 = safe_new(index1, 256);
    The    = safe_new(The, m_fft_len);
    arr    = safe_new(arr, 256);
    tmpP1  = safe_new(tmpP1, m_fft_len);

    m_K_fxp = K[m_p1_the];
    if(m_fft_len <= 2048){
        Tdiff = 64;
    }
    else if(m_fft_len > 2048 && m_fft_len <= 8192){
        Tdiff = 128;
    }
    else{
        Tdiff = 256;
    }
    m = m_fft_len / (Tdiff / 2);

    for(i = 0; i < m; i++){
        index1[i] = (i + 1) * (Tdiff / 2);
    }

    for(i = 0; i < m - 2; i++){
        for(j = 0; j < Tdiff; j++){
            arr[j] = m_APreambleFftESum2[index1[i] + j];
        }
        getThreadValue(arr, Tdiff, &THEtmp); //24.19
        for(j = 0; j < Tdiff; j++){
            The[index1[i] + j] = THEtmp;
        }
    }

    #ifdef DUMP_RTL_UD
    for(i = 0; i < m_fft_len; i++){
        if(i < Tdiff || i >= m_fft_len-Tdiff){
            m_midval_fxp = 0.0;
            dump(m_fp_getThreadValue,&m_midval_fxp,1,m_midval_fxp);
        }else {
            dump(m_fp_getThreadValue,&The[i],1,m_midval_fxp);
        }
        dump(m_fp_get_pow ,&m_APreambleFftESum2[i],1,m_pre_pow_comb_fxp);
     }
    #endif

    m_HLPosLen = 0;
    ones(m_HighLightPos,m_MaxOutLen);
    zeros(tmpP1,m_fft_len);

    for(i = 0; i < (m_fft_len - Tdiff*2); i++){
        if(m_APreambleFftESum2[i+Tdiff] > (The[i+Tdiff] * m_K_fxp)){
            m_HighLightPos[m_HLPosLen] = i+Tdiff;
            #ifdef DUMP_RTL_UD
	            fxp_P1_HLpos = m_HighLightPos[m_HLPosLen];
                dump(m_fp_P1_HLP,&fxp_P1_HLpos ,1,fxp_P1_HLpos);
            #endif
            m_HLPosLen = m_HLPosLen + 1;
            tmpP1[i+Tdiff] = 1;
            if(m_HLPosLen>m_MaxOutLen){
                break;
            }
        }
    }

    /*for(int i1 = 0; i1 < m_fft_len; i1++){
        file_o_in_Tmp1 << fixed << setprecision(26) <<The[i1]<<"\t"<<endl;
    }*/

    zeros(m_P1,m_MaxOutLen);
    for(int ck0 = 0;ck0 < m_HLPosLen;ck0++)
    {
        m_P1[ck0] = 1;
    }

    /*for(int i1 = 0; i1 < m_MaxOutLen; i1++){
        file_o_in_Tmp1 << fixed << setprecision(26) <<m_HighLightPos[i1]<<"\t"<<endl;
    }*/
    InterfrencePeriodicIdxCalcuNew(tmpP1); //int

    free(index1);
    free(The);
    free(arr);
    free(tmpP1);
}

template <class T>
void Cls_ud<T>::getThreadValue(double * in, int Len_in, double * out)
{
    int ii;
    int greater, less, rate;
    int n_min, n_max;
    double m_eps = 0.031250000000000;//0.0000001;
    double maxval, minval, midval;

    maxval = in[0];
    for(ii = 1; ii < Len_in; ii++){
        if(in[ii] > maxval){
            maxval = in[ii];
        }
    }
    minval = in[0];//16.10
    for(ii = 1; ii < Len_in; ii++){
        if(in[ii] < minval){
            minval = in[ii];
        }
    }
    if(minval < m_eps){
        minval = m_eps;
    }
    //m_midval_fxp = sqrt(maxval * minval);
    //m_midval_fxp = sqrt_bisection(maxval * minval, DOUBLE_POW2[64 - 5]);
    m_midval_fxp = m_sqrt.sqrt(maxval * minval);

    midval = m_midval_fxp;
    n_min = (int)(Len_in * m_GWRX_P1_Para1 + 0.5);
    n_max = (int)(Len_in * m_GWRX_P1_Para2 + 0.5);

    for (int ck0 = 0; ck0 < 7; ck0++){//10
        greater = 0;
        less = 0;
        for(ii = 0; ii < Len_in; ii++){
            if(in[ii] > midval){
                greater += 1;
            }
            if(in[ii] < midval){
                less += 1;
            }
        }
        m_val_rate_fxp = maxval / minval;
		rate = m_val_rate_fxp < m_anoise_the_fxp ? 1 : 0;
        if(((greater >= n_min) && (greater <= n_max)) || rate){
            *out = midval;
	    #ifdef DUMP_RTL_UD
	         //cout << "getThreadValue for cnt = " << "\t" << ck0 << endl;
            #endif
            break;
        }
        if(greater > less){
            minval = midval;
        }
        else{
            maxval = midval;
        }
        //m_midval_fxp = sqrt(maxval * minval);
        //m_midval_fxp = sqrt_bisection(maxval * minval, DOUBLE_POW2[64 - 5]);
        m_midval_fxp = m_sqrt.sqrt(maxval * minval);
        midval = m_midval_fxp;
    }
}

template <class T>
void Cls_ud<T>::InterfrencePeriodicIdxCalcuNew(int *tmpP1)
{
    int ii, jj;
    int xtmp, maxtmp, maxtmpidx;
    int indxLen, indxLen1;
    int arrNum;

    int *signSig   ;
    int *trend     ;
    int *diffTrend ;
    int *index     ;
    int *diffVal0  ;
    int *diffVal   ;
    int *numElement;

    signSig    = NULL;
    trend      = NULL;
    diffTrend  = NULL;
    index      = NULL;
    diffVal0   = NULL;
    diffVal    = NULL;
    numElement = NULL;
    signSig    = safe_new(signSig, m_fft_len)   ;
    trend      = safe_new(trend, m_fft_len)     ;
    diffTrend  = safe_new(diffTrend, m_fft_len) ;
    index      = safe_new(index, m_fft_len)     ;
    diffVal0   = safe_new(diffVal0, m_fft_len)  ;
    diffVal    = safe_new(diffVal, m_fft_len)   ;
    numElement = safe_new(numElement, m_fft_len);

    for(ii = 0; ii < m_fft_len - 1; ii++){
        signSig[ii] = (int)(tmpP1[ii + 1] - tmpP1[ii]);
    }
    for(ii = 0; ii < m_fft_len - 2; ii++){
        if(signSig[ii] > 0 || signSig[ii] < 0){
            trend[ii] = signSig[ii];
        }
        else if(signSig[ii] == 0 && signSig[ii + 1] >= 0){
            trend[ii] = 1;
        }
        else if(signSig[ii] == 0 && signSig[ii + 1] < 0){
            trend[ii] = -1;
        }
    }

    indxLen = 0;
    for(ii = 0; ii < m_fft_len - 3; ii++){
        diffTrend[ii] = trend[ii + 1] - trend[ii];
        if(diffTrend[ii] == -2){
            index[indxLen] = ii;
            indxLen += 1;
        }
    }
    if(indxLen <= 1){
        m_Idx = 0;
    }
    else{
        indxLen1 = 0;
        for(ii = 0; ii < indxLen - 1; ii++){
            diffVal0[ii] = index[ii + 1] - index[ii];
            if(diffVal0[ii] > (m_fft_len / 128)){//8192/128=64
                diffVal[indxLen1] = diffVal0[ii];
                #ifdef DUMP_RTL_UD
                    fxp_diffval = diffVal[indxLen1];
                    dump(m_fp_diffVal,&fxp_diffval,1,fxp_diffval);
                #endif
                indxLen1 += 1;
            }
        }

        if(indxLen1 < 1){
           m_Idx = 0;
           return;
        }

        for(ii = 0; ii < indxLen1; ii++){
            arrNum = 0;
            xtmp = diffVal[ii];
            for(jj = 0; jj < indxLen1; jj++){
                if((xtmp >= (diffVal[jj] - 1)) && (xtmp <= (diffVal[jj] + 1))){
                    arrNum += 1;
                }
            }
            numElement[ii] = arrNum;
        }

        maxtmp = numElement[0];
        maxtmpidx = 0;
        for(ii = 1; ii < indxLen1; ii++){
            if(numElement[ii] > maxtmp){
                maxtmp = numElement[ii];
                maxtmpidx = ii;
            }
        }

        if(maxtmp >= 4){
            m_Idx = diffVal[maxtmpidx];
        }
        else{
            m_Idx = 0;
        }
        if(m_Idx > (m_fft_len / 8)){
            m_Idx = 0;
        }
        if(m_Idx < (m_fft_len / 128)){
            m_Idx = 0;
        }
    }

    free(signSig)   ;
    free(trend)     ;
    free(diffTrend) ;
    free(index)     ;
    free(diffVal0)  ;
    free(diffVal)   ;
    free(numElement);
}

template <class T>
void Cls_ud<T>::P2_Rule()
{
    int ii;
    double maxval;
    double the;
    int poslen0, tmpLen;

    double cand  ;

    m_K_fxp = K[m_p2_the];
    maxval = m_APreambleFftESum2[0];
    for(ii = 1; ii < m_fft_len; ii++){
        if(m_APreambleFftESum2[ii] > maxval){
            maxval = m_APreambleFftESum2[ii]; //24.19
        }
    }
    m_p2_the_fxp = maxval / m_K_fxp;
    poslen0 = 0;

    mem_copy(m_P2,m_P1,m_MaxOutLen);

    for(ii = 0; ii < m_HLPosLen; ii++){
        if(m_HighLightPos[ii]==1){
            m_P2[ii] = 0;
        }else{
            cand = m_APreambleFftESum2[m_HighLightPos[ii]];
            if(cand < m_p2_the_fxp){
                m_P2[ii] = 0;
                m_HighLightPos[ii] = 1;
            }
        }
        #ifdef DUMP_RTL_UD
            m_fp_P2_d<<m_P2[ii]<<endl;
        #endif
    }

}
template <class T>
void Cls_ud<T>::P3_Rule()
{
    int  a, b;
    int ii, jj;
    int maxval_idx;
    double P3_The;
    double maxval;
    double fa, fb;
    double out;
    int r;
    int Len, Size;
    p_corr_flg = 3;//print use
    P3_The = K[m_p3_the];//pow(10, (m_p3_the / 10));

    for(ii=0;ii<m_MaxOutLen;ii++){
        m_P3[ii] = m_P2[ii];
    }
    if((m_UDSwitch>>2 & 0x1)==0){
        return;
    }
    for(ii = 0; ii < m_HLPosLen; ii++){

        if(m_HighLightPos[ii]==1){

            m_P3[ii] = 0;
            #ifdef DUMP_RTL_UD
                m_cor_fxp = m_P3[ii];
                dump(m_fp_P3_d,&m_cor_fxp,1,m_cor_fxp);
            #endif
        }else{

            a = m_HighLightPos[ii];
            b = m_fft_len + 4 - a - 2;
            maxval = m_APreambleFftESum2[b - 2];
            maxval_idx = 0;

            for(jj = b - 1; jj < b + 3; jj++){
                if(m_APreambleFftESum2[jj] > maxval){
                    maxval = m_APreambleFftESum2[jj];
                    maxval_idx = jj - b + 2;
                }
            }

            b = b + maxval_idx - 2;
            fa = m_APreambleFftESum2[a];
            fb = m_APreambleFftESum2[b];

            if(fb > P3_The * fa){
                Len = m_ant_num;
                Size = 1;
                correlation(a, &b, Len, Size, 0, 1, &out);
                m_P3[ii] = m_P3[ii] * (1 - out);
                r = 1;
            }
            #ifdef DUMP_RTL_UD
                m_cor_fxp = r == 1 ? out : 0;
                dump(m_fp_P3_d,&m_cor_fxp,1,m_cor_fxp);
            #endif
            r = 0;
        }

    }

    /*for(int i1 = 0; i1 < m_MaxOutLen; i1++){
        file_o_in_Tmp1 << fixed << setprecision(26) <<m_P3[i1]<<"\t"<<endl;
    }*/
}
template <class T>
void Cls_ud<T>::P4_Rule()
{
    int m;
    int ii, jj;
    int x1, x2, x3;
    int a, Len, Size, b[3];
    double fa, fb[3], y[3], out[3], Q[3];

    double maxval;
    p_corr_flg = 4;//print use
    Len = m_ant_num;
    Size = 3;

    for(ii = 0; ii < m_MaxOutLen; ii++){
        m_P4[ii] = m_P3[ii];
    }
    if((m_UDSwitch>>3 & 0x1)==0){
        return;
    }
    for(ii = 0; ii < m_MaxOutLen; ii++){
        if(m_HighLightPos[ii]==1){

            m_P4[ii] = 0;
            #ifdef DUMP_RTL_UD
                m_Q_fxp = m_P4[ii];
                dump(m_fp_P4_d,&maxval,1,m_Q_fxp);
            #endif
        }else{
            a = m_HighLightPos[ii];
            fa = m_APreambleFftESum2[a];

            if(a == 0){
                x1 = m_fft_len - 1;
                x2 = a;
                x3 = a + 1;
            }
            else if(a == m_fft_len - 1){
                x1 = a - 1;
                x2 = a;
                x3 = 0;
            }
            else{
                x1 = a - 1;
                x2 = a;
                x3 = a + 1;
            }

            fb[0] = m_APreambleFftESum3[x1];
            fb[1] = m_APreambleFftESum3[x2];
            fb[2] = m_APreambleFftESum3[x3];

            for(jj = 0; jj < 3; jj++){
                m_fa_fb_frac = fa / fb[jj];
                m_y_fxp = (2 * m_fa_fb_frac) / (1 + m_fa_fb_frac * m_fa_fb_frac);
                y[jj] = m_y_fxp;//15.-6
            }
            b[0] = a - 1;
            b[1] = a;
            b[2] = a + 1;

            correlation(a, b, Len, Size, 1, 0, out);

            for(jj = 0; jj < 3; jj++){
                //Q[jj] = sqrt(y[jj]) * out[jj];
                //m_Q_fxp = sqrt_bisection(y[jj], DOUBLE_POW2[64 - 5]) * out[jj];
                //m_Q_fxp = m_sqrt.sqrt(y[jj]*out[jj]);
				m_y_sqrt_fxp = m_sqrt.sqrt(y[jj]);
                m_Q_fxp = m_y_sqrt_fxp*out[jj];//yingxiangjieguo
                Q[jj] = m_Q_fxp;
            }

            maxval = Q[0];
            if(Q[1] > maxval){
                maxval = Q[1];
            }
            if(Q[2] > maxval){
                maxval = Q[2];
            }
            #ifdef DUMP_RTL_UD
                dump(m_fp_P4_d,&maxval,1,m_Q_fxp);
            #endif
            m_P4[ii] = m_P4[ii] * (1 - maxval);
        }

    }
    /*for(int i1 = 0; i1 < m_MaxOutLen; i1++){
        file_o_in_Tmp1 << fixed << setprecision(26) <<m_P4[i1]<<"\t"<<endl;
    }*/
}
template <class T>
void Cls_ud<T>::P5_Rule()
{
    int ii, jj;
    int a, Len, Size, b[2];
    double fa, fb[2], y[2], out[2], Q[2];
    double maxval;
    p_corr_flg = 5;//print use
    Len = m_ant_num;
    Size = 1;

    for(ii = 0; ii < m_MaxOutLen; ii++){
        m_P5[ii] = m_P4[ii];
    }
    if((m_UDSwitch>>4 & 0x1)==0){
        return;
    }
    for(ii = 0; ii < m_HLPosLen; ii++){

        if(m_HighLightPos[ii]==1){

            m_P5[ii] = 0;
            #ifdef DUMP_RTL_UD
                m_p5_Q_fxp = m_P5[ii];
                dump(m_fp_P5_d,&maxval,1,m_p5_Q_fxp);
            #endif
        }else{

            a = m_HighLightPos[ii];
            fa = m_APreambleFftESum2[a];
            fb[0] = m_APreambleFftESum2[a - 1];
            fb[1] = m_APreambleFftESum2[a + 1];

            for(jj = 0; jj < 2; jj++){
                if(fb[jj] > fa){
                    y[jj] = 1;
                }
                else{
                    y[jj] = 0;
                }
            }
            b[0] = a - 1;
            b[1] = a + 1;
            correlation(a, &b[0], Len, Size, 0, 1, &out[0]);
            correlation(a, &b[1], Len, Size, 0, 1, &out[1]);
            for(jj = 0; jj < 2; jj++){
                m_p5_Q_fxp = y[jj] * out[jj];
                Q[jj] = m_p5_Q_fxp;
            }
            maxval = Q[0];
            if(Q[1] > maxval){
                maxval = Q[1];
            }
            #ifdef DUMP_RTL_UD
               dump(m_fp_P5_d,&maxval,1,m_p5_Q_fxp);
            #endif
            m_P5[ii] = m_P5[ii] * (1 - maxval);

        }
    }
    /*for(int i1 = 0; i1 < m_MaxOutLen; i1++){
        file_o_in_Tmp1 << fixed << setprecision(26) <<m_P5[i1]<<"\t"<<endl;
    }*/
}
template <class T>
void Cls_ud<T>::P6_Rule()
{
    int ii, jj;
    int TempStart, n;
    int a, b, amax, amin;
    int Len, Size;
    double P6_The;
    double fa, fb;
    double out, maxval, x;
    double Q0,QMax;
    int Num = 0;
    p_corr_flg = 6;//print use
    Len = m_ant_num;
    Size = 1;

    TempStart = (int)(128.0 * (m_fft_len/8192.0));
    TempStart = 20;
    P6_The = K[m_p6_the];


    for(ii = 0; ii < m_MaxOutLen; ii++){
        m_P6[ii] = m_P5[ii];
    }
    if((m_UDSwitch>>5 & 0x1)==0){
        return;
    }
    for(ii = 0; ii < m_HLPosLen; ii++){
        if(ii==535){
            Size = 1;
        }
        if(m_HighLightPos[ii]==1){
            m_P6[ii] = 0;
            #ifdef DUMP_RTL_UD
                m_cor_fxp = m_P6[ii];
                dump(m_fp_P6_d,&m_cor_fxp,1,m_cor_fxp);
            #endif
        }else{
            a = m_HighLightPos[ii];
            amax = a + TempStart;
            amin = a - TempStart;
            Q0 = 0;
            QMax = 0;
            for(int j = 0; j < TempStart;j++)
            {
                if(ii - j > 0)
                {
                    if(m_HighLightPos[ii - j] >= amin)
                    {
                        b = m_HighLightPos[ii - j];
                        fb = m_APreambleFftESum2[b];
                        fa = m_APreambleFftESum2[a];
                        if(fb > fa * P6_The){
                            x = 1.0;
                        }
                        else{
                            x = 0;
                        }
                        if(x == 1)
                        {
                            correlation(a, &b, Len, Size, 0, 1, &Q0);
                            Num++;
                        }else{
                            Q0 = 0;
                        }
                        if(Q0 > QMax)
                            QMax = Q0;
                    }
                }
                if(ii + j <=m_MaxOutLen)
                {
                    if(m_HighLightPos[ii+j] <= amax)
                    {
                        b = m_HighLightPos[ii+j];
                        fb = m_APreambleFftESum2[b];
                        fa = m_APreambleFftESum2[a];
                        if(fb > fa * P6_The){
                            x = 1.0;
                        }
                        else{
                            x = 0;
                        }
                        if(x == 1)
                        {
                            correlation(a, &b, Len, Size, 0, 1, &Q0);
                            Num++;
                        }
                        else{
                            Q0 = 0;
                        }
                        if(Q0 > QMax)
                            QMax = Q0;

                    }
                }
                if(Num>=m_MaxOutLen*2){
                    break;
                }
            }
            m_P6[ii] = m_P6[ii ] * (1 - QMax);
            #ifdef DUMP_RTL_UD
                m_cor_fxp = QMax;
                dump(m_fp_P6_d,&m_cor_fxp,1,m_cor_fxp);
            #endif
        }
    }
    //LOG2DEV << Num << " " << m_HLPosLen << endl;
    /*for(int i1 = 0; i1 < m_MaxOutLen; i1++){
        file_o_in_Tmp1 << fixed << setprecision(26) <<m_P6[i1]<<"\t"<<endl;
    }*/
}

template <class T>
void Cls_ud<T>::P7_Rule()
{
    int m, ii, jj, ij;
    int a, b[2], b1[2], c[2], c1[2], x2;
    int Len, Size;
    double maxval;
    double fa, fb[2];
    double out[2], Q[2], y[2];
    p_corr_flg = 7;//print use
    Len = m_ant_num;
    Size = 2;

    m = m_HLPosLen;

    for(ii = 0; ii < m_MaxOutLen; ii++){
        m_P7[ii] = m_P6[ii];
    }
    if((m_UDSwitch>>6 & 0x1)==0){
        return;
    }
    if(m_Idx == 0){
    #ifdef DUMP_MATLAB_UD_HJH
        for(int i1 = 0; i1 < m_MaxOutLen; i1++){
            file_UD_P7 << fixed << setprecision(26) <<m_P7[i1]<<"\t"<<endl;
        }
    #endif
        #ifdef DUMP_RTL_UD
        for(ii = 0; ii < m_MaxOutLen; ii++){
	        m_cor_fxp = 0;
            dump(m_fp_P7_d,&m_cor_fxp,1,m_cor_fxp);
        }
        #endif
        return;
    }
    for(ii = 0; ii < m_HLPosLen; ii++){

        if(m_HighLightPos[ii]==1){
            m_P7[ii] = 0;
            #ifdef DUMP_RTL_UD
	            m_cor_fxp = m_P7[ii];
                dump(m_fp_P7_d,&m_cor_fxp,1,m_cor_fxp);
            #endif
        }else{
            a = m_HighLightPos[ii];
            b[0] = a - m_Idx;
            b[1] = a + m_Idx;
            for(jj = 0; jj < 2; jj++){
                if(b[jj] > m_fft_len - 1){
                    b[jj] = b[jj] - m_fft_len;
                }
                if(b[jj] < 0){
                    b[jj] = b[jj] + m_fft_len;
                }
                c[0] = b[jj] - 1;
                c[1] = b[jj] + 1;
                if(b[jj] < 0){
                    c[0] = 0;
                }
                if(b[jj] + 1 > m_fft_len - 1){
                    c[1] = m_fft_len - 1;
                }
                maxval = m_APreambleFftESum2[c[0]];
                x2 = 0;
                for(ij = c[0] + 1; ij < c[1] + 1; ij++){
                    if(m_APreambleFftESum2[ij] > maxval){
                        maxval = m_APreambleFftESum2[ij];
                        x2 = ij - c[0];
                    }
                }
                b[jj] = b[jj] + x2 - 1;
                if(b[jj] >= m_fft_len - 1){
                    b[jj] = m_fft_len - 1;
                }
                if(b[jj] < 0){
                    b[jj] = 0;
                }
            }
            correlation(a, b, Len, Size, 0, 1, out);
            Q[0] = out[0];
            Q[1] = out[1];
            maxval = Q[0];
            if(Q[1] > maxval){
                maxval = Q[1];
            }
            #ifdef DUMP_RTL_UD
                m_cor_fxp = maxval;
                dump(m_fp_P7_d,&m_cor_fxp,1,m_cor_fxp);
            #endif
            m_P7[ii] = m_P7[ii] * (1 - maxval);
        }
    }
    #ifdef DUMP_MATLAB_UD_HJH
        for(int i1 = 0; i1 < m_MaxOutLen; i1++){
            file_UD_P7 << fixed << setprecision(26) <<m_P7[i1]<<"\t"<<endl;
        }
    #endif
}

template <class T>
void Cls_ud<T>::correlation(int a, int *b, int Len, int Size, int fft0, int flag, double *out)
{
    int ii, jj;
    Complex<T>      v0[16];
    Complex<T>      v1[16];
    Complex<T>      v2[16];
    Complex<T>      v1v2;
    double          v1_norm, v2_norm;
    double          v1v2_norm;
    double          TmpD;

    for(ii = 0; ii < Len; ii++){
        v0[ii] = m_APreambleFft2[ii][a];
    }
    for(ii = 0; ii < Size; ii++){
        if(flag == 0){
            if(fft0 == 0){
                for(jj = 0; jj < Len; jj++){
                    v2[jj] = 0.0;
                }
            }
            else{
                for(jj = 0; jj < Len; jj++){
                    v2[jj] = m_APreambleFft3[jj][b[ii]];
                }
            }
        }
        else{
            for(jj = 0; jj < Len; jj++){
                v2[jj] = m_APreambleFft2[jj][b[ii]];
            }
        }
        for(jj = 0; jj < Len; jj++){
            v1[jj] = v0[jj].conjug();
        }
        v1_norm = 0;
        v2_norm = 0;
        /*for(jj = 0; jj < Len; jj++){
            v1_norm += v1[jj].power();
            v2_norm += v2[jj].power();
        }
        m_v1_norm_tmp_fxp = v1_norm; // 22.16
        m_v2_norm_tmp_fxp = v2_norm; // 22.16
        m_v1_norm_fxp = sqrt_bisection(m_v1_norm_tmp_fxp, DOUBLE_POW2[64 - 5]);
        m_v2_norm_fxp = sqrt_bisection(m_v2_norm_tmp_fxp, DOUBLE_POW2[64 - 5]);
        v1v2 = 0.0;
        for(jj = 0; jj < Len; jj++){
            v1v2 += v1[jj] * v2[jj];
        }
        m_v1v2_fxp = v1v2;
        m_abs_fxp = sqrt_bisection(m_v1v2_fxp.power(),  DOUBLE_POW2[64 - 5]);
        m_cor_fxp = m_abs_fxp / m_v1_norm_fxp / m_v2_norm_fxp;
//        m_cor_fxp = m_abs_fxp / (m_v1_norm_fxp * m_v2_norm_fxp);
        out[ii] = m_cor_fxp;*/
        v1v2 = 0.0;
        for(jj = 0; jj < Len; jj++){
            v1_norm += v1[jj].power();//v1 16.10
            v2_norm += v2[jj].power();
            v1v2 += v1[jj] * v2[jj];
        }
        if(v1v2.power()>DOUBLE_POW2[64 + 30]){
            v1v2 = v1v2/32;v1_norm = v1_norm/32;v2_norm = v2_norm/32;
        }else if(v1v2.power()<DOUBLE_POW2[64 - 10]){
            v1v2 = v1v2*32;v1_norm = v1_norm*32;v2_norm = v2_norm*32;
        }else{
        }
        //
        v1v2_norm = (v1_norm * v2_norm);
        m_v1_norm_tmp_fxp = v1v2_norm;
        //m_v2_norm_tmp_fxp = v1v2.power()/m_v1_norm_tmp_fxp;
        m_v3_norm_tmp_fxp = v1v2.power();
        m_v2_norm_tmp_fxp = m_v3_norm_tmp_fxp/m_v1_norm_tmp_fxp;
        //m_abs_fxp = sqrt_bisection(m_v2_norm_tmp_fxp,  DOUBLE_POW2[64 - 5]);
        m_abs_fxp = m_sqrt.sqrt(m_v2_norm_tmp_fxp);
        m_cor_fxp = m_abs_fxp;
        #ifdef DUMP_RTL_UD
        if(p_corr_flg == 4){
            for(jj = 0; jj < Len; jj++){
                if(jj != Len-1)
                    dump(m_fp_fft_o_2,&v0[jj],1,m_out_fxp.I ,0);
                else
                    dump(m_fp_fft_o_2,&v0[jj],1,m_out_fxp.I);
            }
            for(jj = 0; jj < Len; jj++){
                if(jj != Len-1)
                    dump(m_fp_fft_o_2,&v2[jj],1,m_out_fxp.I ,0);
                else
                    dump(m_fp_fft_o_2,&v2[jj],1,m_out_fxp.I);
            }
            dump(m_fp_fft_o_2,&m_v1_norm_tmp_fxp,1,m_v1_norm_tmp_fxp);
            dump(m_fp_fft_o_2,&m_v2_norm_tmp_fxp,1,m_v2_norm_tmp_fxp);
            dump(m_fp_fft_o_2,&m_abs_fxp,1,m_abs_fxp);
            dump(m_fp_fft_o_2,&m_cor_fxp,1,m_cor_fxp);
            m_fp_fft_o_2 <<"============="<<endl;
		}
        #endif
        out[ii] = m_cor_fxp;
    }
}

template <class T>
void Cls_ud<T>::getmaxsort()
{
    int ii, jj;
    int maxidx;
    double maxval;
    for(ii = 0; ii < m_HLPosLen; ii++){
        m_AFreqBinDtmp[ii] = m_AFreqBinD[ii];
    }
    if(m_freq_alloc_en==0){
        for(ii = 0; ii < m_GWRxAllUser; ii++){
            maxval = m_AFreqBinDtmp[0];
            maxidx = 0;
            for(jj = 1; jj < m_HLPosLen; jj++){
                if(m_AFreqBinDtmp[jj] > maxval){
                    maxval = m_AFreqBinDtmp[jj];
                    maxidx = jj;
                }
            }
            m_AFreqBinD[ii] = maxval;
            m_AFreqBinDMax[ii] = m_HighLightPos[maxidx];
            m_AFreqBinDtmp[maxidx] = 0;
        }
    }else{
        if((m_GWRxAllUser-m_RealFreqAllocationNum) > 0){
            for(ii=0;ii<(m_GWRxAllUser-m_RealFreqAllocationNum);ii++){
                maxval = m_AFreqBinDtmp[m_RealFreqAllocationNum];
                maxidx = m_RealFreqAllocationNum;
                for(jj = m_RealFreqAllocationNum+1; jj < m_HLPosLen; jj++){//m_MaxOutLen m_HLPosLen
                    if(m_AFreqBinDtmp[jj] > maxval){
                        maxval = m_AFreqBinDtmp[jj];
                        maxidx = jj;
                    }
                }
                m_AFreqBinD[m_RealFreqAllocationNum+ii] = maxval;
                m_AFreqBinDMax[m_RealFreqAllocationNum+ii] = m_HighLightPos[maxidx];
                m_AFreqBinDtmp[maxidx] = 0;
            }
        }
    }
        #ifdef DUMP_MATLAB_UD
    for(ii = 0; ii < m_GWRxAllUser; ii++)
    {
        AFreqBinDMax << m_AFreqBinDMax[ii] << endl;
    }
        #endif

    #ifdef DUMP_RTL_UD
    for(ii = 0; ii < m_GWRxAllUser; ii++){
        fxp_P1_HLpos = m_AFreqBinDMax[ii];
        dump(m_fp_AFreqBinDMax,&fxp_P1_HLpos,1,fxp_P1_HLpos);
    }
    #endif

#ifdef DUMP_MATLAB_UD_HJH
    int TmpAFreqBinDMax[128];
    mem_copy(TmpAFreqBinDMax, m_AFreqBinDMax, m_GWRxAllUser);
    quicksort(TmpAFreqBinDMax, 0, m_GWRxAllUser-1);
    for(ii = 0; ii < m_GWRxAllUser; ii++){
        m_AFreqBinDMax[ii] = TmpAFreqBinDMax[ii];
    }
    for(int i1 = 0; i1 < m_GWRxAllUser; i1++){
        file_UD_Out << fixed << setprecision(26) <<m_AFreqBinDMax[i1]<<"\t"<<endl;
    }
    for(int i1 = 0; i1 < m_GWRxAllUser; i1++){
        file_UD_Out << fixed << setprecision(26) <<m_AFreqBinD[i1]<<"\t"<<endl;
    }
#endif

}


template <class T>
void Cls_ud<T>::generate_HCfilter()
{
    for (int ck0 = 0; ck0 < m_halfBL; ck0++)
    {
        m_HC_fxp = m_HC_tmp1[m_fft_len - m_halfBL + ck0] * m_HCE;
        m_HC[ck0] = m_HC_fxp;
        //LOG2DEV <<m_HC_fxp.I<<endl;
        m_HC_fxp = m_HC_tmp1[ck0] * m_HCE;
        m_HC[m_halfBL + ck0] = m_HC_fxp;
    }
/*#ifdef DUMP_MATLAB_IS
    for(int ck0 = 0; ck0 < m_halfBL * 2; ck0++)
        file_mat_hc << m_HC[ck0].I << "\t" << m_HC[ck0].Q << endl;
#endif*/
/*#ifdef DUMP_RTL_IS
    dump(file_rtl_HC, &m_HC[0], m_halfBL * 2, m_HC_fxp.I);
#endif*/
}


template <class T>
void Cls_ud<T>::upsample(int time)
{
    double htmp[8] = {0,-0.0634756387831438,0.0368512994716393,0.303904820085406,0.463389896180281,0.303904820085406,0.0368512994716393,-0.0634756387831438};

    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for(int ck1 = 0; ck1 < time; ck1 ++)
        {
            if(ck1 == 0)
                m_htmp1[ck0 * time + ck1] = htmp[ck0];
            else
                m_htmp1[ck0 * time + ck1] = 0;
        }
    }

}


template <class T>
void Cls_ud<T>::generate_para()
{
    int time = 1;
    switch (m_cfg.u_mode)
    {
    case 1://125/128/8
        time = 1;
        break;
    case 2://125/128/4
        time = 1;
        break;
    case 3://125/128/2
        time = 8;
        break;
    case 4://125/128
        time = 8;
        break;
    case 5://125/64
        time = 8;
        break;
    case 6://125/32
        time = 8;
        break;
    case 7://125/16
        time = 4;
        break;
    case 8://125/8
        time = 2;
        break;
    case 9://125/4
        time = 1;
        break;
    case 10://125/4
        time = 1;
        break;
    default:
        break;
    }
    m_htmp1 = safe_new(m_htmp1, 8*time);
    upsample(time);

    int htmp1_len = 0;
    htmp1_len = 8*time;
    for (int ck0 = 0; ck0 < 8 * time; ck0++)
    {
        m_h[m_fft_len / 2 - htmp1_len / 2 + ck0].I = m_htmp1[ck0];
        //LOG2DEV <<m_htmp1[ck0]<<endl;
    }
}

template <class T>
void Cls_ud<T>::ud_out_module()
{
    int ii, jj;
    for(ii = 0; ii < m_GWRxAllUser; ii++){
        t_freq_offset_o = (m_AFreqBinDMax[ii] - m_fft_len / 2) * m_GWRxSampleRate / m_fft_len;//tx fxp s25.18
        m_freq_offset_o[ii] = t_freq_offset_o;//(m_AFreqBinDMax[ii] - m_fft_len / 2) * m_GWRxSampleRate / m_fft_len;//tx fxp s25.18
        m_pilot_power_o[ii] = m_APreambleFftESum2[m_AFreqBinDMax[ii]];
        #ifdef DUMP_RTL_UD
            dump(m_fp_freq_offset,&t_freq_offset_o,1,t_freq_offset_o);
            dump(m_fp_pilot_power,&m_pilot_power_o[ii],1,m_pre_pow_comb_fxp );
        #endif
        m_AH_sum = 0;
        for(jj = 0; jj < m_ant_num; jj++){
            m_ah_o[jj][ii].I = m_APreambleFft2[jj][m_AFreqBinDMax[ii]].I;
            m_ah_o[jj][ii].Q = m_APreambleFft2[jj][m_AFreqBinDMax[ii]].Q;
            m_AHNorm_o[jj][ii].I = m_APreambleFft2[jj][m_AFreqBinDMax[ii]].I;
            m_AHNorm_o[jj][ii].Q = m_APreambleFft2[jj][m_AFreqBinDMax[ii]].Q;
            #ifdef DUMP_RTL_UD
            if(jj == m_ant_num -1) {
                dump(m_fp_ah_i,&m_ah_o[jj][ii].I,1,m_out_fxp.I);
                dump(m_fp_ah_q,&m_ah_o[jj][ii].Q,1,m_out_fxp.I);
            } else{
                dump(m_fp_ah_i,&m_ah_o[jj][ii].I,1,m_out_fxp.I,0);
                dump(m_fp_ah_q,&m_ah_o[jj][ii].Q,1,m_out_fxp.I,0);
            }
            #endif
            m_AH_sum += (m_AHNorm_o[jj][ii].power());
        }

        //m_AH_sum_abs = sqrt(m_AH_sum);
        m_AH_sum_abs = m_sqrt.sqrt(m_AH_sum);
        m_AH_sum_abs_frac = 1.0 / m_AH_sum_abs;
        for(jj = 0; jj < m_ant_num; jj++){
            m_h_norm_fxp = m_AHNorm_o[jj][ii] * m_AH_sum_abs_frac;
            m_AHNorm_o[jj][ii] = m_h_norm_fxp;
        }
    }

    for(ii = 0; ii < m_GWRxAllUser; ii++){
        m_UMode_o[ii] = m_TerminalTxMode[0];
        #ifdef DUMP_RTL_UD
            m_fp_umode<<m_UMode_o[ii]<<endl;
        #endif
    }

}

template <class T>
double Cls_ud<T>::sqrt_bisection(double in, double epsilon) {
    int times = 0;
    double a;
    int times_int;
    if(in == 0)
        return 0;
    a = in;
    if(in >= 1024 && in <= 2048) {
        times = 0;
    } else if(a < 1024) {
        while(a < 1024) {
            times = times + 1;
            a = in * DOUBLE_POW2[64 + times];
        }
    } else if(a > 2048) {
        while(a > 2048) {
            times = times - 1;
            a = in * DOUBLE_POW2[64 + times];
        }
    }
    a = round(a);
    double root;
    if (a < 0) {
        printf("Input must be non-negative\n");
        exit(1);
    } else if (a == 0 || a == 1) {
        root = a;
        return root;
    } else {
        double low = 0;
        double high = fmax(1, a);
        double mid = (low + high) / 2;

        for(int ck0 = 0; ck0 < 22; ck0++) {
            if(abs(mid * mid - a) > epsilon) {
                if (mid * mid > a) {
                    high = mid;
                } else {
                    low = mid;
                }
                mid = (low + high) / 2;
            } else {
                break;
            }
        }
        root = mid;
    }
    if(times > 0) {
        times_int = (int)(times / 2);
        root = root * DOUBLE_POW2[64 - times_int];
        if((times - 2 * times_int) == 1)
            root = root * 46341 * DOUBLE_POW2[64 - 16];// sqrt(2);
    } else if(times < 0) {
        times = times * -1;
        times_int = (int)(times / 2);
        root = root * DOUBLE_POW2[64 + times_int];
        if((times - 2 * times_int) == 1)
            root = root * 92682 * DOUBLE_POW2[64 - 16]; //sqrt(2);
    }
    return root;
}

template <class T>
void Cls_ud<T>::quicksort(int a[8], int first,int end)
{
    if(first > end)
    {
        return;
    }
    int i = first,j = end;
    int flag = a[i],exchange = 0;

    while(i != j)
    {
        while(i < j && a[j] > flag)
        {
            j--;
        }

        while(i < j && a[i] <= flag)
        {
            i++;
        }
        if(j > i)
        {
            exchange = a[i];
            a[i] = a[j];
            a[j] = exchange;
        }
    }
    a[first] = a[i];
    a[i] = flag;
    //if(isnan(a[i]))
    //    return;
    quicksort(a, first,i - 1);
    quicksort(a, i + 1,end);
}
#endif
