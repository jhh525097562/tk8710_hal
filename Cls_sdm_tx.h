/****************************************************************************
**
**  Copyright  (c)  2023-2028,  All Rights Reserved.
**
**  File          : Cls_sdm_tx.h
**  Created By    :
**  Created On    :
**
**  History     :
**  Programmer    	Date        Rev       	Description
**  --------------- ---------- 	-------- ------------------------------
**  Darren.Ren      08/01/2024  V1.0        Initial
**  Notice: Interference Suppression
**
****************************************************************************/
#ifndef __Cls_sdm_tx
#define __Cls_sdm_tx

//#define DUMP_MATLAB_SDM_TX
#define DUMP_RTL_SDM_TX
#include "common_type.h"

typedef struct SDM_TX_CFG
{
    //Read Write
}SDM_TX_CFG;

template <class T>
class Cls_sdm_tx
{
	public:
		Cls_sdm_tx() {
		}
		~Cls_sdm_tx() {
#ifdef DUMP_MATLAB_SDM_TX
        fp_mat_in.close();
        fp_mat_err_i.close();
        fp_mat_err_q.close();
#endif
#ifdef DUMP_RTL_SDM_TX
        fp_rtl_in.close();
        fp_rtl_err_i.close();
        fp_rtl_err_q.close();
        fp_rtl_int_i.close();
        fp_rtl_int_q.close();
        fp_rtl_out_i.close();
        fp_rtl_out_q.close();
#endif
		}
		void			init(char* module_name);
		void			go(Complex<double> in);
		void 			reset();
		void			configure(SDM_TX_CFG cfg);
	private:
        SDM_TX_CFG      m_cfg;
        Complex<T>      m_in;
        T               m_integrator_i;
        T               m_integrator_q;
        T               m_error_i;
        T               m_error_q;
        T               m_quantizerOutput_i;
        T               m_quantizerOutput_q;
        T               m_out_i;
        T               m_out_q;
        Complex<double> m_out;
#ifdef DUMP_MATLAB_SDM_TX
        ofstream        fp_mat_in;
        ofstream        fp_mat_err_i;
        ofstream        fp_mat_err_q;
#endif
#ifdef DUMP_RTL_SDM_TX
        ofstream        fp_rtl_in;
        ofstream        fp_rtl_err_i;
        ofstream        fp_rtl_err_q;
        ofstream        fp_rtl_int_i;
        ofstream        fp_rtl_int_q;
        ofstream        fp_rtl_out_i;
        ofstream        fp_rtl_out_q;
#endif
};

template <class T>
void Cls_sdm_tx<T>::configure(SDM_TX_CFG cfg)
{
	m_cfg = cfg;
}

template <class T>
void Cls_sdm_tx<T>::init(char* module_name)
{
    fix_set(m_in, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_error_i, 11, 2, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(m_error_i, "m_error_i", 1, 1);
    fix_set(m_error_q, 11, 2, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(m_error_q, "m_error_q", 1, 1);
    fix_set(m_integrator_i, 11, 2, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(m_integrator_i, "m_integrator_i", 1, 100);
    fix_set(m_integrator_q, 11, 2, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(m_integrator_q, "m_integrator_i", 1, 100);
    fix_set(m_quantizerOutput_i, 2, 2, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(m_quantizerOutput_i, "m_quantizerOutput_i", 1, 100);
    fix_set(m_quantizerOutput_q, 2, 2, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(m_quantizerOutput_q, "m_quantizerOutput_i", 1, 100);
    fix_set(m_out_i, 1, 1, FXP_US, FXP_RND, FXP_SAT_SYM); //statistic_set(m_out_i, "m_out_i", 1, 100);
    fix_set(m_out_q, 1, 1, FXP_US, FXP_RND, FXP_SAT_SYM); //statistic_set(m_out_q, "m_out_q", 1, 100);
    //mkdir Folder
#ifdef DUMP_MATLAB_SDM_TX
    struct stat statExist;
    int statMd = 0;
    if (stat(module_name, &statExist) != 0) {
            statMd = mkdir(module_name,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    char filename_mat[300] = {};
    strcpy(filename_mat, module_name); strcat(filename_mat, "mat_in.txt");          fp_mat_in.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "mat_err_i.txt");       fp_mat_err_i.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "mat_err_q.txt");       fp_mat_err_q.open(filename_mat);
#endif
#ifdef DUMP_RTL_SDM_TX
    struct stat statExist;
    int statMd = 0;
    if (stat(module_name, &statExist) != 0) {
            statMd = mkdir(module_name,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    char filename_rtl[300] = {};
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "in.dat");          fp_rtl_in.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "err_i.dat");       fp_rtl_err_i.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "err_q.dat");       fp_rtl_err_q.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "int_i.dat");       fp_rtl_int_i.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "int_q.dat");       fp_rtl_int_q.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "out_i.dat");       fp_rtl_out_i.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "out_q.dat");       fp_rtl_out_q.open(filename_rtl);
#endif
	reset();
}

template <class T>
void Cls_sdm_tx<T>::reset()
{
    m_integrator_i = 0;
    m_integrator_q = 0;
    m_error_i = 0;
    m_error_q = 0;
    m_quantizerOutput_i = 0;
    m_quantizerOutput_q = 0;
}

template <class T>
void Cls_sdm_tx<T>::go(Complex<double> in)
{
    m_in = in;
    m_error_i = m_in.I - m_quantizerOutput_i;
    m_integrator_i = m_integrator_i + m_error_i;
    m_quantizerOutput_i = (m_integrator_i >= 0) ? 1 : -1;
    //LOG2DEV << m_quantizerOutput_i << endl;
    m_error_q = m_in.Q - m_quantizerOutput_q;
    m_integrator_q = m_integrator_q + m_error_q;
    m_quantizerOutput_q = (m_integrator_q >= 0) ? 1 : -1;

    m_out_i = (m_quantizerOutput_i == 1) ? 0 : 1;
    m_out_q = (m_quantizerOutput_q == 1) ? 0 : 1;
    m_out.I = m_out_i;
    m_out.Q = m_out_q;
#ifdef DUMP_MATLAB_SDM_TX
    fp_mat_in << m_out_i << "\t" << m_out_q << endl;
    fp_mat_err_i << m_error_i << endl;
    fp_mat_err_q << m_error_q << endl;
#endif
#ifdef DUMP_RTL_SDM_TX
    dump(fp_rtl_in, &m_in, 1, m_in.I);
    dump(fp_rtl_err_i, &m_error_i, 1, m_error_i);
    dump(fp_rtl_err_q, &m_error_q, 1, m_error_q);
    dump(fp_rtl_int_i, &m_integrator_i, 1, m_integrator_i);
    dump(fp_rtl_int_q, &m_integrator_q, 1, m_integrator_q);
    dump(fp_rtl_out_i, &m_out_i, 1, m_out_i);
    dump(fp_rtl_out_q, &m_out_q, 1, m_out_q);
#endif

}
#endif
