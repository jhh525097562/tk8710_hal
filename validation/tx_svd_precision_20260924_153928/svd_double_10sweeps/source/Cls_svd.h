/****************************************************************************
**
**  Copyright  (c)  2023-2028,  All Rights Reserved.
**
**  File          : Cls_svd.h
**  Created By    :
**  Created On    :
**
**  History     :
**  Programmer    	Date        Rev       	Description
**  --------------- ---------- 	-------- ------------------------------
**
**  Notice: SVD
**
****************************************************************************/
#ifndef __Cls_svd
#define __Cls_svd

/*#define DUMP_MATLAB_SVD
#define DUMP_RTL_SVD*/
//#define DUMP_MATLAB_SVD_log
/*#define DUMP_RTL_SVD*/
#include "common_type.h"
#include <stdint.h>
#include "iomanip"
#include "Cls_sqrt.h"
int index_svd_p[7][8] = {   {0, 1, 2, 3, 4, 5, 6, 7},
                            {0, 3, 2, 5, 4, 6, 1, 7},
                            {0, 5, 2, 6, 4, 7, 1, 3},
                            {0, 6, 2, 4, 3, 7, 1, 5},
                            {0, 4, 2, 7, 3, 5, 1, 6},
                            {0, 2, 5, 7, 3, 6, 1, 4},
                            {0, 7, 5, 6, 3, 4, 1, 2}};

typedef struct SVD_CFG
{
    //Read Write
    int     ant_num;
}SVD_CFG;

typedef union {
    float floatValue;
    uint32_t binary_representation;
} FloatUnion;

template <class T>
class Cls_svd
{
	public:
		Cls_svd() {
            m_U = NULL;
            m_V = NULL;
            m_S = NULL;
            m_in = NULL;
            m_A_1_1 = NULL;
            m_A_1_2 = NULL;
            m_A_2_1 = NULL;
            m_A_2_2 = NULL;
            m_A_3_1 = NULL;
            m_A_3_2 = NULL;
            m_A_4_1 = NULL;
            m_A_4_2 = NULL;
            m_V_1_1 = NULL;
            m_V_1_2 = NULL;
            m_V_2_1 = NULL;
            m_V_2_2 = NULL;
            m_V_3_1 = NULL;
            m_V_3_2 = NULL;
            m_V_4_1 = NULL;
            m_V_4_2 = NULL;
            m_A_1 = NULL;
            m_A_2 = NULL;
            m_V_1 = NULL;
            m_V_2 = NULL;
            m_A = NULL;
            m_S_o = NULL;
		}
		~Cls_svd() {
            free(m_U);
            free(m_V);
            free(m_S);
            free(m_in);
            free(m_A);
            free(m_A_1_1);
            free(m_A_1_2);
            free(m_A_2_1);
            free(m_A_2_2);
            free(m_A_3_1);
            free(m_A_3_2);
            free(m_A_4_1);
            free(m_A_4_2);
            free(m_V_1_1);
            free(m_V_1_2);
            free(m_V_2_1);
            free(m_V_2_2);
            free(m_V_3_1);
            free(m_V_3_2);
            free(m_V_4_1);
            free(m_V_4_2);
            free(m_A_1);
            free(m_A_2);
            free(m_V_1);
            free(m_V_2);
            free(m_S_o);
#ifdef DUMP_MATLAB_SVD
        file_mat_U.close();
        file_mat_S.close();
        file_mat_V.close();
        file_mat_A.close();
        file_mat_m_s.close();
        file_mat_V_tmp.close();
#endif
#ifdef DUMP_RTL_SVD
        file_rtl_in_1.close();
        file_rtl_in_2.close();
        file_rtl_in_3.close();
        file_rtl_in_4.close();
        file_rtl_in_5.close();
        file_rtl_in_6.close();
        file_rtl_in_7.close();
        file_rtl_in_8.close();
        file_rtl_A_1_in.close();
        file_rtl_A_2_in.close();
        file_rtl_V_1_in.close();
        file_rtl_V_2_in.close();
        file_rtl_A_1_out.close();
        file_rtl_A_2_out.close();
        file_rtl_V_1_out.close();
        file_rtl_V_2_out.close();
        file_rtl_c.close();
        file_rtl_s.close();
        file_rtl_G.close();
        file_rtl_xiu_tmp.close();
        file_rtl_V.close();
        file_rtl_in_tmp.close();
        file_rtl_max_frac.close();
        file_rtl_max.close();
        file_rtl_max_frac_q.close();
        file_rtl_max_frac_exp.close();
#endif
		}
		void			init(char* module_name);
		void			go(Complex<double> **in);
		void 			reset();
		void			configure(SVD_CFG cfg);
        Complex<double> **m_U;
        Complex<double> **m_V;
        double          **m_S;
	private:
        SVD_CFG         m_cfg;
        Complex<double> **m_in;
        Complex<double> **m_A;
        Complex<double> *m_A_1;
        Complex<double> *m_A_2;
        Complex<double> *m_V_1;
        Complex<double> *m_V_2;
        Complex<double> *m_A_1_1;
        Complex<double> *m_A_1_2;
        Complex<double> *m_A_2_1;
        Complex<double> *m_A_2_2;
        Complex<double> *m_A_3_1;
        Complex<double> *m_A_3_2;
        Complex<double> *m_A_4_1;
        Complex<double> *m_A_4_2;
        Complex<double> *m_V_1_1;
        Complex<double> *m_V_1_2;
        Complex<double> *m_V_2_1;
        Complex<double> *m_V_2_2;
        Complex<double> *m_V_3_1;
        Complex<double> *m_V_3_2;
        Complex<double> *m_V_4_1;
        Complex<double> *m_V_4_2;
        Complex<T>      m_in_fxp;
        T               m_wk;
        T               m_wl;
        T               m_QKL;
        T               m_rsqrt_in;
        T               m_t_tmp;
        T               m_t_tmp_div;
        T               m_t;
        T               m_t_div;
        T               m_c;
        T               m_s;
        T               m_sqrt_temp;
        Complex<T>      m_A_fxp;
        Complex<T>      m_A_fxp_max;
        Complex<T>      m_V_fxp;
        T               m_s_o_fxp;
        double          m_S_rsqrt[8];
        T               m_S_rsqrt_fxp;
        T               m_S_sqrt_fxp;
        T               m_S_fxp;
        T               m_max_val;
        T               m_max_val_fxp;
        T               test_fxp;
        T               m_mult_0;
        T               m_mult_1;
        T               m_mult_2;
        T               m_mult_3;
        T               m_mult_4;
        T               m_max_val_frac;
        T               m_xiu_sqrt_fxp;
        T               m_t_sqrt_fxp;
        //T               m_sqrt_in_fxp;
        T               m_sqrt_q_in_fxp;
        Complex<T>      m_xiu_norm_div;
        T               m_sqrt_temp_in_fxp;
        T               m_sqrt_q_temp_in_fxp;
        T               m_t_sqrt_in_fxp;
        T               m_sqrt_t_in_fxp;
        T               m_mantissaResult_fxp;
        T               m_q_max_val;
        T               m_exp_max_val;
        T               m_A_power_fxp;
        double          m_S_sqrt[8];
        Complex<T>      m_c_xiu_norm;
        Complex<T>      m_s_xiu_norm;
        double          m_xiu_power_in;
        double          **m_S_o;
        Complex<T>      m_xiu_norm;
        Complex<T>      m_xiu;
        Complex<T>      m_xiu_temp_fxp;
        Complex<T>      m_U_fxp;
        Complex<T>      m_U_div_fxp;
        Complex<T>      m_in_fxp_tmp;
        T               m_xiu_rsqrt;
        int             m_matrix_size;
        int             m_iter_num;
        int             m_svd_time;
        int             m_pos[8];
        void            input(Complex<double> **in);
        void            read_matrix(int times);
        void            write_matrix(int times);
        double          max(Complex<double> **in);
        double          rsqrt(double in);
        void            svd_cal();
        void            s_cal();
        void            u_cal();
        void            output();
        void            dump_matlab();
        void            col_rot(Complex<double> *in_1, Complex<double> *in_2, Complex<double> *in_3, Complex<double> *in_4);
        void            quicksort(double a[8], int first,int end);
        void            index_get(double a[8], double b[8], int pos[8]);
        double          divide(double a, double b);
        double          sqrt_q(double in);
        double          sqrt_q_temp(double in);
        double          sqrt_q_t(double in_temp);
        void            dump_input();
        void            dump_col_rot_in(Complex<double> *A_1, Complex<double> *A_2, Complex<double> *V_1, Complex<double> *V_2);
        void            dump_A_in(ofstream &file, int ck1);
        double          sqrt_bit(double in);
        //Float_struct    mult(Float_struct in_1, Float_struct in_2);
        //Float_struct    set_float(double in);
        //double          get_float(Float_struct in);
        //Float_struct    minus_float(Float_struct in_1, Float_struct in_2);
        Cls_sqrt<T>     m_sqrt;
        SQRT_CFG        m_sqrt_cfg;
#ifdef DUMP_MATLAB_SVD
        ofstream        file_mat_U;
        ofstream        file_mat_S;
        ofstream        file_mat_V;
        ofstream        file_mat_A;
        ofstream        file_mat_m_s;
        ofstream        file_mat_V_tmp;
#endif
#ifdef DUMP_RTL_SVD
        ofstream        file_rtl_in_1;
        ofstream        file_rtl_in_2;
        ofstream        file_rtl_in_3;
        ofstream        file_rtl_in_4;
        ofstream        file_rtl_in_5;
        ofstream        file_rtl_in_6;
        ofstream        file_rtl_in_7;
        ofstream        file_rtl_in_8;
        ofstream        file_rtl_A_1_in;
        ofstream        file_rtl_A_2_in;
        ofstream        file_rtl_V_1_in;
        ofstream        file_rtl_V_2_in;
        ofstream        file_rtl_A_1_out;
        ofstream        file_rtl_A_2_out;
        ofstream        file_rtl_V_1_out;
        ofstream        file_rtl_V_2_out;
        ofstream        file_rtl_c;
        ofstream        file_rtl_s;
        ofstream        file_rtl_xiu_tmp;
        ofstream        file_rtl_G;
        ofstream        file_rtl_V;
        ofstream        file_rtl_in_tmp;
        ofstream        file_rtl_max_frac;
        ofstream        file_rtl_max;
        ofstream        file_rtl_max_frac_q;
        ofstream        file_rtl_max_frac_exp;
		ofstream		f_in, f_max_val, f_dbg, f_s_fxp, f_xiu, f_a_col, f_v_col, f_s_o, f_u_o, f_v_o;
#endif
};

