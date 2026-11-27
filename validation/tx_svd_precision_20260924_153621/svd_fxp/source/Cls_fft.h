/****************************************************************************
**
**  Copyright  (c)  2023-2028,  All Rights Reserved.
**
**  File          : Cls_fft.h
**  Created By    :
**  Created On    :
**
**  History     :
**  Programmer    	Date        Rev       	Description
**  --------------- ---------- 	-------- ------------------------------
**
**  Notice: FFT
**
****************************************************************************/
#ifndef  __Cls_fft
#define  __Cls_fft
#include <iostream>
#include <fstream>

#include "Lib_complex.h"
#include "common_type.h"
#include "Lib_io.h"
#define FFT_2_1 1
#define FFT_4_1 2
#define FFT_8_1 3
#define FFT_4_2 5
#define FFT_8_2 6
#define FFT_8_3 9

typedef struct FFT_CFG
{
    //Read Write
    int fft_log2_len;
}FFT_CFG;

//#define DUMP_RTL_FFT
template <class T>
class Cls_fft
{
public:
    Cls_fft() {
        m_addr_reversed     = NULL;
        m_MO                = NULL;
        m_AO                = NULL;
        m_in                = NULL;
        m_temp              = NULL;
        m_temp1             = NULL;
        m_FXP_A0            = NULL;
        m_FXP_M0            = NULL;
        m_fft_in            = NULL;
    }
    ~Cls_fft (){
        if (m_addr_reversed!=NULL)      delete [] m_addr_reversed;
        if (m_in!=NULL)                 delete [] m_in;
        if (m_temp!=NULL)               delete [] m_temp;
        if (m_temp1!=NULL)              delete [] m_temp1;
        free(m_MO);
        free(m_AO);
        free(m_fft_in);
        free(m_FXP_A0);
        free(m_FXP_M0);
#ifdef DUMP_RTL_FFT
        if (fp_len.is_open()) fp_len.close();
        if (fp_in_i.is_open()) fp_in_i.close(); if (fp_in_q.is_open()) fp_in_q.close();
        if (fp_out_i.is_open()) fp_out_i.close(); if (fp_out_q.is_open()) fp_out_q.close();
        if (fp_wn_i.is_open()) fp_wn_i.close(); if (fp_wn_q.is_open()) fp_wn_q.close();
        if (fp_wn_lut_i.is_open()) fp_wn_lut_i.close(); if (fp_wn_lut_q.is_open()) fp_wn_lut_q.close();
        if (fp_s0m_outi.is_open()) fp_s0m_outi.close(); if (fp_s0a_outi.is_open()) fp_s0a_outi.close();
        if (fp_s1m_outi.is_open()) fp_s1m_outi.close(); if (fp_s1a_outi.is_open()) fp_s1a_outi.close();
        if (fp_s2m_outi.is_open()) fp_s2m_outi.close(); if (fp_s2a_outi.is_open()) fp_s2a_outi.close();
        if (fp_s3m_outi.is_open()) fp_s3m_outi.close(); if (fp_s3a_outi.is_open()) fp_s3a_outi.close();
        if (fp_s4m_outi.is_open()) fp_s4m_outi.close(); if (fp_s4a_outi.is_open()) fp_s4a_outi.close();
        if (fp_s5m_outi.is_open()) fp_s5m_outi.close(); if (fp_s5a_outi.is_open()) fp_s5a_outi.close();
        if (fp_s6m_outi.is_open()) fp_s6m_outi.close(); if (fp_s6a_outi.is_open()) fp_s6a_outi.close();
        if (fp_s7m_outi.is_open()) fp_s7m_outi.close(); if (fp_s7a_outi.is_open()) fp_s7a_outi.close();
        if (fp_s8m_outi.is_open()) fp_s8m_outi.close(); if (fp_s8a_outi.is_open()) fp_s8a_outi.close();
        if (fp_s9m_outi.is_open()) fp_s9m_outi.close(); if (fp_s9a_outi.is_open()) fp_s9a_outi.close();
        if (fp_s10m_outi.is_open()) fp_s10m_outi.close(); if (fp_s10a_outi.is_open()) fp_s10a_outi.close();
        if (fp_s11m_outi.is_open()) fp_s11m_outi.close(); if (fp_s11a_outi.is_open()) fp_s11a_outi.close();
        if (fp_s12m_outi.is_open()) fp_s12m_outi.close(); if (fp_s12a_outi.is_open()) fp_s12a_outi.close();
        if (fp_s13m_outi.is_open()) fp_s13m_outi.close(); if (fp_s13a_outi.is_open()) fp_s13a_outi.close();
        if (fp_s0m_outq.is_open()) fp_s0m_outq.close(); if (fp_s0a_outq.is_open()) fp_s0a_outq.close();
        if (fp_s1m_outq.is_open()) fp_s1m_outq.close(); if (fp_s1a_outq.is_open()) fp_s1a_outq.close();
        if (fp_s2m_outq.is_open()) fp_s2m_outq.close(); if (fp_s2a_outq.is_open()) fp_s2a_outq.close();
        if (fp_s3m_outq.is_open()) fp_s3m_outq.close(); if (fp_s3a_outq.is_open()) fp_s3a_outq.close();
        if (fp_s4m_outq.is_open()) fp_s4m_outq.close(); if (fp_s4a_outq.is_open()) fp_s4a_outq.close();
        if (fp_s5m_outq.is_open()) fp_s5m_outq.close(); if (fp_s5a_outq.is_open()) fp_s5a_outq.close();
        if (fp_s6m_outq.is_open()) fp_s6m_outq.close(); if (fp_s6a_outq.is_open()) fp_s6a_outq.close();
        if (fp_s7m_outq.is_open()) fp_s7m_outq.close(); if (fp_s7a_outq.is_open()) fp_s7a_outq.close();
        if (fp_s8m_outq.is_open()) fp_s8m_outq.close(); if (fp_s8a_outq.is_open()) fp_s8a_outq.close();
        if (fp_s9m_outq.is_open()) fp_s9m_outq.close(); if (fp_s9a_outq.is_open()) fp_s9a_outq.close();
        if (fp_s10m_outq.is_open()) fp_s10m_outq.close(); if (fp_s10a_outq.is_open()) fp_s10a_outq.close();
        if (fp_s11m_outq.is_open()) fp_s11m_outq.close(); if (fp_s11a_outq.is_open()) fp_s11a_outq.close();
        if (fp_s12m_outq.is_open()) fp_s12m_outq.close(); if (fp_s12a_outq.is_open()) fp_s12a_outq.close();
        if (fp_s13m_outq.is_open()) fp_s13m_outq.close(); if (fp_s13a_outq.is_open()) fp_s13a_outq.close();
        #endif
    }

