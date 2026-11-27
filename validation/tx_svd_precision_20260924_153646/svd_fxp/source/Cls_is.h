/****************************************************************************
**
**  Copyright  (c)  2023-2028,  All Rights Reserved.
**
**  File          : Cls_is.h
**  Created By    :
**  Created On    :
**
**  History     :
**  Programmer    	Date        Rev       	Description
**  --------------- ---------- 	-------- ------------------------------
**
**  Notice: Interference Suppression
**
****************************************************************************/
#ifndef __Cls_is
#define __Cls_is
//#define DUMP_MATLAB_IS
//#define DUMP_RTL_IS
#include "common_type.h"
#include "Cls_fft.h"
#include "Cls_svd.h"

typedef struct IS_CFG
{
    double          freqOffsetCh;
    int             Antenna;
    int             FFTLen;
    int             mode;
    int             filterLen;
    int             ifftLen;
    int             GWRxSampleRate;
    int             GWRXBFMode;
    int             HCE;
}IS_CFG;

template <class T>
class Cls_is
{
	public:
		Cls_is() {
            m_in = NULL;
            m_htmp1 = NULL;
            m_h = NULL;
            m_HC_tmp = NULL;
            m_HC_tmp1 = NULL;
            m_HC = NULL;
            m_signal1 = NULL;
            m_sigSeg = NULL;
            m_AH = NULL;
            m_HNormal = NULL;
            m_inphaseComponent = NULL;
            m_orthogonalComponent = NULL;
            m_R = NULL;
            m_V = NULL;
            m_G = NULL;
            m_SDDecoder = NULL;
            m_S = NULL;
            m_tempS = NULL;
            m_SDout = NULL;
            m_SigRxOut = NULL;
            m_SigRxOut_tmp = NULL;
            m_ifftin = NULL;
		}
		~Cls_is() {
            free(m_in);
            free(m_htmp1);
            free(m_h);
            free(m_HC_tmp);
            free(m_HC_tmp1);
            free(m_HC);
            free(m_signal1);
            free(m_sigSeg);
            free(m_AH);
            free(m_HNormal);
            free(m_inphaseComponent);
            free(m_orthogonalComponent);
            free(m_R);
            free(m_V);
            free(m_G);
            free(m_SDDecoder);
            free(m_S);
            free(m_tempS);
            free(m_SDout);
            free(m_SigRxOut);
            free(m_SigRxOut_tmp);
            free(m_ifftin);
#ifdef DUMP_MATLAB_IS
        out.close();
        fft_out.close();
        sigseg.close();
        G_out.close();
        V_out.close();
        R_out.close();
#endif
#ifdef DUMP_RTL_IS
        file_rtl_in.close();
        file_rtl_HC.close();
        file_rtl_sigseg.close();
        file_rtl_ah.close();
        file_rtl_h_norm.close();
        file_rtl_in_phase.close();
        file_rtl_r.close();
        file_rtl_orth_comp.close();
        file_rtl_s.close();
        file_rtl_sumptmp.close();
        file_rtl_a.close();
        file_rtl_sdout.close();
#endif
		}
		void			init(char* module_name);
		void			go();
		void 			reset();
		void			configure(IS_CFG cfg);
        int             m_in1_finish;
        int             m_in2_finish;
        int             m_GWBF_finish;
        double          m_in1_0_I;
        double          m_in1_0_Q;
        double          m_in1_1_I;
        double          m_in1_1_Q;
        double          m_in1_2_I;
        double          m_in1_2_Q;
        double          m_in1_3_I;
        double          m_in1_3_Q;
        double          m_in1_4_I;
        double          m_in1_4_Q;
        double          m_in1_5_I;
        double          m_in1_5_Q;
        double          m_in1_6_I;
        double          m_in1_6_Q;
        double          m_in1_7_I;
        double          m_in1_7_Q;
        double          m_in2_I;
        double          m_in2_Q;
        double          m_freqOffsetCh;
        Complex<double> **m_SigRxOut;
        Complex<T>      * m_AH;
        Complex<double> **m_in;
	private:
        IS_CFG              m_cfg;
        Cls_fft<double>     m_fft;
        Cls_fft<double>     m_ifft;
        FFT_CFG             m_fft_cfg;
        FFT_CFG             m_ifft_cfg;
        Cls_svd<double>          m_svd;
        SVD_CFG             m_svd_cfg;
        int                 m_Antenna;
        int                 m_FFTLen;
        int                 m_mode;
        int                 m_filterLen;
        int                 m_ifftLen;
        int                 m_GWRxSampleRate;
        int                 m_GWRXBFMode;
        int                 m_HCE;
        int                 m_halfBL;
        double              *m_htmp1;
        Complex<double>     *m_h;
        Complex<double>     *m_HC_tmp;
        Complex<double>     *m_HC_tmp1;
        Complex<double>     *m_HC;
        Complex<double>     **m_signal1;
        Complex<double>     **m_sigSeg;
        // Complex<T>      * m_AH;
        Complex<double>     *m_HNormal;
        Complex<double>     *m_inphaseComponent;
        Complex<double>     ** m_orthogonalComponent;
        Complex<double>     ** m_R;
        Complex<double>     ** m_V;
        double              ** m_G;
        Complex<T>          ** m_SDDecoder;
        Complex<double>     ** m_S;
        double              * m_tempS;
        Complex<T>          ** m_SDout;
        Complex<T>          m_A;
        Complex<T>          m_in_fxp;
        Complex<T>          m_HC_fxp;
        Complex<T>          m_sig_fxp;
        Complex<T>          m_orth_fxp;
        Complex<T>          m_AH_fxp;
        Complex<T>          m_h_norm_fxp;
        Complex<T>          m_inphaseComponent_fxp;
        Complex<double>     m_tmp1;
        Complex<double>     m_tmp2;
        Complex<T>          m_R_fxp;
        T                   m_AH_sum;
        T                   m_AH_sum_abs;
        T                   m_AH_sum_abs_frac;
        Complex<T>          m_S_fxp;
        Complex<double>     m_Sumtmp;
        Complex<T>          m_Sumtmp_fxp;
        double              m_max;
        int                 m_shift;
        // Complex<double> **m_SigRxOut;
        Complex<double> *m_SigRxOut_tmp;
        Complex<double> *m_ifftin;
        int             m_GWRxBFModule_en;
        int             m_in_cnt;
        void            get_output_msg(Complex<double> *out);
        void            generate_para();
        void            upsample(int time);
        void            generate_HCfilter();
        void            generate_SigSeg();
        void            svd_module();
        void            ifft();
        void            svd(Complex<double> **R, int R_len,double **G, Complex<double> **V);
        int             shift_cal();
        void            ifft_shift(Complex<double> in, int status);
#ifdef DUMP_MATLAB_IS
        ofstream        R_out;
        ofstream        V_out;
        ofstream        G_out;
        ofstream        out;
        ofstream        fft_out;
        ofstream        sigseg;
#endif
#ifdef DUMP_RTL_IS
        ofstream        file_rtl_in;
        ofstream        file_rtl_HC;
        ofstream        file_rtl_sigseg;
        ofstream        file_rtl_ah;
        ofstream        file_rtl_h_norm;
        ofstream        file_rtl_in_phase;
        ofstream        file_rtl_r;
        ofstream        file_rtl_orth_comp;
        ofstream        file_rtl_s;
        ofstream        file_rtl_sumptmp;
        ofstream        file_rtl_a;
        ofstream        file_rtl_sdout;
#endif
};

