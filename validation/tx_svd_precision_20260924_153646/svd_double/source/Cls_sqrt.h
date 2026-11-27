/****************************************************************************
**
**  Copyright  (c)  2023-2028,  All Rights Reserved.
**
**  File          : Cls_sqrt.h
**  Created By    :
**  Created On    :
**
**  History     :
**  Programmer    	Date        Rev       	Description
**  --------------- ---------- 	-------- ------------------------------
**  Darren.Ren      06/05/2024  V1.0        Initial
**  Notice: Sqrt
**
****************************************************************************/
#ifndef __Cls_sqrt
#define __Cls_sqrt

//#define DUMP_MATLAB_SQRT
//#define DUMP_RTL_SQRT
//#define DUMP_MATLAB_SVD_log
#include "common_type.h"
#include <stdint.h>
#include "iomanip"

typedef struct SQRT_CFG
{
    //Read Write
    int     bit_len;
}SQRT_CFG;

template <class T>
class Cls_sqrt
{
	public:
		Cls_sqrt() {
            m_in_bit = NULL;
		}
		~Cls_sqrt() {
            free(m_in_bit);
#ifdef DUMP_MATLAB_SQRT

#endif
#ifdef DUMP_RTL_SQRT

#endif
		}
		void			init(char* module_name);
		void			go(Complex<double> **in);
		void 			reset();
		void			configure(SQRT_CFG cfg);
		double          sqrt(double in);
	private:
        SQRT_CFG        m_cfg;
        T               m_sqrt_in;
        T               m_Q;
        int             m_in_bit_dec;
        int             *m_in_bit;
        int             m_bit_len;
        int             m_exp;
		double          sqrt_bit(double in);
#ifdef DUMP_MATLAB_SQRT
        ofstream        file_mat_U;
#endif
#ifdef DUMP_RTL_SQRT
        ofstream        file_rtl_in_1;
#endif
};

template <class T>
void Cls_sqrt<T>::configure(SQRT_CFG cfg)
{
    m_bit_len = cfg.bit_len;
}


template <class T>
void Cls_sqrt<T>::init(char* module_name)
{
    //mkdir Folder
#ifdef DUMP_MATLAB_SQRT
    struct stat statExist;
    int statMd = 0;
    if (stat(module_name, &statExist) != 0) {
            statMd = mkdir(module_name, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    char filename_mat[300] = {};
    strcpy(filename_mat, module_name); strcat(filename_mat, "U_o.txt");       file_mat_U.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "S_o.txt");       file_mat_S.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "V_o.txt");       file_mat_V.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "A_o.txt");       file_mat_A.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "V_tmp_o.txt");   file_mat_V_tmp.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_s.txt");       file_mat_m_s.open(filename_mat);
#endif
#ifdef DUMP_RTL_SQRT
    struct stat statExist;
    int statMd = 0;
    if (stat(module_name, &statExist) != 0) {
            statMd = mkdir(module_name, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    char filename_rtl[300] = {};
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "rtl_in_1.dat");        file_rtl_in_1.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "rtl_in_2.dat");        file_rtl_in_2.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "rtl_in_3.dat");        file_rtl_in_3.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "rtl_in_4.dat");        file_rtl_in_4.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "rtl_in_5.dat");        file_rtl_in_5.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "rtl_in_6.dat");        file_rtl_in_6.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "rtl_in_7.dat");        file_rtl_in_7.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "rtl_in_8.dat");        file_rtl_in_8.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "rtl_in_tmp.dat");      file_rtl_in_tmp.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "A_1_in.dat");          file_rtl_A_1_in.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "A_2_in.dat");          file_rtl_A_2_in.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "V_1_in.dat");          file_rtl_V_1_in.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "V_2_in.dat");          file_rtl_V_2_in.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "A_1_out.dat");         file_rtl_A_1_out.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "A_2_out.dat");         file_rtl_A_2_out.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "V_1_out.dat");         file_rtl_V_1_out.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "V_2_out.dat");         file_rtl_V_2_out.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "c_out.dat");           file_rtl_c.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "s_out.dat");           file_rtl_s.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "G.dat");               file_rtl_G.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "V.dat");               file_rtl_V.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "xiu_tmp_out.dat");     file_rtl_xiu_tmp.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "max_frac.dat");        file_rtl_max_frac.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "in_max.dat");          file_rtl_max.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "max_frac_q.dat");      file_rtl_max_frac_q.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "max_frac_exp.dat");    file_rtl_max_frac_exp.open(filename_rtl);
#endif
    fix_set(m_Q, m_bit_len, 0, FXP_US, FXP_TRN, FXP_SAT_SYM);
    m_in_bit = safe_new(m_in_bit, m_bit_len);
	reset();
}

template <class T>
void Cls_sqrt<T>::reset()
{
    zeros(m_in_bit, m_bit_len);
}

template <class T>
double Cls_sqrt<T>::sqrt(double in)
{
    if(in == 0 || in == 1)
        return in;
    double out;
    double m_Q_d;

    int exp;
    exp = ceil(log2(fabs(in)));

    if(exp % 2 != 0)
        exp = exp + 1;
    m_exp = exp;
    if(exp <= -62) {
        m_Q_d = in * DOUBLE_POW2[64 + 62] * DOUBLE_POW2[64 - (exp + 62)];
    } else {
        m_Q_d = in * DOUBLE_POW2[64 - exp];
    }

    if(m_Q_d == 1) {
        out = 1;
    } else {
        m_Q = m_Q_d;
        out = sqrt_bit(m_Q);
    }

    return out * DOUBLE_POW2[64 + (int)(exp / 2)];
}

template <class T>
double Cls_sqrt<T>::sqrt_bit(double in)
{
    int r = 0;
    int t = 0;
    int t_2 = 0;

    m_sqrt_in = in;
    m_in_bit_dec = m_sqrt_in * DOUBLE_POW2[64 + m_bit_len];
    for (int ck0 = m_bit_len - 1; ck0 >= 0; ck0--){
        m_in_bit[m_bit_len - 1 - ck0] = (m_in_bit_dec >> ck0) & 0x1;
    }

    for (int ck0 = (int)(m_bit_len / 2); ck0 >= 1; ck0--) {
        t = t * 4 + m_in_bit[2 *((int)(m_bit_len / 2) - ck0)] * 2 + m_in_bit[2 * ((int)(m_bit_len / 2) - ck0) + 1];
        t_2 = r * 4 + 1;
        r = r * 2;
        if(t >= t_2) {
            t = t - t_2;
            r = r + 1;
        }
    }
    return r * 1.0 * DOUBLE_POW2[64 - (int)(m_bit_len / 2)] ;
}
#endif
