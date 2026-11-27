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

#define DUMP_MATLAB_SVD
//#define DUMP_RTL_SVD
//#define DUMP_MATLAB_SVD_log
#include "common_type.h"
#include <stdint.h>
#include "iomanip"
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
        Complex<T>      **m_A;
        Complex<T>      *m_A_1;
        Complex<T>      *m_A_2;
        Complex<T>      *m_V_1;
        Complex<T>      *m_V_2;
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
        T               m_t;
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

        T               m_max_val_frac;
        T               m_xiu_sqrt_fxp;
        T               m_t_sqrt_fxp;
        T               m_sqrt_in_fxp;
        T               m_sqrt_q_in_fxp;
        T               m_sqrt_temp_in_fxp;
        T               m_sqrt_q_temp_in_fxp;
        T               m_t_sqrt_in_fxp;
        T               m_sqrt_t_in_fxp;
        T               m_mantissaResult_fxp;
        T               m_q_max_val;
        T               m_exp_max_val;
        double          **m_S_o;
        Complex<T>      m_xiu_norm;
        Complex<T>      m_xiu;
        Complex<T>      m_U_fxp;
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
    fix_set(m_xiu, 40, 2, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_xiu, "m_xiu", 1, 1);
    fix_set(m_sqrt_in_fxp , 61, 4, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sqrt_in_fxp, "m_sqrt_in_fxp", 1, 1);
    fix_set(m_xiu_sqrt_fxp, 50, 3, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_xiu_sqrt_fxp, "m_xiu_sqrt_fxp", 1, 1);
    fix_set(m_sqrt_q_in_fxp, 32, 0, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sqrt_q_in_fxp, "m_sqrt_q_in_fxp", 1, 1);
    fix_set(m_t_sqrt_fxp, 40, 16, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_t_sqrt_fxp, "m_t_sqrt_fxp", 1, 1);
    fix_set(m_S_fxp, 60, 4, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_S_fxp, "m_S_fxp", 1, 1);
    fix_set(m_S_sqrt_fxp, 32, 3, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_S_sqrt_fxp, "m_S_sqrt_fxp", 1, 1);
    fix_set(m_S_rsqrt_fxp, 40, 20, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_S_rsqrt_fxp, "m_S_rsqrt_fxp", 1, 1);


    fix_set(m_wk, 27, 4, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(m_wl, 27, 4, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(m_in_fxp_tmp, 24, 15, FXP_TC, FXP_RND, FXP_SAT_SYM); // 28.15
    fix_set(m_q_max_val, 20, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_exp_max_val, 7, 7, FXP_TC, FXP_RND, FXP_SAT_SYM);
    //fix_set(m_max_val_frac, 30, 20, FXP_US, FXP_RND, FXP_SAT_SYM);
    //fix_set(m_max_val, 24, 15, FXP_US, FXP_RND, FXP_SAT_SYM);

    fix_set(m_sqrt_q_temp_in_fxp, 32, 18, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_xiu, "m_xiu", 1, 100);
    fix_set(m_xiu_rsqrt, 32, 16, FXP_US, FXP_RND, FXP_SAT_SYM); //statistic_set(m_xiu_rsqrt, "m_xiu_rsqrt", 1, 100);
    //fix_set(m_sqrt_t_in_fxp, 32, 6, FXP_US, FXP_RND, FXP_SAT_SYM); //statistic_set(m_xiu_rsqrt, "m_xiu_rsqrt", 1, 100);
    fix_set(m_xiu_norm, 20, 2, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(m_xiu_norm, "m_xiu_norm", 1, 1);
    fix_set(m_QKL, 30, 5, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(m_QKL, "m_QKL", 1, 1);
    fix_set(m_t_tmp, 34, 16, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_t_tmp, "m_t_tmp", 1, 1);
    fix_set(m_sqrt_temp_in_fxp, 44, 30, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sqrt_temp_in_fxp, "m_sqrt_temp_in_fxp", 1, 1);
    fix_set(m_sqrt_temp, 36, 22, FXP_US, FXP_RND, FXP_SAT_SYM); //statistic_set(m_sqrt_temp, "m_sqrt_temp", 1, 1);
    fix_set(m_t, 40, 16, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_t, "m_t", 1, 1);

    fix_set(m_t_sqrt_in_fxp, 50, 24, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_t_sqrt_in_fxp, "m_t_sqrt_in_fxp", 1, 1);
    fix_set(m_c, 24, 1, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_c, "m_c", 1, 1);
    fix_set(m_s, 28, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_s, "m_s", 1, 1);
    fix_set(m_A_fxp, 28, 4, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_A_fxp, "m_A_fxp", 1, 2);
    fix_set(m_A_fxp_max, 24, 4, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_A_fxp, "m_A_fxp", 1, 2);

    fix_set(m_V_fxp, 23, 4, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_V_fxp, "m_V_fxp", 1, 2);
    //fix_set(m_S_rsqrt, 21, 3, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_S_rsqrt, "m_S_rsqrt", 1, 1);
    fix_set(m_s_o_fxp, 32, 4, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_s_o_fxp, "m_s_o_fxp", 1, 1);
    fix_set(m_U_fxp, 17, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_A_fxp, "m_A_fxp", 1, 1);

    fix_set(m_S_fxp, 32, 4, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_S_fxp, "m_S_fxp", 1, 1);

    fix_set(m_mantissaResult_fxp, 20, 1, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_A_fxp, "m_A_fxp", 1, 1);
    fix_set(m_max_val_fxp, 20, 15, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_A_fxp, "m_A_fxp", 1, 1);

    fix_set(test_fxp, 20, 5, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_A_fxp, "m_A_fxp", 1, 1);

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
#ifdef DUMP_RTL_SVD
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

	reset();
}

template <class T>
void Cls_svd<T>::reset()
{
    ones_val(m_in, 8, 8, 1e-10);
    zeros(m_V, 8, 8);
    eye(m_V, 8);
    m_iter_num = 3;
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
    double max_val = 0;
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
    m_max_val = max(m_in); //24.15
    m_max_val_fxp = m_max_val; //24.15->20.15

    m_max_val_frac = 1.0 / m_max_val;
    m_exp_max_val = ceil(log2(fabs(m_max_val_frac)));
    m_q_max_val = m_max_val_frac * DOUBLE_POW2[64 - (int)m_exp_max_val];
   // m_max_val_frac = (1.0*DOUBLE_POW2[64 +18]) / (m_max_val_fxp*DOUBLE_POW2[64 + 15]);

    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            m_in_fxp_tmp = m_in[ck0][ck1];
            m_in[ck0][ck1] = m_in_fxp_tmp;
            //m_A_fxp = m_in[ck0][ck1] * m_q_max_val * DOUBLE_POW2[64 + (int)m_exp_max_val];
            //m_A[ck0][ck1] = m_A_fxp;
            m_A_fxp_max = m_in[ck0][ck1].mult(m_max_val_frac) ;
            m_A[ck0][ck1] = m_A_fxp_max;
        }
    }

    //Dump for RTL
#ifdef DUMP_RTL_SVD
    dump_input();
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
        }
    }