template <class T>
void Cls_svd<T>::configure(SVD_CFG cfg)
{

}

template <class T>
void Cls_svd<T>::init(char* module_name)
{
    m_in = safe_new(m_in, 8, 8);
    m_A = safe_new(m_A, 8, 8);
    m_U = safe_new(m_U, 8, 8);
    m_V = safe_new(m_V, 8, 8);
    m_S = safe_new(m_S, 8, 8);
    m_A_1_1 = safe_new(m_A_1_1, 8);
    m_A_1_2 = safe_new(m_A_1_2, 8);
    m_A_2_1 = safe_new(m_A_2_1, 8);
    m_A_2_2 = safe_new(m_A_2_2, 8);
    m_A_3_1 = safe_new(m_A_3_1, 8);
    m_A_3_2 = safe_new(m_A_3_2, 8);
    m_A_4_1 = safe_new(m_A_4_1, 8);
    m_A_4_2 = safe_new(m_A_4_2, 8);
    m_V_1_1 = safe_new(m_V_1_1, 8);
    m_V_1_2 = safe_new(m_V_1_2, 8);
    m_V_2_1 = safe_new(m_V_2_1, 8);
    m_V_2_2 = safe_new(m_V_2_2, 8);
    m_V_3_1 = safe_new(m_V_3_1, 8);
    m_V_3_2 = safe_new(m_V_3_2, 8);
    m_V_4_1 = safe_new(m_V_4_1, 8);
    m_V_4_2 = safe_new(m_V_4_2, 8);
    m_A_1 = safe_new(m_A_1, 8);
    m_A_2 = safe_new(m_A_2, 8);
    m_V_1 = safe_new(m_V_1, 8);
    m_V_2 = safe_new(m_V_2, 8);
    m_S_o = safe_new(m_S_o, 8, 8);

    fix_set(m_in_fxp, 18, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_in_fxp, "m_in_fxp", 1, 100);
    fix_set(m_xiu, 36, 4, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_xiu, "m_xiu", 1, 1);//40.4
    //fix_set(m_xiu_power_in, 62, 5, FXP_US, FXP_RND, FXP_SAT_SYM);statistic_set(m_xiu_power_in, "m_xiu_power_in", 1, 1);
    fix_set(m_xiu_temp_fxp, 36, 2, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_xiu_temp_fxp, "m_xiu_temp_fxp", 1, 1);
    //fix_set(m_sqrt_in_fxp , 50, 5, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sqrt_in_fxp, "m_sqrt_in_fxp", 1, 1);
    fix_set(m_xiu_sqrt_fxp, 36, 4, FXP_US, FXP_TRN, FXP_SAT_SYM);//statistic_set(m_xiu_sqrt_fxp, "m_xiu_sqrt_fxp", 1, 1);
    fix_set(m_sqrt_q_in_fxp, 32, 0, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sqrt_q_in_fxp, "m_sqrt_q_in_fxp", 1, 1);
    fix_set(m_t_sqrt_fxp, 12, 2, FXP_US, FXP_TRN, FXP_SAT_SYM);//statistic_set(m_t_sqrt_fxp, "m_t_sqrt_fxp", 1, 1);
    //fix_set(m_S_fxp, 60, 4, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_S_fxp, "m_S_fxp", 1, 1);
    fix_set(m_S_sqrt_fxp, 17, 3, FXP_US, FXP_TRN, FXP_SAT_SYM);//statistic_set(m_S_sqrt_fxp, "m_S_sqrt_fxp", 1, 1);
    fix_set(m_S_rsqrt_fxp, 40, 15, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_S_rsqrt_fxp, "m_S_rsqrt_fxp", 1, 1);

    fix_set(m_wk, 27, 4, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(m_wl, 27, 4, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(m_in_fxp_tmp, 36, 26, FXP_TC, FXP_RND, FXP_SAT_SYM); //38.26 40.29 statistic_set(m_in_fxp_tmp, "m_in_fxp_tmp", 1, 1);38.26
    fix_set(m_q_max_val, 20, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_exp_max_val, 7, 7, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_max_val_frac, 28, 1, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM);//28.1statistic_set(m_max_val_frac, "m_max_val_frac", 1, 1);
    fix_set(m_max_val, 36, 26, FXP_US, FXP_RND, FXP_SAT_SYM);//36.26

    fix_set(m_sqrt_q_temp_in_fxp, 32, 18, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_xiu, "m_xiu", 1, 100);
    fix_set(m_xiu_rsqrt, 46, 32, FXP_US, FXP_RND, FXP_SAT_SYM); //statistic_set(m_xiu_rsqrt, "m_xiu_rsqrt", 1, 1);
    //fix_set(m_sqrt_t_in_fxp, 32, 6, FXP_US, FXP_RND, FXP_SAT_SYM); //statistic_set(m_xiu_rsqrt, "m_xiu_rsqrt", 1, 100);
    fix_set(m_xiu_norm, 20, 2, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(m_xiu_norm, "m_xiu_norm", 1, 1);
    fix_set(m_xiu_norm_div, 19, 1, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM); //statistic_set(m_xiu_norm, "m_xiu_norm", 1, 1);
    fix_set(m_c_xiu_norm, 20, 2, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(m_c_xiu_norm, "m_c_xiu_norm", 1, 1);
    fix_set(m_QKL, 28, 5, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(m_QKL, "m_QKL", 1, 1);
    fix_set(m_t_tmp_div, 33, 15, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM);//statistic_set(m_t_tmp, "m_t_tmp", 1, 1);
    fix_set(m_t_tmp, 34, 16, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_t_tmp, "m_t_tmp", 1, 1);
    fix_set(m_sqrt_temp_in_fxp, 46, 32, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sqrt_temp_in_fxp, "m_sqrt_temp_in_fxp", 1, 1);
    fix_set(m_sqrt_temp, 23, 16, FXP_US, FXP_TRN, FXP_SAT_SYM); //statistic_set(m_sqrt_temp, "m_sqrt_temp", 1, 1);
    fix_set(m_t, 28, 2, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_t, "m_t", 1, 100);
    fix_set(m_t_div, 27, 1, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM);//statistic_set(m_t, "m_t", 1, 100);

    fix_set(m_t_sqrt_in_fxp, 24, 4, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_t_sqrt_in_fxp, "m_t_sqrt_in_fxp", 1, 1);
    fix_set(m_c, 16, 1, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM);//statistic_set(m_c, "m_c", 1, 1);
    fix_set(m_s, 28, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_s, "m_s", 1, 1);
    fix_set(m_s_xiu_norm, 30, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_s_xiu_norm, "m_s_xiu_norm", 1, 1);
    fix_set(m_A_fxp, 32, 2, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_A_fxp, "m_A_fxp", 1, 2);
    fix_set(m_A_fxp_max, 32, 2, FXP_TC, FXP_RND, FXP_SAT_SYM);//32.1statistic_set(m_A_fxp, "m_A_fxp", 1, 2);
    fix_set(m_A_power_fxp, 37, 4, FXP_US, FXP_RND, FXP_SAT_SYM);//37.1statistic_set(m_A_power_fxp, "m_A_power_fxp", 1, 50);

    fix_set(m_V_fxp, 23, 4, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_V_fxp, "m_V_fxp", 1, 2);
    //fix_set(m_S_rsqrt, 21, 3, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_S_rsqrt, "m_S_rsqrt", 1, 1);
    fix_set(m_s_o_fxp, 36, 28, FXP_US, FXP_RND, FXP_SAT_SYM);//38.30statistic_set(m_s_o_fxp, "m_s_o_fxp", 1, 1);
    fix_set(m_U_fxp, 17, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_A_fxp, "m_A_fxp", 1, 1);
    fix_set(m_U_div_fxp, 16, 0, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM);//statistic_set(m_A_fxp, "m_A_fxp", 1, 1);

    fix_set(m_S_fxp, 34, 6, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_S_fxp, "m_S_fxp", 1, 1);

    fix_set(m_mantissaResult_fxp, 20, 1, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_A_fxp, "m_A_fxp", 1, 1);
    fix_set(m_max_val_fxp, 36, 26, FXP_US, FXP_RND, FXP_SAT_SYM);//36.26statistic_set(m_A_fxp, "m_A_fxp", 1, 1);36 26

    fix_set(test_fxp, 20, 5, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_A_fxp, "m_A_fxp", 1, 1);
    fix_set(m_mult_0, 47, 33, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_mult_0, "m_mult_0", 1, 1);
    fix_set(m_mult_1, 40, 3, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_mult_1, "m_mult_1", 1, 1);
    fix_set(m_mult_2, 47, 33, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_mult_2, "m_mult_2", 1, 1);
    fix_set(m_mult_3, 47, 33, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_mult_3, "m_mult_3", 1, 1);
    fix_set(m_mult_4, 47, 33, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_mult_4, "m_mult_4", 1, 1);

    //mkdir Folder
#ifdef DUMP_MATLAB_SVD
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
    m_sqrt_cfg.bit_len = 22;//22
    m_sqrt.configure(m_sqrt_cfg);
    m_sqrt.init(module_name);
#ifdef DUMP_RTL_SVD
//    struct stat statExist;
//    int statMd = 0;
//    if (stat(module_name, &statExist) != 0) {
//            statMd = mkdir(module_name, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
//            if (statMd)
//                LOG2DEV << "Cannot create folder.\n";
//    }
    char filename_rtl[300] = {};
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "f_in.dat");            f_in.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "f_max_val.dat");       f_max_val.open(filename_rtl);

    strcpy(filename_rtl, module_name); strcat(filename_rtl, "f_dbg_svd.dat");           f_dbg.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "f_s_fxp.dat");         f_s_fxp.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "f_xiu.dat");           f_xiu.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "f_a_col.dat");         f_a_col.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "f_v_col.dat");         f_v_col.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "f_s_o.dat");           f_s_o.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "f_u_o.dat");           f_u_o.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "f_v_o.dat");           f_v_o.open(filename_rtl);
    /*
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
    */
