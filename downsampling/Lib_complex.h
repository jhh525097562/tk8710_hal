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
**  Notice:  ÊôÓÚ¸½Êô¿âº¯Êý
****************************************************************************/
#ifndef __COMPLEX_H
#define __COMPLEX_H

#include <iostream>
//#include <iomanip>
#include <fstream>
#include <cmath>
#include <cassert>
#include "Lib_fixpoint.h"

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
            I = T(d.I);
            Q= T(d.Q);
            return *this;}
        Complex<T>& operator = (const Complex<fxp   >& d){I = T(d.I);Q= T(d.Q);return *this;}
        T& real()    { return I; }                                            //return the real part of a complx
        T& imag()    { return Q; }                                            //return the image part of a complx
        double abs();
        double abs_a();
        double abs_b();
        double power();
        Complex<T> conjug() {return Complex<T>(I,-Q); }                        //conjuge a complx

        Complex<T>& operator>>=(const int nbit);                                //Complex >>= nbits
        Complex<T>& operator<<=(const int nbit);                                //Complex <<= nbits
        Complex<T> operator>>(const int nbit);                                //Complex >> nbits
        Complex<T> operator<<(const int nbit);                                //Complex << nbits

        Complex<T>& operator+=(const Complex<T>& rhs);                            //Complex += Complex
        Complex<T>& operator-=(const Complex<T>& rhs);                            //Complex -= Complex

        Complex<double> todouble(){return Complex<double>(double(I), double(Q));}
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


        T I;// = 0;                                                                //the real part of a complx
        T Q;// = 0;                                                                //the image part of a complx
};

template <class T>
inline double Complex<T>::power()
{
    return(I*I + Q*Q);
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