#ifdef DUMP_RTL_SVD
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 7; ck1 >= 0; ck1--)
        {
            dump_no_n(file_rtl_V, &m_V[ck1][ck0], 1, m_V_fxp.I);
        }
        file_rtl_V << endl;
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
    }
}

template <class T>
void Cls_svd<T>::s_cal()
{
    double s_rsqrt[8];
    double S_rsqrt_d[8];
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        m_S_rsqrt[ck0] = 0;
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
           m_S_rsqrt[ck0] = m_S_rsqrt[ck0] + m_A[ck1][ck0].power();
           //LOG2DEV  << m_S_rsqrt[ck0] << endl;
        }
        m_S_fxp = m_S_rsqrt[ck0];

        m_S_sqrt_fxp = sqrt(m_S_fxp);
        if(m_S_sqrt_fxp == 0)
            m_S_sqrt_fxp = 1e-4;
        m_S_rsqrt_fxp = 1.0 / m_S_sqrt_fxp;
        //LOG2DEV  << m_S_rsqrt_fxp << endl;
        m_S_rsqrt[ck0] = m_S_rsqrt_fxp;
    }
    mem_copy(s_rsqrt, m_S_rsqrt, 8);
    mem_copy(S_rsqrt_d, m_S_rsqrt, 8);
    quicksort(S_rsqrt_d, 0, 7);
    index_get(s_rsqrt, S_rsqrt_d, m_pos);
    mem_copy(m_S_rsqrt, S_rsqrt_d, 8);
    zeros(m_S_o, 8, 8);
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        m_s_o_fxp = 1.0 /  m_S_rsqrt[ck0] * m_max_val;
        //LOG2DEV << m_s_o_fxp << endl;
        m_S_o[ck0][ck0] = m_s_o_fxp;
