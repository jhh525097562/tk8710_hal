/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File          : Cls_tx_phy_2_1_encode.h (PHY 2.1 TX Data Generation)
**  Created By    : Darren
**  Created On    : 29/04/2022
**
**  History     :
**  Programmer    Date         Rev          Description
**  Darren        29/04/2022   v1.0         Initial
**
**  Notice:
**
****************************************************************************/

#pragma once

#define pi 3.141592653589793

#define DUMP_MATLAB
#include <cmath>
#include "Lib_complex.h"
#include "Lib_fixpoint.h"
#include "Lib_random.h"
#include "Lib_utility.h"
#include "common_type.h"
#include "Lib_dump.h"
#include <iomanip>
using namespace std;
//#define CRCLen 14

typedef struct TX_MOD_CFG
{
    int len_infobit;
    int SymNumData;
    int LenSymPhase;
    double TransRatio;
    int DSSSNum;
    int BlockNum;
    int reg_agc_len;
    int reg_agc_num;

}TX_MOD_CFG;


template <class T>
class Cls_tx_mod
{
public:
    Cls_tx_mod() {

    m_sigDPFSKtempArr = NULL;
    m_FskPhase = NULL;
    m_sigDPFSKtempArrDsss = NULL;
    m_sigDPFSKtempArrTS = NULL;
    m_sigPreArr = NULL;
    m_sigPreArrR = NULL;
    m_sigPreArrL = NULL;
    m_halfWin = NULL;
    m_WinR = NULL;
    m_WinL = NULL;
    m_DSeqPhase = NULL;
    m_DSeq = NULL;
    m_FirstPreamble = NULL;
    m_FirstPreamble_ts = NULL;

    }
    ~Cls_tx_mod()   {
    free(m_sigPreArr);
    free(m_sigDPFSKtempArr);
    free(m_FskPhase);
    free(m_sigDPFSKtempArrDsss);
    free(m_sigDPFSKtempArrTS);
    free(m_sigPreArrR);
    free(m_sigPreArrL);
    free(m_halfWin);
    free(m_WinR);
    free(m_WinL);
    free(m_DSeqPhase);
    free(m_DSeq);
    free(m_FirstPreamble);
    free(m_FirstPreamble_ts);
#ifdef DUMP_MATLAB
    fp_mat_sigTx.close();
#endif
    }
    void init(char* module_name);
    void go(int *in1,int *in2,double *out);
    void reset();
    void config(TX_MOD_CFG cfg);
    int m_tx_len;
    int m_iblock;
    int m_outlen;
    int m_agc_len;
    int m_tone_len;
private:
    TX_MOD_CFG m_cfg;

    int m_len_infobit;
    int m_SymNumData;
    int m_LenSymPhase;
    T m_TransRatio;
    int m_DSSSNum;
    int m_BlockNum;
    int m_SymNumPreamble;
    //reg dump use
    T t_len_infobit;
    T t_SymNumData;
    T t_LenSymPhase;
    T t_DSSSNum;
    T t_iBlock;
    T t_BlockNum;
    T t_agc_len;


    int m_LenSym1 ;
    int m_rollLen ;// 1/2 1/4 1/8 符号周期
    int m_HalfWinLen ;
    int m_Ri ;
    int m_Li ;
    int m_Midi ;

