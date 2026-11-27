/****************************************************************************
**
**  Copyright  (c)  2008,  Amlogic, Inc.
**          All Rights Reserved.
**
**  Subsystem   :
**  File            : fir.h
**  Created By    : Alan Niu
**  Created On    : 11/03/2008
**
**
**  History     : Modify from Cindy Wang, 04/28/2004
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
** *************************************************************************/
#ifndef __FIR_CMPLX_H
#define __FIR_CMPLX_H

const int MAX_TAPS = 16;
#include "Lib_complex.h"
#include "Lib_fixpoint.h"

template <class T>
class ClsFir_Cmplx
{

    int         m_halfTaps;
    int         m_taps;                // filter taps
    Complex<T>  *m_coeff;                // filter coefficient

public:
    Complex<double>  m_dataOut;
    Complex<double> *m_shftReg;
    Complex<T>  m_dataOut_fxp;
    Complex<T>  m_shftReg_fxp;


    // initialization
    ClsFir_Cmplx () {
        m_shftReg = NULL;
        m_coeff   = NULL;
    }

    ~ClsFir_Cmplx ();

    void init (char *coeffFileName);
    void init (int tapsIn, Complex<T> *coeffIn);
    void init (int tapsIn);

    void init_fix_set (int shftReg_wl, int shftReg_iwl, int dataOut_wl, int dataOut_iwl);

    // initialize filter shift registers
    void reset ();
    // data shift in, direct form
    void dShftIn (const Complex<double>& dataIn);

    // filter out, direct form
    Complex<T> dFiltOut (void);
    Complex<T> dFiltOut (Complex<double> *coeff);
    // data shift in and filter out, direct form
    Complex<T> dFilt (const Complex<double>& dataIn);
    Complex<T> dFilt (const Complex<double>& dataIn, Complex<double> *coeff);

#ifdef DEBUG_ADPFILT
    ofstream out_adpFilt;
#endif
};

template <class T>
inline ClsFir_Cmplx<T>::~ClsFir_Cmplx ()
{
    free(m_shftReg);
    free(m_coeff);
#ifdef DEBUG_ADPFILT
    out_adpFilt.close();
#endif
}

// initialize the coefficient from a file
template <class T>
void ClsFir_Cmplx<T>::init (char *coeffFileName)
{

    std::ifstream coeffFile(coeffFileName);

    //PRINT2DEVICE << "\n***FIR Filter:\n";
    // read the filter coefficient
    if(!coeffFile)    {
        std::cout << "Cannot open filter coefficient file : " << coeffFileName << "\n";
        exit(1);
    }

    m_coeff = safe_new(m_coeff,MAX_TAPS );

    int j = 0;
    while(!coeffFile.eof())
    {
        coeffFile >> m_coeff[j].I;
        j++;
        coeffFile >> m_coeff[j].Q;
        j++;
    }

    if (j>MAX_TAPS)
        std::cerr << "coeff length over limit\n";

    coeffFile.close();

    m_taps = j - 1;
    m_halfTaps = m_taps/2;

    m_shftReg = safe_new(m_shftReg, m_taps);
    reset();

#ifdef DEBUG_ADPFILT
    out_adpFilt.open("dataOut\\debug_adpFilt.dat");
#endif
}

// initialize the coefficient from an array
template <class T>
void ClsFir_Cmplx<T>::init (int tapsIn, Complex<T> *coeffIn)
{
    int j;

    m_taps = tapsIn;
    m_halfTaps = m_taps/2;

    m_coeff = safe_new(m_coeff, m_taps);

    for(j=0; j<m_taps; j++)
    {
        m_coeff[j] = coeffIn[j];
    }

    m_shftReg = safe_new(m_shftReg, m_taps);
    reset();

}

template <class T>
void ClsFir_Cmplx<T>::init (int tapsIn)
{
    m_taps = tapsIn;
    m_halfTaps = m_taps/2;
    m_coeff = safe_new(m_coeff, m_taps);
    m_shftReg = safe_new(m_shftReg, m_taps);
    reset();
}

template <class T>
void ClsFir_Cmplx<T>::init_fix_set (int shftReg_wl, int shftReg_iwl, int dataOut_wl, int dataOut_iwl)
{
      fix_set(m_dataOut_fxp,dataOut_wl, dataOut_iwl);
      fix_set(m_shftReg_fxp,shftReg_wl, shftReg_iwl);
}

// reset shift registers
template <class T>
void  ClsFir_Cmplx<T>::reset ()
{
    zeros(m_shftReg,m_taps);
}

// data shift in, direct form
template <class T>
void ClsFir_Cmplx<T>::dShftIn (const Complex<double>& dataIn)
{
    register int i;

    for (i=m_taps - 1; i>0; i--)    {
        m_shftReg[i] = m_shftReg[i - 1];
    }
    m_shftReg[0] = dataIn;
}

// filter out, direct form
template <class T>
Complex<T> ClsFir_Cmplx<T>::dFiltOut (void)
{
    int i;
    m_dataOut = 0.0;
    for (i=0; i<m_taps; i++)
    {
        m_dataOut = m_dataOut + (m_shftReg[i] * m_coeff[i]);
    }
    return(m_dataOut);
}

template <class T>
Complex<T> ClsFir_Cmplx<T>::dFiltOut (Complex<double> *coeff)
{
    int i;
    m_dataOut = 0.0;
    for (i=0; i<m_taps; i++)
    {
        m_shftReg_fxp = m_shftReg[i];
        m_dataOut = m_dataOut + (m_shftReg_fxp.todouble() * coeff[i]);
    }
    m_dataOut_fxp = m_dataOut;
    return(m_dataOut_fxp);
}

// data shift in and filter out, direct form
template <class T>
Complex<T> ClsFir_Cmplx<T>::dFilt (const Complex<double>& dataIn)
{
    dShftIn(dataIn);
    return (dFiltOut());
}

template <class T>
Complex<T> ClsFir_Cmplx<T>::dFilt (const Complex<double>& dataIn, Complex<double> *coeff)
{
    dShftIn(dataIn);
    return (dFiltOut(coeff));
}


#endif

