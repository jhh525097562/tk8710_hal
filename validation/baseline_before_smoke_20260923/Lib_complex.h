/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File              : lib_complex.h
**  Created By    : Alan.Niu
**  Created On    : 11/25/2019
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**  Notice:  ���ڸ����⺯��
****************************************************************************/
#ifndef __COMPLEX_H
#define __COMPLEX_H

#include <iostream>
//#include <iomanip>
#include <fstream>
#include <cmath>
#include <cassert>
#include "Lib_fixpoint.h"

struct Float_struct {
    double frac;
    int exp;
};

//#if _MSC_VER < 1350        //linux version
//#else
template <class T>
class Complex;
template <class T>
Complex<T> operator+(const Complex<T>&, const Complex<T>&);    //Complex + Complex
template <class T>
Complex<T> operator+(const Complex<T>&, const T&);        //Complex + T
template <class T>
Complex<T> operator+(const T&, const Complex<T>&);        //T + Complex
template <class T>
Complex<T> operator-(const Complex<T>&, const Complex<T>&);    //Complex - Complex
template <class T>
Complex<T> operator-(const Complex<T>&, const T&);        //Complex - T
template <class T>
Complex<T> operator-(const T&, const Complex<T>&);        //T - Complex
template <class T>
Complex<T> operator*(const Complex<T>&, const Complex<T>&);        //Complex * Complex
template <class T>
Complex<T> operator*(const double&, const Complex<T>&);            //double * Complex
template <class T>
Complex<T> operator*(const Complex<T>&, const double&);            //Complex  * double
template <class T>
Complex<T> operator/(const Complex<T>&, const double&);
//bool operator == (const Complex<double>& lhs, const Complex<double>& rhs);
//bool operator != (const Complex<double>& lhs, const Complex<double>& rhs);
//#endif

template <class T>
class Complex
{
    public:
        Complex(){ ;}                                    //construction function1
        Complex(T a){I = a; Q = T (0);}                //construction function2
        Complex(T a, T b) {I = a; Q = b;}                        //construction function3
        Complex(const Complex<T> &a){I = a.I; Q = a.Q; }            //construction function4

       // assignment operator
        Complex<T>& operator = (const double& d){I = T(d);Q = T(0);return *this;}
        Complex<T>& operator = (const Complex<double>& d){
            I = double(d.I);
            Q = double(d.Q);
            /*I_f.exp = floor(log2(fabs(I)));
            if(I == 0)
                I_f.exp = 0;
            Q_f.exp = floor(log2(fabs(Q)));
            if(Q == 0)
                Q_f.exp = 0;
            I_f.frac = I * DOUBLE_POW2[64 - (int)I_f.exp];
            Q_f.frac = Q * DOUBLE_POW2[64 - (int)Q_f.exp];*/
            //std::cout << Q_f.frac << "\t" << Q_f.exp << std::endl;
            return *this;}
        Complex<T>& operator = (const Complex<fxp   >& d){I = T(d.I);Q= T(d.Q);return *this;}
        T& real()    { return I; }                                            //return the real part of a complx
        T& imag()    { return Q; }                                            //return the image part of a complx
        double abs();
        double abs_a();
        double abs_b();
        double power();
        double rsqrt();
        Complex<T> floatmult(T in);
        Complex<T> conjug() {return Complex<T>(I,-Q); }                        //conjuge a complx

        Complex<T>& operator>>=(const int nbit);                                //Complex >>= nbits
        Complex<T>& operator<<=(const int nbit);                                //Complex <<= nbits
        Complex<T> operator>>(const int nbit);                                //Complex >> nbits
        Complex<T> operator<<(const int nbit);                                //Complex << nbits

        Complex<T>& operator+=(const Complex<T>& rhs);                            //Complex += Complex
        Complex<T>& operator-=(const Complex<T>& rhs);                            //Complex -= Complex

