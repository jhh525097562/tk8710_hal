/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File          : Cls_FIR.h (FIR filter)
**  Created By    : Darren
**  Created On    : 06/08/2020
**
**  History     :
**  Programmer    Date         Rev          Description
**  Darren        07/08/2020   v1.0
**
**  Notice:
**
****************************************************************************/
#pragma once
#include "Lib_io.h"
#include <iostream>
#include<iomanip>
using namespace std;

enum   FIR_Coeff { TX_MFIR = 0, TX_HFIR = 1, RX_ACF_FIR = 2, RX_HALF_FIR = 3, RX_MFIR = 4, TX_2_MFIR = 5, SDM_FIR = 6};
template <class T>
class Cls_FIR
{
public:
    // initialization
    Cls_FIR()
	{
        m_coeff = NULL;
        t_coef = NULL;
        t_reg = NULL;
	}
    ~Cls_FIR() {
        free(m_coeff);
        free(t_coef);
        free(t_reg);
    }

    // data shift in and filter out, direct form
    Complex<T> dFilt(const Complex<double>& dataIn);

    void init (FIR_Coeff coeff_type);
    void reset ();
private:
    unsigned int            m_fir_len;// = 0;
    double*                 t_coef;
    double*                 m_coeff;
    Complex<double>*        t_reg;
};

template <class T>
void Cls_FIR<T>::init (FIR_Coeff coeff_type)
{
    unsigned int ck0 = 0;
    double *coeff;
    coeff = NULL;
    coeff = safe_new(coeff,1024*4);
    m_coeff = safe_new(m_coeff, 1024);
    t_coef = safe_new(t_coef, 1024);
    t_reg = safe_new(t_reg, 1024);

    char fileName[300];
    strcpy(fileName, "/home/hrren/work/K2201/alg/c_model/fir_coeff/");

    switch (coeff_type)
    {
    case TX_MFIR:
        strcat(fileName, "tx_match_fir.dat");
        break;
    case TX_HFIR:
        strcat(fileName, "tx_half_fir.dat");
        break;
    case RX_ACF_FIR:
        strcat(fileName, "rx_acf_fir.dat");
        break;
    case RX_HALF_FIR:
        strcat(fileName, "rx_half_fir_new.dat");
        break;
    case RX_MFIR:
        strcat(fileName, "rx_match_fir.dat");
        break;
    case TX_2_MFIR:
        strcat(fileName, "tx_phy2_match_fir.dat");
        break;
    case SDM_FIR:
        strcat(fileName, "sdm_fir.dat");
        break;
    default:
        std::cout << "ACF Cannot open filter coefficient file. \n";
        std::cout << "error acf mode\n";
        exit(0);
        return;
    }

    std::ifstream coeffFile;
    coeffFile.open(fileName);
    if (!coeffFile) {
        std::cout << "ACF Cannot open filter coefficient file. \n";
        exit(0);
    }

    int j = 0;
    while(!coeffFile.eof()){
        coeffFile >> coeff[j];
        j++;
    }

    if (j > 1024 * 4) {
        std::cout << "coeff length over limit\n";
        exit(0);
    }
    coeffFile.close();

    m_fir_len = j;

    for(ck0 = 0; ck0 < m_fir_len; ck0++)
    {
        m_coeff[ck0] = coeff[ck0];
    }
    mem_copy(t_coef, m_coeff, m_fir_len);

    fix_set_group(t_reg, m_fir_len, 16, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set( m_FXP_A0[ck0],strcat(featurechar,stringchar),true, 256);

    reset();
    free(coeff);

}

template <class T>
void Cls_FIR<T>::reset()
{
    zeros(t_reg, m_fir_len);
    mem_copy(t_coef, m_coeff, m_fir_len);
}

template <class T>
Complex<T>  Cls_FIR<T> ::dFilt (const Complex<double>& dataIn)
{
    unsigned int i;
    Complex<T> tmpout, dataOut;
    Complex<T> tmp;

    for (i=m_fir_len - 1; i>0; i--)    {
        t_reg[i] = t_reg[i - 1];
    }
    t_reg[0] = dataIn;

    tmpout.I = double(0);
    tmpout.Q = double(0);

    for (i=0; i<m_fir_len; i++)
    {
        tmp.I = (t_reg[i].I * t_coef[i]);
        tmp.Q = (t_reg[i].Q * t_coef[i]);
        tmpout += tmp;
    }

    dataOut = tmpout;

    return (dataOut);
}