#endif

	reset();
}

template <class T>
void Cls_svd<T>::reset()
{
    ones_val(m_in, 8, 8, 1e-10);
    zeros(m_V, 8, 8);
    eye(m_V, 8);
    m_iter_num = 10;//3
    m_svd_time = 7;

    zeros(m_A_1_1, 8);
    zeros(m_A_1_2, 8);
    zeros(m_A_2_1, 8);
    zeros(m_A_2_2, 8);
    zeros(m_A_3_1, 8);
    zeros(m_A_3_2, 8);
    zeros(m_A_4_1, 8);
    zeros(m_A_4_2, 8);
    zeros(m_V_1_1, 8);
    zeros(m_V_1_2, 8);
    zeros(m_V_2_1, 8);
    zeros(m_V_2_2, 8);
    zeros(m_V_3_1, 8);
    zeros(m_V_3_2, 8);
    zeros(m_V_4_1, 8);
    zeros(m_V_4_2, 8);
}

template <class T>
void Cls_svd<T>::go(Complex<double> **in)
{
    //Input
    input(in);

    // SVD CAL
    svd_cal();

    // S Cal
    s_cal();

    // U cal
    u_cal();

    // output
    output();

#ifdef DUMP_MATLAB_SVD
    // DUMP Matlab
    dump_matlab();
#endif
}

