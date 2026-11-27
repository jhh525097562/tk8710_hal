#pragma once
//#include "Lib_io.h"
#include "common_type.h"
#include <cmath>
#include "Lib_utility.h"
#include "Cls_log10.h"
//#include "Cls_Dpfsk_Demod_int.h"
#include "Cls_Fix_Polar_decoder.h"
#include <iomanip>
#include "Lib_dump.h"

#ifndef PI
#define SystemPI    3.1415926
#else
#define SystemPI    PI
#endif

#define DEBUG_INIT_RX    (0)

#define DEBUG_FIX        (0)
/*
 * variable memory, define in MatchFilter
 */
#define STATES_NUM 64
#define BITS 0x01
#define DEC_LEN 50
//#define DUMP_MATLAB_CC
/*#define DUMP_MATLAB_PHY2_1_DATA*/
//#define DUMP_RTL_RXDEMOD
#define CC_NEW

using namespace std;
unsigned int SCcode[24] = {0x4E431080,0x2765EC35,0x26FCB50A,0x99593F69,0xA58A6301,0xD35C68BF,
                  0xD60BBE3C,0x57D68376,0xDD3D4A05,0xEBC97381,0xF439848A,0xF0F70B36,
                  0x4E431080,0x2765EC35,0x26FCB50A,0x99593F69,0xA58A6301,0xD35C68BF,
                  0xD60BBE3C,0x57D68376,0xDD3D4A05,0xEBC97381,0xF439848A,0xF0F70B36};

int                  DSS8[8] =  {1,1,-1,-1,1,1,-1,-1};
int                  DSS4[4] = {1,1,-1,-1};
int                  DSS2[2] = {1,1};

typedef struct RX_DATA_2_1_CFG
{
    double                          avg_power;
    int                             SymNumData;
    //int                             LenSym;
    double                          FsRx;
    int                             data_FDComp_Num;
    double                          data_FDComp_Delta;
    int                             CRC_EN;
    int                             DSSSNum;
    int                             FskaddLen;
//    int                             DqpskaddLen;
}RX_DATA_2_1_CFG;

template <class T>
class Cls_rx_data_phy_2_1
{
public:
    Cls_rx_data_phy_2_1() {
    m_FskFreqsLoc = NULL;//GenFskFreqsTS
    m_DSeq = NULL;//GenDSeq
    m_symRxCombineDsss = NULL;//Fsk_Cor_Combine
    tmpS = NULL;
    tmpC = NULL;
    m_symRxCombineDsssFDC = NULL;//DemodFreqDriftComp
    tmpExp = NULL;
    tmpDSR = NULL;//DeDSSS
    tmpDSR1Sum = NULL;
    m_symRxCombine = NULL;
    m_dem1 = NULL;//fsk_llr
    m_dem2 = NULL;
    m_indx_Sim = NULL;
    m_FskDemodLLR = NULL;
    m_SymRxCombineBest = NULL;
    m_FskDeInterlvLLR = NULL;//
    m_FskDecodedOut = NULL;
    m_FskRecodedOut = NULL;
    m_FskReInterlvOut = NULL;
    m_DqpskDecodedOut = NULL;
    m_DqpskDemodLLR = NULL;
    m_DqpskDeInterlvLLR = NULL;
    DemodFreqDriftComp_tmp = NULL;
    m_DqpskRecodedOut = NULL;
    sigFTO = NULL;
    tmpDS = NULL;
    tmpDSR1 = NULL;
    Ref = NULL;
    OutTemp = NULL;
    InTemp = NULL;
    m_FskDemodLLR_mean = NULL;
    m_dem3 = NULL;
    m_dem4 = NULL;
    m_dem1h = NULL;
    m_dem2h = NULL;

    decoded_out_fsk = NULL;
    decoded_out_dqpsk = NULL;
    byte_rxB = NULL;

    buff_double1 = NULL;
    buff_int1 = NULL;


    }
    ~Cls_rx_data_phy_2_1() {
        free(m_FskFreqsLoc);
        free(m_DSeq);

        free(m_symRxCombineDsss);
        free(tmpS);
        free(tmpC);

        free(m_symRxCombineDsssFDC);
        free(tmpExp);

        free(m_symRxCombine);
        free(tmpDSR);
        free(tmpDSR1Sum);

        free(m_FskDemodLLR);
        free(m_SymRxCombineBest);
        free(m_dem1);
        free(m_dem2);
        free(m_indx_Sim);

        free(m_FskDeInterlvLLR);

        free(m_FskDecodedOut);
        free(m_FskRecodedOut);
        free(m_FskReInterlvOut);
        free(m_DqpskDecodedOut);

        free(m_DqpskDemodLLR);
        free(m_DqpskDeInterlvLLR);

        free(DemodFreqDriftComp_tmp);
        free(m_DqpskRecodedOut);

        free(sigFTO);
        free(tmpDS);
        free(tmpDSR1);
        free(Ref);
        free(OutTemp);
        free(InTemp);
        free(m_FskDemodLLR_mean);
        free(m_dem3);
        free(m_dem4);
        free(m_dem1h);
        free(m_dem2h);

        free(decoded_out_fsk);
        free(decoded_out_dqpsk);
        free(byte_rxB);
        free(RxByte_o);

        free(buff_double1);
        free(buff_int1);
#ifdef DUMP_MATLAB_PHY2_1_DATA
        fp_mat_decode_out_fsk.close();
        fp_mat_data_in.close();
        fp_mat_decode_out_dqpsk.close();
        fp_mat_byte_rxB.close();
        fp_mat_fsk_llr.close();
        fp_mat_qpsk_llr.close();
#endif
#ifdef DUMP_RTL_RXDEMOD
        fp_in_i.close();
        fp_corr_timing.close();
        fp_corr.close();
        fp_reg.close();
        fp_fdc.close();
        fp_debug.close();
        fp_combine.close();
        fp_fsk_llr.close();
        fp_deilv.close();
        fp_dec.close();
        fp_rec.close();
        fp_ilv.close();
        fp_psk_llr.close();
        fp_crc_in.close();
        fp_crc.close();
        fp_snr.close();
        fp_crc_rev.close();
#endif
#ifdef DUMP_MATLAB_CC
        fp_cc_path.close();
        fp_cc_path_recorder.close();
#endif
    }
//        void init(int * module_name, Fsk_Demod_Cfg param_in);
    void init(char* module_name);
    void config(const RX_DATA_2_1_CFG cfg);
    void reset();
    void go(Complex<double> * SigFltFreqComp      ,
            Complex<double> * correctionTiming);
    double                  m_avg_power ;
    double              m_crc_error ;
    int                 Sbit;
    int                 Fbit;
    double              snr_est_fsk;
    double              snr_est_dqpsk;
    int                 *RxByte_o;
    int                 RxByteLen;
private:
    RX_DATA_2_1_CFG      m_cfg           ;
    int                  m_InputLen1      ;
    int                  m_InputLen2      ;

    double                  m_data_FDComp_Delta ;
    int                  m_data_FDComp_Num ;
    int                  m_DqpskaddLen ;
    int                  m_DqpskOutLen  ;
    int                  m_DSSSNum;
    int                  m_CRC_EN ;
    int                  m_FskaddLen ;
    int                  m_FskOutLen ;
    double                  m_FsRx;
    int                  m_LenSym ;
    int                  m_SymNumData ;
    int                  SymNumPreamble;

    Complex<double>      **m_FskFreqsLoc;
    int                  *m_DSeq;

    Complex<double>      *m_symRxCombineDsss;
    int                  m_symRxCombineDsss_len;
    Complex<double>      *tmpS;
    Complex<double>      *tmpC;

    Complex<double>      *m_symRxCombineDsssFDC;
    Complex<double>      *tmpExp;

    Complex<double>      *m_symRxCombine;
    int                  m_symRxCombine_len;
    Complex<double>    **tmpDSR;
    Complex<double>    *tmpDSR1Sum;

    double               *m_FskDemodLLR;
    int                   m_FskDemodLLR_len;
    Complex<double>      *m_SymRxCombineBest;
    int                   m_SymRxCombineBest_len;
    double *m_dem1;
    double *m_dem2;
    int *m_indx_Sim;

    double               *m_FskDeInterlvLLR;
    int                  m_FskDeInterlvLLR_len;
    int                  *m_FskDecodedOut;
    int                   m_FskDecodedOut_len;
    int                  *m_FskRecodedOut;
    int                  m_FskRecodedOut_len;
    int                  *m_FskReInterlvOut;
    int                  m_FskReInterlvOut_len;
    double               m_Avg_qpsk;
    double               m_FskSNREst;
    int                  *m_DqpskDecodedOut;
    int                  m_DqpskDecodedOut_len;
    double               m_DqpskSNREst;
    double              *m_DqpskDemodLLR;
    int                  m_DqpskDemodLLR_len;
    double              *m_DqpskDeInterlvLLR;
    int                 m_DqpskDeInterlvLLR_len;
    int                 ExternalCRCLen;
    int                 *m_DqpskRecodedOut;

    double              *DemodFreqDriftComp_tmp ;
    double              m_Fs;
    Cls_log10<T>        log10_f;

    Complex<double>    *sigFTO;//first function
    Complex<double>    *tmpDS;
    Complex<double>    **tmpDSR1;
    Complex<double>    *Ref;
    double             *InTemp;
    double             *OutTemp;
    double        *m_FskDemodLLR_mean;

    Complex<double> m_symRxCombineTmp;
    double Proj[8];
    double *m_dem3;
    double *m_dem4;
    int *m_dem1h;
    int *m_dem2h;

    int errorCRC;
    //double snr_est_fsk;
    //double snr_est_dqpsk;
    double RSSI;
    int *decoded_out_fsk;
    int *decoded_out_dqpsk;
    int *byte_rxB;

    double *buff_double1;
    int    *buff_int1;

    T    m_t_llr;
    T   t_fre;
    T   t_fre1;
    T   t_fre3cos;
    T   t_fre3sin;
    T   t_frecos;
    T   t_fresin;

    Complex<T> t_SigFltFreqComp;
    Complex<T> t_tmpS ;
    Complex<T> t_tmpC  ;
    Complex<T> t_CorrectPhaseOffset;