template <class T>
void Cls_is<T>::configure(IS_CFG cfg)
{
	m_cfg = cfg;
	m_freqOffsetCh = m_cfg.freqOffsetCh;
    m_Antenna = m_cfg.Antenna;
    m_FFTLen = m_cfg.FFTLen;
    m_mode = m_cfg.mode;
    m_filterLen = m_cfg.filterLen;
    m_ifftLen = m_cfg.ifftLen;
    m_GWRxSampleRate = m_cfg.GWRxSampleRate;
    m_GWRXBFMode = m_cfg.GWRXBFMode;
    m_HCE = m_cfg.HCE;

    m_halfBL = m_filterLen / 2;

    m_h = safe_new(m_h, m_FFTLen);
    zeros(m_h, m_FFTLen);
    m_HC_tmp = safe_new(m_HC_tmp, m_FFTLen);

    generate_para();

    m_fft_cfg.fft_log2_len = log2(m_FFTLen);
    m_fft.configure(m_fft_cfg);

    m_ifft_cfg.fft_log2_len = log2(m_ifftLen);
    m_ifft.configure(m_ifft_cfg);

    m_svd_cfg.ant_num = m_Antenna;
    m_svd.configure(m_svd_cfg);

}

template <class T>
void Cls_is<T>::init(char* module_name)
{
    m_HC_tmp1 = safe_new(m_HC_tmp1, m_FFTLen);

    m_fft.init(module_name);
    m_ifft.init(module_name);
    m_fft.fft_n(m_h, m_HC_tmp, 1, 0, 1);
    m_svd.init(module_name);

    //double HC_sqrt = sqrt(m_FFTLen);
    for (int ck0 = 0; ck0 < m_FFTLen; ck0++)
    {
        m_HC_tmp1[ck0] = m_HC_tmp[ck0].conjug() ;
        //LOG2DEV << m_HC_tmp1[ck0].I<<endl;
    }

    m_halfBL = m_filterLen / 2;
    m_in = safe_new(m_in, m_Antenna, m_FFTLen);
    m_HC = safe_new(m_HC, m_filterLen);
    m_signal1 = safe_new(m_signal1, m_FFTLen, m_Antenna);
    m_sigSeg = safe_new(m_sigSeg , m_Antenna, m_filterLen);

    m_AH                  = safe_new(m_AH, m_Antenna);
    m_HNormal             = safe_new(m_HNormal, m_Antenna);
    m_inphaseComponent    = safe_new(m_inphaseComponent, m_filterLen);
    m_orthogonalComponent = safe_new(m_orthogonalComponent, m_Antenna, m_filterLen);
    m_R                   = safe_new(m_R, m_Antenna, m_Antenna);
    m_V                   = safe_new(m_V, m_Antenna, m_Antenna);
    m_G                   = safe_new(m_G, m_Antenna, m_Antenna);
    m_SDDecoder           = safe_new(m_SDDecoder, m_Antenna, m_filterLen);
    m_S                   = safe_new(m_S, m_Antenna, m_filterLen);
    m_tempS               = safe_new(m_tempS, m_Antenna);
    m_SDout               = safe_new(m_SDout, m_Antenna, m_ifftLen);
    m_SigRxOut_tmp        = safe_new(m_SigRxOut_tmp, m_ifftLen);
    m_SigRxOut            = safe_new(m_SigRxOut, m_Antenna, m_ifftLen);
    m_ifftin              = safe_new(m_ifftin, m_ifftLen);

    fix_set(m_in_fxp , 17, 14, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_in_fxp,"m_in_fxp", 1, 100);
    fix_set(m_HC_fxp , 17, 6, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_HC_fxp,"m_HC_fxp", 1, 100);
    fix_set(m_sig_fxp , 24, 14, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sig_fxp,"m_sig_fxp", 1, 100);
    fix_set(m_orth_fxp , 24, 14, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_orth_fxp,"m_orth_fxp", 1, 100);
    fix_set(m_AH_fxp, 21, 10, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_AH_fxp,"m_AH_fxp", 1, 1);
    fix_set(m_AH_sum, 32, 18, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_AH_sum,"m_AH_sum", 1, 1);
    fix_set(m_AH_sum_abs, 20, 9, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_AH_sum,"m_AH_sum", 1, 1);
    fix_set(m_AH_sum_abs_frac, 17, 0, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_AH_sum_abs_frac,"m_AH_sum_abs_frac", 1, 1);
    fix_set(m_h_norm_fxp, 16, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_h_norm_fxp,"m_h_norm_fxp", 1, 1);
    fix_set(m_inphaseComponent_fxp, 16, 8, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_inphaseComponent_fxp,"m_inphaseComponent_fxp", 1, 1);
    fix_set(m_R_fxp, 28, 15, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_R_fxp,"m_R_fxp", 1, 1);
    fix_set(m_S_fxp, 32, 5, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_S_fxp,"m_S_fxp", 1, 1000);
    fix_set(m_Sumtmp, 45, 32, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_Sumtmp,"m_Sumtmp", 1, 1000);
    fix_set(m_A, 45, 32, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_A, "m_A", 1, 1000);
    fix_set(m_Sumtmp_fxp, 20, 16, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_Sumtmp_fxp, "m_Sumtmp_fxp", 1, 1);
    /*fix_set(m_HC_tmp1 , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_in , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_HC , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_signal1 , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_sigSeg , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_AH , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_HNormal , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_inphaseComponent , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_R , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_V , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_G , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_SDDecoder , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_S , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_tempS , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_SDout , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_SigRxOut_tmp , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_SigRxOut , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_ifftin , 64, 32, FXP_TC, FXP_TRN, FXP_SAT_SYM);
*/

#ifdef DUMP_MATLAB_IS
    char filename_mat[300] = {};
    strcpy(filename_mat, module_name); strcat(filename_mat, "out.txt");         out.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fft_out.txt");     fft_out.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "sigseg.txt");      sigseg.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "R_out.txt");       R_out.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "V_out.txt");       V_out.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "G_out.txt");       G_out.open(filename_mat);
#endif
#ifdef DUMP_RTL_IS
    char filename_rtl[300] = {};
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "in.dat");          file_rtl_in.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "hc.dat");          file_rtl_HC.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "sigseg.dat");      file_rtl_sigseg.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "ah.dat");          file_rtl_ah.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "h_norm.dat");      file_rtl_h_norm.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "in_phase.dat");    file_rtl_in_phase.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "r.dat");           file_rtl_r.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "orth_comp.dat");   file_rtl_orth_comp.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "s.dat");           file_rtl_s.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "sumtmp.dat");      file_rtl_sumptmp.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "a.dat");           file_rtl_a.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "sdout.dat");       file_rtl_sdout.open(filename_rtl);
#endif
	reset();
}