template <class T>
void Cls_svd<T>::dump_matlab()
{
#ifdef DUMP_MATLAB_SVD
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            file_mat_U << std::fixed<<setprecision(32) << m_U[ck0][ck1].I << "\t" << m_U[ck0][ck1].Q << endl;
            file_mat_V << std::fixed<<setprecision(32) << m_V[ck0][ck1].I << "\t" << m_V[ck0][ck1].Q << endl;
            file_mat_S << std::fixed<<setprecision(32) << m_S[ck0][ck1] << endl;
        }
    }
#endif
        /*        for (int ck2 = 0; ck2 < 8; ck2++)
            {
                for (int ck3 = 0; ck3 < 8; ck3++)
                {
                    LOG2DEV << m_V[ck2][ck3].I << " + " << m_V[ck2][ck3].Q << "\t";
                }
                LOG2DEV << endl;
            }*/
}

template <class T>
double Cls_svd<T>::max(Complex<double> **in)
{
    double max_val = DOUBLE_POW2[64 - 10];
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            if(abs(in[ck0][ck1].I) > max_val)
                max_val = abs(in[ck0][ck1].I);
            if(abs(in[ck0][ck1].Q) > max_val)
                max_val = abs(in[ck0][ck1].Q);
            //LOG2DEV << in[ck0][ck1].I << endl;
        }
    }
    return max_val;
}

template <class T>
void Cls_svd<T>::input(Complex<double> **in)
{
    mem_copy(m_in, in, 8, 0, 8);
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            m_in_fxp_tmp = m_in[ck0][ck1];
            m_in[ck0][ck1] = m_in_fxp_tmp;
        }
        //LOG2DEV  << m_in[ck0][0].I << endl;
    }

    m_max_val = max(m_in); //28.15
    m_max_val_fxp = m_max_val; //24.15->20.15

    m_max_val_frac = 1.0 / m_max_val;

    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            m_A_fxp_max = m_in[ck0][ck1] * m_max_val_frac;
            m_A[ck0][ck1] = m_A_fxp_max;
        }
        //LOG2DEV  << m_A[ck0][0].I << endl;
    }

    //Dump for RTL
#ifdef DUMP_RTL_SVD
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            dump(f_in, &m_in[ck0][ck1],  1, m_in_fxp_tmp.I);
        }
    }
    dump(f_max_val, &m_max_val_frac,  1, m_max_val_frac);
#endif

}

template <class T>
void Cls_svd<T>::output()
{
    Complex<double> **m_V_tmp;
    m_V_tmp = NULL;
    m_V_tmp = safe_new(m_V_tmp, 8, 8);
    mem_copy(m_S, m_S_o, 8, 0, 8);
    mem_copy(m_V_tmp, m_V, 8, 0, 8);
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            m_V[ck0][ck1] = m_V_tmp[ck0][m_pos[ck1]];
            //LOG2DEV << m_V[ck0][ck1].I << " " <<m_V[ck0][ck1].Q << endl;
            //LOG2DEV << ck0 << " " << ck1 << endl;
        }
    }

#ifdef DUMP_RTL_SVD
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            dump(f_v_o, &m_V[ck0][ck1], 1, m_V_fxp.I);
        }
    }
#endif
    free(m_V_tmp);
}

template <class T>
void Cls_svd<T>::read_matrix(int times)
{
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        m_A_1_1[ck0] = m_A[ck0][index_svd_p[times][0]];
        m_A_1_2[ck0] = m_A[ck0][index_svd_p[times][1]];
        m_A_2_1[ck0] = m_A[ck0][index_svd_p[times][2]];
        m_A_2_2[ck0] = m_A[ck0][index_svd_p[times][3]];
        m_A_3_1[ck0] = m_A[ck0][index_svd_p[times][4]];
        m_A_3_2[ck0] = m_A[ck0][index_svd_p[times][5]];
        m_A_4_1[ck0] = m_A[ck0][index_svd_p[times][6]];
        m_A_4_2[ck0] = m_A[ck0][index_svd_p[times][7]];
        m_V_1_1[ck0] = m_V[ck0][index_svd_p[times][0]];
        m_V_1_2[ck0] = m_V[ck0][index_svd_p[times][1]];
        m_V_2_1[ck0] = m_V[ck0][index_svd_p[times][2]];
        m_V_2_2[ck0] = m_V[ck0][index_svd_p[times][3]];
        m_V_3_1[ck0] = m_V[ck0][index_svd_p[times][4]];
        m_V_3_2[ck0] = m_V[ck0][index_svd_p[times][5]];
        m_V_4_1[ck0] = m_V[ck0][index_svd_p[times][6]];
        m_V_4_2[ck0] = m_V[ck0][index_svd_p[times][7]];
        //LOG2DEV << m_A_1_1[ck0].I << " " <<m_A_1_1[ck0].Q << endl;
        //LOG2DEV << m_V_1_1[ck0].I << " " << m_V_1_1[ck0].Q << endl;
    }
}

template <class T>
void Cls_svd<T>::write_matrix(int times)
{
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        m_A[ck0][index_svd_p[times][0]] = m_A_1_1[ck0];
        m_A[ck0][index_svd_p[times][1]] = m_A_1_2[ck0];
        m_A[ck0][index_svd_p[times][2]] = m_A_2_1[ck0];
        m_A[ck0][index_svd_p[times][3]] = m_A_2_2[ck0];
        m_A[ck0][index_svd_p[times][4]] = m_A_3_1[ck0];
        m_A[ck0][index_svd_p[times][5]] = m_A_3_2[ck0];
        m_A[ck0][index_svd_p[times][6]] = m_A_4_1[ck0];
        m_A[ck0][index_svd_p[times][7]] = m_A_4_2[ck0];
        m_V[ck0][index_svd_p[times][0]] = m_V_1_1[ck0];
        m_V[ck0][index_svd_p[times][1]] = m_V_1_2[ck0];
        m_V[ck0][index_svd_p[times][2]] = m_V_2_1[ck0];
        m_V[ck0][index_svd_p[times][3]] = m_V_2_2[ck0];
        m_V[ck0][index_svd_p[times][4]] = m_V_3_1[ck0];
        m_V[ck0][index_svd_p[times][5]] = m_V_3_2[ck0];
        m_V[ck0][index_svd_p[times][6]] = m_V_4_1[ck0];
        m_V[ck0][index_svd_p[times][7]] = m_V_4_2[ck0];
        //LOG2DEV << m_A_1_1[ck0].I << " " <<m_A_1_1[ck0].Q << endl;
        //LOG2DEV << m_V_1_1[ck0].I << " " << m_V_1_1[ck0].Q << endl;
    }
}