    T   t_DemodFreqDriftComp_tmp;

    T t_FreqOffsetDriftTabletmp;
    T t_tmpPhasedelta;
    T t_tmpPhase;
    Complex<T> t_tmpExp;
    Complex<T> t_symRxCombineDsssFDC;

    Complex<T> t_tmpDSR1Sum;

    T avg_powerTmp;//fsk_llr
    T rtmp1;
    T rtmp2;
    T rtmp3;
    T rtmp4;
    T tmpmax0;
    T tmpmax1;
    T tmpmax2;

    T      t_p ;
    T      t_p_s ;
    T      t_tmpMean ;
    T      t_tmpVar ;
    T      t_avg_qpsk ;
    T      t_err_p ;
    T      t_tmpMean_fsk;
    T       t_snr_est;

//    T t_qpsk_sum;
    T t_var_sum;
    T t_Mean_qpsk ;
    T t_Var_qpsk ;

    Complex<T> t_diff;
    T avg_qpskTmp ;
    T t_bit0 ;
    T t_bit1 ;
    T t_sub ;

    T t_demodBitsLLR;
    T t_data_FDComp_Delta;
    Complex<T> t_FskFreqsLoc;
    T t_tmpphase_tmp;
    fxp m_state_metrics[STATES_NUM];
    double m_path_metrics_from_smaller[STATES_NUM];
    double m_path_metrics_from_larger[STATES_NUM];
    T m_branch_metrics;
    /*Generate two */
    void GenFskFreqsTS();
    void GenDSeq();
    void DemodFreqDriftComp(double DemodFreqDriftComp_tmp);
    void DeDsss();
    /**²ÎÊý³õÊ¼»¯º¯Êý*/
    void gen_cfg(int               Mode      ,
                 int               PolarFlag ,
                 RX_DATA_2_1_CFG  *param_in  );


    void     Fsk_Cor_Combine(Complex<double> *SigFltFreqComp  ,Complex<double> *CorrectPhaseOffset); //Output

    void     Fsk4_LLR();     //Output


    /*...*/
    void      DeInterlv(double     * In         ,   //Input
                        double     * Out        ,   //Output
                        int        addLen       ,
                        int        Inlen    );  //REG

    void      Cal_SNR( double    *snr_est   );   //Output);

    void Decode_Recode(int      SecFlag           ,   //Local
                       double * demodBits1_quant  ,   //Input
                       int    * DecodeOut         ,   //Output
                       int    * RecodeOut         ,   //output
                       int    Datain              );   //Output

    void CC_Decode(double * demodBits1_quant , //Input
                   int    * vit_out          , //Output
                   int      DatalenIn        );//Local

    void viterbi_decoder_217(double * dec_input    ,  //Input
                             int    * dec_output   ,  //Output
                             int      len_output   ); //Local

    void CC_Encode(int * in       ,  //Input
                   int * out      ,  //Output
                   int   lengthIn ); //Local

    void Interlv(); //REG

    void Dqpsk_Demod(double * snr_est,double * RSSI);  //Output)

    void Dqpsk_LLR(double   *RSSI); //Local

    void CRC_Check(int * errorCRC ); //Local

    void crc_decode(int * rev               ,  //Input
                    int   ExternalCRCLen    ,  //REG
                    int * error2            ,  //Output
                    int   DatalenIn         ); //Local


    int abs_b(int I, int Q);
    double abs(double aa);

#ifdef DUMP_MATLAB_PHY2_1_DATA
    std::ofstream           fp_mat_decode_out_fsk;
    std::ofstream           fp_mat_data_in;
    std::ofstream           fp_mat_decode_out_dqpsk;
    std::ofstream           fp_mat_byte_rxB;
    std::ofstream           fp_mat_fsk_llr;
    std::ofstream           fp_mat_qpsk_llr;
#endif
#ifdef DUMP_RTL_RXDEMOD
    std::ofstream  fp_in_i, fp_corr_timing, fp_corr, fp_reg, fp_fdc, fp_debug, fp_combine, fp_fsk_llr, fp_deilv, fp_dec, fp_rec, fp_ilv, fp_psk_llr, fp_crc_in, fp_crc, fp_snr, fp_crc_rev;
#endif
#ifdef DUMP_MATLAB_CC
    std::ofstream fp_cc_path;
    std::ofstream fp_cc_path_recorder;
#endif
};

//void Cls_Dpfsk_Demod<T>::init(int* module_name, Fsk_Demod_Cfg param_in)

template<class T>
void Cls_rx_data_phy_2_1<T>::config(const RX_DATA_2_1_CFG  cfg)
{
    m_cfg = cfg;

    m_avg_power = m_cfg.avg_power;
    m_data_FDComp_Delta = m_cfg.data_FDComp_Delta;
    m_data_FDComp_Num = m_cfg.data_FDComp_Num;
    m_DqpskaddLen = m_cfg.FskaddLen;
    m_DqpskOutLen = m_cfg.SymNumData * 2;
    m_DSSSNum = m_cfg.DSSSNum;
    m_CRC_EN = m_cfg.CRC_EN;
    m_FskaddLen = m_cfg.FskaddLen;
    m_FskOutLen = m_cfg.SymNumData * 2;
    m_FsRx = m_cfg.FsRx;
   // m_LenSym = m_cfg.LenSym;
    m_SymNumData = m_cfg.SymNumData;
}