template <class T>
void Cls_is<T>::reset()
{
    zeros(m_in, m_Antenna, m_filterLen);
    zeros(m_sigSeg, m_Antenna, m_filterLen);
    zeros(m_AH, m_Antenna);
    zeros(m_HNormal, m_Antenna);
    zeros(m_inphaseComponent, m_filterLen);
    zeros(m_orthogonalComponent, m_Antenna, m_filterLen);
    zeros(m_R, m_Antenna, m_Antenna);
    zeros(m_V, m_Antenna, m_Antenna);
    zeros(m_G, m_Antenna, m_Antenna);
    zeros(m_SDDecoder, m_Antenna, m_filterLen);
    zeros(m_S, m_Antenna, m_filterLen);
    zeros(m_tempS, m_Antenna);
    zeros(m_SDout, m_Antenna, m_ifftLen);

    m_GWRxBFModule_en = 0;
    m_in_cnt = 0;
    m_in2_finish = 0;
    m_in1_finish = 0;
    m_GWBF_finish = 0;
    generate_HCfilter();
}

template <class T>
void Cls_is<T>::go()
{
    //generate sigseg
    generate_SigSeg();
    //svd
    svd_module();
    // ifft
    ifft();
}

template <class T>
void Cls_is<T>::get_output_msg(Complex<double> *out)
{

}