    double **m_sigPreArr;
    double **m_sigDPFSKtempArr;
    double **m_FskPhase;
    double **m_sigDPFSKtempArrDsss;
    double **m_sigDPFSKtempArrTS;
    double *m_sigPreArrR;
    double *m_sigPreArrL;
    double *m_halfWin;
    double *m_WinR;
    double *m_WinL;
    double *m_DSeqPhase;
    double m_pi;
    double m_1_2pi;
    double m_Reference;
    T t_tmp ;
    T t_sigDPFSKtempArr;
    T t_halfWin_tmp;
    T t_FskPhase_tmp;
    T t_tmp2phase;
    T t_sigDPFSKtempArrDsss_tmp;
    T t_phaseTmp;
    T tmp2phase;
    T t_sigDPFSKtempArr_tmp;
    T t_DSeqPhase_tmp;
    T t_sigDPFSKtempArrDsss_tmp2;
    double phaseTmp;
    T t_sigPreArrR_tmp;
    T t_sigPreArrL_tmp;
    T t_sigDPFSKtempArrTS_tmp;
    T t_phase_tmp;
    T t_phase_tmp2;
    T t_phase_tmp3;
    int *m_DSeq;
    double m_phase_tmp;
    double *m_FirstPreamble;
    double *m_FirstPreamble_ts;
    T t_sigDPFSKtempArrDsss_tmp3;
    T t_cos;
    T m_agc_fxp;



    void GenFskFreqsTS(double **FskPhase);
    void GenDSeq(int *DSeq , int DSSSNum);
#ifdef DUMP_MATLAB
    std::ofstream           fp_mat_sigTx;
#endif
};

template<class T>
void Cls_tx_mod<T>::config(TX_MOD_CFG cfg)
{
    m_cfg = cfg;
}