    void    configure(FFT_CFG cfg);
    void    init(char* module_name);
    void    ifft_n(Complex<double> *in,Complex<double> *out, int input_iwl, int iwl_sel, bool is_DIT);
    void    fft_n(Complex<double> *in,Complex<double> *out, int input_iwl, int iwl_sel, bool is_DIT);
    void    index_reversed(Complex<double> *in, Complex<double> *out);
    void    index_reversed(Complex<double> *in, Complex<fxp> *out);
    void    index_reversed(Complex<fxp> *in, Complex<double> *out);

    void    idft(Complex<double> *in,Complex<double> *out);// theoretical calculating value
    void    dft(Complex<double> *in,Complex<double> *out);

    void    fft_dit_n(Complex<double> *in,Complex<double> *out);
    void    fft_dif_n(Complex<double> *in,Complex<double> *out);
    int     m_fft_log2_len;// = 0;
    int     m_fft_len;// = 0;

    Complex<double>     **m_MO;
    Complex<double>     **m_AO;
    Complex<double>     *m_in;
    Complex<T>          m_W;// TF output
    int                 *m_addr_reversed;
    double              gain;// =0;

private:
    Complex<double> TF(int r, int N){
        Complex<double> out;
        out.I = cos(2*PI*r/N);
        out.Q = -sin(2*PI*r/N);
        return(out);
    }
    int         bitrev(int in , int bitwidth);
    void        fft_mult(int offset,int mode,Complex<double> *in,Complex<double> *out, int idx = 0);
    void        fft_add(int offset,Complex<double> *in,Complex<double> *out, int idx = 0);
    void        fft_dit_256();
    void        fft_dit_4096_2048();
    void        fft_dif_256();
    void        fft_dif_2048();
    void        fft_dif_4096_2048();
    Complex<double> *m_temp,*m_temp1;
    Complex<double> *m_fft_in;
    FFT_CFG         m_cfg;
    // FXP
    Complex<T>      temp_for_fix;
    Complex<T>      *m_FXP_A0;//[16];
    Complex<T>      *m_FXP_M0;//[16];
    int             FFT_2N_WL[20][2];// = {};
    int             FFT_2N_WL_IN[2];// = {};


#ifdef DUMP_RTL_FFT
    std::ofstream   fp_len,fp_in_i,fp_in_q,fp_out_i,fp_out_q,fp_wn_i,fp_wn_q;
    std::ofstream   fp_wn_lut_i,fp_wn_lut_q;
    std::ofstream   fp_s0m_outi,fp_s0m_outq,fp_s0a_outi,fp_s0a_outq,fp_s1m_outi,fp_s1m_outq,fp_s1a_outi,fp_s1a_outq;
    std::ofstream   fp_s2m_outi,fp_s2m_outq,fp_s2a_outi,fp_s2a_outq,fp_s3m_outi,fp_s3m_outq,fp_s3a_outi,fp_s3a_outq;
    std::ofstream   fp_s4m_outi,fp_s4m_outq,fp_s4a_outi,fp_s4a_outq,fp_s5m_outi,fp_s5m_outq,fp_s5a_outi,fp_s5a_outq;
    std::ofstream   fp_s6m_outi,fp_s6m_outq,fp_s6a_outi,fp_s6a_outq,fp_s7m_outi,fp_s7m_outq,fp_s7a_outi,fp_s7a_outq;
    std::ofstream   fp_s8m_outi,fp_s8m_outq,fp_s8a_outi,fp_s8a_outq,fp_s9m_outi,fp_s9m_outq,fp_s9a_outi,fp_s9a_outq;
    std::ofstream   fp_s10m_outi,fp_s10m_outq,fp_s10a_outi,fp_s10a_outq,fp_s11m_outi,fp_s11m_outq,fp_s11a_outi,fp_s11a_outq;
    std::ofstream   fp_s12m_outi,fp_s12m_outq,fp_s12a_outi,fp_s12a_outq,fp_s13m_outi,fp_s13m_outq,fp_s13a_outi,fp_s13a_outq;
#endif
};

//const int  FFT_2N_WL_0[16]  = {14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14};
//const int  FFT_2N_IWL_0[16] = { 2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 14};

//const int  FFT_2N_WL_0[16]  = {21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21};
const int  FFT_2N_WL_0[16]  = {20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20};
//const int  FFT_2N_WL_0[16]  = {19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19};
//const int  FFT_2N_WL_0[16]  = {18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18};
const int  FFT_2N_IWL_t[16] = { 1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 13, 13};
const int  FFT_2N_IWL_0[16] = { 2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 13, 13, 13};
int test = 1;
//const int  FFT_2N_WL_0[16]  = {20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20};
//const int  FFT_2N_IWL_0[16] = {3,  4,  5,  6,  7,  8,  9,  10,  11, 12, 13, 15, 17, 19};


const int  FFT_2N_WL_1[16]  = {12, 13, 14, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16};
const int  FFT_2N_IWL_1[16] = { 1,  2,  3,  4,  5,  5,  5,  5,  6,  7,  7,  7,  7};

const int  IFFT_2N_WL_0[16]  = {14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14};
const int  IFFT_2N_IWL_0[16] = { 2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 14};

const int IFFT_2N_WL_1[16]  = {12, 13, 14, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16};
const int IFFT_2N_IWL_1[16] = { 1,  2,  3,  4,  5,  5,  5,  6,  6,  6,  7,  7,  7};

template <class T>
void Cls_fft<T>:: configure(FFT_CFG cfg)
{
    m_cfg = cfg;
    m_fft_log2_len = m_cfg.fft_log2_len;
}