template <class T>
void Cls_is<T>::generate_para()
{
    int time = 1;
    switch (m_mode)
    {
    case 1://125/128/8
        time = 1;
        break;
    case 2://125/128/4
        time = 1;
        break;
    case 3://125/128/2
        time = 16.0*m_GWRxSampleRate/125000;
        break;
    case 4://125/128
        time = 8.0*m_GWRxSampleRate/125000;
        break;
    case 5://125/64
        time = 4.0*m_GWRxSampleRate/125000;
        break;
    case 6://125/32
        time = 2.0*m_GWRxSampleRate/125000;
        break;
    case 7://125/16
        time = 1.0*m_GWRxSampleRate/125000;
        break;
    case 8://125/8
        time = 0.5*m_GWRxSampleRate/125000;
        break;
    case 9://125/4
        time = 0.25*m_GWRxSampleRate/125000;
        break;
    case 10://125/4
        time = 0.25*m_GWRxSampleRate/125000;
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
        m_h[m_FFTLen / 2 - htmp1_len / 2 + ck0].I = m_htmp1[ck0];
        //LOG2DEV <<m_htmp1[ck0]<<endl;
    }

}

template <class T>
void Cls_is<T>::upsample(int time)
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
void Cls_is<T>::generate_HCfilter()
{
    for (int ck0 = 0; ck0 < m_halfBL; ck0++)
    {
        m_HC_fxp = m_HC_tmp1[m_FFTLen - m_halfBL + ck0] * m_HCE;
        m_HC[ck0] = m_HC_fxp;
        //LOG2DEV <<m_HC_fxp.I<<endl;
        m_HC_fxp = m_HC_tmp1[ck0] * m_HCE;
        m_HC[m_halfBL + ck0] = m_HC_fxp;
    }
#ifdef DUMP_RTL_IS
    dump(file_rtl_HC, &m_HC[0], m_halfBL * 2, m_HC_fxp.I);
#endif
}