        Complex<double> todouble(){return Complex<double>(double(I), double(Q));}
        Complex<double>  mult(double in);
        Complex<double>  mult(Complex<double> in);
        Float_struct  mult_float(Float_struct in_1, Float_struct in_2);
        Float_struct  set_float(double in);
        double   get_float(Float_struct in);
        double   add(Float_struct in_1, Float_struct in_2);
        double   minus(Float_struct in_1, Float_struct in_2);
//        operator double() const;

#if    _MSC_VER < 1350
//        friend Complex<T> operator+(const Complex<T>& lhs,const Complex<T>& rhs);    //Complex + Complex
//        friend Complex<T> operator+(const Complex<T>& lhs,const T& rhs);        //Complex + T
//        friend Complex<T> operator+(const T& lhs,const Complex<T>& rhs);        //T + Complex

//        friend Complex<T> operator-(const Complex<T>& lhs,const Complex<T>& rhs);    //Complex - Complex
//        friend Complex<T> operator-(const Complex<T>& lhs,const T& rhs);        //Complex - T
//        friend Complex<T> operator-(const T& lhs,const Complex<T>& rhs);        //T - Complex

//        friend Complex<T> operator*(const Complex<T>& lhs, const Complex<T>& rhs);        //Complex * Complex
//        friend Complex<T> operator*(const double& lhs, const Complex<T>& rhs);            //double * Complex
//        friend Complex<T> operator*(const Complex<T>& rhs, const double& lhs);            //Complex  * double

//        friend Complex<T> operator/(const Complex<T>& lhs, const double& rhs);
//        friend bool operator == (const Complex<double>& lhs,const Complex<double>& rhs);
//        friend bool operator != (const Complex<double>& lhs,const Complex<double>& rhs);
#else
        //friend Complex<T> operator+<>(const Complex<T>& lhs,const Complex<T>& rhs);    //Complex + Complex
        //friend Complex<T> operator+<>(const Complex<T>& lhs,const T& rhs);        //Complex + T
        //friend Complex<T> operator+<>(const T& lhs,const Complex<T>& rhs);        //T + Complex
        //
        //friend Complex<T> operator-<>(const Complex<T>& lhs,const Complex<T>& rhs);    //Complex - Complex
        //friend Complex<T> operator-<>(const Complex<T>& lhs,const T& rhs);        //Complex - T
        //friend Complex<T> operator-<>(const T& lhs,const Complex<T>& rhs);        //T - Complex
        //
        //friend Complex<T> operator*<>(const Complex<T>& lhs, const Complex<T>& rhs);        //Complex * Complex
        //friend Complex<T> operator*<>(const Complex<T>& lhs, const double& rhs);            //Complex * T
        //friend Complex<T> operator*<>(const double& lhs, const Complex<T>& rhs);            //T * Complex
        //
        //friend Complex<T> operator/<>(const Complex<T>& lhs, const double& rhs);
        //friend bool     operator == (const Complex<double>& lhs, const Complex<double>& rhs);
        //friend bool     operator != (const Complex<double>& lhs, const Complex<double>& rhs);
#endif
        Float_struct I_f;
        Float_struct Q_f;

        T I;// = 0;                                                                //the real part of a complx
        T Q;// = 0;                                                                //the image part of a complx
};

template <class T>
inline double Complex<T>::power()
{
    return(I*I + Q*Q);
}

template <class T>
inline Complex<double> Complex<T>::mult(double in)
{
    Float_struct in_f;
    in_f = set_float(in);
    I_f = mult_float(I_f, in_f);
    Q_f = mult_float(Q_f, in_f);
    I = get_float(I_f);
    Q = get_float(Q_f);
    return *this;
}

template <class T>
inline double Complex<T>::add(Float_struct in_1, Float_struct in_2)
{
    Float_struct out;
    double temp;
    int exp_temp;
    T frac;

    if(in_1.frac == 0 || in_2.frac == 0) {
        if(in_1.frac == 0 && in_2.frac == 0) {
            out.exp = 0;
            out.frac = 0;
        } else if(in_1.frac == 0) {
            out.exp = in_2.exp;
            out.frac = in_2.frac;
        } else if(in_2.frac == 0) {
            out.exp = in_1.exp;
            out.frac = in_1.frac;
        }
        return out;
    }

    if(in_1.exp > in_2.exp) {
        temp = in_2.frac * DOUBLE_POW2[64 - in_1.exp + in_2.exp];
        temp = in_1.frac + temp;
        exp_temp = floor(log2(fabs(temp)));
        frac = temp * DOUBLE_POW2[64 - exp_temp];
        exp_temp = in_1.exp + exp_temp;
    } else {
        temp = in_1.frac * DOUBLE_POW2[64 - in_2.exp + in_1.exp];
        temp = temp + in_2.frac;
        exp_temp = floor(log2(fabs(temp)));
        frac = temp * DOUBLE_POW2[64 - exp_temp];
        exp_temp = in_2.exp + exp_temp;
    }
    out.exp = exp_temp;
    out.frac = frac;
    return out;
}