template <class T>
void Cls_fft<T>:: init(char* module_name)
{
    m_fft_len          = int(UINT64_POW2[m_fft_log2_len]);
    m_MO               = safe_new(m_MO, m_fft_log2_len + 5, m_fft_len);
    m_AO               = safe_new(m_AO, m_fft_log2_len + 5, m_fft_len);
    m_addr_reversed    = safe_new(m_addr_reversed, m_fft_len);
    m_in               = safe_new(m_in,m_fft_len);
    m_temp             = safe_new(m_temp,m_fft_len);
    m_temp1            = safe_new(m_temp1,m_fft_len);

    m_FXP_A0           = safe_new(m_FXP_A0, 16);
    m_FXP_M0           = safe_new(m_FXP_M0, 16);
    m_fft_in           = safe_new(m_fft_in, 8192);
    zeros(m_FXP_A0,16);
    zeros(m_FXP_M0,16);

#ifdef DUMP_RTL_FFT
    char filename[300] = {};
    strcpy(filename, module_name); strcat(filename, "fft_len.dat");     fp_len.open(filename);
    strcpy(filename, module_name); strcat(filename, "in_i.dat");        fp_in_i.open(filename);
    strcpy(filename, module_name); strcat(filename, "in_q.dat");        fp_in_q.open(filename);
    strcpy(filename, module_name); strcat(filename, "out_i.dat");       fp_out_i.open(filename);
    strcpy(filename, module_name); strcat(filename, "out_q.dat");       fp_out_q.open(filename);
    strcpy(filename, module_name); strcat(filename, "wn_i.dat");        fp_wn_i.open(filename);
    strcpy(filename, module_name); strcat(filename, "wn_q.dat");        fp_wn_q.open(filename);
    strcpy(filename, module_name); strcat(filename, "wn_lut_i.dat");    fp_wn_lut_i.open(filename);
    strcpy(filename, module_name); strcat(filename, "wn_lut_q.dat");    fp_wn_lut_q.open(filename);
    strcpy(filename, module_name); strcat(filename, "s0m_i.dat");       fp_s0m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s0m_q.dat");       fp_s0m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s0a_i.dat");       fp_s0a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s0a_q.dat");       fp_s0a_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s1m_i.dat");       fp_s1m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s1m_q.dat");       fp_s1m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s1a_i.dat");       fp_s1a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s1a_q.dat");       fp_s1a_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s2m_i.dat");       fp_s2m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s2m_q.dat");       fp_s2m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s2a_i.dat");       fp_s2a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s2a_q.dat");       fp_s2a_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s3m_i.dat");       fp_s3m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s3m_q.dat");       fp_s3m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s3a_i.dat");       fp_s3a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s3a_q.dat");       fp_s3a_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s4m_i.dat");       fp_s4m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s4m_q.dat");       fp_s4m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s4a_i.dat");       fp_s4a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s4a_q.dat");       fp_s4a_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s5m_i.dat");       fp_s5m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s5m_q.dat");       fp_s5m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s5a_i.dat");       fp_s5a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s5a_q.dat");       fp_s5a_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s6m_i.dat");       fp_s6m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s6m_q.dat");       fp_s6m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s6a_i.dat");       fp_s6a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s6a_q.dat");       fp_s6a_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s7m_i.dat");       fp_s7m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s7m_q.dat");       fp_s7m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s7a_i.dat");       fp_s7a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s7a_q.dat");       fp_s7a_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s8m_i.dat");       fp_s8m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s8m_q.dat");       fp_s8m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s8a_i.dat");       fp_s8a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s8a_q.dat");       fp_s8a_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s9m_i.dat");       fp_s9m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s9m_q.dat");       fp_s9m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s9a_i.dat");       fp_s9a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s9a_q.dat");       fp_s9a_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s10m_i.dat");      fp_s10m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s10m_q.dat");      fp_s10m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s10a_i.dat");      fp_s10a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s10a_q.dat");      fp_s10a_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s11m_i.dat");      fp_s11m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s11m_q.dat");      fp_s11m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s11a_i.dat");      fp_s11a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s11a_q.dat");      fp_s11a_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s12m_i.dat");      fp_s12m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s12m_q.dat");      fp_s12m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s12a_i.dat");      fp_s12a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s12a_q.dat");      fp_s12a_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s13m_i.dat");      fp_s13m_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s13m_q.dat");      fp_s13m_outq.open(filename);
    strcpy(filename, module_name); strcat(filename, "s13a_i.dat");      fp_s13a_outi.open(filename);
    strcpy(filename, module_name); strcat(filename, "s13a_q.dat");      fp_s13a_outq.open(filename);
#endif

    FFT_2N_WL_IN[0] = 14;//14
    FFT_2N_WL_IN[1] = 1 ;

    fix_set(m_W,11,2,FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_W,"m_W",true, 2000);
    fix_set_group(m_in,m_fft_len , FFT_2N_WL_IN[0], FFT_2N_WL_IN[1], FXP_TC,FXP_RND, FXP_SAT_SYM);

    for(int i=0;i<m_fft_len;i++){
        m_addr_reversed[i] = bitrev(i,m_fft_log2_len);
    }
    if ((m_fft_log2_len > 15)||(m_fft_log2_len < 8)){
        LOG2DEV << "FFT init error: init(int) must belongs to [8, 15] == [2^8, 2^15], program is hanging up!\n";
        while(1){}
    }
}