template <class T>
void Cls_is<T>::generate_SigSeg()
{
    int freqOffsetChGrid;
    freqOffsetChGrid = m_freqOffsetCh;//round((m_freqOffsetCh + m_GWRxSampleRate) / m_GWRxSampleRate * m_FFTLen);

    int index = 0;
    for (int ck0 = 0; ck0 < m_Antenna; ck0++)
    {
        for (int ck1 = 0; ck1 < m_FFTLen; ck1++)
        {
            index = ck1 - freqOffsetChGrid;
            while (1)
            {
                if ( index >= 0 && index < m_FFTLen)
                    break;
                if(index > 0)
                    index = index - m_FFTLen;
                if(index < 0)
                    index = index + m_FFTLen;
            }
            m_in_fxp = m_in[ck0][ck1];
#ifdef DUMP_RTL_IS
            dump(file_rtl_in, &m_in_fxp, 1, m_in_fxp.I);
#endif
            m_signal1[index][ck0] = m_in_fxp;
        }
    }

    for (int ck0 = 0; ck0 < m_Antenna; ck0++)
    {
        for (int ck1 = 0; ck1 < m_halfBL; ck1++)
        {
            m_sig_fxp = m_signal1[m_FFTLen - m_halfBL + ck1][ck0] * m_HC[ck1];
            m_sigSeg[ck0][ck1] = m_sig_fxp;
            m_sig_fxp = m_signal1[ck1][ck0] * m_HC[ck1 + m_halfBL];
            m_sigSeg[ck0][ck1 + m_halfBL] = m_sig_fxp;
        }
#ifdef DUMP_RTL_IS
        for (int ck1 = 0; ck1 < 2 * m_halfBL; ck1++)
            dump(file_rtl_sigseg, &m_sigSeg[ck0][ck1], 1, m_sig_fxp.I);
#endif
    }
#ifdef DUMP_MATLAB_IS
    for (int ck0 = 0; ck0 < m_Antenna; ck0++)
    {
        for (int ck1 = 0; ck1 < 2*m_halfBL; ck1++)
        {
           sigseg << fixed<<setprecision(30) << m_sigSeg[ck0][ck1].I << endl;
           sigseg << fixed<<setprecision(30) << m_sigSeg[ck0][ck1].Q << endl;
        }
    }
#endif // DUMP_MATLAB_IS
}