template <class T>
inline double Complex<T>::minus(Float_struct in_1, Float_struct in_2)
{
    Float_struct out;

    in_2.frac = in_2.frac * -1.0;
    out = add(in_1, in_2);
    return out;
}

template <class T>
inline Complex<double> Complex<T>::mult(Complex<double> in)
{
    Float_struct in_f;
    Float_struct temp_1, temp_2;

    temp_1 = mult_float(I_f, in.I_f);
    temp_2 = mult_float(Q_f, in.Q_f);
    I_f = minus(temp_1, temp_2);

    temp_1 = mult_float(Q_f, in.I_f);
    temp_2 = mult_float(I_f, in.Q_f);
    Q_f = add(temp_1, temp_2);

    I = get_float(I_f);
    Q = get_float(Q_f);
    return *this;
}

template <class T>
Float_struct Complex<T>::mult_float(Float_struct in_1, Float_struct in_2)
{
    Float_struct out;
    int exp;
    double temp;
    int exp_temp;

    T frac;

    temp = in_1.frac * in_2.frac;
    if(temp == 0) {
        out.exp = 0;
        out.frac = 0;
        return out;
    }
    exp_temp = floor(log2(fabs(temp)));
    frac = temp * DOUBLE_POW2[64 - exp_temp];
    exp = in_1.exp + in_2.exp + exp_temp;

    out.exp = exp;
    out.frac = frac;
    return out;
}

template <class T>
Float_struct Complex<T>::set_float(double in)
{
    Float_struct out;
    int exp;
    T frac;

    if(in == 0) {
        out.exp = 0;
        out.frac = 0;
        return out;
    }

    exp = floor(log2(fabs(in)));
    frac = in * DOUBLE_POW2[64 - exp];

    out.exp = exp;
    out.frac = frac;
    return out;
}

template <class T>
double Complex<T>::get_float(Float_struct in)
{
    double out;

    out = in.frac * DOUBLE_POW2[64 + in.exp];
    return out;
}

template <class T>
inline double Complex<T>::rsqrt()
{
    double number;
    int i;
    float x2, y;
    const float threehalfs = 1.5F;
    double out;

    number = I * I + Q * Q;
    x2 = number * 0.5F;
    y  = number;
    i  = * ( int * ) &y;                       // evil floating point bit level hacking
    i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
    y  = * ( float * ) &i;
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed
    y  = y * ( threehalfs - ( x2 * y * y ) );

    out = y;
    return out;
}

template <class T>
inline double Complex<T>::abs()
{
    return(sqrt(I*I + Q*Q));
}

template <class T>
inline double Complex<T>::abs_a()
{
    double m, M;
    double re, im;
    double out;
    re = (I > 0)? I:-I;
    im = (Q > 0)? Q:-Q;

    M = (re > im)? re:im;
    m = (re > im)? im:re;

    out = (M > 2.0 * m) ? 8.0 * M + 2.0 * m: 7.0 * M + 4.0 * m ;

    return(out / 8.0);

}

template <class T>
inline double Complex<T>::abs_b()
{
    double re, im;
    double out;
    re = (I > 0)? I:-I;
    im = (Q > 0)? Q:-Q;
    out =(re > im)?  (re + im/2): (im + re/2);
    return out;
}

template <class T>
inline Complex<T>& Complex<T>::operator+=(const Complex<T>& rhs)
{
    T tempI, tempQ;
    tempI = I + rhs.I;
    tempQ = Q + rhs.Q;
    I = tempI;
    Q = tempQ;
    return(*this);
}

