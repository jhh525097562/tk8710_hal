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
#ifndef __Cls_float_fix
#define __Cls_float_fix

#define DUMP_MATLAB_FLOAT_FIX
//#define DUMP_RTL_FLOAT_FIX
//#define DUMP_MATLAB_SVD_log
#include "common_type.h"
#include <stdint.h>
#include "iomanip"

typedef union {
    float floatValue;
    uint32_t binary_representation;
} FloatUnion;

struct Float_struct {
    double frac;
    int exp;
};

template <class T>
class Cls_float_fix
{
	public:
		Cls_float_fix() {

		}
		~Cls_float_fix() {
#ifdef DUMP_RTL_FLOAT_FIX
        file_rtl_exp_o.close();
        file_rtl_fix_in_o.close();
        file_rtl_q_o.close();
        file_rtl_exp_i.close();
        file_rtl_fix_o.close();
        file_rtl_q_i.close();
        file_rtl_sqrt_i.close();
        file_rtl_root.close();
#endif
#ifdef DUMP_MATLAB_FLOAT_FIX
        file_mat_rsqrt_o.close();
        file_mat_rsqrt_d_o.close();
#endif
		}
        void init(char* module_name);
        void go(Complex<double> in);
        void reset();
        double divide(double a, double b);
	private:
        void        fix2float(double in);
        void        float2fix(double in, int exp);
        void        sqrt_bit(double in);
        T           m_exp;
        T           m_Q;
        T           m_in_fxp;
        T           m_out;
        T           m_sqrt_in;
        T           m_root;
        double           m_mult_0;
        double           m_mult_1;
        double           m_mult_2;
        double           m_mult_3;
        double           m_mult_4;
        Float_struct        mult(Float_struct in_1, Float_struct in_2);
        Float_struct        set_float(double in);
        double              get_float(Float_struct in);
        Float_struct        minus_float(Float_struct in_1, Float_struct in_2);
        uint64_t    m_in_bit_dec;
        int         m_in_bit[32];
        double      rsqrt(double in);
#ifdef DUMP_RTL_FLOAT_FIX
        ofstream    file_rtl_exp_o;
        ofstream    file_rtl_fix_in_o;
        ofstream    file_rtl_q_o;
        ofstream    file_rtl_exp_i;
        ofstream    file_rtl_fix_o;
        ofstream    file_rtl_q_i;
        ofstream    file_rtl_sqrt_i;
        ofstream    file_rtl_root;
#endif
#ifdef DUMP_MATLAB_FLOAT_FIX
        ofstream    file_mat_rsqrt_o;
        ofstream    file_mat_rsqrt_d_o;
#endif
};

template <class T>
void Cls_float_fix<T>::init(char* module_name)
{
    fix_set(m_Q, 24, 1, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_exp, 9, 9, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_in_fxp, 32, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_out, 32, 1, FXP_TC, FXP_TRN, FXP_SAT_SYM);
    fix_set(m_sqrt_in, 32, 0, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(m_root, 16, 0, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(m_mult_0, 36, 15, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_mult_0, "m_mult_0", 1, 1);
    fix_set(m_mult_1, 36, 15, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_mult_1, "m_mult_1", 1, 1);
    fix_set(m_mult_2, 36, 15, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_mult_2, "m_mult_2", 1, 1);
    fix_set(m_mult_3, 36, 15, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_mult_3, "m_mult_3", 1, 1);
    fix_set(m_mult_4, 36, 15, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_mult_4, "m_mult_4", 1, 1);
#ifdef DUMP_MATLAB_FLOAT_FIX
    struct stat statExist;
    int statMd = 0;
    if (stat(module_name, &statExist) != 0) {
            statMd = mkdir(module_name, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    char filename_rtl[300] = {};
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "rsqrt_o.txt");             file_mat_rsqrt_o.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "rsqrt_d_o.txt");              file_mat_rsqrt_d_o.open(filename_rtl);
#endif
#ifdef DUMP_RTL_FLOAT_FIX
    struct stat statExist;
    int statMd = 0;
    if (stat(module_name, &statExist) != 0) {
            statMd = mkdir(module_name, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    char filename_rtl[300] = {};
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "exp_o.dat");           file_rtl_exp_o.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fix_in.dat");          file_rtl_fix_in_o.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "q_o.dat");             file_rtl_q_o.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "exp_i.dat");           file_rtl_exp_i.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fix_o.dat");           file_rtl_fix_o.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "q_i.dat");             file_rtl_q_i.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "sqrt_i.dat");          file_rtl_sqrt_i.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "root.dat");            file_rtl_root.open(filename_rtl);
#endif

	//reset();
}

template <class T>
void Cls_float_fix<T>::reset()
{

}