template <class T>
void Cls_GWRxBFModule<T>::svd_module()
{
    int i, j, k;

    if(m_GWRXBFMode == 0){
        m_AH_sum = 0;
        for(i = 0; i < m_Antenna; i++){
            m_AH_fxp = m_AH[i];
            m_AH[i] = m_AH_fxp;
#ifdef DUMP_RTL_IS
            dump(file_rtl_ah, &m_AH_fxp, 1, m_AH_fxp.I);
#endif
            m_AH_sum += (m_AH[i].I * m_AH[i].I + m_AH[i].Q * m_AH[i].Q);
        }
        m_AH_sum_abs = sqrt(m_AH_sum);
        m_AH_sum_abs_frac = 1.0 / m_AH_sum_abs;
        for(i = 0; i < m_Antenna; i++){
            m_h_norm_fxp = m_AH[i] * m_AH_sum_abs_frac;
            m_HNormal[i] = m_h_norm_fxp;
#ifdef DUMP_RTL_IS
            dump(file_rtl_h_norm, &m_h_norm_fxp, 1, m_h_norm_fxp.I);
#endif
        }

        Complex<double> m_inphaseComponent_tmp;
        for(i = 0; i < m_filterLen; i++){
            m_inphaseComponent_tmp = 0;
            for(j = 0; j < m_Antenna; j++)
                m_inphaseComponent_tmp = m_HNormal[j].conjug() * m_sigSeg[j][i] + m_inphaseComponent_tmp;
            m_inphaseComponent_fxp = m_inphaseComponent_tmp;
            m_inphaseComponent[i] = m_inphaseComponent_fxp;
#ifdef DUMP_RTL_IS
            dump(file_rtl_in_phase, &m_inphaseComponent_fxp, 1, m_inphaseComponent_fxp.I);
#endif
//    #ifdef DUMP_MATLAB_IS
//            is_mat_out1 <<fixed<<setprecision(30)<< m_inphaseComponent[i].I <<endl;
//            is_mat_out1 <<fixed<<setprecision(30)<< m_inphaseComponent[i].Q <<endl;
//    #endif
        }

        for(i = 0; i < m_filterLen; i++)
            for(j = 0; j < m_Antenna; j++) {
                m_orth_fxp = m_sigSeg[j][i]- m_HNormal[j] * m_inphaseComponent[i] ;
                m_orthogonalComponent[j][i] = m_orth_fxp;
#ifdef DUMP_RTL_IS
                dump(file_rtl_orth_comp, &m_orth_fxp, 1, m_orth_fxp.I);
#endif
            }

    #ifdef DUMP_MATLAB_IS
//        for(i = 0; i < m_Antenna; i++){
//            for(j = 0; j < m_filterLen; j++){
//                is_mat_out2 <<fixed<<setprecision(30)<< m_orthogonalComponent[i][j].I <<endl;
//                is_mat_out2 <<fixed<<setprecision(30)<< m_orthogonalComponent[i][j].Q <<endl;
//            }
//        }
    #endif

        for(i = 0; i < m_Antenna; i++){
            for(j = 0; j < m_Antenna; j++){
                m_R[i][j].I = 0;
                m_R[i][j].Q = 0;
                for(k = 0; k < m_filterLen; k++){
                    m_tmp1 = m_orthogonalComponent[i][k];

                    m_tmp2 = m_orthogonalComponent[j][k].conjug();
                    m_R_fxp = m_tmp1 * m_tmp2 + m_R[i][j];
                    //LOG2DEV << m_R_fxp.I << "\t" << m_R_fxp.Q << endl;
                    m_R[i][j] = m_R_fxp;
#ifdef DUMP_RTL_IS
                    dump(file_rtl_r, &m_R_fxp, 1, m_R_fxp.I);
#endif
                }
            }
        }

    #ifdef DUMP_MATLAB_IS
//        for(i = 0; i < m_Antenna; i++){
//            for(j = 0; j < m_Antenna; j++){
//                is_mat_out3 <<fixed<<setprecision(30)<< m_R[i][j].I <<endl;
//                is_mat_out3 <<fixed<<setprecision(30)<< m_R[i][j].Q <<endl;
//            }
//        }
    #endif
        //SVD
        for (int ck0 = 0; ck0 < 8; ck0++)
        {
            for (int ck1 = 0; ck1 < 8; ck1++)
            {
                m_R[ck0][ck1] = m_R[ck0][ck1]  ;// * 1e3;// 1024;
                //LOG2DEV << m_R[ck0][ck1].I << endl;
            }
        }

        m_svd.go(m_R);
        for(int ck0 = 0 ; ck0 < 8;ck0++)
        {
            for(int ck1 = 0 ; ck1 < 8; ck1++)
            {
                m_G[ck0][ck1] = m_svd.m_S[ck0][ck1];
                m_V[ck0][ck1] = m_svd.m_V[ck0][ck1];
            }
        }
#ifdef DUMP_MATLAB_IS
        for(i = 0; i < 8; i++){
            for(j = 0; j < 8; j++){
                R_out <<fixed<<setprecision(30)<< m_R[i][j].I <<endl;
                R_out <<fixed<<setprecision(30)<< m_R[i][j].Q <<endl;

                G_out <<fixed<<setprecision(30)<< m_G[i][j] <<endl;

                V_out <<fixed<<setprecision(30)<< m_V[i][j].I <<endl;
                V_out <<fixed<<setprecision(30)<< m_V[i][j].Q <<endl;

            }
        }
#endif
        // svd(m_R, m_Antenna, m_G, m_V);//////////////******************************************//////////////////////////////////////
        //Add SVD C code

        for(i = 0; i < m_filterLen; i++) {
            m_SDDecoder[0][i] = m_inphaseComponent[i];
        }

        for(i = 0; i < m_Antenna; i++){
            for(j = 0; j < m_filterLen; j++){
                m_S_fxp = 0.0;
                for(k = 0; k < m_Antenna; k++){
                    m_tmp1 = m_V[k][i].conjug();

                    m_tmp2 = m_orthogonalComponent[k][j];

                    m_S_fxp = m_tmp1 * m_tmp2 + m_S_fxp.todouble();
                }
                m_S[i][j] = m_S_fxp;
#ifdef DUMP_RTL_IS
            dump(file_rtl_s, &m_S_fxp, 1, m_S_fxp.I);
#endif
            }
        }

        //double m_G_tmp[8] = {3.011124856472660e-08, 2.793562954983170e-08, 2.647267447428283e-08, 2.375626381807472e-08,
                             //2.230135376113445e-08, 2.031620193210982e-08, 1.910867290431565e-08, 1.580334720167331e-24};
        //double m_G_tmp[8] = {9.05707144489200e-07, 8.84165544058079e-07, 8.56079976494773e-07, 8.20310702333305e-07,
        //                     7.83017346211945e-07, 7.49815806368854e-07, 7.38631300152362e-07, 8.51577473205987e-23};
        for(i = 0; i < m_Antenna; i++){
            m_tempS[i] = m_G[i][i];//m_G_tmp[i]
            //m_tempS[i] = m_G_tmp[i] * 1e5; //ZH
        }


        for(i = 0; i < m_Antenna - 1; i++){
            if(m_tempS[i] == 0) {
                m_A = 0.0;
            }
            else {
                m_Sumtmp = 0.0;
                for(j = 0; j < m_filterLen; j++){
                    m_tmp1 = m_inphaseComponent[j];

                    m_tmp2 = m_S[i][j].conjug();

                    m_Sumtmp = m_Sumtmp.todouble() + m_tmp1 * m_tmp2;
                }
                m_Sumtmp_fxp = m_Sumtmp;
                m_A = m_Sumtmp / m_tempS[i];
#ifdef DUMP_RTL_IS
                dump(file_rtl_sumptmp, &m_Sumtmp_fxp, 1, m_Sumtmp_fxp.I);
                dump(file_rtl_a, &m_A, 1, m_A.I);
#endif


                for(j = 0; j < m_filterLen; j++){
                    m_inphaseComponent_fxp = m_inphaseComponent[j].todouble() - (m_A.todouble() * m_S[i][j].todouble());
                    m_inphaseComponent[j] = m_inphaseComponent_fxp;
                    m_SDDecoder[i + 1][j] = m_inphaseComponent[j];
                }
            }
        }

        for(i = 0; i < m_Antenna; i++){
            for(j = 0; j < m_halfBL; j++){
                m_SDout[i][j] =  m_SDDecoder[i][j + m_halfBL];
                m_SDout[i][j + m_ifftLen - m_halfBL] =  m_SDDecoder[i][j];
            }
        }
    }
    else if(m_GWRXBFMode == 1){
        for(i = 0; i < m_Antenna; i++){
            for(j = 0; j < m_halfBL; j++){
                m_SDout[i][j] =  m_sigSeg[i][j + m_halfBL];
                m_SDout[i][j + m_ifftLen - m_halfBL] =  m_sigSeg[i][j];
            }
        }
    }
#ifdef DUMP_RTL_IS
    for(int ck0 = 0; ck0 < m_Antenna; ck0++) {
        for(int ck1 = 0; ck1 < m_halfBL; ck1++) {
            dump(file_rtl_sdout, &m_SDout[ck0][ck1], 1, m_inphaseComponent_fxp.I);
        }
    }
#endif
#ifdef DUMP_MATLAB_IS
        for(i = 0; i < m_Antenna; i++){
            for(j = 0; j < m_ifftLen; j++){
                fft_out <<fixed<<setprecision(30)<< m_SDout[i][j].I <<endl;
                fft_out <<fixed<<setprecision(30)<< m_SDout[i][j].Q <<endl;
            }
        }
#endif
}