template<class T>
void Cls_tx_mod<T>::init(char* module_name)
{

    m_len_infobit = m_cfg.len_infobit;
    m_SymNumData = m_cfg.SymNumData;
    m_LenSymPhase = m_cfg.LenSymPhase;
    m_TransRatio = m_cfg.TransRatio;
    m_DSSSNum = m_cfg.DSSSNum;
    m_BlockNum = m_cfg.BlockNum;
    m_agc_len = m_cfg.reg_agc_len;//*32/(1<<m_cfg.m_updata);
//    m_iblock = m_cfg.iblock;
    m_SymNumPreamble = 12;
    m_LenSym1 = m_LenSymPhase;
    m_rollLen = m_LenSym1*m_TransRatio ;// 1/2 1/4 1/8 符号周期
    m_HalfWinLen = m_rollLen/2 - 1;
    m_Ri = m_HalfWinLen*2;
    m_Li = m_HalfWinLen;
    m_Midi = m_LenSym1 - m_HalfWinLen;
    m_pi = 3.1416;
    m_1_2pi = 1.0/2*m_pi;

    m_sigPreArr = safe_new(m_sigPreArr,m_SymNumPreamble,m_LenSymPhase);
    m_sigDPFSKtempArr = safe_new(m_sigDPFSKtempArr, (m_SymNumData+m_SymNumPreamble),m_LenSymPhase);
    m_FskPhase = safe_new(m_FskPhase, m_LenSymPhase ,4);
    m_sigDPFSKtempArrDsss = safe_new(m_sigDPFSKtempArrDsss,(m_SymNumData+m_SymNumPreamble)*m_DSSSNum +1,m_LenSymPhase);
    m_sigDPFSKtempArrTS = safe_new(m_sigDPFSKtempArrTS,(m_SymNumData+m_SymNumPreamble)*m_DSSSNum +1,m_LenSymPhase);
    m_sigPreArrR = safe_new(m_sigPreArrR,m_Ri + 1);
    m_sigPreArrL = safe_new(m_sigPreArrL,m_Ri + 1);
    m_halfWin = safe_new(m_halfWin,m_rollLen + 1);
    m_WinR = safe_new(m_WinR,m_rollLen + 1);
    m_WinL = safe_new(m_WinL,m_rollLen + 1);
    m_DSeqPhase = safe_new(m_DSeqPhase,m_DSSSNum);
    m_DSeq = safe_new(m_DSeq,m_DSSSNum);

    m_FirstPreamble = safe_new(m_FirstPreamble, m_LenSym1);
    m_FirstPreamble_ts = safe_new(m_FirstPreamble_ts, m_LenSym1);


    fix_set(t_tmp, 10, 1, FXP_US, FXP_RND, FXP_WRAP);// statistic_set(t_tmp, "t_tmp", 1, 10);
    fix_set(t_halfWin_tmp,12,0,FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(t_halfWin_tmp, "t_halfWin_tmp", 1, 10);
    fix_set(t_FskPhase_tmp,10,1,FXP_TC, FXP_RND, FXP_WRAP);//statistic_set(t_FskPhase_tmp, "t_FskPhase_tmp", 1, 1);

    fix_set(t_sigDPFSKtempArr,10, 1, FXP_TC, FXP_RND, FXP_WRAP);//statistic_set(t_sigDPFSKtempArr, "t_sigDPFSKtempArr", 1, 100);
    fix_set(m_agc_fxp,10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(t_sigDPFSKtempArr, "t_sigDPFSKtempArr", 1, 100);
    fix_set(t_tmp2phase,10,1,FXP_TC, FXP_RND, FXP_WRAP);//statistic_set(t_tmp2phase, "t_tmp2phase", 1, 10);
    fix_set(t_sigDPFSKtempArr_tmp,10,1,FXP_TC, FXP_RND_INF, FXP_WRAP);//statistic_set(t_sigDPFSKtempArr_tmp, "t_sigDPFSKtempArr_tmp", 1, 1000);
    fix_set(t_sigDPFSKtempArrDsss_tmp,10,1,FXP_TC, FXP_RND, FXP_WRAP);//statistic_set(t_sigDPFSKtempArrDsss_tmp, "t_sigDPFSKtempArrDsss_tmp", 1, 1000);
//    fix_set(t_phaseTmp,13,9,FXP_TC, FXP_RND_INF, FXP_SAT_SYM);//statistic_set(t_phaseTmp, "t_phaseTmp", 1, 100);
    fix_set(t_sigDPFSKtempArrDsss_tmp2,13,4,FXP_TC, FXP_RND_INF, FXP_SAT_SYM);//statistic_set(t_sigDPFSKtempArrDsss_tmp2, "t_sigDPFSKtempArrDsss_tmp2", 1, 1000);
    fix_set(t_sigPreArrR_tmp,13,4,FXP_TC, FXP_RND_INF, FXP_SAT_SYM);//statistic_set(t_sigPreArrR_tmp, "t_sigPreArrR_tmp", 1, 1000);
    fix_set(t_sigPreArrL_tmp,13,4,FXP_TC, FXP_RND_INF, FXP_SAT_SYM);//statistic_set(t_sigPreArrL_tmp, "t_sigPreArrL_tmp", 1, 100);
    fix_set(t_sigDPFSKtempArrTS_tmp,10,1,FXP_TC, FXP_RND, FXP_WRAP);//statistic_set(t_sigDPFSKtempArrTS_tmp, "t_sigDPFSKtempArrTS_tmp", 1, 100);
    fix_set(t_sigDPFSKtempArrDsss_tmp3,12,3,FXP_TC, FXP_RND_INF, FXP_SAT_SYM);//statistic_set(t_sigDPFSKtempArrDsss_tmp3, "t_sigDPFSKtempArrDsss_tmp3", 1, 1000);
    fix_set(t_phase_tmp,11,2,FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(t_phase_tmp, "t_phase_tmp", 1, 1000);
    fix_set(t_cos,14,1,FXP_TC, FXP_RND_INF, FXP_SAT_SYM);//statistic_set(t_cos, "t_cos", 1, 10);

    //reg dump
    fix_set(t_len_infobit,10,10,FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(t_SymNumData,9,9,FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(t_LenSymPhase,10,10,FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(m_TransRatio,3,1,FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(t_DSSSNum,4,4,FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(t_BlockNum,8,8,FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(t_agc_len,20,20,FXP_US, FXP_RND, FXP_SAT_SYM);

#ifdef DUMP_MATLAB
    char filename_mat[300] = {};
    strcpy(filename_mat, module_name); strcat(filename_mat, "o_sigtx.txt");        fp_mat_sigTx.open(filename_mat);
#endif
    reset();
}

template<class T>
void Cls_tx_mod<T>::reset()
{
    //m_tmp = 1.0 * pi / m_rollLen;
    zeros(m_sigPreArr,m_SymNumPreamble,m_LenSymPhase);
    zeros(m_sigDPFSKtempArr, (m_SymNumData + m_SymNumPreamble),m_LenSymPhase);
    zeros(m_FskPhase, m_LenSymPhase ,4);
    zeros(m_sigDPFSKtempArrDsss,(m_SymNumData + m_SymNumPreamble)*m_DSSSNum +1,m_LenSymPhase);
    zeros(m_sigDPFSKtempArrTS,(m_SymNumData+m_SymNumPreamble)*m_DSSSNum +1,m_LenSymPhase);
    zeros(m_sigPreArrR,m_Ri + 1);
    zeros(m_sigPreArrL,m_Ri + 1);
    zeros(m_halfWin,m_rollLen + 1);
    zeros(m_WinR,m_rollLen - 1);
    zeros(m_WinL,m_rollLen - 1);
    zeros(m_DSeqPhase,m_DSSSNum);
    zeros(m_DSeq,m_DSSSNum);
    zeros(m_FirstPreamble, m_LenSym1);
    zeros(m_FirstPreamble_ts, m_LenSym1);
}

template<class T>
void Cls_tx_mod<T>::go(int *in1,int *in2,double *out)
{
    int i,j,k;
    int tmp1,tmp2;
    int GrayMap[4] = {0,1,3,2}; //GrayMap[16] = {0,1,3,2,7,6,4,5,15,14,12,13,8,9,11,10};
    int PhaseMod4Psk[4] = {0,1,-1,2};
    int FskPreamble[12] = {0,3,0,3,0,3,0,3,0,3,0,3};
    int PskPreamblePhase[12] = {-1,-1,0,-1,0,0,-1,0,0,-1,-1,-1};
    //m_agc_len = m_cfg.reg_agc_len;//*32/(1<<m_cfg.m_updata);
//    int Block_len = m_len_infobit;

        for(i = 0;i<=m_rollLen;i++)
        {
            t_tmp = 1.0/m_rollLen*i;
            t_cos = cos(t_tmp*PI);
            t_halfWin_tmp = 0.5 + 0.5*t_cos;
            m_halfWin[i] = t_halfWin_tmp;//cos, m_halfWin fix
        }

        for(i = 0;i<m_rollLen-1;i++)
        {
            m_WinR[i] = m_halfWin[i+1];
            m_WinL[m_rollLen-2 - i] = m_WinR[i];//m_WinL,m_WinR  fix
        }

        GenFskFreqsTS(m_FskPhase);//m_FskPhase  fix
        GenDSeq(m_DSeq,m_DSSSNum);
/*preamble部分*/
        for ( i = 0; i < m_SymNumPreamble; ++i)
        {
            for ( j = 0; j < m_LenSym1; ++j)
            {
                t_sigDPFSKtempArr =  m_FskPhase[j][FskPreamble[i]] + PskPreamblePhase[i]; //PskPreamblePhase[i]* m_pi;
                m_sigPreArr[i][j] =t_sigDPFSKtempArr;// m_FskPhase[j][FskPreamble[i]] + PskPreamblePhase[i]; //m_sigDPFSKtempArr,PskPreamblePhase  fix
            }
        }


    for(i = 0;i<m_SymNumPreamble;i++)
    {
        for(j = 0;j<m_LenSym1;j++)
        {
            m_sigDPFSKtempArr[i][j] = m_sigPreArr[i][j];
        }

    }
    m_Reference = PskPreamblePhase[m_SymNumPreamble-1];// PskPreamblePhase[m_SymNumPreamble-1]* m_pi;//Reference  fix
    for(i = 0;i<m_SymNumData;i++)
    {
        tmp1 = in1[2*i]*2 + in1[2*i+1];
        tmp2 = in2[2*i]*2 + in2[2*i+1];//00 0,01 1,10 2,11 3
        t_tmp2phase = m_Reference + (double)PhaseMod4Psk[tmp2]/2;
        tmp2phase = t_tmp2phase;//tmp2phase  fix
        for(j = 0;j<m_LenSym1;j++)
        {
            t_sigDPFSKtempArr_tmp = m_FskPhase[j][GrayMap[tmp1]] + tmp2phase;// *1/(3*pi)
            m_sigDPFSKtempArr[i + m_SymNumPreamble][j] = t_sigDPFSKtempArr_tmp;//m_FskPhase[j][GrayMap[tmp1]] + tmp2phase;//m_sigDPFSKtempArr ,m_FskPhase fix
        }
        m_Reference = tmp2phase;
    }

//DSSS
    for(i = 0;i<m_DSSSNum;i++)
    {
        m_DSeqPhase[i] = m_DSeq[i];
    }

    for(i = 0;i < (m_SymNumData+m_SymNumPreamble);i++)
    {
        for(k = 0;k<m_DSSSNum;k++)
        {
            for(j = 0;j < m_LenSym1;j++)
            {
                t_sigDPFSKtempArrDsss_tmp = m_sigDPFSKtempArr[i][j] + m_DSeqPhase[k];
                m_sigDPFSKtempArrDsss[i*m_DSSSNum + k ][j] =t_sigDPFSKtempArrDsss_tmp;//m_sigDPFSKtempArrDsss  fix
            }
        }
    }
//end countinue
    for(j = 0;j < m_LenSym1;j++)
    {
        m_sigDPFSKtempArrDsss[(m_SymNumData + m_SymNumPreamble)*m_DSSSNum ][j] =m_sigPreArr[0][j];//m_sigDPFSKtempArrDsss  fix
    }

    if(m_iblock > 0)
    {
            for(j = 0;j < m_LenSym1;j++)
        {
            m_sigDPFSKtempArrDsss[0][j] = m_FirstPreamble[j];
        }
    }


//Transition
    if(m_iblock == 0)
    {
        for(i = 0;i<m_LenSym1-m_HalfWinLen;i++)
        {
            m_sigDPFSKtempArrTS[0][i] = m_sigDPFSKtempArrDsss[0][i];//m_sigDPFSKtempArrTS fix
        }
    }
    else
    {
        for(i = 0;i<m_LenSym1-m_HalfWinLen;i++)
        {
            m_sigDPFSKtempArrTS[0][i] = m_FirstPreamble_ts[i];//m_sigDPFSKtempArrDsss[0][i];//m_sigDPFSKtempArrTS fix
        }
    }
        //symbal phase countinue
        double DSS_tmp;
        for(i = 0; i < (m_SymNumData+m_SymNumPreamble)*m_DSSSNum+1;i++)
        {
            for(j = 0; j < m_LenSym1 - 1;j++)
            {
                DSS_tmp = m_sigDPFSKtempArrDsss[i][j+1] - m_sigDPFSKtempArrDsss[i][j];
                if(DSS_tmp > 1)
                    {
                        t_sigDPFSKtempArrDsss_tmp3 = m_sigDPFSKtempArrDsss[i][j+1] - 2*(int(DSS_tmp/2) + 1);
                        m_sigDPFSKtempArrDsss[i][j+1] = t_sigDPFSKtempArrDsss_tmp3;
                    }
                if(DSS_tmp < -1)
                    {
                        t_sigDPFSKtempArrDsss_tmp3 = m_sigDPFSKtempArrDsss[i][j+1] + 2*(int(-1*DSS_tmp/2) + 1);
                        m_sigDPFSKtempArrDsss[i][j+1] = t_sigDPFSKtempArrDsss_tmp3;
                    }
#ifdef DUMP_MATLAB
                fp_mat_sigTx <<fixed<<setprecision(30)<< out[i*m_LenSym1 + j + m_agc_len] <<endl;
                fp_mat_sigTx <<fixed<<setprecision(30)<< out[i*m_LenSym1 + j + m_agc_len] <<endl;
#endif
            }

        }

        int move_count;
        for(i = 0;i<(m_SymNumData + m_SymNumPreamble)*m_DSSSNum ;i++)
        {
            move_count = 0;
            while(1)
            {
                //t_phaseTmp = m_sigDPFSKtempArrDsss[i+1][0] - m_sigDPFSKtempArrDsss[i][m_LenSym1-1];
                phaseTmp = m_sigDPFSKtempArrDsss[i+1][0] - m_sigDPFSKtempArrDsss[i][m_LenSym1-1];//sigPreArrPhase  sigPreArr(iSym,LenSym1)
                if (phaseTmp > 1)
                    {
                        for(j = 0;j<m_LenSym1;j++)
                        {
                            t_sigDPFSKtempArrDsss_tmp2 = m_sigDPFSKtempArrDsss[i+1][j] - 2*(int)((phaseTmp+1)/2);//zhongdian
                            m_sigDPFSKtempArrDsss[i+1][j] = t_sigDPFSKtempArrDsss_tmp2;
                        }
                        move_count = move_count + (int)((phaseTmp+1)/2);
                    }
                else if(phaseTmp < -1)
                    {
                        for(j = 0;j<m_LenSym1;j++)
                        {
                            t_sigDPFSKtempArrDsss_tmp2 = m_sigDPFSKtempArrDsss[i+1][j] + 2*(int)((-1*phaseTmp+1)/2);
                            m_sigDPFSKtempArrDsss[i+1][j] = t_sigDPFSKtempArrDsss_tmp2;
                        }
                            move_count = move_count -(int)((-1*phaseTmp+1)/2);
                    }
                else if(phaseTmp <=1 && phaseTmp >= -1)
                    break ;
            }

            for(j = 0;j< m_Ri + 1;j++)
            {
                //m_sigPreArrR_tmp = m_sigDPFSKtempArrDsss[i][j] + m_sigDPFSKtempArrDsss[i][m_Midi] - m_sigDPFSKtempArrDsss[i][0];
                t_sigPreArrR_tmp = m_sigDPFSKtempArrDsss[i][j + m_LenSym1 - m_Ri - 1] + m_sigDPFSKtempArrDsss[i][m_Midi] - m_sigDPFSKtempArrDsss[i][m_LenSym1 - m_Ri - 1];
                t_sigPreArrL_tmp = m_sigDPFSKtempArrDsss[i+1][j] + m_sigDPFSKtempArrDsss[i+1][m_Li] - m_sigDPFSKtempArrDsss[i + 1][m_Ri];
                m_sigPreArrR[j] = t_sigPreArrR_tmp;//m_sigPreArrR ,m_sigPreArrL fix
                m_sigPreArrL[j] = t_sigPreArrL_tmp;
            }

            for(j = 0;j<m_HalfWinLen;j++)
            {
//                t_sigDPFSKtempArrTS_tmp = m_sigPreArrR[j] * m_WinR[j] + m_sigPreArrL[j] * m_WinL[j];
                m_sigDPFSKtempArrTS[i][m_LenSym1 - m_HalfWinLen + j] = m_sigPreArrR[j] * m_WinR[j] + m_sigPreArrL[j] * m_WinL[j];

//                t_sigDPFSKtempArrTS_tmp = m_sigPreArrR[m_Li + j] * m_WinR[m_Li + j] + m_sigPreArrL[m_Li + j] * m_WinL[m_Li + j];
                m_sigDPFSKtempArrTS[i+1][j] = m_sigPreArrR[m_Li + j] * m_WinR[m_Li + j] + m_sigPreArrL[m_Li + j] * m_WinL[m_Li + j];
            }

            for(j = m_HalfWinLen; j<m_LenSym1;j++)
            {
                m_sigDPFSKtempArrTS[i+1][j] = m_sigDPFSKtempArrDsss[i+1][j];
            }
            for(j = 0;j<m_LenSym1;j++)
            {
                t_sigDPFSKtempArrDsss_tmp2 = m_sigDPFSKtempArrDsss[i+1][j] + 2 * move_count;
                m_sigDPFSKtempArrDsss[i+1][j] = t_sigDPFSKtempArrDsss_tmp2;
            }
        }

        for(i = 0; i<m_LenSym1-m_HalfWinLen;i++)
        {
            m_FirstPreamble_ts[i]= m_sigDPFSKtempArrTS[(m_SymNumData+m_SymNumPreamble)*m_DSSSNum][i];
        }
        for(i = 0;i<m_LenSym1;i++)
        {
             m_FirstPreamble[i] = m_sigDPFSKtempArrDsss[(m_SymNumData+m_SymNumPreamble)*m_DSSSNum][i];
        }

        if(m_agc_len != 0 && m_iblock == 0)//m_agc_len [max] =  65535
        {
            m_agc_fxp = -1;
            for(i = 0 ; i < m_agc_len; i++)
            {
                out[i] = m_agc_fxp;
#ifdef DUMP_RTL
                dump(fp_out,&m_agc_fxp,1,m_agc_fxp);
#endif
            }
            m_tx_len = (m_SymNumData + m_SymNumPreamble) * m_DSSSNum  * m_LenSym1*m_BlockNum + m_agc_len;
        }
        else
        {
            m_agc_len = 0;
        }

        for(i = 0;i<(m_SymNumData+m_SymNumPreamble)*m_DSSSNum;i++)
        {
            for(j = 0;j<m_LenSym1;j++)
            {
                t_sigDPFSKtempArrTS_tmp = m_sigDPFSKtempArrTS[i][j];
                out[i*m_LenSym1 + j + m_agc_len] = t_sigDPFSKtempArrTS_tmp;
 #ifdef DUMP_MATLAB
                 fp_mat_sigTx <<fixed<<setprecision(30)<< out[i*m_LenSym1 + j + m_agc_len]<<endl;
 #endif
            }
        }

        for(int ck0 = 0; ck0 < m_tone_len; ck0++) {
            m_agc_fxp = -1;
            out[(m_SymNumData + m_SymNumPreamble) * m_DSSSNum  * m_LenSym1 + m_agc_len + ck0] = m_agc_fxp;
        }

        m_outlen = (m_SymNumData + m_SymNumPreamble) * m_DSSSNum  * m_LenSym1 + m_agc_len + m_tone_len;
}

template<class T>
void Cls_tx_mod<T>::GenFskFreqsTS(double **m_FskPhase)
{
    int i;
    double tmp1;

    tmp1 = 0.5/m_LenSymPhase*2;//0.5/m_LenSymPhase*2*PI;
    for(i = 0;i < m_LenSymPhase ;i++)
    {
        t_FskPhase_tmp = 3*i * tmp1;
        m_FskPhase[i][0] = t_FskPhase_tmp;

        t_FskPhase_tmp = i * tmp1;
        m_FskPhase[i][1] = t_FskPhase_tmp;

        t_FskPhase_tmp = -1*i * tmp1;
        m_FskPhase[i][2] = t_FskPhase_tmp;

        t_FskPhase_tmp = -3*i * tmp1;
        m_FskPhase[i][3] = t_FskPhase_tmp;
    }
}

template<class T>
void Cls_tx_mod<T>::GenDSeq(int *DSeq , int DSSSNum)
{
    int i;
    int DSS8[8] =  {0,0,-1,-1,0,0,-1,-1};
    int DSS4[4] = {0,0,-1,-1};
    int DSS2[2] = {0,0};

    switch(DSSSNum)
    {
        case 8 :
            for(i = 0;i<8;i++)
            {
                DSeq[i] = DSS8[i];
            }
            break;
        case 4 :
            for(i = 0;i<4;i++)
            {
                DSeq[i] = DSS4[i];
            }
            break;
        case 2 :
            for(i = 0;i<2;i++)
            {
                DSeq[i] = DSS2[i];
            }
            break;
        case 1 :
            DSeq[0] = 0;
            break;
    }
}
