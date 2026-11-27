#ifndef __BT_CORDIC_H
#define __BT_CORDIC_H

#include <cmath>
#include "Lib_complex.h"
// define of PI
//const double PI = atan(1.0)*4;
const double HALF_PI = PI * 0.5;

//#define DUMP_CORDIC
// class definition
template <class T>
class ClsCORDIC
{
    public:
    // Class constructor
        ClsCORDIC() {
            m_cordic_table = NULL;
            m_scaleVal=NULL;
        }

    // Class destructor
        ~ClsCORDIC() {
            free(m_cordic_table);
            free(m_scaleVal);

            if (fH_angle.is_open())        fH_angle.close();
            if (fH_x.is_open())            fH_x.close();
            if (fH_y.is_open())            fH_y.close();
            if (fH_cordic_table.is_open()) fH_cordic_table.close();
            if (fH_scaleVal.is_open())     fH_scaleVal.close();
        }

        int init(int max_L = 33);
            // rotation
        int rotate(Complex<T>& output, Complex<T>& input, T phase_rads, int L);
        // phase calculation
        double vector(Complex<double> input_n, int L=16);
        // normalize
        Complex<double> normalize(Complex<double> input_n);

    private:
        int     m_max_L;
        bool    m_scale;
        T       *m_scaleVal;
        T       *m_cordic_table;
        T       m_output_phase;

        T       m_real, m_imag, m_acc_I, m_acc_Q;
        T       m_shift_I, m_shift_Q;
        T       m_acc_phase_rads;
        T       m_hpi;
        std::ofstream   f_in, f_new_in;
        std::ofstream   fH_scaleVal, fH_cordic_table;
        std::ofstream   fH_angle, fH_x, fH_y;
        std::ofstream   f_shift;
};

// implementations
template <class T>
int ClsCORDIC<T>::init(int max_L)
{
    double      K;
    int         L;  //cordic table number of lines
    m_cordic_table = safe_new(m_cordic_table,max_L);
    m_scaleVal = safe_new(m_scaleVal,max_L);

    fix_set_group(m_cordic_table, max_L, 15+5,-2,FXP_US,FXP_RND,FXP_SAT_SYM);
    fix_set_group(m_scaleVal    , max_L, 18  , 2,FXP_US,FXP_RND,FXP_SAT_SYM);
    fix_set(m_output_phase      ,        18+5, 1,FXP_TC,FXP_RND,FXP_WRAP);
    fix_set(m_acc_phase_rads,            18+5, 1,FXP_TC,FXP_RND,FXP_WRAP);
    fix_set(m_hpi,                       18+5, 1,FXP_US,FXP_RND,FXP_WRAP);

    fix_set(m_real,    16,16,FXP_TC,FXP_RND,FXP_SAT_SYM);
    fix_set(m_imag,    16,16,FXP_TC,FXP_RND,FXP_SAT_SYM);
    fix_set(m_acc_I,   18,18,FXP_TC,FXP_RND,FXP_SAT_SYM);
    fix_set(m_acc_Q,   18,18,FXP_TC,FXP_RND,FXP_SAT_SYM);
    fix_set(m_shift_I, 16, 16, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_shift_Q, 16, 16, FXP_TC, FXP_RND, FXP_SAT_SYM);


#ifdef DUMP_CORDIC
    char filename[300];
    strcpy(filename, module_name); f_in.open(strcat(filename, "cordic_in.dat"));
    strcpy(filename, module_name); f_new_in.open(strcat(filename, "cordic_new_in.dat"));
    strcpy(filename, module_name); fH_scaleVal.open(strcat(filename, "cordic_scale.dat"));
    strcpy(filename, module_name); fH_cordic_table.open(strcat(filename, "cordic_table.dat"));

    strcpy(filename, module_name); fH_angle.open(strcat(filename, "cordic_angle_accum.dat"));
    strcpy(filename, module_name); fH_x.open(strcat(filename, "cordic_x.dat"));
    strcpy(filename, module_name); fH_y.open(strcat(filename, "cordic_y.dat"));

    strcpy(filename, module_name); f_shift.open(strcat(filename, "cordic_shift.dat"));

#endif

        K = 1.0;
        for(L = 0; L < max_L; L++) {
        m_scaleVal[L] = sqrt(1+K*K);
#ifdef  DUMP_CORDIC
        dump(fH_scaleVal, m_scaleVal[L]);
#endif
        m_cordic_table[L] = atan(K) / PI;
#ifdef  DUMP_CORDIC
                dump(fH_cordic_table, m_cordic_table[L]);
#endif
        K *= 0.5;
    }
    m_max_L = max_L;
    m_scale = true;

        return 1;    // success
}

