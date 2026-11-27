/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File          : Cls_ds_tkg800.h
**  Created By    : Darren
**  Created On    : 17/07/2023
**
**  History     :
**  Programmer    Date         Rev          Description
**  Darren        17/07/2023   v1.0
**
**  Notice:
**
****************************************************************************/
#pragma once
#include "Lib_io.h"
#include <iostream>
#include <iomanip>
#include "Cls_FIR.h"

#define DUMP_RTL
#define DUMP_MATLAB

using namespace std;

typedef struct DS_TKG800_CFG
{

}DS_TKG800_CFG;

template <class T>
class Cls_ds_tkg800
{
public:
    // initialization
    Cls_ds_tkg800()
	{

	}
    ~Cls_ds_tkg800() {
#ifdef DUMP_RTL
        fp_input.close();
        fp_output.close();
#endif
#ifdef DUMP_MATLAB
        fp_mat_out.close();
#endif
    }

    Complex<T> dFilt(const Complex<double>& dataIn);

    void    init(char* module_name);
    void    reset();
    void    config(DS_TKG800_CFG param);
    void    go(Complex<double> in);
private:
    DS_TKG800_CFG           m_cfg;
    Cls_FIR<double>         m_fir;
    Complex<T>              m_in;
    Complex<T>              m_fir_o;
    Complex<T>              m_out;
    int                     m_cnt;
    bool                    m_out_en;
#ifdef DUMP_RTL
    std::ofstream           fp_input;
    std::ofstream           fp_output;
#endif
#ifdef DUMP_MATLAB
    std::ofstream           fp_mat_out;
#endif
};

template <class T>
void Cls_ds_tkg800<T>::config(DS_TKG800_CFG param)
{
    m_cfg = param;
}

template <class T>
void Cls_ds_tkg800<T>::init(char* module_name)
{
    m_fir.init(TKG_DOWN);
#ifdef DUMP_RTL
     char filename_rtl[300] = {};
     strcpy(filename_rtl, module_name); strcat(filename_rtl, "ds_in.dat");          fp_input.open(filename_rtl);
     strcpy(filename_rtl, module_name); strcat(filename_rtl, "ds_out.dat");         fp_output.open(filename_rtl);
#endif
#ifdef DUMP_MATLAB
     char filename_mat[300] = {};
     strcpy(filename_mat, module_name); strcat(filename_mat, "mat_ds_out.txt");     fp_mat_out.open(filename_mat);
#endif
    fix_set(m_in, 16, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_in, "m_in", 1, 1000);
    fix_set(m_out, 16, 1, FXP_TC, FXP_TRN, FXP_SAT_SYM);//statistic_set(m_out, "m_out", 1, 1000);
    reset();
}

template <class T>
void Cls_ds_tkg800<T>::reset()
{
    m_fir.reset();
    m_cnt = 0;
}

template <class T>
void Cls_ds_tkg800<T>::go(Complex<double> in)
{
    // FIR
    m_in = in;
    m_fir_o = m_fir.dFilt(m_in.todouble());

    // Downsampling
    if(m_cnt == 4)
        m_cnt = 0;
    if(m_cnt == 0)
        m_out_en = 1;
    else
        m_out_en = 0;

    m_cnt++;
    if(m_out_en)
        m_out = m_fir_o;
#ifdef DUMP_RTL
    dump(fp_input, &m_in, 1, m_in.I);
    if(m_out_en)
        dump(fp_output, &m_out, 1, m_out.I);
#endif

#ifdef DUMP_MATLAB
    if(m_out_en)
        fp_mat_out<< setprecision(20) << m_out.I << "\t" << m_out.Q << endl;
#endif
}