template <class T>
void Cls_fft<T>::fft_n(Complex<double> *in,Complex<double> *out, int input_iwl, int iwl_sel, bool is_DIT)
{
    int ck0;
    char stringchar[10]="0";
    char featurechar[63];
    if(iwl_sel == 0){
        if (is_DIT){
            for(ck0=0;ck0<15;ck0++){
                itoa(ck0,stringchar,16);
                strcpy(featurechar,"M");
                fix_set(m_FXP_M0[ck0], FFT_2N_WL_0[ck0], FFT_2N_IWL_t[ck0], FXP_TC,FXP_RND, FXP_SAT_SYM);
                if(ck0==test-1){
                    //statistic_set( m_FXP_M0[ck0],strcat(featurechar,stringchar),true, 1);
                }
                strcpy(featurechar,"A");
                fix_set(m_FXP_A0[ck0], FFT_2N_WL_0[ck0], FFT_2N_IWL_0[ck0], FXP_TC,FXP_RND, FXP_SAT_SYM);
                if(ck0==test-1){
                    //statistic_set( m_FXP_A0[ck0],strcat(featurechar,stringchar),true, 1);
                }
            }
        }
        else{
            for(ck0=0;ck0<15;ck0++){
                itoa(ck0,stringchar,16);
                strcpy(featurechar,"A");
                fix_set(m_FXP_A0[ck0],FFT_2N_WL_0[ck0], FFT_2N_IWL_0[ck0], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_A0[ck0],strcat(featurechar,stringchar),true, 4096);
                strcpy(featurechar,"M");
                fix_set(m_FXP_M0[ck0],FFT_2N_WL_0[ck0+1], FFT_2N_IWL_0[ck0+1], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_M0[ck0],strcat(featurechar,stringchar),true, 4096);
            }
        }
    }else if(iwl_sel == 1){
        if (is_DIT){
            for(ck0=0;ck0<15;ck0++){
                itoa(ck0,stringchar,16);
                strcpy(featurechar,"M");
                fix_set(m_FXP_M0[ck0], FFT_2N_WL_1[ck0], FFT_2N_IWL_1[ck0], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_M0[ck0],strcat(featurechar,stringchar),true, 256);
                strcpy(featurechar,"A");
                fix_set(m_FXP_A0[ck0], FFT_2N_WL_1[ck0], FFT_2N_IWL_1[ck0], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_A0[ck0],strcat(featurechar,stringchar),true, 256);
            }
        }
        else{
            for(ck0=0;ck0<15;ck0++){
                itoa(ck0,stringchar,16);
                strcpy(featurechar,"A");
                fix_set(m_FXP_A0[ck0],FFT_2N_WL_1[ck0], FFT_2N_IWL_1[ck0], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_A0[ck0],strcat(featurechar,stringchar),true, 2048);
                strcpy(featurechar,"M");
                fix_set(m_FXP_M0[ck0],FFT_2N_WL_1[ck0+1], FFT_2N_IWL_1[ck0+1], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_M0[ck0],strcat(featurechar,stringchar),true, 2048);
            }
        }
    }

    int i;
    Complex<double> *y = NULL;
    y = safe_new(y,m_fft_len);

    gain = pow(2.0, 1 - input_iwl);
    //fix_set(temp_for_fix, FFT_2N_WL_IN[0], FFT_2N_WL_IN[1], FXP_TC, FXP_RND, FXP_SAT_SYM);

    for(i = 0; i < m_fft_len; i++){
        temp_for_fix = in[i] * gain;
        y[i] = temp_for_fix;
        //cout << in[i].I <<"\t"<<temp_for_fix.I<<"\t"<<y[i].I<< endl;

    }

    if (is_DIT){
        index_reversed(y, y);//This function has a internal buffer
        fft_dit_n(y , out);
    }
    else{
        fft_dif_n(y , out);
        index_reversed(out , out);//This function has a internal buffer
    }

    for(i = 0; i < m_fft_len; i++){
        out[i] = out[i] / gain;
    }

    free(y);
}

template <class T>
void Cls_fft<T>::ifft_n(Complex<double> *in,Complex<double> *out, int input_iwl, int iwl_sel, bool is_DIT)
{
    int ck0;
    char stringchar[10]="0";
    char featurechar[63];

    if(iwl_sel == 0){
        if (is_DIT){
            for(ck0=0;ck0<15;ck0++){
                itoa(ck0,stringchar,16);
                strcpy(featurechar,"M");
                fix_set(m_FXP_M0[ck0], IFFT_2N_WL_0[ck0], IFFT_2N_IWL_0[ck0], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_M0[ck0],strcat(featurechar,stringchar),true, 2048);
                strcpy(featurechar,"A");
                fix_set(m_FXP_A0[ck0], IFFT_2N_WL_0[ck0], IFFT_2N_IWL_0[ck0], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_A0[ck0],strcat(featurechar,stringchar),true, 2048);
            }
        }
        else{
            for(ck0=0;ck0<15;ck0++){
                itoa(ck0,stringchar,16);
                strcpy(featurechar,"A");
                fix_set(m_FXP_A0[ck0],IFFT_2N_WL_0[ck0], IFFT_2N_IWL_0[ck0], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_A0[ck0],strcat(featurechar,stringchar),true, 4096);
                strcpy(featurechar,"M");
                fix_set(m_FXP_M0[ck0],IFFT_2N_WL_0[ck0+1], IFFT_2N_IWL_0[ck0+1], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_M0[ck0],strcat(featurechar,stringchar),true, 4096);
            }
        }
    }else if(iwl_sel == 1){
        if (is_DIT){
            for(ck0=0;ck0<15;ck0++){
                itoa(ck0,stringchar,16);
                strcpy(featurechar,"M");
                fix_set(m_FXP_M0[ck0], IFFT_2N_WL_1[ck0], IFFT_2N_IWL_1[ck0], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_M0[ck0],strcat(featurechar,stringchar),true, 2048);
                strcpy(featurechar,"A");
                fix_set(m_FXP_A0[ck0], IFFT_2N_WL_1[ck0], IFFT_2N_IWL_1[ck0], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_A0[ck0],strcat(featurechar,stringchar),true, 2048);
            }
        }
        else{
            for(ck0=0;ck0<15;ck0++){
                itoa(ck0,stringchar,16);
                strcpy(featurechar,"A");
                fix_set(m_FXP_A0[ck0],IFFT_2N_WL_1[ck0], IFFT_2N_IWL_1[ck0], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_A0[ck0],strcat(featurechar,stringchar),true, 2048);
                strcpy(featurechar,"M");
                fix_set(m_FXP_M0[ck0],IFFT_2N_WL_1[ck0+1], IFFT_2N_IWL_1[ck0+1], FXP_TC,FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_M0[ck0],strcat(featurechar,stringchar),true, 2048);
            }
        }
    }

    m_temp[0] = out[0];
    for (ck0=0;ck0<m_fft_len;ck0++){
        m_temp1[ck0] = in[ck0].conjug();
    }

    int i;
    gain = pow(2.0, 1 - input_iwl);
    fix_set(temp_for_fix, FFT_2N_WL_IN[0], FFT_2N_WL_IN[1], FXP_TC, FXP_RND, FXP_SAT_SYM);

    for(i = 0; i < m_fft_len; i++){
        temp_for_fix = m_temp1[i] * gain;
        m_fft_in[i] = temp_for_fix;
    }

    if (is_DIT){
        index_reversed(m_fft_in, m_fft_in);//This function has a internal buffer
        fft_dit_n(m_fft_in , out);
    }
    else{
        fft_dif_n(m_fft_in , out);
        index_reversed(out , out);//This function has a internal buffer
    }

    for(i = 0; i < m_fft_len; i++){
        out[i] = out[i] / gain;
    }

    //free(m_fft_in);


    for (ck0=0;ck0<m_fft_len;ck0++){
        m_temp[ck0] = out[ck0].conjug()/m_fft_len;
    }
    mem_copy(out, m_temp, m_fft_len);
}