template <class T>
void Cls_svd<T>::s_cal()
{
    double s_rsqrt[8];
    double S_rsqrt_d[8];
    double s_temp[8];

    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        m_S_rsqrt[ck0] = 0;
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            m_A_power_fxp = m_A[ck1][ck0].power();
            m_S_rsqrt[ck0] = m_S_rsqrt[ck0] + m_A_power_fxp;
            //LOG2DEV  << m_S_rsqrt[ck0] << endl;
        }
        m_S_fxp = m_S_rsqrt[ck0];

        m_S_sqrt_fxp = m_sqrt.sqrt(m_S_fxp);

        if(m_S_sqrt_fxp == 0)
            m_S_sqrt_fxp = 8 * DOUBLE_POW2[64 - 16];//1 * DOUBLE_POW2[64 - 20];
        m_S_rsqrt_fxp = 1.0 / m_S_sqrt_fxp;
        //LOG2DEV  << m_S_rsqrt_fxp << endl;
        m_S_rsqrt[ck0] = m_S_rsqrt_fxp;
        m_S_sqrt[ck0] = m_S_sqrt_fxp;
#ifdef DUMP_RTL_SVD
        dump(f_s_fxp, &m_S_fxp, 1, m_S_fxp, 0);
        dump(f_s_fxp, &m_S_sqrt_fxp, 1, m_S_sqrt_fxp, 0);
        dump(f_s_fxp, &m_S_rsqrt_fxp, 1, m_S_rsqrt_fxp);
#endif
    }
    mem_copy(s_rsqrt, m_S_sqrt, 8);
    mem_copy(S_rsqrt_d, m_S_sqrt, 8);
    quicksort(S_rsqrt_d, 0, 7);
    for (int ck0 = 0; ck0 < 8; ck0++) {
        s_temp[ck0] = S_rsqrt_d[7 - ck0];
    }
    mem_copy(S_rsqrt_d, s_temp, 8);
    index_get(s_rsqrt, S_rsqrt_d, m_pos);
    mem_copy(m_S_sqrt, S_rsqrt_d, 8);
    zeros(m_S_o, 8, 8);
    //LOG2DEV << m_max_val << endl;
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        m_S_sqrt_fxp = m_S_sqrt[ck0];
        m_s_o_fxp = m_S_sqrt_fxp * m_max_val;
        //LOG2DEV << m_s_o_fxp << endl;
        m_S_o[ck0][ck0] = m_s_o_fxp;
#ifdef DUMP_RTL_SVD
        dump(f_s_o, &m_S_o[ck0][ck0], 1, m_s_o_fxp);
#endif
    }
#ifdef DUMP_RTL_SVD
        file_rtl_G << endl;
#endif
}

template <class T>
void Cls_svd<T>::index_get(double a[8], double b[8], int pos[8])
{
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            if(b[ck0] == a[ck1])
            {
                pos[ck0] = ck1;
                a[ck1] = -1;
                break;
            }
        }
    }
}

template <class T>
void Cls_svd<T>::u_cal()
{
    Complex<double> temp ;
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            m_U_div_fxp.I = fabs(m_A[ck0][m_pos[ck1]].I) / m_S_sqrt[ck1];
            m_U_div_fxp.Q = fabs(m_A[ck0][m_pos[ck1]].Q) / m_S_sqrt[ck1];
            m_U_fxp.I = (double)m_U_div_fxp.I * sign(m_A[ck0][m_pos[ck1]].I);
            m_U_fxp.Q = (double)m_U_div_fxp.Q * sign(m_A[ck0][m_pos[ck1]].Q);
            m_U[ck0][ck1] = m_U_fxp;
#ifdef DUMP_RTL_SVD
        dump(f_dbg, &m_A[ck0][m_pos[ck1]], 1, m_A_fxp.I);
#endif
//	    dump(f_dbg, &m_S_sqrt[ck1], 1, m_S_sqrt_fxp);
//	    dump(f_dbg, &m_U_div_fxp, 1, m_U_div_fxp.I);
        }
    }

#ifdef DUMP_RTL_SVD
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            dump(f_u_o, &m_U[ck0][ck1], 1, m_U_fxp.I);
        }
    }
#endif
}

template <class T>
void Cls_svd<T>::svd_cal()
{
    for (int ck0 = 0; ck0 < m_iter_num; ck0++)
    {
        for(int ck1 = 0; ck1 < m_svd_time; ck1++)
        {
            //LOG2DEV << ck0 << "\t" << ck1 << endl;
            //if(ck0 == 2 && ck1 == 6)
            //    int a = 1;
            read_matrix(ck1);
            col_rot(m_A_1_1, m_A_1_2, m_V_1_1, m_V_1_2);
            col_rot(m_A_2_1, m_A_2_2, m_V_2_1, m_V_2_2);
            col_rot(m_A_3_1, m_A_3_2, m_V_3_1, m_V_3_2);
            col_rot(m_A_4_1, m_A_4_2, m_V_4_1, m_V_4_2);
            write_matrix(ck1);
            //LOG2DEV<<ck0<<"\t"<<ck1<<endl;
#ifdef DUMP_MATLAB_SVD
        for (int ck2 = 0; ck2 < 8; ck2++)
        {
            for (int ck3 = 0; ck3 < 8; ck3++)
            {
                file_mat_A << std::fixed<<setprecision(32) << m_A[ck2][ck3].I << " \t " << m_A[ck2][ck3].Q << endl;
                file_mat_V_tmp << std::fixed<<setprecision(32) << m_V[ck2][ck3].I << " \t " << m_V[ck2][ck3].Q << endl;
            }
        }
#endif
        }
        //fix_set(m_xiu, 32, 3 - ck0 * 4, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_xiu, "m_xiu", 1, 1);
#ifdef DUMP_MATLAB_SVD_log
        for (int ck2 = 0; ck2 < 8; ck2++)
            {
                for (int ck3 = 0; ck3 < 8; ck3++)
                {
                    LOG2DEV << m_A[ck2][ck3].I << " + " << m_A[ck2][ck3].Q << "\t";
                }
                LOG2DEV << endl;
            }
#endif
    }
}