#ifdef DUMP_RTL_SVD
        dump_no_n(file_rtl_G, &m_S_o[ck0][ck0], 1, m_s_o_fxp);
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
                break;
            }
        }
    }
}

template <class T>
void Cls_svd<T>::u_cal()
{
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            m_U_fxp = m_A[ck0][m_pos[ck1]] * m_S_rsqrt[ck1];
            m_U[ck0][ck1] = m_U_fxp;
        }
    }
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
        wk_tmp = wk_tmp + A_1[ck0].power();
        wl_tmp = wl_tmp + A_2[ck0].power();
        temp_1 = A_1[ck0].conjug();
        temp_2 = A_2[ck0];
        temp_3 = temp_1 * temp_2;
        xiu_temp = xiu_temp + A_1[ck0].conjug() * A_2[ck0];
    }
    m_wk = wk_tmp;
    m_wl = wl_tmp;
    m_xiu = xiu_temp;
    //LOG2DEV << m_xiu.I << "\t" <<m_xiu.Q<<endl;
    //LOG2DEV<<m_wk<<endl;

    //m_sqrt_in_fxp = m_xiu.power();
    m_xiu_sqrt_fxp = sqrt(m_xiu.power());//sqrt(m_sqrt_in_fxp);
    if(m_xiu_sqrt_fxp == 0)
        m_xiu_sqrt_fxp = 1 * DOUBLE_POW2[64 - 16];
    //m_xiu_rsqrt = 1.0 / m_xiu_sqrt_fxp;//m_xiu.rsqrt();
    //LOG2DEV << m_xiu_rsqrt << endl;
    /*I = divide(m_xiu.I, m_xiu_sqrt_fxp);
    Q = divide(m_xiu.Q, m_xiu_sqrt_fxp);
    m_xiu_norm.I = I;
    m_xiu_norm.Q = Q;*/
    m_xiu_norm = m_xiu / m_xiu_sqrt_fxp;
    //LOG2DEV << m_xiu_norm.I << "\t"<<m_xiu_norm.Q << endl;
    //m_xiu_norm = m_xiu * 1.0 / m_xiu_sqrt_fxp;
    //m_xiu_norm = m_xiu * m_xiu_rsqrt;
    //LOG2DEV << m_xiu_norm.I << "\t"<<m_xiu_norm.Q << endl;
    m_QKL = m_wk - m_wl;

    if (abs(m_QKL) < 1e-4)
        m_QKL = 1e-4;

    //m_t_tmp = m_QKL * m_xiu_rsqrt ;
    m_t_tmp = m_QKL / m_xiu_sqrt_fxp;//divide(m_QKL, );
    //m_t_tmp = m_QKL / m_xiu_sqrt_fxp;
    //LOG2DEV << m_t_tmp << endl;
    m_sqrt_temp_in_fxp = m_t_tmp * m_t_tmp + 4;
    m_sqrt_temp = sqrt(m_sqrt_temp_in_fxp);//sqrt(m_sqrt_temp_in_fxp);//1.0 / rsqrt(m_t_tmp * m_t_tmp + 4);
    //LOG2DEV << m_sqrt_temp << endl;
    m_t = 2.0 / (m_t_tmp + (double)sign(m_QKL) * m_sqrt_temp);
    //LOG2DEV << m_t << endl;
    m_t_sqrt_in_fxp = m_t * m_t + 1;
    m_t_sqrt_fxp = sqrt(m_t_sqrt_in_fxp);//sqrt(m_t_sqrt_in_fxp);//
    m_c = 1.0 / m_t_sqrt_fxp;//rsqrt(m_t * m_t + 1);
    m_s = m_t * m_c;
    //file_mat_m_s << std::fixed<<setprecision(32) << m_s  << endl;
    //LOG2DEV << std::fixed<<setprecision(32) << m_s  << endl;
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        m_A_fxp = m_c * m_xiu_norm.todouble() * A_1[ck0] ;      temp_A = m_A_fxp; m_A_fxp = m_s * A_2[ck0]; temp_B = m_A_fxp; m_A_fxp = temp_A +  temp_B;  m_A_1[ck0] = m_A_fxp; //LOG2DEV << m_A_fxp.I << "\t" << m_A_fxp.Q << endl;
        m_A_fxp = -1 * m_s * m_xiu_norm.todouble() * A_1[ck0];  temp_A = m_A_fxp; m_A_fxp = m_c * A_2[ck0]; temp_B = m_A_fxp; m_A_fxp = temp_A +  temp_B;  m_A_2[ck0] = m_A_fxp; //LOG2DEV << m_A_fxp.I << "\t" << m_A_fxp.Q << endl;

        m_V_fxp = m_c * m_xiu_norm.todouble() * V_1[ck0]; v_tmp_1 = m_V_fxp;
        m_V_fxp = + m_s * V_2[ck0];  v_tmp_2 = m_V_fxp;
        m_V_fxp = v_tmp_1 + v_tmp_2; m_V_1[ck0] = m_V_fxp;

        m_V_fxp = -1 * m_s * m_xiu_norm.todouble() * V_1[ck0]; v_tmp_1 = m_V_fxp;
        m_V_fxp = m_c * V_2[ck0]; v_tmp_2 = m_V_fxp;
        m_V_fxp = v_tmp_1 + v_tmp_2;    m_V_2[ck0] = m_V_fxp;
        //LOG2DEV<<m_A_1[ck0].I <<"\t"<<m_A_1[ck0].Q<< "\t" << m_A_2[ck0].I <<"\t"<<m_A_2[ck0].Q<< endl;
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
    if(isnan(a[i]))
        return;
    quicksort(a, first,i - 1);
    quicksort(a, i + 1,end);
}