// CORDIC engine in vectoring mode
template <class T>
double ClsCORDIC<T>::vector(Complex<double> input_n, int L)
{
    if (sizeof(T) == sizeof(double)) return( atan2(input_n.Q, input_n.I)/PI);
    T K;
        //T real, imag; //use default

    m_hpi = HALF_PI / PI;
    m_acc_phase_rads = 0;
    Complex<T>  input_int;
    fix_set(input_int,16,16);

#ifdef  DUMP_CORDIC
    m_real = input_n.I; input_n.I = m_real;
    m_imag = input_n.Q; input_n.Q = m_imag;
    dump(f_in,m_real);
    dump(f_in,m_imag);
#endif

    Complex<double> t_input = normalize(input_n);
    input_int=t_input * DOUBLE_POW2[64+16-1];

    Complex<T>  input_new;
    fix_set(input_new,16,16);
//  amplify to full scale
    if ((fabs(input_int.I)>=DOUBLE_POW2[64+14])||(fabs(input_int.Q)>=DOUBLE_POW2[64+14]))
             input_new=input_int;
    else if ((fabs(input_int.I)>=DOUBLE_POW2[64+12])||(fabs(input_int.Q)>=DOUBLE_POW2[64+12]))
             input_new=input_int*DOUBLE_POW2[64+1];
    else if ((fabs(input_int.I)>=DOUBLE_POW2[64+10])||(fabs(input_int.Q)>=DOUBLE_POW2[64+10]))
                 input_new=input_int*DOUBLE_POW2[64+3];
    else if ((fabs(input_int.I)>=DOUBLE_POW2[64+8])||(fabs(input_int.Q)>=DOUBLE_POW2[64+8]))
             input_new=input_int*DOUBLE_POW2[64+5];
    else if ((fabs(input_int.I)>=DOUBLE_POW2[64+6])||(fabs(input_int.Q)>=DOUBLE_POW2[64+6]))
             input_new=input_int*DOUBLE_POW2[64+7];
    else if ((fabs(input_int.I)>=DOUBLE_POW2[64+4])||(fabs(input_int.Q)>=DOUBLE_POW2[64+4]))
             input_new=input_int*DOUBLE_POW2[64+9];
    else if ((fabs(input_int.I)>=DOUBLE_POW2[64+2])||(fabs(input_int.Q)>=DOUBLE_POW2[64+2]))
             input_new=input_int*DOUBLE_POW2[64+11];
    else
             input_new=input_int*DOUBLE_POW2[64+13];

    m_real = input_new.I;
    m_imag = input_new.Q;

#ifdef  DUMP_CORDIC
    dump(f_new_in,m_real);
    dump(f_new_in,m_imag);
#endif

    if ((input_new.I==0)&&(input_new.Q==0))
    {
                //printf("Warning of Cordic. Input is 0 + 0i, Output is default to 0(pi).");
        return (0.0);
    }

    if(input_new.I < 0) {
        m_real = -1.0*m_real;
        m_imag = -1.0*m_imag;
        //m_acc_phase_rads += (m_hpi*2.0);
    }

    K = T(1.0);
        // iterative rotating (multiply 1+j*K or 1-j*K)
    m_acc_I = m_real;
    m_acc_Q = m_imag;
    for (int i=0; i < L; i++) {
        m_shift_I = m_acc_Q * K;
        m_shift_Q = m_acc_I * K;

        if (m_acc_Q >= 0.0) {
                        // do negative rotation (multiply 1-j*K)
            m_acc_I = m_acc_I + m_shift_I;
            m_acc_Q = m_acc_Q - m_shift_Q;
            m_acc_phase_rads += m_cordic_table[i];
        }
        else {
                        // do positive rotation (multiply 1+j*K)
            m_acc_I = m_acc_I - m_shift_I;
            m_acc_Q = m_acc_Q + m_shift_Q;
            m_acc_phase_rads -= m_cordic_table[i];
        }
        K /= 2.0;
#ifdef DUMP_CORDIC
       m_acc_phase_rads_rtl = m_acc_phase_rads;
       dump(fH_angle,m_acc_phase_rads_rtl)<<std::endl;
       dump(fH_x,m_acc_I)<<std::endl;
       dump(fH_y,m_acc_Q)<<std::endl;
       dump(f_shift,m_shift_I);
       dump(f_shift,m_shift_Q);
#endif
    }
    if (input_new.Q==0)
            m_acc_phase_rads = (input_new.I < 0)? double(m_hpi*2.0): 0;                   // PI or o
    else if (input_new.I==0)
            m_acc_phase_rads = (input_new.Q < 0)? double(m_hpi*3.0): double(m_hpi*1.0);   // 3/2PI or 1/2PI
    else
        m_acc_phase_rads = (input_new.I < 0)? double(m_acc_phase_rads+(m_hpi*2.0)): double( m_acc_phase_rads);

    m_output_phase =  m_acc_phase_rads;
    return (m_output_phase);
}