template <class T>
int Cls_fft<T>:: bitrev(int in , int bitwidth)
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

template <class T>
inline void Cls_fft<T>::index_reversed( Complex<double> *in, Complex<double> *out)
{
    for (int i=0;i<m_fft_len;i++)
        m_temp[i] = in[m_addr_reversed[i]];
        mem_copy(out,m_temp,m_fft_len);
}

template <class T>
void Cls_fft<T>::idft(Complex<double> *in,Complex<double> *out)
{
    int i,j;
    for (i=0;i<m_fft_len;i++){
        out[i]=0.0;
        for(j=0;j<m_fft_len;j++)
        {out[i] += in[j] * TF(-i*j,m_fft_len);}
    }
}

template <class T>
void Cls_fft<T>::dft(Complex<double> *in,Complex<double> *out)
{
    int i,j;
    for (i=0;i<m_fft_len;i++)
    {
        out[i]=0.0;
        for(j=0;j<m_fft_len;j++){
            out[i] = out[i]+ in[j] * TF(i*j,m_fft_len);
        }
    }
}

template <class T>
void Cls_fft<T>::fft_add(int offset,Complex<double> *in,Complex<double> *out, int idx)
{
    int i=0,j=0,adr1,adr2;
    for(i=0;i*j<(m_fft_len/2);i++)
    {
        for(j=0;j<offset;j++)
        {
            adr1 = 2*i*offset+j;
            adr2 = adr1+offset;
            m_FXP_A0[idx] = in[adr1]+in[adr2]; out[adr1] = m_FXP_A0[idx];
            m_FXP_A0[idx] = in[adr1]-in[adr2]; out[adr2] = m_FXP_A0[idx];
        }
    }
}

template <class T>
void Cls_fft<T>::fft_mult(int offset,int mode,Complex<double> *in,Complex<double> *out, int idx)
{
    int mode1,mode2;
    if(mode<4)
        mode1 = offset;
    else
        mode1 = 2;
    mode2 = mode%4;
    Complex<double> temp1,temp2;
    int i,block_frac,block_integ,inter_block;
    for(i=0;i<m_fft_len;i++){
        block_frac = i/offset;
        block_frac = block_frac%(1<<mode2);
        block_frac = bitrev(block_frac,mode2);
        block_integ = i/(offset/mode1);
        block_integ = block_integ%mode1;
        inter_block = m_fft_len/mode1/(1<<mode2);
        m_W=TF(block_frac*block_integ*inter_block,m_fft_len);
        m_FXP_M0[idx] = in[i]* m_W.todouble();
        out[i] = m_FXP_M0[idx];
    }
}

template <class T>
void Cls_fft<T>::fft_dit_n(Complex<double> *in,Complex<double> *out)
{
    int i,offset;
    mem_copy(m_in,in,m_fft_len);

   // Include R8+R8+R4
    if(m_fft_len < 1024){
        fft_dit_256();
        for (i = 8; i < m_fft_log2_len;i++){
            offset = int(UINT64_POW2[i]);
            fft_mult(offset,FFT_2_1,m_AO[i-1],m_MO[i],i); fft_add(offset,m_MO[i],m_AO[i],i);
        }
    }
    else
       fft_dit_4096_2048();

    // Normalize
    mem_copy(out, m_AO[m_fft_log2_len-1], m_fft_len);
#ifdef DUMP_RTL_FFT
    int para = 2;
    fxp rtl_dump0, rtl_dump1, rtl_dump00, rtl_dump01, rtl_dump02, rtl_dump03,rtl_dump04, rtl_dump05, rtl_dump06, rtl_dump07, rtl_dump08, rtl_dump09, rtl_dump10, rtl_dump11;
    fix_set(rtl_dump0,14,1,FXP_TC,FXP_RND, FXP_SAT_SYM);
    //fix_set(rtl_dump1,16,13,FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump1,14, FFT_2N_IWL_0[m_fft_log2_len-1] ,FXP_TC,FXP_RND, FXP_SAT_SYM);//2022 02 22

    fix_set(rtl_dump00,FFT_2N_WL_0[0],  FFT_2N_IWL_0[0],FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump01,FFT_2N_WL_0[1],  FFT_2N_IWL_0[1],FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump02,FFT_2N_WL_0[2],  FFT_2N_IWL_0[2],FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump03,FFT_2N_WL_0[3],  FFT_2N_IWL_0[3],FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump04,FFT_2N_WL_0[4],  FFT_2N_IWL_0[4],FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump05,FFT_2N_WL_0[5],  FFT_2N_IWL_0[5],FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump06,FFT_2N_WL_0[6],  FFT_2N_IWL_0[6],FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump07,FFT_2N_WL_0[7],  FFT_2N_IWL_0[7],FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump08,FFT_2N_WL_0[8],  FFT_2N_IWL_0[8],FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump09,FFT_2N_WL_0[9],  FFT_2N_IWL_0[9],FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump10,FFT_2N_WL_0[10],  FFT_2N_IWL_0[10],FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump11,FFT_2N_WL_0[11],  FFT_2N_IWL_0[11],FXP_TC,FXP_RND, FXP_SAT_SYM);

   for (int i = 0 ; i < m_block ; i ++){
    for(unsigned int ck0 = 0; ck0 < m_fft_len; ck0++)
    {
        dump(fp_in_i,&m_in[ck0],1,rtl_dump0);
        dump(fp_out_i,&out[ck0].I,1, rtl_dump1);
        dump(fp_out_q,&out[ck0].Q,1, rtl_dump1);
    }
   }
      dump(fp_s1m_outi, m_MO[ 1],m_fft_len,rtl_dump01);
      dump(fp_s2m_outi, m_MO[ 2],m_fft_len,rtl_dump02);
      dump(fp_s3m_outi, m_MO[ 3],m_fft_len,rtl_dump03);
      dump(fp_s4m_outi, m_MO[ 4],m_fft_len,rtl_dump04);
      dump(fp_s5m_outi, m_MO[ 5],m_fft_len,rtl_dump05);
      dump(fp_s6m_outi, m_MO[ 6],m_fft_len,rtl_dump06);
      dump(fp_s7m_outi, m_MO[ 7],m_fft_len,rtl_dump07);
      dump(fp_s8m_outi, m_MO[ 8],m_fft_len,rtl_dump08);
      dump(fp_s9m_outi, m_MO[ 9],m_fft_len,rtl_dump09);
      dump(fp_s11m_outi, m_MO[ 11],m_fft_len,rtl_dump11);
      fp_len << m_fft_len <<endl;//dump fft len

      dump(fp_s0a_outi, m_AO[ 0],m_fft_len,rtl_dump00);
      dump(fp_s1a_outi, m_AO[ 1],m_fft_len,rtl_dump01);
      dump(fp_s4a_outi, m_AO[ 4],m_fft_len,rtl_dump04);
      dump(fp_s5a_outi, m_AO[ 5],m_fft_len,rtl_dump05);
      dump(fp_s6a_outi, m_AO[ 6],m_fft_len,rtl_dump06);
      dump(fp_s7a_outi, m_AO[ 7],m_fft_len,rtl_dump07);
      dump(fp_s8a_outi, m_AO[ 8],m_fft_len,rtl_dump08);
      dump(fp_s9a_outi, m_AO[ 9],m_fft_len,rtl_dump09);
      dump(fp_s10a_outi, m_AO[ 10],m_fft_len,rtl_dump10);
      dump(fp_s11a_outi, m_AO[ 11],m_fft_len,rtl_dump11);
#endif
}