template <class T>
double Cls_svd<T>::sqrt_q(double in)
{
    double out;
    int shift = 1;
    double times = 0;
    if(in < 1) {
        shift = 0;
        times = 1;
    } else if(in < 2) {
        shift = 1;
        times = 92682 * DOUBLE_POW2[64 - 16];
    } else if(in < 4) {
        shift = 2;
        times = 2;
    } else if(in < 8) {
        shift = 3;
        times = 185364 * DOUBLE_POW2[64 - 16];
    } else if(in < 16) {
        shift = 4;
        times = 4;
    } else if(in < 32) {
        shift = 5;
        times = 370728 * DOUBLE_POW2[64 - 16];
    } else if(in < 64) {
        shift = 6;
        times = 8;
    } else if(in < 128) {
        shift = 7;
        times = 741455 * DOUBLE_POW2[64 - 16];
    } else if(in < 256) {
        shift = 8;
        times = 16;
    } else if(in < 512) {
        shift = 9;
        times = 1482910 * DOUBLE_POW2[64 - 16];
    } else if(in < 1024) {
        shift = 10;
        times = 32;
    } else if(in < 2048) {
        shift = 11;
        times = 2965821 * DOUBLE_POW2[64 - 16];
    } else if(in < 4096) {
        shift = 12;
        times = 64;
    } else if(in < 8192) {
        shift = 13;
        times = 5931642 * DOUBLE_POW2[64 - 16];
    } else if(in < 16384) {
        shift = 14;
        times = 128;
    } else if(in < 32768) {
        shift = 15;
        times = 11863283 * DOUBLE_POW2[64 - 16];
    } else if(in < 65536) {
        shift = 16;
        times = 256;
    } else if(in < 131072) {
        shift = 17;
        times = 23726566 * DOUBLE_POW2[64 - 16];
    } else if(in < 262144) {
        shift = 18;
        times = 512;
    } else if(in < 524288) {
        shift = 19;
        times = 47453133 * DOUBLE_POW2[64 - 16];
    } else if(in < 1048576) {
        shift = 20;
        times = 1024;
    }
    m_sqrt_q_in_fxp = in * DOUBLE_POW2[64 - shift];
    //LOG2DEV << m_sqrt_q_in_fxp<<endl;
    out = sqrt(m_sqrt_q_in_fxp) * times;
    return out;
}