template <class T>
inline Complex<T>& Complex<T>::operator-=(const Complex<T>& rhs)
{
    I -= rhs.I;
    Q -= rhs.Q;
    return(*this);
}

template <class T>
inline Complex<T> Complex<T>::operator>>(const int nbit)
{
    assert(nbit < sizeof(T) * 8);
    return( Complex(I>>nbit,Q>>nbit));
}

template <class T>
inline Complex<T> Complex<T>::operator<<(const int nbit)
{
    assert(nbit < sizeof(T) * 8);
    return( Complex<T>(I<<nbit,Q<<nbit));
}

template <class T>
inline Complex<T>& Complex<T>::operator>>=(const int nbit)
{
    assert(nbit < sizeof(T) * 8);
    I = I>>nbit;
    Q = Q>>nbit;
    return(*this);
}

template <class T>
inline Complex<T>& Complex<T>::operator<<=(const int nbit)
{
    assert(nbit < sizeof(T) * 8);
    I = I<<nbit;
    Q = Q<<nbit;
    return(*this);
}

template <class T>
inline Complex<T> operator+(const Complex<T>& lhs, const Complex<T>& rhs)
{
    T tempI;
    T tempQ;
    tempI = lhs.I + rhs.I;
    tempQ = lhs.Q + rhs.Q;
    Complex<T> temp;
    temp = Complex<T>(tempI, tempQ);
    return (temp);
}

template <class T>
inline Complex<T> operator+(const Complex<T>& lhs, const T& rhs)
{
    T tempI;
    T tempQ;
    tempI = lhs.I + rhs;
    tempQ = lhs.Q;
    Complex<T> temp;
    temp = Complex<T>(tempI, tempQ);
    return (temp);
}

template <class T>
inline Complex<T> operator+(const T& lhs, const Complex<T>& rhs)
{
    T tempI;
    T tempQ;
    tempI = lhs + rhs.I;
    tempQ = rhs.Q;
    Complex<T> temp;
    temp = Complex<T>(tempI, tempQ);
    return (temp);
}

template <class T>
inline Complex<T> operator-(const Complex<T>& lhs, const Complex<T>& rhs)
{
    Complex<T> temp;
    temp.I = lhs.I - rhs.I;
    temp.Q = lhs.Q - rhs.Q;
    return (temp);
}

template <class T>
inline Complex<T> operator-(const Complex<T>& lhs, const T& rhs)
{
    Complex<T> temp;
    temp.I = lhs.I - rhs;
    temp.Q = lhs.Q ;
    return (temp);
}

template <class T>
inline Complex<T> operator-(const T& lhs, const Complex<T>& rhs)
{
    Complex<T> temp;
    temp.I = lhs - rhs.I;
    temp.Q = -rhs.Q ;
    return (temp);
}

template <class T>
inline Complex<T> operator* (const Complex<T>& lhs, const Complex<T>& rhs)
{
    Complex<T> temp;
    temp.I = lhs.I * rhs.I - lhs.Q * rhs.Q;
    temp.Q = lhs.Q * rhs.I + lhs.I * rhs.Q;
    return (temp);
}

template <class T>
inline Complex<T> operator* (const double& lhs, const Complex<T>& rhs)
{
    Complex<T> temp;
    temp.I = rhs.I * lhs;
    temp.Q = rhs.Q * lhs;
    return (temp);
}

template <class T>
inline Complex<T> operator* (const Complex<T>& rhs, const double& lhs)
{
    Complex<T> temp;
    temp.I = rhs.I * lhs;
    temp.Q = rhs.Q * lhs;
    return (temp);
}

template <class T>
inline Complex<T> operator/(const Complex<T>& lhs, const double& rhs)
{
    Complex<T> temp;
    temp.I = lhs.I / rhs;
    temp.Q = lhs.Q / rhs;
    return (temp);
}

inline bool operator == (const Complex<double>& lhs, const Complex<double>& rhs) { return ((lhs.I==rhs.I)&&(lhs.Q==rhs.Q));}
inline bool operator != (const Complex<double>& lhs, const Complex<double>& rhs) { return (!((lhs.I==rhs.I)&&(lhs.Q==rhs.Q)));}

#endif