template <class T>
void Cls_rx_data_phy_2_1<T>::init(char* module_name)
{
    m_LenSym = 16;
    m_InputLen1 =  (m_cfg.SymNumData + 1)*m_LenSym*m_DSSSNum;
    m_InputLen2 = 4;
    SymNumPreamble = 12;
    ExternalCRCLen = 14;

    m_FskFreqsLoc = safe_new(m_FskFreqsLoc, 4,m_LenSym);
    m_DSeq = safe_new(m_DSeq, m_cfg.DSSSNum);

    m_symRxCombineDsss_len = (m_SymNumData+1)*4*m_DSSSNum;//fsk_cor_combine
    m_symRxCombineDsss = safe_new(m_symRxCombineDsss,m_symRxCombineDsss_len);
    tmpS = safe_new(tmpS,m_LenSym);
    tmpC = safe_new(tmpC,4);

    m_Fs = double(m_FsRx) / m_LenSym ;//DemodFreqDriftComp
    DemodFreqDriftComp_tmp = safe_new(DemodFreqDriftComp_tmp,m_data_FDComp_Num+1);
    m_symRxCombineDsssFDC = safe_new(m_symRxCombineDsssFDC,m_symRxCombineDsss_len);

    m_symRxCombine_len = (m_SymNumData+1)*4;//DE DSSS
    m_symRxCombine = safe_new(m_symRxCombine, m_symRxCombine_len);
    tmpDSR = safe_new(tmpDSR,m_DSSSNum,4);
    tmpDSR1Sum = safe_new(tmpDSR1Sum,4);
    tmpExp = safe_new(tmpExp,(m_SymNumData + SymNumPreamble)*m_DSSSNum);

    m_FskDemodLLR_len = m_SymNumData * 2;//fsk_llr
    m_FskDemodLLR = safe_new(m_FskDemodLLR, m_FskDemodLLR_len);
    m_FskDemodLLR_mean = safe_new(m_FskDemodLLR_mean,m_FskDemodLLR_len);
    m_SymRxCombineBest_len = m_SymNumData;
    m_SymRxCombineBest = safe_new(m_SymRxCombineBest, m_SymRxCombineBest_len );
    m_dem1 = safe_new(m_dem1,m_SymNumData);
    m_dem2 = safe_new(m_dem2,m_SymNumData);
    m_indx_Sim = safe_new(m_indx_Sim,m_SymNumData+1);

    m_FskDeInterlvLLR_len = m_FskDemodLLR_len  + m_FskaddLen;
    m_FskDeInterlvLLR = safe_new(m_FskDeInterlvLLR, m_FskDeInterlvLLR_len );
    InTemp = safe_new(InTemp,m_FskDemodLLR_len + m_FskaddLen);
    OutTemp = safe_new(OutTemp,m_FskDemodLLR_len + m_FskaddLen);

    m_FskDecodedOut_len = m_FskDeInterlvLLR_len/2;
    m_FskDecodedOut = safe_new(m_FskDecodedOut, m_FskDecodedOut_len );

    m_FskRecodedOut_len = m_FskDecodedOut_len*2;
    m_FskRecodedOut = safe_new(m_FskRecodedOut, m_FskRecodedOut_len );

    m_FskReInterlvOut_len = m_FskOutLen;
    m_FskReInterlvOut = safe_new(m_FskReInterlvOut, m_FskReInterlvOut_len );

    m_DqpskDecodedOut_len = m_FskDecodedOut_len;
    m_DqpskDecodedOut = safe_new(m_DqpskDecodedOut, m_DqpskDecodedOut_len );
    m_dem3 = safe_new(m_dem3,m_SymNumData);
    m_dem4 = safe_new(m_dem4,m_SymNumData);
    m_dem1h = safe_new(m_dem1h,m_SymNumData+1);
    m_dem2h = safe_new(m_dem2h,m_SymNumData+1);

    m_DqpskDemodLLR_len = m_SymNumData * 2;
    m_DqpskDemodLLR = safe_new(m_DqpskDemodLLR, m_DqpskDemodLLR_len );

    m_DqpskDeInterlvLLR_len = m_DqpskDemodLLR_len + m_DqpskaddLen;
    m_DqpskDeInterlvLLR = safe_new(m_DqpskDeInterlvLLR, m_DqpskDeInterlvLLR_len );

    m_DqpskRecodedOut = safe_new(m_DqpskRecodedOut, m_FskRecodedOut_len );


    sigFTO = safe_new(sigFTO,m_InputLen1 - m_LenSym*m_DSSSNum);
    tmpDS = safe_new(tmpDS,m_LenSym*m_DSSSNum);
    tmpDSR1 = safe_new(tmpDSR1,m_DSSSNum,m_LenSym);
    Ref = safe_new(Ref,4);

    decoded_out_fsk = safe_new(decoded_out_fsk,m_FskDecodedOut_len);
    decoded_out_dqpsk = safe_new(decoded_out_dqpsk,m_DqpskDecodedOut_len);
    byte_rxB = safe_new(byte_rxB,m_FskDecodedOut_len+m_DqpskDecodedOut_len+14);
    RxByte_o = safe_new(RxByte_o,(m_FskDecodedOut_len+m_DqpskDecodedOut_len+16)/8);

    buff_double1 = safe_new(buff_double1,8192);
    buff_int1 = safe_new(buff_int1,8192);



    fix_set(t_fre3cos, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);// statistic_set(t_fre3cos,"t_fre3cos", 1, 10);
    fix_set(t_fre3sin, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);// statistic_set(m_fre3sin_tmp,"m_fre3sin_tmp", 1, 10);
    fix_set(t_frecos, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);// statistic_set(m_frecos_tmp,"m_frecos_tmp", 1, 10);
    fix_set(t_fresin, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);// statistic_set(m_fresin_tmp,"m_fresin_tmp", 1, 10);
    fix_set(t_SigFltFreqComp , 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(t_SigFltFreqComp,"t_SigFltFreqComp", 1, 10);

    fix_set(t_tmpS  , 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(t_tmpS,"t_tmpS", 1, 10);
    fix_set(t_tmpC  , 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(t_tmpC,"t_tmpC", 1, 10);
    fix_set(t_CorrectPhaseOffset  , 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(t_CorrectPhaseOffset,"t_CorrectPhaseOffset", 1, 10);// sync same
    fix_set(t_DemodFreqDriftComp_tmp  , 11, 9, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(t_DemodFreqDriftComp_tmp,"t_DemodFreqDriftComp_tmp", 1, 1);//add 1 xiaoshu
    fix_set(t_FreqOffsetDriftTabletmp  , 23, -12, FXP_US, FXP_RND, FXP_SAT_SYM);// statistic_set(t_FreqOffsetDriftTabletmp,"t_FreqOffsetDriftTabletmp", 1, 1);//%%%%%%%%%debug
    fix_set(t_tmpPhasedelta  , 17, 1, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(t_tmpPhasedelta,"t_tmpPhasedelta", 1, 10);

    fix_set(t_tmpPhase  , 10, 1, FXP_TC, FXP_RND, FXP_WRAP); //statistic_set(t_tmpPhase,"t_tmpPhase", 1, 1);

    fix_set(t_tmpExp  , 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);// statistic_set(t_tmpExp,"t_tmpExp", 1, 1);
    fix_set(t_symRxCombineDsssFDC  , 11, 1, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(t_symRxCombineDsssFDC,"t_symRxCombineDsssFDC", 1, 10);
    fix_set(t_tmpDSR1Sum  , 11, 1, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(t_tmpDSR1Sum,"t_tmpDSR1Sum", 1, 10);

    fix_set(avg_powerTmp  , 15, 8, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM);// statistic_set(m_avg_power,"m_avg_power", 1, 1);//fsk_llr
    fix_set(rtmp1  , 11, 1, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(rtmp1,"rtmp1", 1, 10);
    fix_set(rtmp2  , 11, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);// statistic_set(rtmp2,"rtmp2", 1, 10);
    fix_set(rtmp3  , 11, 1, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(rtmp3,"rtmp3", 1, 10);
    fix_set(rtmp4  , 11, 1, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(rtmp4,"rtmp4", 1, 10);
    fix_set(tmpmax0  , 10, 0, FXP_US, FXP_RND, FXP_SAT_SYM);// statistic_set(tmpmax0,"tmpmax0", 1, 10);
    fix_set(tmpmax1  , 10, 0, FXP_US, FXP_RND, FXP_SAT_SYM); //statistic_set(tmpmax1,"tmpmax1", 1, 10);
    fix_set(tmpmax2  , 11, 1, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(tmpmax2,"tmpmax2", 1, 10);
    fix_set(m_t_llr, 6, 6, FXP_TC, FXP_RND_INF, FXP_SAT_SYM); //statistic_set(m_t_llr,"m_t_llr", 1, 1);//%%%%%%%%%%%%%%%debug
    fix_set(t_p, 11, 2, FXP_US, FXP_RND, FXP_SAT_SYM); //statistic_set(t_p,"t_p", 1, 1);

    fix_set(t_p_s, 14, 7, FXP_US, FXP_RND, FXP_SAT_SYM);// statistic_set(t_p_s,"t_p_s", 1, 1);
    fix_set(t_avg_qpsk, 11, 4, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM);// statistic_set(t_avg_qpsk,"t_avg_qpsk", 1, 1);
    fix_set(t_tmpMean_fsk, 14, 14, FXP_US, FXP_RND_INF, FXP_SAT_SYM);// statistic_set(t_tmpMean_fsk,"t_tmpMean_fsk", 1, 1);
    fix_set(t_tmpMean, 10, 8, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM); //statistic_set(t_tmpMean,"t_tmpMean", 1, 1);
    fix_set(t_err_p, 10, 8, FXP_US, FXP_RND, FXP_SAT_SYM);// statistic_set(t_err_p,"t_err_p", 1, 10);
    fix_set(t_tmpVar, 18, 18, FXP_US, FXP_RND, FXP_SAT_SYM);// statistic_set(t_tmpVar,"t_tmpVar", 1, 10);

    fix_set(t_Mean_qpsk, 7, 6, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM);// statistic_set(t_Mean_qpsk,"t_Mean_qpsk", 1, 1);
    fix_set(t_var_sum, 12, 10, FXP_US, FXP_RND, FXP_SAT_SYM);// statistic_set(t_var_sum,"t_var_sum", 1, 10);
    fix_set(t_Var_qpsk, 12, 10, FXP_US, FXP_TRN_ZERO, FXP_SAT_SYM);// statistic_set(t_Var_qpsk,"t_Var_qpsk", 1, 1);
    fix_set(t_diff, 11, 2, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(t_diff,"t_diff", 1, 10);
    fix_set(avg_qpskTmp, 8, 2, FXP_US, FXP_RND, FXP_SAT_SYM); //statistic_set(avg_qpskTmp,"avg_qpskTmp", 1, 1);
    fix_set(t_bit0, 11, 2, FXP_TC, FXP_RND, FXP_SAT_SYM);// statistic_set(t_bit0,"t_bit0", 1, 10);
    fix_set(t_bit1, 11, 2, FXP_TC, FXP_RND, FXP_SAT_SYM);// statistic_set(t_bit1,"t_bit1", 1, 10);
    fix_set(t_sub, 12, 3, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(t_sub,"t_sub", 1, 10);

    fix_set(t_data_FDComp_Delta, 13, 12, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(t_data_FDComp_Delta,"t_data_FDComp_Delta", 1, 1);
    fix_set(t_FskFreqsLoc, 10, 2, FXP_TC, FXP_RND, FXP_SAT_SYM); //statistic_set(t_FskFreqsLoc,"t_FskFreqsLoc", 1, 10);
    fix_set(t_tmpphase_tmp, 12, 3, FXP_TC, FXP_RND, FXP_SAT_SYM);// statistic_set(t_tmpphase_tmp,"t_tmpphase_tmp", 1, 10);
//    fix_set(t_RSSI,10,-9,FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(t_RSSI,"t_RSSI", 1, 1);

#ifdef CC_NEW
    fix_set_group(m_state_metrics, STATES_NUM, 9, 9, FXP_TC, FXP_RND, FXP_WRAP);
    //fix_set(m_branch_metrics, 7, 6, FXP_TC, FXP_TRN, FXP_WRAP);//statistic_set(m_branch_metrics,"m_branch_metrics", 1, 10);
#endif
    //fix_set_group(m_path_metrics_from_smaller, STATES_NUM, 9, 9, FXP_TC, FXP_RND, FXP_WRAP);
    //fix_set_group(m_path_metrics_from_larger, STATES_NUM, 9, 9, FXP_TC, FXP_RND, FXP_WRAP);

#ifdef DUMP_RTL_RXDEMOD
    char filename_rtl[300] = {};
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_in.dat");                fp_in_i.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_corr_timing.dat");       fp_corr_timing.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_fdc.dat");               fp_fdc.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "fsk_corr.dat");              fp_corr.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_reg.dat");               fp_reg.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_debug.dat");             fp_debug.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_combine.dat");           fp_combine.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_fsk_llr.dat");           fp_fsk_llr.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_deilv.dat");             fp_deilv.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_dec.dat");               fp_dec.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_rec.dat");               fp_rec.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_ilv.dat");               fp_ilv.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_psk_llr.dat");           fp_psk_llr.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_crc_in.dat");            fp_crc_in.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_crc.dat");               fp_crc.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_crc_rev.dat");           fp_crc_rev.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "dem_snr.dat");               fp_snr.open(filename_rtl);

    fxp rtl_dump1, rtl_dump2, rtl_dump3;
    fix_set(rtl_dump1,   4,  4, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump2,  12, 12, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump3,  16, 16, FXP_US, FXP_RND, FXP_SAT_SYM);
    rtl_dump3 = m_FsRx==1953.125 ? 16384 : m_FsRx==3906.25 ? 8192 : m_FsRx==7812.5 ? 4096 : m_FsRx==15625 ? 2048 : m_FsRx==31250 ? 1024 :  m_FsRx==62500 ? 512 : m_FsRx==125000 ? 256 : m_FsRx==250000 ? 128 : 64;
    dump(fp_reg, &rtl_dump3,  1, rtl_dump3,0);
    dump(fp_reg, &m_cfg.data_FDComp_Delta,  1, t_data_FDComp_Delta,0);
    rtl_dump1 = m_cfg.data_FDComp_Num==1 ? 0 : m_cfg.data_FDComp_Num==2 ? 1 : m_cfg.data_FDComp_Num==4 ? 2 : m_cfg.data_FDComp_Num==8 ? 3 : m_cfg.data_FDComp_Num==16 ? 4 : 0;
    dump(fp_reg, &rtl_dump1,  1, rtl_dump1,0);
    rtl_dump1 = m_cfg.DSSSNum==1 ? 0 : m_cfg.DSSSNum==2 ? 1 : m_cfg.DSSSNum==4 ? 2 : 3;
    dump(fp_reg, &rtl_dump1,  1, rtl_dump1,0);
    rtl_dump2=m_cfg.FskaddLen;
    dump(fp_reg, &rtl_dump2,  1, rtl_dump2,0);
    rtl_dump2=m_cfg.SymNumData;
    dump(fp_reg, &rtl_dump2,  1, rtl_dump2);
#endif
#ifdef DUMP_MATLAB_PHY2_1_DATA
    char filename_mat[300] = {};
    strcpy(filename_mat, module_name); strcat(filename_mat, "decoded_out_fsk.txt");        fp_mat_decode_out_fsk.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "data_in.txt");                fp_mat_data_in.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "decoded_out_dqpsk.txt");      fp_mat_decode_out_dqpsk.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "byte_rxB.txt");               fp_mat_byte_rxB.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fsk_llr.txt");                fp_mat_fsk_llr.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "qpsk_llr.txt");               fp_mat_qpsk_llr.open(filename_mat);
#endif
#ifdef DUMP_MATLAB_CC
    fp_cc_path.open("path_2.txt");
    fp_cc_path_recorder.open("result.txt");
#endif
    reset();

}

template <class T>
void Cls_rx_data_phy_2_1<T>::reset()
{
    m_crc_error = 1;

    zeros(sigFTO,m_InputLen1 - m_LenSym*m_DSSSNum);//Fsk_Cor_Combine
    zeros(tmpDS,m_LenSym*m_DSSSNum);
    zeros(tmpDSR,m_DSSSNum,4);
    zeros(tmpDSR1,m_DSSSNum,m_LenSym);
    zeros(tmpDSR1Sum,4);
    zeros(Ref,4);
    zeros(buff_double1,8192);
    zeros(buff_int1,8192);



    zeros(m_FskFreqsLoc, 4,m_LenSym);
    zeros(m_DSeq,m_cfg.DSSSNum);
    zeros(m_symRxCombine,m_symRxCombine_len);
    zeros(m_symRxCombineDsssFDC,m_symRxCombine_len);
    zeros(m_FskDemodLLR,m_cfg.SymNumData*2);
    zeros(m_SymRxCombineBest,m_cfg.SymNumData);
    zeros(m_FskDeInterlvLLR,m_FskDeInterlvLLR_len);
    zeros(m_FskDecodedOut,m_FskDecodedOut_len);
    zeros(m_FskRecodedOut,m_FskRecodedOut_len);
    zeros(m_FskReInterlvOut,m_FskReInterlvOut_len);
    zeros(m_DqpskDecodedOut,m_DqpskDecodedOut_len);
    zeros(m_DqpskDemodLLR,m_DqpskDemodLLR_len);
    zeros(m_DqpskDeInterlvLLR,m_DqpskDeInterlvLLR_len);
    zeros(DemodFreqDriftComp_tmp,m_data_FDComp_Num+1);
    zeros(m_indx_Sim,m_SymNumData+1);

}

template <class T>
void Cls_rx_data_phy_2_1<T>::go(Complex<double>          * SigFltFreqComp      ,
                            Complex<double>          * CorrectPhaseOffset   )

{
    GenFskFreqsTS();
    GenDSeq();

    Fsk_Cor_Combine(SigFltFreqComp ,CorrectPhaseOffset );//input SigFltFreqComp and CorrectPhaseOffset,output:m_symRxCombineDsss

    double MaxF3,DeltaF3;
    int i,Ntemp3;

    t_data_FDComp_Delta = m_data_FDComp_Delta;
    m_data_FDComp_Delta = t_data_FDComp_Delta;
    DeltaF3 = m_data_FDComp_Delta;
    Ntemp3 = m_data_FDComp_Num;
    if(Ntemp3 ==0 )
        Ntemp3 = 1;
    MaxF3 = DeltaF3 * double(Ntemp3-1)/2;
    for( i = 0;i < Ntemp3;i++)
    {
        t_DemodFreqDriftComp_tmp = -1*MaxF3 + m_data_FDComp_Delta * i;
        DemodFreqDriftComp_tmp[i] = t_DemodFreqDriftComp_tmp;
    }
    for(i = 0;i<Ntemp3;i++)
    {
        DemodFreqDriftComp(DemodFreqDriftComp_tmp[i]);//input:m_symRxCombineDsss output:m_symRxCombineDsssFDC

//            * DriftPerSec = DemodFreqDriftComp_tmp[i];

        DeDsss();//input:m_symRxCombineDsssFDC output:m_symRxCombine;
        Fsk4_LLR();//input:m_symRxCombine,avg_power,output:m_FskDemodLLR ,avg_qpsk, m_SymRxCombineBest
        DeInterlv(m_FskDemodLLR ,m_FskDeInterlvLLR , m_FskaddLen ,m_FskDemodLLR_len);//input:m_FskDemodLLR,output:m_FskDeInterlvLLR
        Decode_Recode(0  , m_FskDeInterlvLLR ,m_FskDecodedOut ,m_FskRecodedOut ,m_FskDeInterlvLLR_len );//input:m_FskDeInterlvLLR,output:m_FskDecodedOut,m_FskRecodedOut
        Interlv();//input:m_FskRecodedOut,output:m_FskReInterlvOut
        Cal_SNR(&snr_est_fsk);//input:m_SymRxCombineBest,m_FskDemodLLR,m_FskReInterlvOut  output:snr_est_fsk and m_Avg_qpsk


        Dqpsk_Demod(&snr_est_dqpsk ,&RSSI  );
        if(m_cfg.CRC_EN == 0)
        {
            errorCRC = 0;
            m_DqpskDecodedOut_len = m_DqpskDecodedOut_len + 14;
        }
        else{
            CRC_Check(&errorCRC);
        }
        if (errorCRC ==0)
            break;
    }
    Sbit = m_DqpskDecodedOut[m_DqpskDecodedOut_len-16];
    Fbit = m_DqpskDecodedOut[m_DqpskDecodedOut_len-15];
    int ck0;
    for(ck0 = 0; ck0 < m_FskDecodedOut_len; ck0++)
    {
        decoded_out_fsk[ck0] = m_FskDecodedOut[ck0];
        byte_rxB[ck0] = m_FskDecodedOut[ck0];
#ifdef DUMP_MATLAB_PHY2_1_DATA
        fp_mat_decode_out_fsk << decoded_out_fsk[ck0] <<endl;
        fp_mat_byte_rxB << byte_rxB[ck0] <<endl;
#endif
    }

    for(ck0 = 0; ck0 < m_DqpskDecodedOut_len - 14; ck0++)
    {
        decoded_out_dqpsk[ck0] = m_DqpskDecodedOut[ck0];
        byte_rxB[ck0 + m_FskDecodedOut_len] = m_DqpskDecodedOut[ck0];
#ifdef DUMP_MATLAB_PHY2_1_DATA
        fp_mat_decode_out_dqpsk << decoded_out_dqpsk[ck0] <<endl;
        fp_mat_byte_rxB << decoded_out_dqpsk[ck0] <<endl;
#endif
    }
    for(ck0 = 0; ck0 < ((m_FskDecodedOut_len+m_DqpskDecodedOut_len+16)/8); ck0++){
        RxByte_o[ck0] = byte_rxB[ck0*8+7]*1 + byte_rxB[ck0*8+6]*2 + byte_rxB[ck0*8+5]*4 + byte_rxB[ck0*8+4]*8 + byte_rxB[ck0*8+3]*16 + byte_rxB[ck0*8+2]*32 + byte_rxB[ck0*8+1]*64 + byte_rxB[ck0*8]*128;
    }
    RxByteLen = (m_FskDecodedOut_len+m_DqpskDecodedOut_len)/8;
    for(ck0 = 0; ck0 < m_FskDeInterlvLLR_len ; ck0++)
    {
#ifdef DUMP_MATLAB_PHY2_1_DATA
//        fp_mat_fsk_llr <<fixed<<setprecision(30)<< m_FskDeInterlvLLR[ck0] <<endl;
        fp_mat_qpsk_llr <<fixed<<setprecision(30)<< m_DqpskDemodLLR[ck0] <<endl;
//        fp_mat_sigTx <<fixed<<setprecision(30)<< m_phase_tmp <<endl;
#endif
    }
    //LOG2DEV <<"CRC:"<<errorCRC<<"  snr_fsk:"<<snr_est_fsk<<"  snr_dqpsk:"<<snr_est_dqpsk<<endl;
    m_crc_error =errorCRC;
}

//
/*
* Fsk_Demod函数
* Mfsk : 4FSK或16FSK
* NumDsss ：
* sigFltFreqComp ： Fsk_Demod函数输入
* FskCombinePower ： 计算符号能量方式选择
* correctionTiming ： 时偏校正矩阵
* avg_power ： FSK符号能量归一化值
* symRxCombine : FSK输出符号
* decoded_out_fsk：FSK译码输出数据
* Recoded_out_fsk ： FSK译码之后再编码回去之后的数据
* avg_p ： 为DQPSK能量归一化的值
* snr_est：FSK计算的SNR
* error : CRC校验结果，0通过，1未通过
* DataLenIn ： 输入长度
* 无返回值
*/
template <class T>
void Cls_rx_data_phy_2_1<T>::Fsk_Cor_Combine(Complex<double> *SigFltFreqComp  ,Complex<double> *CorrectPhaseOffset)
{

    int i,j,k;
    Complex<double> tmpS_SUM;

    for(i = 0;i<(m_SymNumData + 1) * m_DSSSNum;i++)
    {
        for(j = 0;j<m_LenSym;j++)
        {
            t_SigFltFreqComp   = SigFltFreqComp[j + i*m_LenSym];
            tmpS[j] = t_SigFltFreqComp;
#ifdef DUMP_MATLAB_PHY2_1_DATA
            fp_mat_data_in << t_SigFltFreqComp.I<<"\t"<<t_SigFltFreqComp.Q<<endl;
#endif
#ifdef DUMP_RTL_RXDEMOD
	    	dump(fp_in_i, &t_SigFltFreqComp,  1, t_SigFltFreqComp.I);
#endif
        }

        for(k = 0;k < 4;k++)
        {
            tmpS_SUM = 0;
            for(j = 0;j<m_LenSym;j++)
            {
                t_FskFreqsLoc = tmpS[j] * m_FskFreqsLoc[k][j];
                tmpS_SUM =  tmpS_SUM + t_FskFreqsLoc.todouble();
            }
            t_tmpS = tmpS_SUM/m_LenSym;
            t_CorrectPhaseOffset = CorrectPhaseOffset[k];
            t_tmpC   = t_tmpS * t_CorrectPhaseOffset;
            tmpC[k] = t_tmpC;
            m_symRxCombineDsss[i * 4 +k] = tmpC[k];

	    #ifdef DUMP_RTL_RXDEMOD
	    	dump(fp_corr, &t_tmpC,  1, t_tmpC.I);
		if(i==0 && k==0)
		{
			dump(fp_corr_timing, &m_avg_power,  1, avg_powerTmp,0);
	    		dump(fp_corr_timing, &CorrectPhaseOffset[0],  1, t_CorrectPhaseOffset.I,0);
            		dump(fp_corr_timing, &CorrectPhaseOffset[1],  1, t_CorrectPhaseOffset.I,0);
       	    		dump(fp_corr_timing, &CorrectPhaseOffset[2],  1, t_CorrectPhaseOffset.I,0);
       	    		dump(fp_corr_timing, &CorrectPhaseOffset[3],  1, t_CorrectPhaseOffset.I);
		}
	    #endif

        }
    }
}

template <class T>
void Cls_rx_data_phy_2_1<T>::Fsk4_LLR()
{
    int iSym;
    int bit0,bit1,bitTmp;
    int BitTable4[4] = {0,1,3,2};

    avg_powerTmp = m_avg_power;

    for(iSym = 0;iSym < m_SymNumData;iSym++)
    {
        rtmp1 =  m_symRxCombine[(iSym+1) *     4].power() ;
        rtmp2 =  m_symRxCombine[(iSym+1) * 4 + 1].power() ;
        rtmp3 =  m_symRxCombine[(iSym+1) * 4 + 2].power() ;
        rtmp4 =  m_symRxCombine[(iSym+1) * 4 + 3].power() ;


        tmpmax0 = rtmp1 > rtmp2 ? rtmp1 : rtmp2;
        tmpmax1 = rtmp3 > rtmp4 ? rtmp3 : rtmp4;
        tmpmax2 = tmpmax0 - tmpmax1;
        m_dem1[iSym] = tmpmax2;

        tmpmax0 = rtmp1 > rtmp4 ? rtmp1 : rtmp4;
        tmpmax1 = rtmp2 > rtmp3 ? rtmp2 : rtmp3;
        tmpmax2 = tmpmax0 - tmpmax1;
        m_dem2[iSym] = tmpmax0 - tmpmax1;

        bit0 = (m_dem1[iSym] > 0) ? 0: 1;
        bit1 = (m_dem2[iSym] > 0) ? 0 : 1;

        bitTmp = 2*bit0 + bit1;
        m_indx_Sim[iSym+1] = BitTable4[bitTmp];

        m_SymRxCombineBest[iSym] = m_symRxCombine[iSym*4+m_indx_Sim[iSym]];

        m_t_llr = (m_dem1[iSym] * avg_powerTmp * 31); m_FskDemodLLR[2 * iSym] = m_t_llr;
        m_t_llr = (m_dem2[iSym] * avg_powerTmp * 31); m_FskDemodLLR[2*iSym+1] = m_t_llr;

	#ifdef DUMP_RTL_RXDEMOD
	    dump(fp_fsk_llr, &m_FskDemodLLR[2 * iSym],  1, m_t_llr);
	    dump(fp_fsk_llr, &m_FskDemodLLR[2*iSym+1],  1, m_t_llr);
	#endif

    }

}


//
/////*
//* FSK解交织函数
//* In : FSK解交织输入
//* Out ： FSK解交织输出
//* cc_row : 交织行数
//* cc_col ： 交织列数
//* 无返回值
//*/
template <class T>
void Cls_rx_data_phy_2_1<T>::DeInterlv(double *In, double *Out,int addLen,int Inlen)
{
    int i,j,ii;
    int row,col;
    int InterLvIndex[24] = {22,10,16,4,19,7,13,1,21,9,15,3,18,6,12,0,2,14,8,20,5,17,11,23};
    int tempIndex;
    int count1 = 0;
    int  In_length;
    int tmp;

    In_length = Inlen + addLen;

    zeros(InTemp , In_length);
    memcpy(InTemp,In,sizeof(double)*Inlen);

    col = 24;
    row = (In_length + col - 1)/col;

    for(i = 0;i < col;i++)
    {
        ii = InterLvIndex[i];
        for(j = 0;j < row;j++)
        {
            tempIndex = j * col + ii;
            if(tempIndex < In_length)
            {
                OutTemp[tempIndex] = InTemp[count1];
                count1 = count1 + 1;
            }
        }
    }

    for(i = 0;i < In_length;i++)
    {
        tmp = 2*((SCcode[i/32]>>(i&31))&0x01);
//        t_demodBitsLLR = round(OutTemp[i]*(1 - tmp)*31);
        Out[i] = OutTemp[i]*(1 - tmp);

	#ifdef DUMP_RTL_RXDEMOD
	    dump(fp_deilv, &Out[i],  1, m_t_llr);
	#endif

    }
}
//
///*
//* 217维特比译码
//* dec_input : 输入
//* dec_output ： 输出
//* len_output : 输出长度
//* 无返回值
//*/
template <class T>
void Cls_rx_data_phy_2_1<T>::viterbi_decoder_217(double *dec_input, int *dec_output, int len_output)
{
    int i,j;
    int outbit1_if_input_0[STATES_NUM];
    int outbit2_if_input_0[STATES_NUM];
    int outbit1_if_input_1[STATES_NUM];
    int outbit2_if_input_1[STATES_NUM];
    int last_state_smaller[STATES_NUM];
    int last_state_larger[STATES_NUM];
    double outbit1_from_last_state_smaller[STATES_NUM];
    double outbit2_from_last_state_smaller[STATES_NUM];
    double outbit1_from_last_state_larger[STATES_NUM];
    double outbit2_from_last_state_larger[STATES_NUM];

    int bit[6];

    for (i = 0; i <STATES_NUM; i++)
    {
        for (j = 0; j < 6; j++)
        {
            bit[j] = ((i &(0x1 << j)) >> j);
        }

        //(133,171)
        /*outbit1_if_input_0[i] = (bit[1] + bit[2] + bit[4] + bit[5]) % 2;
        outbit2_if_input_0[i] = (bit[0] + bit[1] + bit[2] + bit[5]) % 2;

        outbit1_if_input_1[i] = (bit[1] + bit[2] + bit[4] + bit[5] + 1) % 2;
        outbit2_if_input_1[i] = (bit[0] + bit[1] + bit[2] + bit[5] + 1) % 2;*/

        //(171,133)
        outbit1_if_input_0[i] = (bit[0] + bit[1] + bit[2] + bit[5]) % 2;
        outbit2_if_input_0[i] = (bit[1] + bit[2] + bit[4] + bit[5]) % 2;

        outbit1_if_input_1[i] = (bit[0] + bit[1] + bit[2] + bit[5] + 1) % 2;
        outbit2_if_input_1[i] = (bit[1] + bit[2] + bit[4] + bit[5] + 1) % 2;

    }


    for (i = 0; i < STATES_NUM; i++)
    {
        if ((i % 2) == 0)
        {
            last_state_smaller[i] = i >> 1;
            outbit1_from_last_state_smaller[i] = outbit1_if_input_0[last_state_smaller[i]];
            outbit2_from_last_state_smaller[i] = outbit2_if_input_0[last_state_smaller[i]];
            last_state_larger[i] = (i + STATES_NUM) >> 1;
            outbit1_from_last_state_larger[i] = outbit1_if_input_0[last_state_larger[i]];
            outbit2_from_last_state_larger[i] = outbit2_if_input_0[last_state_larger[i]];
        }
        else
        {
            last_state_smaller[i] = last_state_smaller[i - 1];
            outbit1_from_last_state_smaller[i] = outbit1_if_input_1[last_state_smaller[i]];
            outbit2_from_last_state_smaller[i] = outbit2_if_input_1[last_state_smaller[i]];
            last_state_larger[i] = last_state_larger[i - 1];
            outbit1_from_last_state_larger[i] = outbit1_if_input_1[last_state_larger[i]];
            outbit2_from_last_state_larger[i] = outbit2_if_input_1[last_state_larger[i]];
        }
    }


    for (i = 0; i < STATES_NUM; i++)
    {
        if (outbit1_from_last_state_smaller[i] == 0)
            outbit1_from_last_state_smaller[i] = 1;
        else
            outbit1_from_last_state_smaller[i] = -1;

        if (outbit2_from_last_state_smaller[i] == 0)
            outbit2_from_last_state_smaller[i] = 1;
        else
            outbit2_from_last_state_smaller[i] = -1;

        if (outbit1_from_last_state_larger[i] == 0)
            outbit1_from_last_state_larger[i] = 1;
        else
            outbit1_from_last_state_larger[i] = -1;

        if (outbit2_from_last_state_larger[i] == 0)
            outbit2_from_last_state_larger[i] = 1;
        else
            outbit2_from_last_state_larger[i] = -1;
    }



    int path_record[STATES_NUM][432*5];

    double state_metrics_last[STATES_NUM];
    double branch_metrics[STATES_NUM];

    m_state_metrics[0] = 0.0;
    for (i = 1; i < STATES_NUM; i++)
    {
        m_state_metrics[i] = 128;//10000
    }

    int step;
    int sm_min;
    int sm_max;
    //t_start = clock();

    for (step = 1; step <= len_output; step++)
    {
        int i;
        int state;

        for (i = 0; i < STATES_NUM; i++)
        {
            state_metrics_last[i] = m_state_metrics[i];
        }

        for (state = 0; state < STATES_NUM; state++)
        {
            //branch metrics can be calculated only once
            branch_metrics[state] = dec_input[2 * (step - 1)] * outbit1_from_last_state_smaller[state]
                                    + dec_input[2 * (step - 1) + 1] * outbit2_from_last_state_smaller[state];
#ifndef CC_NEW
            m_path_metrics_from_smaller[state] = state_metrics_last[last_state_smaller[state]] - branch_metrics[state];
            m_path_metrics_from_larger[state] = state_metrics_last[last_state_larger[state]] + branch_metrics[state];
            path_record[state][step - 1] = (m_path_metrics_from_smaller[state] >= m_path_metrics_from_larger[state]);
#else
            m_branch_metrics = branch_metrics[state] / 2;
            //LOG2DEV<<m_branch_metrics<<endl;
            m_path_metrics_from_smaller[state] = state_metrics_last[last_state_smaller[state]] - m_branch_metrics;
            m_path_metrics_from_larger[state] = state_metrics_last[last_state_larger[state]] + m_branch_metrics;
            path_record[state][step - 1] = m_path_metrics_from_smaller[state] >= m_path_metrics_from_larger[state];
#endif

#ifdef DUMP_MATLAB_CC
            fp_cc_path<<m_path_metrics_from_smaller[state] <<"\t" <<  m_path_metrics_from_larger[state]<<"\t"<<path_record[state][step - 1] <<endl;
            fp_cc_path_recorder<<path_record[state][step - 1]<<endl;
#endif
            m_state_metrics[state] = path_record[state][step - 1]?m_path_metrics_from_larger[state]:m_path_metrics_from_smaller[state];

        }
        sm_min = 1000000;
        sm_max = -100000;
        for (state = 0; state < STATES_NUM; state++)
        {
            if(m_state_metrics[state] < sm_min)
                sm_min = m_state_metrics[state];
            if(m_state_metrics[state] > sm_max)
                sm_max = m_state_metrics[state];
        }

        if(sm_max > 32 && sm_min < -32)
        {
            for (state = 0; state < STATES_NUM; state++)
            {
                m_state_metrics[state] = (double)m_state_metrics[state];
            }
        }
        else if(sm_max > 32)
        {
            for (state = 0; state < STATES_NUM; state++)
            {
                m_state_metrics[state] = (double)m_state_metrics[state] - 64;
            }
        }
        else if(sm_min < -32)
        {
            for (state = 0; state < STATES_NUM; state++)
            {
                m_state_metrics[state] = (double)m_state_metrics[state] + 64;
            }
        }
    }

    int from_state = 0;
    for (step = len_output; step > 0; step--)
    {
        dec_output[step - 1] = from_state % 2;
        from_state = path_record[from_state][step - 1]?last_state_larger[from_state]:last_state_smaller[from_state];
    }

}
//
template <class T>
void Cls_rx_data_phy_2_1<T>::Cal_SNR(double * snr_est)
{
    int PEsti;
    int DataLenIn1 = m_SymRxCombineBest_len;
    int DataLenIn = m_FskDemodLLR_len;
    double p_s;
    double tmpMean_fsk,tmpVar;
    double t_snr_est_fxp;

    m_Avg_qpsk = 0;
    p_s = 0.0;
    for(PEsti = 0;PEsti < DataLenIn1;PEsti++)
    {
        t_p =  m_SymRxCombineBest[PEsti].power();
        p_s += t_p;
    }
    t_p_s = p_s;
    t_avg_qpsk = double(DataLenIn1)/t_p_s;//t_p_s / double(DataLenIn1);
    m_Avg_qpsk = t_avg_qpsk;

    tmpMean_fsk = 0;
    tmpVar = 0;
    for(PEsti = 0;PEsti < DataLenIn;PEsti++)
    {
        m_FskDemodLLR_mean[PEsti] = m_FskDemodLLR[PEsti]*(1 - 2*m_FskReInterlvOut[PEsti]);
        tmpMean_fsk += m_FskDemodLLR_mean[PEsti];
    }
    if(tmpMean_fsk<0){
        tmpMean_fsk = 0;
    }
    t_tmpMean_fsk = tmpMean_fsk;
    t_tmpMean = ((double)t_tmpMean_fsk / DataLenIn);
    for(PEsti = 0;PEsti < DataLenIn;PEsti++)
    {
        t_err_p = (m_FskDemodLLR_mean[PEsti] - t_tmpMean) * (m_FskDemodLLR_mean[PEsti] - t_tmpMean);
        tmpVar += t_err_p;
    }
    t_tmpVar = tmpVar;
    //t_snr_est = 20*log10(t_tmpMean) - 10*log10(t_tmpVar) + 10*log10(DataLenIn - 1);
    t_snr_est_fxp = 2*log10_f.run(t_tmpMean) - 1*log10_f.run(t_tmpVar) + 1*log10_f.run(DataLenIn - 1);
    //LOG2DEV << t_snr_est_fxp<<endl;
#ifdef DUMP_RTL_RXDEMOD
    fxp rtl_dump;
    fix_set(rtl_dump , 9, 7, FXP_TC, FXP_RND, FXP_SAT_SYM);
    rtl_dump = t_snr_est_fxp;
    dump(fp_snr, &rtl_dump,  1, rtl_dump, 0);
#endif
    //cout<<"t_snr_est = "<<t_snr_est<<"\tt_snr_est_fxp="<<t_snr_est_fxp<<endl;
    *snr_est = t_snr_est_fxp;
    // cout << "SNR = " << *snr_est << "dB" << endl;

}
//
//
template <class T>
void Cls_rx_data_phy_2_1<T>::Decode_Recode(int SecFlag,double * demodBits1_quant,int * DecodeOut, int *RecodeOut,int Datain)
{
    int InputLen = Datain;
    int CCEncodeLen;

    //浮点维特比译码
    for(int i = 0;i<Datain;i++)
    {
//        demodBits1_quant[i] = round(demodBits1_quant[i]*31);
        if(demodBits1_quant[i]>31)
            demodBits1_quant[i] = 31;
        if(demodBits1_quant[i]<-31)
            demodBits1_quant[i] = -31;
    }

    CC_Decode(demodBits1_quant,DecodeOut,InputLen);


    if(SecFlag == 0)
    {
        CCEncodeLen = InputLen/2;
        CC_Encode(DecodeOut,RecodeOut,CCEncodeLen);
    }

    #ifdef DUMP_RTL_RXDEMOD
    int i;
    fxp rtl_dump0;
    fix_set(rtl_dump0,  4, 4, FXP_US, FXP_RND, FXP_SAT_SYM);
    for(i=0;i<(Datain/2);i=i+1){
    	rtl_dump0 = DecodeOut[i];
    	dump(fp_dec, &rtl_dump0,  1, rtl_dump0);
	rtl_dump0 = (RecodeOut[2*i]<<1) + RecodeOut[2*i+1];
    	if(SecFlag == 0) dump(fp_rec, &rtl_dump0,  1, rtl_dump0);
    }
    #endif

}
//
//
///*
//* FSK卷积译码函数
//* demodBits1_quant : FSK交织输入
//* vit_out ： FSK译码输出
//* DatalenIn : 译码输出长度
//* 无返回值
//*/
template <class T>
void Cls_rx_data_phy_2_1<T>::CC_Decode(double * demodBits1_quant,int * vit_out,int DatalenIn)
{
    int tblen = 72;
    int total_N,Deci;
    int decoded_len;

    total_N = DatalenIn/2;

    if(total_N>tblen*2)
    {
        mem_copy(buff_double1,demodBits1_quant, DatalenIn);
        mem_copy(buff_double1 + DatalenIn,demodBits1_quant, DatalenIn);
        mem_copy(buff_double1 + 2*DatalenIn,demodBits1_quant, DatalenIn);
        mem_copy(buff_double1 + 3*DatalenIn,demodBits1_quant, DatalenIn);
        decoded_len = DatalenIn*2;
    }
    else
    {
        mem_copy(buff_double1,demodBits1_quant, DatalenIn);
        mem_copy(buff_double1 + DatalenIn,demodBits1_quant, DatalenIn);
        mem_copy(buff_double1 + 2*DatalenIn,demodBits1_quant, DatalenIn);
        mem_copy(buff_double1 + 3*DatalenIn,demodBits1_quant, DatalenIn);
        mem_copy(buff_double1 + 4*DatalenIn,demodBits1_quant, DatalenIn);
        mem_copy(buff_double1 + 5*DatalenIn,demodBits1_quant, DatalenIn);
        mem_copy(buff_double1 + 6*DatalenIn,demodBits1_quant, DatalenIn);
        mem_copy(buff_double1 + 7*DatalenIn,demodBits1_quant, DatalenIn);
        mem_copy(buff_double1 + 8*DatalenIn,demodBits1_quant, DatalenIn);
        mem_copy(buff_double1 + 9*DatalenIn,demodBits1_quant, DatalenIn);
        decoded_len = DatalenIn*5;
    }

#ifdef CC_NEW
    decoded_len = total_N + tblen + 120 ;
#endif

    memset(buff_int1,0,sizeof(int) * decoded_len);

    viterbi_decoder_217(buff_double1, buff_int1, decoded_len);

    for(Deci = 0;Deci < total_N;Deci++)
    {
        vit_out[Deci] = buff_int1[Deci + total_N];
    }

    if(total_N >= tblen)
    {
        for(Deci = 0;Deci < total_N - tblen;Deci++)
        {
            vit_out[Deci + tblen] = buff_int1[Deci + tblen];
        }
    }
}
//
///*
//* FSK卷积编码函数
//* In : FSK卷积编码输入
//* Out ： FSK卷积编码输出
//* 无返回值
//*/
template <class T>
void Cls_rx_data_phy_2_1<T>::CC_Encode(int * in,int * out,int lengthIn)
{
    int convol_outs[2][64] = {{0,3,1,2,0,3,1,2,3,0,2,1,3,0,2,1,3,0,2,1,3,0,2,1,0,3,1,2,0,3,1,2,2,1,3,0,2,1,3,0,1,2,0,3,1,2,0,3,1,2,0,3,1,2,0,3,2,1,3,0,2,1,3,0},
                              {3,0,2,1,3,0,2,1,0,3,1,2,0,3,1,2,0,3,1,2,0,3,1,2,3,0,2,1,3,0,2,1,1,2,0,3,1,2,0,3,2,1,3,0,2,1,3,0,2,1,3,0,2,1,3,0,1,2,0,3,1,2,0,3}};
    int i;
    int circle_tail = 6;
    int state = (in[lengthIn - 1] << 5) + (in[lengthIn - 2] << 4) + (in[lengthIn - 3] << 3)
                + (in[lengthIn - 4] << 2) + (in[lengthIn - 5] << 1) + in[lengthIn - 6];
    int val;

    for (i = 0; i < circle_tail; ++i) {
        state = (state >> 1) + in[lengthIn - 6 + i] * 32;
    }

    for (i = 0; i < lengthIn; i ++){
        val = convol_outs[in[i]][state];
        out[i * 2      ] = val >> 1;
        out[i * 2 + 1] = val & 0x1;
        state = (state >> 1) + in[i] * 32;
    }

}
//
template <class T>
void Cls_rx_data_phy_2_1<T>::Interlv()
{
    int i,j;
    int row,col;
    int InterLvIndex[24] = {22,10,16,4,19,7,13,1,21,9,15,3,18,6,12,0,2,14,8,20,5,17,11,23};
    int tempIndex = 0;
    int ii;
    int counttmp = 0;

    col = 24;
    row = (m_FskRecodedOut_len + col-1)/ col;

    for(i = 0;i < m_FskRecodedOut_len;i++)
    {
        buff_int1[i] = m_FskRecodedOut[i]^((SCcode[i>>5]>>(i&31))&(0x1));
    }

    for(i = 0;i< col;i++)
    {
        ii = InterLvIndex[i];
        for(j = 0;j < row;j++)
        {
            tempIndex = j * col + ii;
            if (tempIndex < m_FskRecodedOut_len && counttmp < m_FskReInterlvOut_len)
            {
                m_FskReInterlvOut[counttmp] = buff_int1 [tempIndex];
                counttmp = counttmp + 1;
#ifdef DUMP_RTL_RXDEMOD
			fxp rtl_dump0;
    			fix_set(rtl_dump0,  4, 4, FXP_US, FXP_RND, FXP_SAT_SYM);
			rtl_dump0=buff_int1 [tempIndex];
	    		dump(fp_ilv, &rtl_dump0,  1, rtl_dump0);
#endif
            }
        }
    }
}
//
///*
//* Dqpsk_Demod函数
//* symRxCombineIn : DQPSK输入
//* avg_qpskIn ： DQPSK归一化值
//* ReInterLv_out_fsk ： FSK交织之后数据
//* vit_out_dqpsk : DQPSK译码输出
//* snr_est：DQPSK计算的SNR
//* DataLenIn ： 输入长度
//* 无返回值
//*/
template <class T>
void Cls_rx_data_phy_2_1<T>::Dqpsk_Demod(double * snr_est,double * RSSI)
{
    int i;
    double t_snr_est_fxp2,var_sum,t_qpsk_sum;
    Dqpsk_LLR(RSSI);
/******  SNR  Est*******************/
    t_qpsk_sum = 0;
    var_sum = 0;
    for(i = 0;i < m_DqpskDemodLLR_len;i++)
    {
        t_qpsk_sum = t_qpsk_sum + abs(m_DqpskDemodLLR[i]);
    }
    t_Mean_qpsk = (double)t_qpsk_sum/m_DqpskDemodLLR_len;
    for(i = 0;i<m_DqpskDemodLLR_len;i++)
    {
        t_var_sum = (abs(m_DqpskDemodLLR[i]) - t_Mean_qpsk) * (abs(m_DqpskDemodLLR[i]) - t_Mean_qpsk);
        var_sum = var_sum + t_var_sum;//(abs(m_DqpskDemodLLR[i]) - t_Mean_qpsk) * (abs(m_DqpskDemodLLR[i]) - t_Mean_qpsk);
    }
    t_Var_qpsk = (double)var_sum/(m_DqpskDemodLLR_len - 1);
    //t_snr_est = 20 * log10(t_Mean_qpsk) - 10*log10(t_Var_qpsk);
    t_snr_est_fxp2 = 2*log10_f.run(t_Mean_qpsk) - 1*log10_f.run(t_Var_qpsk) ;
    *snr_est = t_snr_est_fxp2;
    //LOG2DEV << t_snr_est_fxp2<<endl;
#ifdef DUMP_RTL_RXDEMOD
    fxp rtl_dump;
    fix_set(rtl_dump , 9, 7, FXP_TC, FXP_RND, FXP_SAT_SYM);
    rtl_dump = t_snr_est_fxp2;
    dump(fp_snr, &rtl_dump,  1, rtl_dump);
#endif
/******  SNR  Est*******************/
    DeInterlv(m_DqpskDemodLLR,m_DqpskDeInterlvLLR,m_DqpskaddLen,m_DqpskDemodLLR_len);

    Decode_Recode(1,m_DqpskDeInterlvLLR,m_DqpskDecodedOut ,m_DqpskRecodedOut,m_DqpskDeInterlvLLR_len);
}
//
///*
//* DQPSKLLR函数
//* symRxCombine : 函数输入
//* avg_qpsk ： 用于DQPSK归一化的值
//* ReInterLv_out_fsk ： FSK译码交织之后的输出
//* demodBits2 ： DQPSKLLR函数输出
//* DataLenIn：输入长度
//* 无返回值
//*/
template <class T>
void Cls_rx_data_phy_2_1<T>::Dqpsk_LLR(double *RSSI)
{
    int SymNumDataNDsss,iSym;
    int tmp1,tmp2;
    int Grayi;
    int DqpskRefBit[2] = {1,0};
    int BitTable4[4] = { 0,1,3,2 };
//    double tmpE = 0;

//    Complex<T> t_diff;
//    T avg_qpskTmp = 0;
//    T t_bit0 = 0;
//    T t_bit1 = 0;
//    T t_sub = 0;


    SymNumDataNDsss = m_cfg.SymNumData;
    avg_qpskTmp = m_Avg_qpsk;

    m_dem1h[0] = DqpskRefBit[0];
    m_dem2h[0] = DqpskRefBit[1];

    for(iSym = 0;iSym < SymNumDataNDsss;iSym++)
    {
        m_dem1h[iSym + 1] = m_FskReInterlvOut[2 * iSym];
        m_dem2h[iSym + 1] = m_FskReInterlvOut[2*iSym+1];
    }

    for(iSym = 0;iSym < SymNumDataNDsss;iSym++)
    {
        Grayi = (2*m_dem1h[iSym] + m_dem2h[iSym]);
        tmp1 = BitTable4[Grayi];
        Grayi = (2*m_dem1h[iSym+1] + m_dem2h[iSym+1]);
        tmp2 = BitTable4[Grayi];
        m_symRxCombineTmp.I = m_symRxCombine[iSym*4 + tmp1].I;
        m_symRxCombineTmp.Q = -m_symRxCombine[iSym*4 + tmp1].Q;

        t_diff = m_symRxCombine[(iSym + 1)*4 + tmp2] * m_symRxCombineTmp;

        //tmpE =  tmpE + m_symRxCombineFDC[(iSym + 1)*4 + tmp2].power();

        Proj[0] = t_diff.I;
        Proj[1] = t_diff.Q;
        Proj[2] = -t_diff.I;
        Proj[3] = -t_diff.Q;

        t_bit0 = Proj[0] > Proj[1] ? Proj[0] : Proj[1];
        t_bit1 = Proj[2] > Proj[3] ? Proj[2] : Proj[3];
        t_sub = t_bit0 - t_bit1;
        m_dem3[iSym] = t_sub;

        t_bit0 = Proj[0] > Proj[3] ? Proj[0] : Proj[3];
        t_bit1 = Proj[1] > Proj[2] ? Proj[1] : Proj[2];
        t_sub = t_bit0 - t_bit1;
        m_dem4[iSym] = t_sub;

        m_t_llr = m_dem3[iSym] * avg_qpskTmp*31  ; m_DqpskDemodLLR[2 * iSym] = m_t_llr;
        m_t_llr = m_dem4[iSym] * avg_qpskTmp*31  ; m_DqpskDemodLLR[2*iSym+1] = m_t_llr;

	#ifdef DUMP_RTL_RXDEMOD
	    dump(fp_psk_llr, &m_DqpskDemodLLR[2 * iSym],  1, m_t_llr);
	    dump(fp_psk_llr, &m_DqpskDemodLLR[2*iSym+1],  1, m_t_llr);
	#endif
    }

    *RSSI = m_Avg_qpsk;

}
//
///*
//* CRC校验函数
//* decoded_out_fsk : fsk译码输出
//* decoded_out_dqpsk : dqpsk译码输出
//* ExternalCRCLen:CRC校验长度
//* CRC_Error_fsk：fsk的校验结果
//* errorCRC : CRC校验结果，0校验通过，1校验未通过
//* DatalenIn : CRC校验计算输入长度
//* 无返回值
//*/
template <class T>
void Cls_rx_data_phy_2_1<T>::CRC_Check(int * errorCRC)
{
    int CRCi;

    {
        for(CRCi = 0;CRCi < m_DqpskDecodedOut_len;CRCi++)
        {
            buff_int1[CRCi] = m_FskDecodedOut[CRCi];
            buff_int1[CRCi+m_DqpskDecodedOut_len] = m_DqpskDecodedOut[CRCi];
//            cout<<buff_int1[CRCi]<<endl;
        }
        crc_decode(buff_int1,ExternalCRCLen,errorCRC,m_DqpskDecodedOut_len * 2);
    }
}
//
///*
//* CRC校验计算函数
//* rev : CRC计算输入
//* ExternalCRCLen : CRC长度
//* error2 : CRC校验结果，0校验通过，1校验未通过
//* DatalenIn : CRC校验计算输入长度
//* 无返回值
//*/
template <class T>
void Cls_rx_data_phy_2_1<T>::crc_decode(int * rev, int ExternalCRCLen,int * error2,int DatalenIn)
{
    int ii,crci,N,ErrSum;
    int poly[16],revTemp[16],M2[16];
    int tmp = 0;

    #ifdef DUMP_RTL_RXDEMOD
    fxp rtl_dump0, rtl_dump1;
    fix_set(rtl_dump0,   4,  4, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(rtl_dump1,   16,  16, FXP_US, FXP_RND, FXP_SAT_SYM);
    for(ii=0;ii<DatalenIn;ii=ii+1){
    	rtl_dump0 = rev[ii];
        dump(fp_crc_in, &rtl_dump0,    1, rtl_dump0);
    }
    #endif

    N = ExternalCRCLen + 1;
    memset(poly,0,sizeof(int) * N);

     if(ExternalCRCLen == 14)
    {
        poly[0]  = 1;
        poly[1]  = 0;
        poly[2]  = 0;
        poly[3]  = 0;
        poly[4]  = 1;
        poly[5]  = 1;
        poly[6]  = 0;
        poly[7]  = 1;
        poly[8]  = 1;
        poly[9]  = 1;
        poly[10] = 0;
        poly[11] = 0;
        poly[12] = 0;
        poly[13] = 1;
        poly[14] = 1;
//        int poly[15] = {1,0,0,0,1,1,0,1,1,1,0,0,0,1,1};
    }

    memset(revTemp,0,sizeof(int) * N);
    memcpy(revTemp,rev,sizeof(int) * N);
    memset(M2,0,sizeof(int) * N);

    for(ii = 0;ii < DatalenIn - N;ii++)
    {
        if(revTemp[0] == 1)
        {
            for(crci = 0;crci < N;crci++)
            {
                M2[crci] = revTemp[crci]^poly[crci];
            }
        }
        else if(revTemp[0] == 0)
        {
            for(crci = 0;crci < N;crci++)
            {
                M2[crci] = revTemp[crci]^0;
            }
        }

	#ifdef DUMP_RTL_RXDEMOD
	int rtl_m2 = 0;
	for(crci = 0;crci < N;crci++)
        {
	    rtl_m2 = (rtl_m2>>1) | ((M2[crci] & 0x1)<<ExternalCRCLen);
        }
	rtl_dump1=rtl_m2;
	dump(fp_crc, &rtl_dump1,    1, rtl_dump1);
	#endif

        for(crci = 0;crci < N-1;crci++)
        {
            revTemp[crci] = M2[crci + 1];
        }
        revTemp[N-1] = rev[N + ii];
    }

    if(revTemp[0] == 1)
    {
        for(crci = 0;crci < N;crci++)
        {
            revTemp[crci] = revTemp[crci]^poly[crci];
        }
    }

    #ifdef DUMP_RTL_RXDEMOD
    int rtl_m2 = 0;
    for(crci = 0;crci < N;crci++)
    {
        rtl_m2 = (rtl_m2>>1) | ((revTemp[crci] & 0x1)<<ExternalCRCLen);
    }
    rtl_dump1=rtl_m2;
    dump(fp_crc_rev, &rtl_dump1,    1, rtl_dump1);
    #endif

    for(ii = 0;ii < DatalenIn;ii++)
    {
        tmp += rev[ii];
    }
    if(tmp == 0)
    {
        *error2 = 0;
        return ;
    }

    ErrSum = 0;
    for(crci = 0;crci < N;crci++)
    {
        ErrSum = ErrSum + revTemp[crci];
    }
    if(ErrSum == 0)
    {
        *error2 = 0;
    }
    else
    {
        *error2 = 1;
    }
}


template <class T>
void Cls_rx_data_phy_2_1<T>::GenFskFreqsTS()
{
    int i;
//    double tmp;
    int LenSymTmp = m_LenSym * 4;

    t_fre = 0.5/LenSymTmp*2*PI;
    t_fre1 = 3*t_fre;
    for(i = 2;i < LenSymTmp ;i = i+4)
    {
        t_fre3cos = cos(i*t_fre1);
        t_fre3sin = sin(i*t_fre1);
        t_frecos = cos(i*t_fre);
        t_fresin = sin(i*t_fre);

        m_FskFreqsLoc[0][i/4].I = t_fre3cos;
        m_FskFreqsLoc[0][i/4].Q = -1*t_fre3sin;

        m_FskFreqsLoc[1][i/4].I = t_frecos;
        m_FskFreqsLoc[1][i/4].Q = -1*t_fresin;

        m_FskFreqsLoc[2][i/4].I = t_frecos;
        m_FskFreqsLoc[2][i/4].Q = t_fresin;

        m_FskFreqsLoc[3][i/4].I = t_fre3cos;
        m_FskFreqsLoc[3][i/4].Q = t_fre3sin;

    }
}

template<class T>
void Cls_rx_data_phy_2_1<T>::GenDSeq()
{
    int i;

    switch(m_cfg.DSSSNum)
    {
        case 8 :
            for(i = 0;i<8;i++)
            {
                m_DSeq[i] = DSS8[i];
            }
            break;
        case 4 :
            for(i = 0;i<4;i++)
            {
                m_DSeq[i] = DSS4[i];
            }
            break;
        case 2 :
            for(i = 0;i<2;i++)
            {
                m_DSeq[i] = DSS2[i];
            }
            break;
        case 1 :
            m_DSeq[0] = 1;
            break;
    }
}

template<class T>
void Cls_rx_data_phy_2_1<T>::DemodFreqDriftComp(double DemodFreqDriftComp_tmp)
{
    int j;
    double tmpphase_tmp;
        tmpphase_tmp = 0;
        t_tmpphase_tmp = 0;
        t_FreqOffsetDriftTabletmp = 2.0 / (m_Fs*m_Fs);
        for(j = 0;j < (m_cfg.SymNumData+SymNumPreamble) * m_DSSSNum;j++)
        {
//            t_FreqOffsetDriftTabletmp = j * DemodFreqDriftComp_tmp / m_Fs;
//            t_tmpPhasedelta = 2*t_FreqOffsetDriftTabletmp / m_Fs;//t_tmpPhasedelta = 2*PI*t_FreqOffsetDriftTabletmp / m_Fs;
            t_tmpPhasedelta = j*DemodFreqDriftComp_tmp *t_FreqOffsetDriftTabletmp;//t_tmpPhasedelta = 2*PI*t_FreqOffsetDriftTabletmp / m_Fs;

            tmpphase_tmp = t_tmpphase_tmp + t_tmpPhasedelta;
            tmpphase_tmp = fmod(tmpphase_tmp,2);//tmpphase = fmod(tmpphase,2*PI);
            if(tmpphase_tmp>1)
                tmpphase_tmp = tmpphase_tmp - 2;//tmpphase = tmpphase - 2*PI;
            t_tmpphase_tmp = tmpphase_tmp;
            t_tmpPhase = t_tmpphase_tmp;//t_tmpPhase = tmpphase / PI;

            t_tmpExp.I = cos(t_tmpPhase * PI);
            t_tmpExp.Q = sin(t_tmpPhase * PI);
            tmpExp[j] = t_tmpExp;
        }

        for(j = 0;j<m_symRxCombineDsss_len / 4;j++)
        {
            t_symRxCombineDsssFDC = (m_symRxCombineDsss[4*j]*tmpExp[j+(SymNumPreamble-1)*m_DSSSNum]);
            m_symRxCombineDsssFDC[4*j] = t_symRxCombineDsssFDC;
            t_symRxCombineDsssFDC = (m_symRxCombineDsss[4*j+1]*tmpExp[j+(SymNumPreamble-1)*m_DSSSNum]);
            m_symRxCombineDsssFDC[4*j+1] = t_symRxCombineDsssFDC;
            t_symRxCombineDsssFDC = (m_symRxCombineDsss[4*j+2]*tmpExp[j+(SymNumPreamble-1)*m_DSSSNum]);
            m_symRxCombineDsssFDC[4*j+2] = t_symRxCombineDsssFDC;
            t_symRxCombineDsssFDC = (m_symRxCombineDsss[4*j+3]*tmpExp[j+(SymNumPreamble-1)*m_DSSSNum]);
            m_symRxCombineDsssFDC[4*j+3] = t_symRxCombineDsssFDC;

	    #ifdef DUMP_RTL_RXDEMOD
	    dump(fp_fdc, &m_symRxCombineDsssFDC[4*j],    1, t_symRxCombineDsssFDC.I);
            dump(fp_fdc, &m_symRxCombineDsssFDC[4*j+1],  1, t_symRxCombineDsssFDC.I);
       	    dump(fp_fdc, &m_symRxCombineDsssFDC[4*j+2],  1, t_symRxCombineDsssFDC.I);
       	    dump(fp_fdc, &m_symRxCombineDsssFDC[4*j+3],  1, t_symRxCombineDsssFDC.I);
	    #endif

        }


}

template<class T>
double Cls_rx_data_phy_2_1<T>::abs(double aa)
{
    if(aa >= 0)
        return aa;
    else
        return -1*aa;

}

template<class T>
void Cls_rx_data_phy_2_1<T>::DeDsss()
{
    int i,j,k;
    Complex<double> tmpDSR1Sum1;

    for(i = 0;i<m_SymNumData+1;i++)
    {
        for(j=0;j < m_DSSSNum;j++)
        {
            for(k = 0;k<4;k++)
            {
                tmpDSR[j][k] = m_symRxCombineDsssFDC[i*4*m_DSSSNum+j*4 +k];

            }
        }


        for(j = 0;j < 4;j++)
        {
            tmpDSR1Sum1 = 0;
            for(k = 0;k < m_DSSSNum;k++)
            {
                tmpDSR1Sum1 = tmpDSR1Sum1 + tmpDSR[k][j]*m_DSeq[k];
            }
            t_tmpDSR1Sum = tmpDSR1Sum1 / m_DSSSNum;
            tmpDSR1Sum[j] = t_tmpDSR1Sum;
            m_symRxCombine[j + 4*i] = tmpDSR1Sum[j];

#ifdef DUMP_RTL_RXDEMOD
	    dump(fp_combine, &m_symRxCombine[j + 4*i],  1, t_tmpDSR1Sum.I);
#endif
        }
    }
}