template <class T>
void Cls_is<T>::ifft()
{
    for (int ck0 = 0; ck0 < m_Antenna; ck0++)
    {
        ifft_shift(0.0, 0);
        for (int ck1 = 0; ck1 < m_ifftLen; ck1++)
        {
            m_ifftin[ck1] = m_SDout[ck0][ck1];//m_SDout[ck0][ck1].conjug();
            ifft_shift(m_SDout[ck0][ck1].todouble(), 1);
        }
        m_shift = shift_cal();
        for  (int ck1 = 0; ck1 < m_ifftLen; ck1++)
        {
            m_ifftin[ck1] = m_ifftin[ck1] * DOUBLE_POW2[64 - m_shift];
        }

        m_ifft.ifft_n(m_ifftin, m_SigRxOut_tmp, 1, 0, 1);
        for  (int ck1 = 0; ck1 < m_ifftLen; ck1++)
        {
            m_SigRxOut_tmp[ck1] = m_SigRxOut_tmp[ck1] * DOUBLE_POW2[64 + m_shift];
        }

        for (int ck1 = 0; ck1 < m_ifftLen / 2; ck1++)
        {
            m_SigRxOut[ck0][ck1] = m_SigRxOut_tmp[m_ifftLen / 2 + ck1] * m_ifftLen;//m_SigRxOut_tmp[m_ifftLen / 2 + ck1].conjug();
            m_SigRxOut[ck0][m_ifftLen / 2 + ck1] = m_SigRxOut_tmp[ck1] * m_ifftLen;//m_SigRxOut_tmp[ck1].conjug();
        }
    }
#ifdef DUMP_MATLAB_IS
    for (int ck0 = 0; ck0 < m_Antenna; ck0++)
    {
        for (int ck1 = 0; ck1 < m_ifftLen; ck1++)
        {
           out << fixed<<setprecision(30) << m_SigRxOut[ck0][ck1].I << endl;
           out << fixed<<setprecision(30) << m_SigRxOut[ck0][ck1].Q << endl;
        }
    }
#endif
}

template <class T>
void Cls_is<T>::ifft_shift(Complex<double> in, int status)
{
    double tmp;
    if(status == 0)
        m_max = 0;
    else {
        tmp = fabs(in.I);
        if(tmp > m_max)
            m_max = tmp;
        tmp = fabs(in.Q);
        if(tmp > m_max)
            m_max = tmp;
    }
}

template <class T>
int Cls_is<T>::shift_cal()
{
    int shift = 0;
    shift = ceil(log2(m_max));
    return shift;
}
#endif