template <class T>
double Cls_svd<T>::sqrt_q_temp(double in)
{
    double out;
    int shift = 0;
    double times = 0;
    //double in;

    if(in < DOUBLE_POW2[64 + 19]) {
        shift = 0;
        times = 1;
    } else if(in < DOUBLE_POW2[64 + 20]) {
        shift = 1;
        times = 92682 * DOUBLE_POW2[64 - 16];
    }  else if(in < DOUBLE_POW2[64 + 21]) {
        shift = 2;
        times = 2;
    }  else if(in < DOUBLE_POW2[64 + 22]) {
        shift = 3;
        times = 92682 * DOUBLE_POW2[64 - 15];
    } else if(in < DOUBLE_POW2[64 + 23]) {
        shift = 4;
        times = 4;
    } else if(in < DOUBLE_POW2[64 + 24]) {
        shift = 5;
        times = 92682 * DOUBLE_POW2[64 - 14];
    } else if(in < DOUBLE_POW2[64 + 25]) {
        shift = 6;
        times = 8;
    } else if(in < DOUBLE_POW2[64 + 26]) {
        shift = 7;
        times = 92682 * DOUBLE_POW2[64 - 13];
    } else if(in < DOUBLE_POW2[64 + 27]) {
        shift = 8;
        times = 16;
    } else if(in < DOUBLE_POW2[64 + 28]) {
        shift = 9;
        times = 92682 * DOUBLE_POW2[64 - 12];
    } else if(in < DOUBLE_POW2[64 + 29]) {
        shift = 10;
        times = 32;
    } else if(in < DOUBLE_POW2[64 + 30]) {
        shift = 11;
        times = 92682 * DOUBLE_POW2[64 - 11];
    } else if(in < DOUBLE_POW2[64 + 31]) {
        shift = 12;
        times = 64;
    }
    m_sqrt_q_temp_in_fxp = in * DOUBLE_POW2[64 - shift];
    out = sqrt(m_sqrt_q_temp_in_fxp) * times;
    return out;
}