template <class T>
void Cls_svd<T>::col_rot(Complex<double> *A_1, Complex<double> *A_2, Complex<double> *V_1, Complex<double> *V_2)
{
    m_wk = 0;
    m_wl = 0;
    m_xiu = 0;
    Complex<double> v_tmp_1, v_tmp_2;
    double wk_tmp, wl_tmp;
    //double I,Q;
    wk_tmp = 0; wl_tmp = 0;
    Complex<double> temp_1, temp_2, temp_3, xiu_temp;
    Complex<double> temp_A, temp_B;
    xiu_temp = 0;

#ifdef DUMP_RTL_SVD
    dump_col_rot_in(A_1, A_2, V_1, V_2);
#endif
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        m_A_power_fxp = A_1[ck0].power();
        wk_tmp = wk_tmp + m_A_power_fxp;
        m_A_power_fxp = A_2[ck0].power();
        wl_tmp = wl_tmp + m_A_power_fxp;
        temp_1 = A_1[ck0].conjug();
        temp_2 = A_2[ck0];
        temp_3 = temp_1 * temp_2;
        m_xiu_temp_fxp = A_1[ck0].conjug() * A_2[ck0];
        xiu_temp = xiu_temp + m_xiu_temp_fxp.todouble();
    }
    m_wk = wk_tmp;
    m_wl = wl_tmp;
    m_xiu = xiu_temp;
    //LOG2DEV << m_xiu.I << "\t" <<m_xiu.Q<<endl;
    //LOG2DEV<<m_wk<<endl;

    //m_sqrt_in_fxp = m_xiu.power();
    //xiu_temp = m_xiu;
    //if(m_xiu.power() == 0) {
    //    m_xiu_rsqrt = DOUBLE_POW2[64 + 32];
    //} else {

    //    m_xiu_rsqrt = 1.0 / m_xiu_sqrt_fxp;//sqrt(m_xiu.power());//rsqrt(m_xiu.power());
    //}

    //if(m_xiu.power() < DOUBLE_POW2[64-48])
        //int a = 1;
    //LOG2DEV << m_xiu.power() << "\t" << 1 / sqrt(m_xiu.power()) << "\t" << rsqrt(m_xiu.power()) << endl;

    //m_xiu_sqrt_fxp = sqrt(m_xiu.power());//sqrt(m_xiu.power());//sqrt(m_sqrt_in_fxp);
    //if(m_xiu_sqrt_fxp == 0)
        //m_xiu_sqrt_fxp = 1 * DOUBLE_POW2[64 - 16];
    //m_xiu_rsqrt = 1.0 / m_xiu_sqrt_fxp;//m_xiu.rsqrt();
    //LOG2DEV << m_xiu_rsqrt << endl;
    //m_xiu_norm = m_xiu / m_xiu_sqrt_fxp;// m_xiu_sqrt_fxp;
    //LOG2DEV << m_xiu_norm.I << "\t"<<m_xiu_norm.Q << endl;
    //m_xiu_norm = m_xiu * 1.0 / m_xiu_sqrt_fxp;
    //m_xiu_norm = m_xiu * m_xiu_rsqrt;
    //LOG2DEV << m_xiu_norm.I << "\t"<<m_xiu_norm.Q << endl;
    m_xiu_power_in = 0.0;
    m_xiu_power_in = m_xiu.power();
    double u0;
    u0 = (m_xiu.I * m_xiu.I + m_xiu.Q * m_xiu.Q);
    /*m_xiu_power_in = u0;
    if(m_xiu.power() == 0){
        m_xiu_sqrt_fxp = DOUBLE_POW2[64 + 32];
    }else{
        if(m_xiu_power_in = 0x8000000000000){
            m_xiu_power_in = 1;
        }
        m_xiu_sqrt_fxp = m_sqrt.sqrt(m_xiu_power_in);
    }*/
    /*m_xiu_power_in = m_xiu.power();
    m_xiu_sqrt_fxp = m_sqrt.sqrt(m_xiu_power_in);*/

    m_xiu_sqrt_fxp = m_sqrt.sqrt(u0);
    m_QKL = m_wk - m_wl;

    if (abs(m_QKL) < 16 * DOUBLE_POW2[64 - 24])
        m_QKL = 16 * DOUBLE_POW2[64 - 24];

    //LOG2DEV << m_QKL << endl ;
    //if (abs(m_xiu_sqrt_fxp) < 1e-6)
        //m_xiu_sqrt_fxp = 1e-6;

    //m_t_tmp = m_QKL * m_xiu_rsqrt ;
    //m_t_tmp = m_QKL * m_QKL / m_xiu.power();//divide(m_QKL, );
    if(m_xiu_sqrt_fxp != 0) {
        m_xiu_norm_div.I = fabs(m_xiu.I) / m_xiu_sqrt_fxp;
        m_xiu_norm_div.Q = fabs(m_xiu.Q) / m_xiu_sqrt_fxp;
        m_xiu_norm.I = (double)m_xiu_norm_div.I * sign((double)m_xiu.I);
        m_xiu_norm.Q = (double)m_xiu_norm_div.Q * sign((double)m_xiu.Q);
        m_t_tmp_div = fabs(m_QKL) / m_xiu_sqrt_fxp;// m_xiu_sqrt_fxp;
        m_t_tmp = sign(m_QKL) * m_t_tmp_div;
    } else {
        m_xiu_norm = 0;//m_xiu * DOUBLE_POW2[64 - 32];
        m_t_tmp  = 0;//m_QKL * DOUBLE_POW2[64 - 32];
        //m_xiu_norm = DOUBLE_POW2[64 - 18];
        //m_t_tmp  = DOUBLE_POW2[64 - 18];
        //LOG2DEV << "error" << endl ;
    }
    //if(sign(m_t_tmp) != sign(m_QKL))
    //    LOG2DEV << "sign error" << endl;
    m_sqrt_temp_in_fxp = 1.0 * m_t_tmp * m_t_tmp + 4;
    //m_sqrt_temp = sqrt(m_sqrt_temp_in_fxp);

    //LOG2DEV << m_t_tmp << endl;
   // m_sqrt_temp_in_fxp = m_t_tmp * m_t_tmp + 4;
    m_sqrt_temp = m_sqrt.sqrt(m_sqrt_temp_in_fxp);//sqrt(m_sqrt_temp_in_fxp);//1.0 / rsqrt(m_t_tmp * m_t_tmp + 4);
    //LOG2DEV << m_sqrt_temp << endl;
    m_t_div = 2.0 / fabs((m_t_tmp + (double)sign(m_QKL) * m_sqrt_temp));
    m_t = (double)m_t_div * sign((m_t_tmp + (double)sign(m_QKL) * m_sqrt_temp));
    //LOG2DEV << m_t << endl;
    m_t_sqrt_in_fxp = m_t * m_t + 1;
    m_t_sqrt_fxp = m_sqrt.sqrt(m_t_sqrt_in_fxp);//sqrt(m_t_sqrt_in_fxp);//
    //LOG2DEV << sqrt(m_t_sqrt_in_fxp) << "\t" << m_sqrt.sqrt(m_t_sqrt_in_fxp)<<endl;
    //LOG2DEV << m_t_sqrt_in_fxp << "\t" << m_t_sqrt_fxp << endl;
    m_c = 1.0 / m_t_sqrt_fxp;//rsqrt(m_t * m_t + 1);
    m_s = m_t * m_c;
    //file_mat_m_s << std::fixed<<setprecision(32) << m_s  << endl;
    //LOG2DEV << std::fixed<<setprecision(32) << m_c  << endl;
    //LOG2DEV << std::fixed<<setprecision(32) << m_s  << endl;
    m_c_xiu_norm = m_c * m_xiu_norm.todouble();
    m_s_xiu_norm = m_s * m_xiu_norm.todouble();