template <class T>
void Cls_fft<T>::fft_dit_256()
{
    int offset;
    // Radix 8
    offset = int(UINT64_POW2[0 ]);mem_copy(m_MO[0 ],m_in,m_fft_len); fft_add(offset,m_MO[0 ],m_AO[0 ],0);//2022 4 16
    // offset = int(UINT64_POW2[0]);fft_mult(offset,FFT_8_1,m_in    ,m_MO[ 0],0); fft_add(offset,m_MO[ 0],m_AO[ 0],0);
    offset = int(UINT64_POW2[1]);fft_mult(offset,FFT_8_2,m_AO[ 0],m_MO[ 1],1); fft_add(offset,m_MO[ 1],m_AO[ 1],1);
    offset = int(UINT64_POW2[2]);fft_mult(offset,FFT_8_3,m_AO[ 1],m_MO[ 2],2); fft_add(offset,m_MO[ 2],m_AO[ 2],2);
    // Radix 8
    offset = int(UINT64_POW2[3]);fft_mult(offset,FFT_8_1,m_AO[ 2],m_MO[ 3],3); fft_add(offset,m_MO[ 3],m_AO[ 3],3);
    offset = int(UINT64_POW2[4]);fft_mult(offset,FFT_8_2,m_AO[ 3],m_MO[ 4],4); fft_add(offset,m_MO[ 4],m_AO[ 4],4);
    offset = int(UINT64_POW2[5]);fft_mult(offset,FFT_8_3,m_AO[ 4],m_MO[ 5],5); fft_add(offset,m_MO[ 5],m_AO[ 5],5);
    // Radix 4
    offset = int(UINT64_POW2[6]);fft_mult(offset,FFT_4_1,m_AO[ 5],m_MO[ 6],6); fft_add(offset,m_MO[ 6],m_AO[ 6],6);
    offset = int(UINT64_POW2[7]);fft_mult(offset,FFT_4_2,m_AO[ 6],m_MO[ 7],7); fft_add(offset,m_MO[ 7],m_AO[ 7],7);
}

template <class T>
void Cls_fft<T>::fft_dit_4096_2048()
{
    int offset;
    // Radix 8
    offset = int(UINT64_POW2[0 ]);mem_copy(m_MO[0 ],m_in,m_fft_len); fft_add(offset,m_MO[0 ],m_AO[0 ],0);//2^0
    offset = int(UINT64_POW2[1 ]);fft_mult(offset,FFT_8_2,m_AO[ 0],m_MO[1 ],1); fft_add(offset,m_MO[1 ],m_AO[1 ],1);//2^1
    offset = int(UINT64_POW2[2 ]);fft_mult(offset,FFT_8_3,m_AO[ 1],m_MO[2 ],2); fft_add(offset,m_MO[2 ],m_AO[2 ],2);//2^2
    // Radix 8
    offset = int(UINT64_POW2[3 ]);fft_mult(offset,FFT_8_1,m_AO[ 2],m_MO[3 ],3); fft_add(offset,m_MO[3 ],m_AO[3 ],3);//2^3
    offset = int(UINT64_POW2[4 ]);fft_mult(offset,FFT_8_2,m_AO[ 3],m_MO[4 ],4); fft_add(offset,m_MO[4 ],m_AO[4 ],4);//2^4
    offset = int(UINT64_POW2[5 ]);fft_mult(offset,FFT_8_3,m_AO[ 4],m_MO[5 ],5); fft_add(offset,m_MO[5 ],m_AO[5 ],5);//2^5
    // Radix 4
    offset = int(UINT64_POW2[6 ]);fft_mult(offset,FFT_4_1,m_AO[ 5],m_MO[6 ],6); fft_add(offset,m_MO[6 ],m_AO[6 ],6);//2^6
    offset = int(UINT64_POW2[7 ]);fft_mult(offset,FFT_4_2,m_AO[ 6],m_MO[7 ],7); fft_add(offset,m_MO[7 ],m_AO[7 ],7);//2^7

    offset = int(UINT64_POW2[8 ]);fft_mult(offset,FFT_2_1,m_AO[ 7],m_MO[8 ],8); fft_add(offset,m_MO[8 ],m_AO[8 ],8);//2^8
    offset = int(UINT64_POW2[9 ]);fft_mult(offset,FFT_2_1,m_AO[ 8],m_MO[9 ],9); fft_add(offset,m_MO[9 ],m_AO[9 ],9);//2^9
    offset = int(UINT64_POW2[10]);fft_mult(offset,FFT_2_1,m_AO[ 9],m_MO[10],10); fft_add(offset,m_MO[10],m_AO[10],10);//2^10

    offset = int(UINT64_POW2[11]);fft_mult(offset,FFT_2_1,m_AO[10],m_MO[11],11); fft_add(offset,m_MO[11],m_AO[11],11);//2^11
    if(m_fft_log2_len>=11){
        offset = int(UINT64_POW2[12]);fft_mult(offset,FFT_2_1,m_AO[11],m_MO[12],12); fft_add(offset,m_MO[12],m_AO[12],12);//2^12
    }
    if(m_fft_log2_len>=12){
        offset = int(UINT64_POW2[13]);fft_mult(offset,FFT_2_1,m_AO[12],m_MO[13],13); fft_add(offset,m_MO[13],m_AO[13],13);//2^12
        offset = int(UINT64_POW2[14]);fft_mult(offset,FFT_2_1,m_AO[13],m_MO[14],14); fft_add(offset,m_MO[14],m_AO[14],14);//2^12
    }

    /*if (m_fft_log2_len == 10) {
        offset = int(UINT64_POW2[9]);fft_mult(offset,FFT_2_1,m_AO[8],m_MO[9],9); fft_add(offset,m_MO[9],m_AO[9],9);
    }
    if (m_fft_log2_len == 12) {
        offset = int(UINT64_POW2[11]);fft_mult(offset,FFT_2_1,m_AO[10],m_MO[11],11); fft_add(offset,m_MO[11],m_AO[11],11);//2^12
    }
    if (m_fft_log2_len == 13) {
        offset = int(UINT64_POW2[11]);fft_mult(offset,FFT_2_1,m_AO[10],m_MO[11],11); fft_add(offset,m_MO[11],m_AO[11],11);//2^13
        offset = int(UINT64_POW2[12]);fft_mult(offset,FFT_2_1,m_AO[11],m_MO[12],12); fft_add(offset,m_MO[12],m_AO[12],12);
    }*/
}