template <class T>
double Cls_svd<T>::sqrt_q_t(double in_temp)
{
    double out;
    int shift = 0;
    double times = 0;
    double in;
    in = in_temp * DOUBLE_POW2[64 - 18];
    if(in < DOUBLE_POW2[64 + 6]) { //6
        shift = 0;
        times = 512;
    } else if(in < DOUBLE_POW2[64 + 7]) {//7
        shift = 1;
        times = 512 * 92682 * DOUBLE_POW2[64 - 16];
    } else if(in < DOUBLE_POW2[64 + 8]) {//8
        shift = 2;
        times = 1024;
    } else if(in < DOUBLE_POW2[64 + 9]) {//9
        shift = 3;
        times = 1024 * 92682 * DOUBLE_POW2[64 - 16];
    } else if(in < DOUBLE_POW2[64 + 10]) {//10
        shift = 4;
        times = 2048;
    } else if(in < DOUBLE_POW2[64 + 11]) {//11
        shift = 5;
        times = 2048 * 92682 * DOUBLE_POW2[64 - 16];
    } else if(in < DOUBLE_POW2[64 + 12]) {//12
        shift = 6;
        times = 4096;
    } else if(in < DOUBLE_POW2[64 + 13]) {//13
        shift = 7;
        times = 4096 * 92682 * DOUBLE_POW2[64 - 16];
    } else if(in < DOUBLE_POW2[64 + 14]) {//14
        shift = 8;
        times = 8192;
    } else if(in < DOUBLE_POW2[64 + 15]) {//15
        shift = 9;
        times = 8192 * 92682 * DOUBLE_POW2[64 - 16];
    } else if(in < DOUBLE_POW2[64 + 16]) {//16
        shift = 10;
        times = 16384;
    } else if(in < DOUBLE_POW2[64 + 17]) {//17
        shift = 11;
        times = 16384 * 92682 * DOUBLE_POW2[64 - 16];
    } else if(in < DOUBLE_POW2[64 + 18]) {//18
        shift = 12;
        times = 32768;
    } else if(in < DOUBLE_POW2[64 + 19]) {//19
        shift = 13;
        times = 32768 * 92682 * DOUBLE_POW2[64 - 16];
    } else if(in < DOUBLE_POW2[64 + 20]) {//20
        shift = 14;
        times = 65536;
    } else if(in < DOUBLE_POW2[64 + 21]) {//21
        shift = 15;
        times = 65536 * 92682 * DOUBLE_POW2[64 - 16];
    } else if(in < DOUBLE_POW2[64 + 22]) {//22
        shift = 16;
        times = 131072;
    } else if(in < DOUBLE_POW2[64 + 23]) {//23
        shift = 17;
        times = 131072 * 92682 * DOUBLE_POW2[64 - 16];
    } else if(in < DOUBLE_POW2[64 + 24]) {//23
        shift = 18;
        times = 262144;
    }
    m_sqrt_t_in_fxp = in * DOUBLE_POW2[64 - shift];
    out = sqrt(m_sqrt_t_in_fxp) * times;
    return out;
}

template <class T>
double Cls_svd<T>::divide(double a, double b)
{
    FloatUnion in_1, in_2;
    in_1.floatValue = a;
    in_2.floatValue = b;

    int signA = (in_1.binary_representation >> 31) & 0x1;
    int signB = (in_2.binary_representation >> 31) & 0x1;
    int exp_A = (in_1.binary_representation >> 23) & 0xff;
    int exp_B = (in_2.binary_representation >> 23) & 0xff;
    uint32_t mantissaA = in_1.binary_representation & 0x7fffff;
    uint32_t mantissaB = in_2.binary_representation & 0x7fffff;

    uint32_t hiddenOne = 1 << 23;
    mantissaA |= hiddenOne;
    mantissaB |= hiddenOne;

    int signResult = signA ^ signB;
    int exp_result = exp_A - exp_B + 127;

    m_mantissaResult_fxp = 1.0 * mantissaA / mantissaB;
    double mantissaResult = m_mantissaResult_fxp * DOUBLE_POW2[64 + 23] ;

    if(mantissaResult >= DOUBLE_POW2[64 + 24]) {
        mantissaResult = mantissaResult / 2;
        exp_result = exp_result + 1;
    } else if(mantissaResult < DOUBLE_POW2[64 + 23] && exp_result > 0) {
        mantissaResult = mantissaResult * 2;
        exp_result = exp_result - 1;
    }
    uint32_t result_final = round(mantissaResult) - DOUBLE_POW2[64 + 23];
    FloatUnion out_temp;
    out_temp.binary_representation = (signResult << 31) | ((exp_result & 0xff) << 23) | (result_final & 0x7fffff);
    double out = out_temp.floatValue;
    return out;
}

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
            dump_no_n(file_rtl_in_tmp, &m_in[ck0][ck1], 1, m_in_fxp_tmp.I);
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
        dump_no_n(file_rtl_A_1_in, &A_1[ck0], 1, m_A_fxp.I);
        dump_no_n(file_rtl_A_2_in, &A_2[ck0], 1, m_A_fxp.I);
        dump_no_n(file_rtl_V_1_in, &V_1[ck0], 1, m_V_fxp.I);
        dump_no_n(file_rtl_V_2_in, &V_2[ck0], 1, m_V_fxp.I);
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
        //dump_no_n(file, &m_A[ck0][ck1], 1, m_A_fxp.I);
        dump_no_n(file, &m_A[ck0][ck1], 1, m_A_fxp_max.I);

    }
    file << endl;
#endif
}
#endif