// CORDIC engine in rotation mode
template <class T>
int ClsCORDIC<T>::rotate(Complex<T> &output, Complex<T>& input, T phase_rads, int L)
{
    int i;

    T acc_phase_rads, desired_phase_rads;
    T hpi;
    T K;
    T real, imag;

    hpi = HALF_PI;

    assert(L <= m_max_L);
    desired_phase_rads = phase_rads;
    output = input;
        // first rotate to (-90, 90)
    if(desired_phase_rads >= hpi)
    {
        while(desired_phase_rads >= hpi)
        {
            real = -output.Q;
            imag =  output.I;
            desired_phase_rads -= hpi;
            output.I = real;
            output.Q = imag;
        }
    }
    else if(desired_phase_rads <= -hpi)
    {
        while(desired_phase_rads < 0)
        {
            real =  output.Q;
            imag = -output.I;
            desired_phase_rads += hpi;
            output.I = real;
            output.Q = imag;
        }
    }

    if(desired_phase_rads == T(0))
        return 1;

    acc_phase_rads = 0;
    K = T(1.0);
        // iterative rotating (multiply 1+j*K or 1-j*K)
    for (i=0; i < L; i++) {
        if (desired_phase_rads - acc_phase_rads < 0.0) {
                        // do negative rotation
            real = output.I + output.Q * K;
            imag = output.Q - output.I * K;
            acc_phase_rads -= m_cordic_table[i];
        }
        else {
                        // do positive rotation
            real = output.I - output.Q * K;
            imag = output.Q + output.I * K;
            acc_phase_rads += m_cordic_table[i];
        }
        output.I = real;
        output.Q = imag;
        K /= 2.0;
    }

        // Scale
    T K1;
    if(m_scale) {
        K1 = T(1.0);
        for (i=0; i < L; i++) {
            K1 *= m_scaleVal[i];
        }
        output.I /= K1;
        output.Q /= K1;
    }

    return 1;
}

template <class T>
Complex<double> ClsCORDIC<T>::normalize(Complex<double> input)
{
    if (input.power()==0)
        return (Complex<double>(1.0,0.0));
    double max;
    max = (fabs(input.I) >= fabs(input.Q))? (input.I): (input.Q);
    int bitwidth;
    double tmp = log(fabs(max))/log(2.0);
    bitwidth = int(floor(tmp) + 1);
        // For case of (-0.5 ,0.1) -> (-1,0.2);
    if ((floor(tmp) == tmp)&&(max < 0))
        bitwidth --;

    Complex<double> output;

    output.I = input.I*DOUBLE_POW2[64-bitwidth];
    output.Q = input.Q*DOUBLE_POW2[64-bitwidth];

    return output;
}

#endif