template <class T>
void Cls_fft<T>::fft_dif_n(Complex<double> *in,Complex<double> *out)
{

    int i,offset;
    mem_copy(m_in, in, m_fft_len);

    if(m_fft_len == 2048){
        fft_dif_2048();//R4 + R8  +  R8 + R8
        mem_copy(out, m_AO[10],m_fft_len);
    }
    else if(m_fft_len == 4096){
        fft_dif_4096_2048();//R2 + R4 + R8  +  R8 + R8
        mem_copy(out, m_AO[11],m_fft_len);
    }
    else{// Include R4+R8+R8
        fft_dif_256();
        for (i = 8; i < m_fft_log2_len ;i++){
            offset = int(UINT64_POW2[m_fft_log2_len - 1 - i]);
            fft_add(offset,m_MO[i-1],m_AO[i]);fft_mult(offset,FFT_2_1,m_AO[i],m_MO[i]);
        }
        mem_copy(out, m_MO[m_fft_log2_len-1], m_fft_len);
    }
}

template <class T>
void Cls_fft<T>::fft_dif_256()
{
    int offset;
    // Radix 4
    offset = int(UINT64_POW2[m_fft_log2_len - 1]);fft_add(offset,m_in    ,m_AO[ 0],0);fft_mult(offset,FFT_4_2,m_AO[ 0],m_MO[ 0],0);
    offset = int(UINT64_POW2[m_fft_log2_len - 2]);fft_add(offset,m_MO[ 0],m_AO[ 1],1);fft_mult(offset,FFT_4_1,m_AO[ 1],m_MO[ 1],1);
    // Radix 8
    offset = int(UINT64_POW2[m_fft_log2_len - 3]);fft_add(offset,m_MO[ 1],m_AO[ 2],2);fft_mult(offset,FFT_8_3,m_AO[ 2],m_MO[ 2],2);
    offset = int(UINT64_POW2[m_fft_log2_len - 4]);fft_add(offset,m_MO[ 2],m_AO[ 3],3);fft_mult(offset,FFT_8_2,m_AO[ 3],m_MO[ 3],3);
    offset = int(UINT64_POW2[m_fft_log2_len - 5]);fft_add(offset,m_MO[ 3],m_AO[ 4],4);fft_mult(offset,FFT_8_1,m_AO[ 4],m_MO[ 4],4);
    // Radix 8
    offset = int(UINT64_POW2[m_fft_log2_len - 6]);fft_add(offset,m_MO[ 4],m_AO[ 5],5);fft_mult(offset,FFT_8_3,m_AO[ 5],m_MO[ 5],5);
    offset = int(UINT64_POW2[m_fft_log2_len - 7]);fft_add(offset,m_MO[ 5],m_AO[ 6],6);fft_mult(offset,FFT_8_2,m_AO[ 6],m_MO[ 6],6);
    offset = int(UINT64_POW2[m_fft_log2_len - 8]);fft_add(offset,m_MO[ 6],m_AO[ 7],7);fft_mult(offset,FFT_8_1,m_AO[ 7],m_MO[ 7],7);
}

template <class T>
void Cls_fft<T>::fft_dif_2048()
{
    int offset;

    mem_copy(m_MO[ 0], m_in, m_fft_len);

    offset = int(UINT64_POW2[11 - 1]);fft_add(offset,m_in    ,m_AO[ 0], 0);fft_mult(offset,FFT_4_2,m_AO[ 0],m_MO[ 0], 0);  // Radix 4
    offset = int(UINT64_POW2[11 - 2]);fft_add(offset,m_MO[ 0],m_AO[ 1], 1);fft_mult(offset,FFT_4_1,m_AO[ 1],m_MO[ 1], 1);

    offset = int(UINT64_POW2[11 - 3]);fft_add(offset,m_MO[ 1],m_AO[ 2], 2);fft_mult(offset,FFT_8_3,m_AO[ 2],m_MO[ 2], 2);  // Radix 8
    offset = int(UINT64_POW2[11 - 4]);fft_add(offset,m_MO[ 2],m_AO[ 3], 3);fft_mult(offset,FFT_8_2,m_AO[ 3],m_MO[ 3], 3);
    offset = int(UINT64_POW2[11 - 5]);fft_add(offset,m_MO[ 3],m_AO[ 4], 4);fft_mult(offset,FFT_8_1,m_AO[ 4],m_MO[ 4], 4);

    offset = int(UINT64_POW2[11 - 6]);fft_add(offset,m_MO[ 4],m_AO[ 5], 5);fft_mult(offset,FFT_8_3,m_AO[ 5],m_MO[ 5], 5);  // Radix 8
    offset = int(UINT64_POW2[11 - 7]);fft_add(offset,m_MO[ 5],m_AO[ 6], 6);fft_mult(offset,FFT_8_2,m_AO[ 6],m_MO[ 6], 6);
    offset = int(UINT64_POW2[11 - 8]);fft_add(offset,m_MO[ 6],m_AO[ 7], 7);fft_mult(offset,FFT_8_1,m_AO[ 7],m_MO[ 7], 7);

    offset = int(UINT64_POW2[11 - 9]);fft_add(offset,m_MO[ 7],m_AO[ 8], 8);fft_mult(offset,FFT_8_3,m_AO[ 8],m_MO[ 8], 8);  // Radix 8
    offset = int(UINT64_POW2[11 -10]);fft_add(offset,m_MO[ 8],m_AO[ 9], 9);fft_mult(offset,FFT_8_2,m_AO[ 9],m_MO[ 9], 9);
    offset = int(UINT64_POW2[11 -11]);fft_add(offset,m_MO[ 9],m_AO[10],10);fft_mult(offset,FFT_8_1,m_AO[10],m_MO[10],10);
}