#ifdef DUMP_RTL_SVD
//	dump(f_dbg, &m_QKL, 1, m_QKL);
//	dump(f_dbg, &m_xiu_sqrt_fxp, 1, m_xiu_sqrt_fxp);
//	dump(f_dbg, &m_t_tmp_div, 1, m_t_tmp_div);
//	dump(f_dbg, &m_t_tmp, 1, m_t_tmp);
//	dump(f_dbg, &m_sqrt_temp, 1, m_sqrt_temp);
//        dump(f_dbg, &m_t, 1, m_t);
//	dump(f_dbg, &m_c, 1, m_c);
//	dump(f_dbg, &m_s, 1, m_s);
//	dump(f_dbg, &m_xiu_norm, 1, m_xiu_norm.I);
//        dump(f_xiu, &m_wk, 1, m_wk, 0);
//	dump(f_xiu, &m_wl, 1, m_wl, 0);
//	dump(f_xiu, &m_xiu, 1, m_xiu.I, 0);
//	dump(f_xiu, &m_xiu_sqrt_fxp, 1, m_xiu_sqrt_fxp, 0);
//	dump(f_xiu, &m_xiu_norm, 1, m_xiu_norm.I, 0);
//	dump(f_xiu, &m_t_tmp, 1, m_t_tmp, 0);
//	dump(f_xiu, &m_sqrt_temp, 1, m_sqrt_temp, 0);
//	dump(f_xiu, &m_t, 1, m_t, 0);
//	dump(f_xiu, &m_c, 1, m_c, 0);
//	dump(f_xiu, &m_s, 1, m_s);
	dump(f_xiu, &m_c_xiu_norm, 1, m_c_xiu_norm.I, 0);
	dump(f_xiu, &m_s_xiu_norm, 1, m_s_xiu_norm.I, 0);
	dump(f_xiu, &m_c, 1, m_c, 0);
	dump(f_xiu, &m_s, 1, m_s);
#endif


    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        m_A_fxp = m_c_xiu_norm.todouble() * A_1[ck0] ;  temp_A = m_A_fxp; m_A_fxp = m_s * A_2[ck0]; temp_B = m_A_fxp; m_A_fxp = temp_A +  temp_B;  m_A_1[ck0] = m_A_fxp; //LOG2DEV << m_A_fxp.I << "\t" << m_A_fxp.Q << endl;

#ifdef DUMP_RTL_SVD
        dump(f_a_col, &m_A_fxp, 1, m_A_fxp.I);
#endif

        m_A_fxp = m_s_xiu_norm.todouble() * A_1[ck0] ;  temp_A = m_A_fxp; m_A_fxp = m_c * A_2[ck0]; temp_B = m_A_fxp; m_A_fxp = temp_B -  temp_A;  m_A_2[ck0] = m_A_fxp; //LOG2DEV << m_A_fxp.I << "\t" << m_A_fxp.Q << endl;

#ifdef DUMP_RTL_SVD
        dump(f_a_col, &m_A_fxp, 1, m_A_fxp.I);
#endif

        /*m_V_fxp = m_c_xiu_norm.todouble() * V_1[ck0]; v_tmp_1 = m_V_fxp;
        m_V_fxp = + m_s * V_2[ck0];  v_tmp_2 = m_V_fxp;
        m_V_fxp = v_tmp_1 + v_tmp_2; m_V_1[ck0] = m_V_fxp;*/
        m_V_fxp = m_c_xiu_norm.todouble() * V_1[ck0];v_tmp_1 = m_V_fxp;
        m_V_fxp = m_s * V_2[ck0];  v_tmp_2 = m_V_fxp;
        m_V_fxp = v_tmp_1 + v_tmp_2; m_V_1[ck0] = m_V_fxp;
#ifdef DUMP_RTL_SVD
        dump(f_v_col, &m_V_fxp, 1, m_V_fxp.I);
#endif
        m_V_fxp = m_s_xiu_norm.todouble() * V_1[ck0]; v_tmp_1 = m_V_fxp;
        m_V_fxp = m_c * V_2[ck0]; v_tmp_2 = m_V_fxp;
        m_V_fxp = v_tmp_2 - v_tmp_1;    m_V_2[ck0] = m_V_fxp;
        //LOG2DEV<<m_A_1[ck0].I <<"\t"<<m_A_1[ck0].Q<< "\t" << m_A_2[ck0].I <<"\t"<<m_A_2[ck0].Q<< endl;
#ifdef DUMP_RTL_SVD
        dump(f_v_col, &m_V_fxp, 1, m_V_fxp.I);
#endif
    }
    mem_copy(A_1, m_A_1, 8);
    mem_copy(A_2, m_A_2, 8);
    mem_copy(V_1, m_V_1, 8);
    mem_copy(V_2, m_V_2, 8);
#ifdef DUMP_RTL_SVD
    dump(file_rtl_c, &m_c, 1, m_c);
    dump(file_rtl_s, &m_s, 1, m_s);
    dump(file_rtl_xiu_tmp, &m_xiu, 1, m_xiu.I);
    for (int ck0 = 0; ck0 < 8; ck0++) {
        dump(file_rtl_A_1_out, &A_1[ck0], 1, m_A_fxp.I);
        dump(file_rtl_A_2_out, &A_2[ck0], 1, m_A_fxp.I);
        dump(file_rtl_V_1_out, &V_1[ck0], 1, m_V_fxp.I);
        dump(file_rtl_V_2_out, &V_2[ck0], 1, m_V_fxp.I);
    }
#endif
    //LOG2DEV<<m_A_2[7].I<<endl;
}