template <class T>
void Cls_float_fix<T>::go(Complex<double> in)
{
    /*m_in_fxp = in.I;
    fix2float(m_in_fxp);
#ifdef DUMP_RTL_FLOAT_FIX
    dump(file_rtl_fix_in_o, &m_in_fxp, 1, m_in_fxp);
    dump(file_rtl_exp_o, &m_exp, 1, m_exp);
    dump(file_rtl_q_o, &m_Q, 1, m_Q);
#endif
    m_Q = m_Q * 1.0 / 1.741;
    float2fix(m_Q, m_exp);
#ifdef DUMP_RTL_FLOAT_FIX
    dump(file_rtl_fix_o, &m_out, 1, m_out);
    dump(file_rtl_exp_i, &m_exp, 1, m_exp);
    dump(file_rtl_q_i, &m_Q, 1, m_Q);
#endif
    sqrt_bit(fabs(m_in_fxp));
#ifdef DUMP_RTL_FLOAT_FIX
    dump(file_rtl_sqrt_i, &m_sqrt_in, 1, m_sqrt_in);
    dump(file_rtl_root, &m_root, 1, m_root);
#endif
*/
    double out = 0;
    double out1 = 0;
    out = rsqrt(in.I);
    out1 = 1.0 / sqrt(in.I);
    //if(abs(out - out1) > 180)
    //    int a = 1;

    out = rsqrt(in.I);
#ifdef DUMP_MATLAB_FLOAT_FIX
    file_mat_rsqrt_o << out << endl;
    file_mat_rsqrt_d_o << out1 << endl;
#endif
}

template <class T>
void Cls_float_fix<T>::fix2float(double in)
{
    int exp;
    exp = ceil(log2(fabs(in)));

    m_exp = exp;
    m_Q = in * DOUBLE_POW2[64 - exp];
}

template <class T>
void Cls_float_fix<T>::float2fix(double in, int exp)
{
    m_out = in * DOUBLE_POW2[64 + exp];
}

template <class T>
void Cls_float_fix<T>::sqrt_bit(double in)
{
    int r = 0;
    int t = 0;
    int t_2 = 0;

    m_sqrt_in = in;
    m_in_bit_dec = m_sqrt_in * DOUBLE_POW2[64 + 32];

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
    m_root = r * 1.0 * DOUBLE_POW2[64 - 16];
}

template <class T>
double Cls_float_fix<T>::rsqrt(double in)
{
    double number;
    int i;
    float x2, y;
    const float threehalfs = 1.5F;
    double out;
    int frac;
    int exp;
    int y_exp;
    int threehalfs_exp;
    double frac_d, threehalfs_frac, y_frac;
    fxp frac_set;
    Float_struct y_float;
    Float_struct threehalfs_float;
    Float_struct y_threehalfs_float;
    Float_struct y_tmp_float;
    Float_struct x2_float;

    double temp0, temp1, temp2, temp3, temp4;

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
    temp0 = get_float(y_tmp_float);

        /*m_mult_0 = y * threehalfs;
    m_mult_1 = x2 * y;
    m_mult_2 = m_mult_1 * y;
    m_mult_3 = m_mult_2 * y;
    m_mult_4 = m_mult_0 - m_mult_3;

y = m_mult_4;

    m_mult_0 = y * threehalfs;
    m_mult_1 = x2 * y;
    m_mult_2 = m_mult_1 * y;
    m_mult_3 = m_mult_2 * y;
    m_mult_4 = m_mult_0 - m_mult_3;

    y = m_mult_4;

    m_mult_0 = y * threehalfs;
    m_mult_1 = x2 * y;
    m_mult_2 = m_mult_1 * y;
    m_mult_3 = m_mult_2 * y;
    m_mult_4 = m_mult_0 - m_mult_3;

    y = m_mult_4;*/
    //y  = y * threehalfs - ( x2 * y * y * y) ;   // 1st iteration
    //y  = y * threehalfs - ( x2 * y * y * y) ;   // 2nd iteration, this can be removed
    // y  = y * threehalfs - ( x2 * y * y * y) ;

    out = get_float(y_tmp_float);
    return out;
}

template <class T>
Float_struct Cls_float_fix<T>::mult(Float_struct in_1, Float_struct in_2)
{
    Float_struct out;
    int exp;
    double temp;
    int exp_temp;

    fxp frac;
    fix_set(frac, 16, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);

    temp = in_1.frac * in_2.frac;
    exp_temp = ceil(log2(fabs(temp)));
    frac = temp * DOUBLE_POW2[64 - exp_temp];
    exp = in_1.exp + in_2.exp + exp_temp;

    out.exp = exp;
    out.frac = frac;
    return out;
}

template <class T>
Float_struct Cls_float_fix<T>::minus_float(Float_struct in_1, Float_struct in_2)
{
    Float_struct out;
    int exp;
    double temp;
    int exp_temp;
    fxp frac;
    fix_set(frac, 16, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);

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
Float_struct Cls_float_fix<T>::set_float(double in)
{
    Float_struct out;
    int exp;
    fxp frac;

    fix_set(frac, 16, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);

    exp = ceil(log2(fabs(in)));
    frac = in * DOUBLE_POW2[64 - exp];

    out.exp = exp;
    out.frac = frac;
    return out;
}

template <class T>
double Cls_float_fix<T>::get_float(Float_struct in)
{
    double out;

    out = in.frac * DOUBLE_POW2[64 + in.exp];
    return out;
}

template <class T>
double Cls_float_fix<T>::divide(double a, double b)
{
    double m_mantissaResult_fxp;
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
#endif