template <class T>
void Cls_fft<T>::fft_dif_4096_2048()
{
#ifdef DUMP_RTL_FFT
    //dump(fp_in_i,m_in,m_fft_len,FFT_2N_WL_IN[0],FFT_2N_WL_IN[1],0);	dump(fp_in_q,m_in,m_fft_len,FFT_2N_WL_IN[0],FFT_2N_WL_IN[1],1);
#endif
	int offset;
    if (m_fft_log2_len == 12){
        offset = int(UINT64_POW2[12-1]);fft_add(offset,m_in ,m_AO[ 0],0);fft_mult(offset,FFT_2_1,m_AO[ 0],m_MO[ 0],0);
    }
    else if(m_fft_log2_len == 11){
        mem_copy(m_MO[ 0], m_in, m_fft_len);
    }

    // Radix 4
    offset = int(UINT64_POW2[12 - 2]);fft_add(offset,m_MO[ 0],m_AO[ 1],1);fft_mult(offset,FFT_4_2,m_AO[ 1],m_MO[ 1],1);
    offset = int(UINT64_POW2[12 - 3]);fft_add(offset,m_MO[ 1],m_AO[ 2],2);fft_mult(offset,FFT_4_1,m_AO[ 2],m_MO[ 2],2);
    // Radix 8
    offset = int(UINT64_POW2[12 - 4]);fft_add(offset,m_MO[ 2],m_AO[ 3],3);fft_mult(offset,FFT_8_3,m_AO[ 3],m_MO[ 3],3);
    offset = int(UINT64_POW2[12 - 5]);fft_add(offset,m_MO[ 3],m_AO[ 4],4);fft_mult(offset,FFT_8_2,m_AO[ 4],m_MO[ 4],4);
    offset = int(UINT64_POW2[12 - 6]);fft_add(offset,m_MO[ 4],m_AO[ 5],5);fft_mult(offset,FFT_8_1,m_AO[ 5],m_MO[ 5],5);
    // Radix 8
    offset = int(UINT64_POW2[12 - 7]);fft_add(offset,m_MO[ 5],m_AO[ 6],6);fft_mult(offset,FFT_8_3,m_AO[ 6],m_MO[ 6],6);
    offset = int(UINT64_POW2[12 - 8]);fft_add(offset,m_MO[ 6],m_AO[ 7],7);fft_mult(offset,FFT_8_2,m_AO[ 7],m_MO[ 7],7);
    offset = int(UINT64_POW2[12 - 9]);fft_add(offset,m_MO[ 7],m_AO[ 8],8);fft_mult(offset,FFT_8_1,m_AO[ 8],m_MO[ 8],8);
    // Radix 8
    offset = int(UINT64_POW2[12 -10]);fft_add(offset,m_MO[ 8],m_AO[ 9],9);fft_mult(offset,FFT_8_3,m_AO[ 9],m_MO[ 9],9);
    offset = int(UINT64_POW2[12 -11]);fft_add(offset,m_MO[ 9],m_AO[10],10);fft_mult(offset,FFT_8_2,m_AO[10],m_MO[10],10);
    offset = int(UINT64_POW2[12 -12]);fft_add(offset,m_MO[10],m_AO[11],11);fft_mult(offset,FFT_8_1,m_AO[11],m_MO[11],11);


#ifdef DUMP_RTL_FFT
    int para = 2;
    fxp rtl_dump0, rtl_dump1, rtl_dump00, rtl_dump01, rtl_dump02;
    fix_set(rtl_dump0,16,1,FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump1,16,12,FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump00,FFT_2N_WL_0[0],  FFT_2N_IWL_0[0],FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump01,FFT_2N_WL_0[1],  FFT_2N_IWL_0[1],FXP_TC,FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump02,FFT_2N_WL_0[2],  FFT_2N_IWL_0[2],FXP_TC,FXP_RND, FXP_SAT_SYM);
//    for(i=0;i<m_fft_len;i++)
//        {m_W=TF(i,m_fft_len);dump(fp_wn_i,&m_W,1,11,2,0);dump(fp_wn_q,&m_W,1,11,2,1);i=i+1;}

    Complex<double> *y = NULL;
    y = safe_new(y,4096);
    mem_copy(y,m_in,4096);
    index_reversed(y, y);
    for(unsigned int ck0 = 0; ck0 < 4096; ck0++)
    {
        dump(fp_in_i,&y[ck0],1,rtl_dump0);
        dump(fp_out_i,&m_AO[m_fft_log2_len-1][ck0],1, rtl_dump1);
//	dump(fp_s0m_outi ,&m_MO[ 1][ck0],1,rtl_dump2);
    }
      dump(fp_s0m_outi, m_MO[ 0],4096,rtl_dump00);
      dump(fp_s1m_outi, m_MO[ 1],4096,rtl_dump01);
      dump(fp_s2m_outi, m_MO[ 2],4096,rtl_dump02);

      dump(fp_s0a_outi, m_AO[ 0],4096,rtl_dump00);
      dump(fp_s1a_outi, m_AO[ 1],4096,rtl_dump01);
      dump(fp_s2a_outi, m_AO[ 0],4096,rtl_dump02);

      free(y);
#endif
}

#endif