template <class T>
void Cls_svd<T>::quicksort(double a[8], int first,int end)
{
    if(first > end)
    {
        return;
    }
    int i = first,j = end;
    double flag = a[i],exchange = 0;

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

template <class T>
double Cls_svd<T>::sqrt_bit(double in)
{
    int r = 0;
    int t = 0;
    int t_2 = 0;
    unsigned int m_in_bit_dec = 0;
    int m_in_bit[32] = {0};
    double root = 0;

    if(in == 0)
        return 0.0;
    if(in == 1)
        return 1.0;

    int exp;
    exp = ceil(log2(fabs(in)));

    if((exp % 2) != 0)
        exp = exp + 1;

    m_in_bit_dec = (unsigned int)(in * DOUBLE_POW2[64 - exp] * DOUBLE_POW2[64 + 32]);

    for (int ck0 = 31; ck0 >=0; ck0--) {
        m_in_bit[31 - ck0] = (m_in_bit_dec >> ck0) & 0x1;
    }

    for (int ck0 = 16; ck0 >= 1; ck0--) {
        t = t * 4 + m_in_bit[2*(16 - ck0)] * 2 + m_in_bit[2*(16 - ck0) + 1];
        t_2 = r * 4 + 1;
        r = r * 2;
        if(t >= t_2) {
            t = t - t_2;
            r = r + 1;
        }
    }
    root = r * 1.0 * DOUBLE_POW2[64 - 16] * DOUBLE_POW2[64 + (int)(exp / 2)];
    return root;
}

/*template <class T>
double Cls_svd<T>::rsqrt(double in)
{
double number;
    int i;
    float x2, y;
    //const float threehalfs = 1.5F;
    double out;
    int frac;
    int exp;
    int threehalfs_exp;
    double frac_d, threehalfs_frac;
    fxp frac_set;
    Float_struct y_float;
    Float_struct threehalfs_float;
    Float_struct y_threehalfs_float;
    Float_struct y_tmp_float;
    Float_struct x2_float;

    if(in == 0)
        LOG2DEV << "Error" << endl;

    number = in;
    x2 = number * 0.5F;
    y  = number;
    exp = floor(log2(y));
    frac = (int)(round((y * pow(2, -1 * exp) - 1) * DOUBLE_POW2[64 + 23]));
    //frac = (int)(round(y * pow(2, -1 * exp) - 1) * DOUBLE_POW2[64 + 23]);
    i = ((127 + exp) << 23) + frac;
    i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
    frac_d = i & 0x7FFFFF;
    exp = ((i >> 23) & 0xff) - 127;
    y = (frac_d * DOUBLE_POW2[64 - 23] + 1) * pow(2, exp);

    // Fractional S16.1 Exptional S7.7
    fix_set(frac_set, 23, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    frac_set = 0.75;
    threehalfs_frac = frac_set;
    threehalfs_exp = 1;
    threehalfs_float.exp = threehalfs_exp;
    threehalfs_float.frac = threehalfs_frac;

    y_float = set_float(y);

    x2_float = set_float(x2);

    y_threehalfs_float = mult(y_float, threehalfs_float);
    y_tmp_float = mult(x2_float, y_float);
    y_tmp_float = mult(y_tmp_float, y_float);
    y_tmp_float = mult(y_tmp_float, y_float);
    y_tmp_float = minus_float(y_threehalfs_float, y_tmp_float);

    y_float = y_tmp_float;
    y_threehalfs_float = mult(y_float, threehalfs_float);
    y_tmp_float = mult(x2_float, y_float);
    y_tmp_float = mult(y_tmp_float, y_float);
    y_tmp_float = mult(y_tmp_float, y_float);
    y_tmp_float = minus_float(y_threehalfs_float, y_tmp_float);

    y_float = y_tmp_float;
    y_threehalfs_float = mult(y_float, threehalfs_float);
    y_tmp_float = mult(x2_float, y_float);
    y_tmp_float = mult(y_tmp_float, y_float);
    y_tmp_float = mult(y_tmp_float, y_float);
    y_tmp_float = minus_float(y_threehalfs_float, y_tmp_float);

    //y  = y * threehalfs - ( x2 * y * y * y) ;   // 1st iteration
    //y  = y * threehalfs - ( x2 * y * y * y) ;   // 2nd iteration, this can be removed
    // y  = y * threehalfs - ( x2 * y * y * y) ;

    out = get_float(y_tmp_float);
    return out;
}
*/
/*
template <class T>
Float_struct Cls_svd<T>::mult(Float_struct in_1, Float_struct in_2)
{
    Float_struct out;
    int exp;
    double temp;
    int exp_temp;

    fxp frac;
    fix_set(frac, 16, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);

    temp = in_1.frac * in_2.frac;
    if(temp == 0) {
        out.exp = 0;
        out.frac = 0;
        return out;
    }
    exp_temp = ceil(log2(fabs(temp)));
    frac = temp * DOUBLE_POW2[64 - exp_temp];
    exp = in_1.exp + in_2.exp + exp_temp;

    out.exp = exp;
    out.frac = frac;
    return out;
}

template <class T>
Float_struct Cls_svd<T>::minus_float(Float_struct in_1, Float_struct in_2)
{
    Float_struct out;
    double temp;
    int exp_temp;
    fxp frac;
    fix_set(frac, 16, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);

    if(in_1.frac == 0 || in_2.frac == 0) {
        if(in_1.frac == 0 && in_2.frac == 0) {
            out.exp = 0;
            out.frac = 0;
        } else if(in_1.frac == 0) {
            out.exp = in_2.exp;
            out.frac = in_2.frac * -1;
        } else if(in_2.frac == 0) {
            out.exp = in_1.exp;
            out.frac = in_1.frac;
        }
        return out;
    }

    if(in_1.exp > in_2.exp) {
        temp = in_2.frac * DOUBLE_POW2[64 - in_1.exp + in_2.exp];
        temp = in_1.frac - temp;
        exp_temp = ceil(log2(fabs(temp)));
        frac = temp * DOUBLE_POW2[64 - exp_temp];
        exp_temp = in_1.exp + exp_temp;
    } else {
        temp = in_1.frac * DOUBLE_POW2[64 - in_2.exp + in_1.exp];
        temp = temp - in_2.frac;
        exp_temp = ceil(log2(fabs(temp)));
        frac = temp * DOUBLE_POW2[64 - exp_temp];
        exp_temp = in_2.exp + exp_temp;
    }
    out.exp = exp_temp;
    out.frac = frac;
    return out;
}

template <class T>
Float_struct Cls_svd<T>::set_float(double in)
{
    Float_struct out;
    int exp;
    fxp frac;

    if(in == 0) {
        out.exp = 0;
        out.frac = 0;
        return out;
    }
    fix_set(frac, 16, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);

    exp = ceil(log2(fabs(in)));
    frac = in * DOUBLE_POW2[64 - exp];

    out.exp = exp;
    out.frac = frac;
    return out;
}

template <class T>
double Cls_svd<T>::get_float(Float_struct in)
{
    double out;

    out = in.frac * DOUBLE_POW2[64 + in.exp];
    return out;
}*/

template <class T>
void Cls_svd<T>::dump_input()
{
#ifdef DUMP_RTL_SVD
    dump(file_rtl_max, &m_max_val_fxp, 1, m_max_val_fxp);
    dump(file_rtl_max_frac, &m_max_val_frac, 1, m_max_val_frac);



    cout  << "div out "<<"\t"<<m_max_val_frac << endl;
    cout  << "exp_a  "<<"\t"<< m_exp_max_val<< endl;

    dump(file_rtl_max_frac_q, &m_q_max_val, 1, m_q_max_val);
    dump(file_rtl_max_frac_exp, &m_exp_max_val, 1, m_exp_max_val);

    for (int ck1 = 0; ck1 < 8; ck1++)
    {
        for (int ck0 = 7; ck0 >= 0; ck0--)
        {
            dump(file_rtl_in_tmp, &m_in[ck0][ck1], 1, m_in_fxp_tmp.I);
        }
        file_rtl_in_tmp << endl;
    }
    dump_A_in(file_rtl_in_1, 0);
    dump_A_in(file_rtl_in_2, 1);
    dump_A_in(file_rtl_in_3, 2);
    dump_A_in(file_rtl_in_4, 3);
    dump_A_in(file_rtl_in_5, 4);
    dump_A_in(file_rtl_in_6, 5);
    dump_A_in(file_rtl_in_7, 6);
    dump_A_in(file_rtl_in_8, 7);
#endif
}

template <class T>
void Cls_svd<T>::dump_col_rot_in(Complex<double> *A_1, Complex<double> *A_2, Complex<double> *V_1, Complex<double> *V_2)
{
#ifdef DUMP_RTL_SVD
    for (int ck0 = 0; ck0 < 8; ck0++) {
        dump(file_rtl_A_1_in, &A_1[ck0], 1, m_A_fxp.I);
        dump(file_rtl_A_2_in, &A_2[ck0], 1, m_A_fxp.I);
        dump(file_rtl_V_1_in, &V_1[ck0], 1, m_V_fxp.I);
        dump(file_rtl_V_2_in, &V_2[ck0], 1, m_V_fxp.I);
    }
    file_rtl_A_1_in << endl;
    file_rtl_A_2_in << endl;
    file_rtl_V_1_in << endl;
    file_rtl_V_2_in << endl;
#endif
}

template <class T>
void Cls_svd<T>::dump_A_in(ofstream &file, int ck1)
{
#ifdef DUMP_RTL_SVD
    for (int ck0 = 7; ck0 >= 0; ck0--) {
        //dump(file, &m_A[ck0][ck1], 1, m_A_fxp.I);
        dump(file, &m_A[ck0][ck1], 1, m_A_fxp_max.I);

    }
    file << endl;
#endif
}
#endif
