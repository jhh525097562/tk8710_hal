/****************************************************************************
**
**  Copyright  (c)  2023-2025,  All Rights Reserved.
**
**  File          : ud_testbench.cpp (main program to run simulation)
**  Created By    :
**  Created On    :
**
**  History     :
**  Programmer      Date        Rev      Description
**  --------------- ----------  -------- ------------------------------
**  Darren.Ren      2023/10/10  V1.0     Initial
**  Notice: 主函数入口
****************************************************************************/

/******************Rx********************/
#include "../Cls_ud.h"
#include "../Cls_GWRxBFModule.h"
#include "../Cls_BFSelect.h"
#include "../Cls_rx_data_phy2_1.h"
#include "../Cls_Rx_Sync_2_1.h"
#include <sys/stat.h>

/******************Tx********************/
#include "../Cls_tx_encode.h"
#include "../Cls_tx_beamforming.h"
#include "../Cls_tx_mod.h"
#include "../Cls_tx_us_2_1.h"
#include "../Cls_TX_US.h"
#include "testbench_options.h"
#include "matlab_tx_inputs.h"
#include <vector>
#include <algorithm>

// Default to fixed point. Floating-point runs must opt in explicitly.
#if defined(CMODEL_FIXED_POINT) && defined(CMODEL_FLOAT_POINT)
#error Select only one numeric mode
#endif
#ifndef CMODEL_FLOAT_POINT
#define Fix
#endif
#ifdef Fix
using ModelScalar = fxp;
#else
using ModelScalar = double;
#endif
//#define Print_Log

typedef struct GWRx2TXCFG
{
    //Read Write
    int                 TxUserNum;
    Complex<double>     AH[8][128];
    double              TxFreq[128];
    double              GWRxPilotPower[128];
    double              ANoise[8];
    Complex<double>     CalibrateFactor[8];
}GWRx2TXCFG;

bool DataArraysCompare(int *arr1,int *arr2,int Size){
    for(int i = 0; i < Size; i++){
        if(arr1[i] != arr2[i]){
            return false;
        }
    }
    return true;
}

int main(int argc, char *argv[])
{
    TestbenchOptions options;
    if (!options.parse(argc, argv)) return 2;
#ifdef Fix
    LOG2DEV << "NUMERIC_MODE=fixed" << std::endl;
#else
    LOG2DEV << "NUMERIC_MODE=float (double)" << std::endl;
#endif
    int						case_num;
    int                    	statMd = 0;
    /****************************Rx****************************/
    int                     ant_num;
    int                     fft_len;
    FFT_LEN                 reg_fft_len;
    UD_CFG                  cfg = {};
    GWRXBFMODULE_CFG        cfg1 = {};
    BFSelect_CFG            cfg2 = {};
    RX_PHY2_1_SYNC_CFG      cfg3 = {};
    RX_DATA_2_1_CFG         cfg4 = {};
    struct 					stat statExist;
	char 					num[10] = {0};
    double                  in_I = 0;
    double                  in_Q = 0;
    int                     reg_fft_len_s;
    int                     WindowNum;
    int                     GWRxSampleRate;
    double                  anoise_the1;
    double                  anoise_the2;
    int                     P1_the;
    double                  p1_para1;
    double                  p1_para2;
    int                     p2_the;
    int                     p3_the;
    int                     p6_the;
    int                     UDSwitch;
    int                     GWAllRxUserNum;
    int                     GWAllRxMode;
    int                     SetFreqRxFlag;
    int                     SetFreqRxUserNum;
    int                     SetFreqRx[128] = {};
    int                     HCE;
    int                     FilterLen;
    int                     IFFTLen;
    int                     BFMode;
    GWRx2TXCFG              GWRX2TXPare = {};

	int                     TOScope = 0;
	double                  Fs = 0;
    int                     len_freq = 0;
	double                  freq_start = 0;
	double                  freq_inter = 0;
	int                     dsss_num = 0;
	double                  freq_drift_s = 0;
	double                  freq_drift_i = 0;
	int                     freq_drift_len = 0;
	int                     seg_num = 0;
	int                     Sym_Num_Data = 0;
	double                  data_FDComp_Delta = 0;
	int                     data_FDComp_Num = 0;
	int                     DqpskaddLen = 0;
	int                     FskaddLen = 0;
	int                     crc_en;
    Complex<double>		    m_data_in = 0.0;

    int                     FrameNum;
    Complex<double>         **in;
    #ifdef Fix
    Cls_ud<fxp>             m_ud;
    Cls_GWRxBFModule<fxp>   m_BF;
    Cls_BFSelect<fxp>       m_BFSelect;
    Cls_rx_data_phy_2_1<fxp> phy2_1_rx_dmod;
    Cls_Rx_Sync_2_1<fxp>	m_sync_FX;
    #else
    Cls_ud<double>          m_ud;
    Cls_GWRxBFModule<double>   m_BF;
    Cls_BFSelect<double>       m_BFSelect;
    Cls_rx_data_phy_2_1<double> phy2_1_rx_dmod;
    Cls_Rx_Sync_2_1<double>	m_sync_FX;
    #endif

    Complex<double>         **DemoBuffer;
    int                     **GWRxDemodDataBuffer;
    Complex<double>         *Demo_in;
    Complex<double>         correctionTiming[4];
    int                     cnt = 0;
    Complex<double>         **m_APreambleFft3_Last;
    double                  *m_APreambleFftESum3_Last;
    double                  *ANoiseLast;
    ifstream                file_1;
    ifstream                file_2;
    ifstream                file_3;
    ifstream                file_4;
    ifstream                file_5;
    ifstream                file_6;
    ifstream                file_7;
    ifstream                file_8;

    in                          = NULL;
    m_APreambleFft3_Last        = NULL;
    m_APreambleFftESum3_Last    = NULL;
    ANoiseLast                  = NULL;
    DemoBuffer                  = NULL;
    Demo_in                     = NULL;
    GWRxDemodDataBuffer         = NULL;

    int fft_reg[4] = {1024, 2048, 4096, 8192};
    double SampleRate_reg[6] = {15625.0, 31250.0, 62500.0, 125000.0, 250000.0, 500000.0};

    sscanf(argv[1],  "%d", &case_num);
    sscanf(argv[2],  "%d", &ant_num);
    sscanf(argv[3],  "%d", &reg_fft_len_s);
    sscanf(argv[4],  "%d",&WindowNum);
    sscanf(argv[5],  "%d", &GWRxSampleRate);
    sscanf(argv[6],  "%lf", &anoise_the1);
    sscanf(argv[7],  "%lf", &anoise_the2);
    sscanf(argv[8],  "%d", &UDSwitch);
    sscanf(argv[9],  "%d", &P1_the);
    sscanf(argv[10],  "%lf", &p1_para1);
    sscanf(argv[11],  "%lf", &p1_para2);
    sscanf(argv[12],  "%d", &p2_the);
    sscanf(argv[13],  "%d", &p3_the);
    sscanf(argv[14],  "%d", &p6_the);
    sscanf(argv[15],  "%d",&GWAllRxUserNum);
    sscanf(argv[16],  "%d",&GWAllRxMode);
    sscanf(argv[17],  "%d",&SetFreqRxFlag);
    sscanf(argv[18],  "%d",&SetFreqRxUserNum);
    sscanf(argv[19],  "%d",&HCE);
    sscanf(argv[20],  "%d",&FilterLen);
    sscanf(argv[21],  "%d",&IFFTLen);
    sscanf(argv[22],  "%d",&BFMode);

    sscanf(argv[23], "%d", &TOScope);
    sscanf(argv[24], "%lf", &Fs);
    sscanf(argv[25], "%d", &len_freq);
    sscanf(argv[26], "%lf", &freq_start);
    sscanf(argv[27], "%lf", &freq_inter);
    sscanf(argv[28], "%d", &dsss_num);
    sscanf(argv[29], "%d", &freq_drift_len);
    sscanf(argv[30], "%lf", &freq_drift_s);
    sscanf(argv[31], "%lf", &freq_drift_i);
    sscanf(argv[32], "%d", &seg_num);
    sscanf(argv[33], "%d", &Sym_Num_Data);
    sscanf(argv[34], "%d", &DqpskaddLen);
    sscanf(argv[35], "%d", &FskaddLen);
    sscanf(argv[36], "%d", &data_FDComp_Num);
    sscanf(argv[37], "%lf", &data_FDComp_Delta);
    sscanf(argv[38], "%d", &crc_en);

    /****************************Tx****************************/
    #ifdef Fix
    Cls_tx_encode<fxp>      m_tx_encode;
    Cls_tx_beamforming<fxp> m_tx_beamforming;
    Cls_tx_mod<fxp>         m_tx_mod;
    Cls_tx_us_2_1<fxp>      m_tx_us;
    Cls_TX_US<fxp>          m_tx_us_h;
    #else
    Cls_tx_encode<double>      m_tx_encode;
    Cls_tx_beamforming<double> m_tx_beamforming;
    Cls_tx_mod<double>         m_tx_mod;
    Cls_tx_us_2_1<double>      m_tx_us;
    Cls_TX_US<double>          m_tx_us_h;
    #endif
    TX_US_2_Cfg                     m_tx_us_cfg = {};
    TX_ENCODE_CFG                   m_encode_cfg = {};
    TX_MOD_CFG                      m_mod_cfg = {};
    TX_BEAMFORMING_CFG              m_beamforming_cfg = {};
    TX_US_2_1_Cfg                   m_us_cfg = {};
    char            		        folderName[300] = {0};

    int                  TxUserNum;
    int                  TxMode;
    double               Freq;
    double               AH[16];
    double               AHPower;
    double               CalibrateFactor[16];
    int                  TxPowerFlag;
    int                  RecipracalCaliFlag;
    double               MMSE;
    int                  TotalTxPower;
    int                  PowerDif1;
    int                  PowerDif2;
    int                  PowerDif3;
    double               AGCLen;
    double               ToneLen;
    double               CodeRate;
    int                  CRC_En;
    int                  DSSSNUM;
    double               TransRatio;
    int                  TxBlockNum;
    int                  Fbit[8];
    int                  Sbit[8];
    int                  SymNumData;
    int                  UpSampleNum;

    sscanf(argv[39], "%d", &TxUserNum);
    sscanf(argv[40], "%d", &TxMode);
    sscanf(argv[41], "%lf", &Freq);
    sscanf(argv[42], "%lf", &AH[0]);
    sscanf(argv[43], "%lf", &AHPower);
    sscanf(argv[44], "%lf", &CalibrateFactor[0]);    sscanf(argv[45], "%lf", &CalibrateFactor[1]);    sscanf(argv[46], "%lf", &CalibrateFactor[2]);    sscanf(argv[47], "%lf", &CalibrateFactor[3]);
    sscanf(argv[48], "%lf", &CalibrateFactor[4]);    sscanf(argv[49], "%lf", &CalibrateFactor[5]);    sscanf(argv[50], "%lf", &CalibrateFactor[6]);    sscanf(argv[51], "%lf", &CalibrateFactor[7]);
    sscanf(argv[52], "%lf", &CalibrateFactor[8]);    sscanf(argv[53], "%lf", &CalibrateFactor[9]);    sscanf(argv[54], "%lf", &CalibrateFactor[10]);    sscanf(argv[55], "%lf", &CalibrateFactor[11]);
    sscanf(argv[56], "%lf", &CalibrateFactor[12]);    sscanf(argv[57], "%lf", &CalibrateFactor[13]);    sscanf(argv[58], "%lf", &CalibrateFactor[14]);    sscanf(argv[59], "%lf", &CalibrateFactor[15]);

    sscanf(argv[60], "%d", &TxPowerFlag);
    sscanf(argv[61], "%d", &RecipracalCaliFlag);
    sscanf(argv[62], "%lf", &MMSE);
    sscanf(argv[63], "%d", &TotalTxPower);
    sscanf(argv[64], "%d", &PowerDif1);
    sscanf(argv[65], "%d", &PowerDif2);
    sscanf(argv[66], "%d", &PowerDif3);
    sscanf(argv[67], "%lf", &AGCLen);
    sscanf(argv[68], "%lf", &ToneLen);
    sscanf(argv[69], "%lf", &CodeRate);
    sscanf(argv[70], "%d", &CRC_En);
    sscanf(argv[71], "%d", &DSSSNUM);
    sscanf(argv[72], "%lf", &TransRatio);
    sscanf(argv[73], "%d", &TxBlockNum);
    sscanf(argv[74], "%d", &Fbit[0]);    sscanf(argv[75], "%d", &Fbit[1]);    sscanf(argv[76], "%d", &Fbit[2]);    sscanf(argv[77], "%d", &Fbit[3]);
    sscanf(argv[78], "%d", &Fbit[4]);    sscanf(argv[79], "%d", &Fbit[5]);    sscanf(argv[80], "%d", &Fbit[6]);    sscanf(argv[81], "%d", &Fbit[7]);
    sscanf(argv[82], "%d", &Sbit[0]);    sscanf(argv[83], "%d", &Sbit[1]);    sscanf(argv[84], "%d", &Sbit[2]);    sscanf(argv[85], "%d", &Sbit[3]);
    sscanf(argv[86], "%d", &Sbit[4]);    sscanf(argv[87], "%d", &Sbit[5]);    sscanf(argv[88], "%d", &Sbit[6]);    sscanf(argv[89], "%d", &Sbit[7]);
    sscanf(argv[90], "%d", &SymNumData);
    sscanf(argv[91], "%d", &UpSampleNum);

    if (case_num < 0 || ant_num != 8 || reg_fft_len_s < 0 || reg_fft_len_s > 3 ||
        GWRxSampleRate < 0 || GWRxSampleRate > 5 || WindowNum < 2 || WindowNum > 9 ||
        GWAllRxUserNum < 1 || GWAllRxUserNum > 128 || GWAllRxMode < 1 || GWAllRxMode > 10 ||
        IFFTLen < 1 || FilterLen < 1 || FilterLen > fft_reg[reg_fft_len_s] ||
        IFFTLen > fft_reg[reg_fft_len_s] || TxMode < 11 || TxMode > 20 ||
        CodeRate <= 0 || CodeRate > 1 || TransRatio <= 0 || TransRatio > 1 ||
        TxBlockNum < 1 || TxBlockNum > 8 || UpSampleNum < 0 || UpSampleNum > 9 ||
        SymNumData < 1 || DSSSNUM < 1 || seg_num < 1 || seg_num > 4) {
        std::cerr << "Invalid dimensions or mode parameters\n";
        return 2;
    }

    LOG2DEV << "CaseNum:" << case_num << " Mode:" << GWAllRxMode <<endl;
    if(SetFreqRxFlag==1){
        char filenameFreq[4096];
        options.case_prefix(filenameFreq, true, "Cmodel_DataIn");
        sprintf(num,"%d",case_num);
        strcat(filenameFreq, num);
        strcat(filenameFreq, "/");
        strcat(filenameFreq, "UD_SetFreq.txt");
        ifstream fw0;
        fw0.open(filenameFreq);
        for(cnt=0;cnt<128;cnt++){
            if (!(fw0 >> in_I)) { std::cerr << "Missing/truncated frequency file\n"; return 3; }
            SetFreqRx[cnt] = in_I;
        }
    }

    char folderName_Log[300];
    options.case_prefix(folderName_Log, false, "LostRate");
    sprintf(num,"%d",case_num);
    strcat(folderName_Log, num);
    strcat(folderName_Log, "/");
    strcat(folderName_Log, "log.txt");
    ofstream fwlog;
    //fwlog.open(folderName_Log);
    fwlog.open(folderName_Log,std::ios::app);
    if (!fwlog) { std::cerr << "Cannot open log: " << folderName_Log << '\n'; return 3; }

    reg_fft_len = (FFT_LEN)reg_fft_len_s;
    fft_len = fft_reg[(int)reg_fft_len];

    m_ud.m_ant_num = ant_num;
    m_ud.m_fft_len =  fft_len;
    cfg.rx_sampling_rate = SampleRate_reg[(int)GWRxSampleRate];
    cfg.anoise_the1 = anoise_the1;
    cfg.anoise_the2 = anoise_the2;
    cfg.p1_the = P1_the;
    cfg.p1_para1 = p1_para1;
    cfg.p1_para2 = p1_para2;
    cfg.p2_the = p2_the;
    cfg.p3_the = p3_the;
    cfg.p6_the = p6_the;
    cfg.GWRxAllUser = GWAllRxUserNum;
    cfg.u_mode = GWAllRxMode;
    cfg.freq_alloc_en = SetFreqRxFlag;
    cfg.freq_alloc_num = SetFreqRxUserNum;
    cfg.FilterLen = FilterLen;
    cfg.IFFTLen = IFFTLen;
    cfg.HCE = HCE;
    cfg.UDSwitch = UDSwitch;
    mem_copy(cfg.freq_alloc_set,SetFreqRx,128);
    m_ud.configure(cfg);
    char folderName_UD[300];
    options.case_prefix(folderName_UD, false, "UDModule_Data");
    sprintf(num,"%d",case_num);
    strcat(folderName_UD, num);
    strcat(folderName_UD, "/");
    if (stat(folderName_UD, &statExist) != 0) {
            statMd = TB_MKDIR(folderName_UD);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    m_ud.init(folderName_UD);

    char folderName_IS[300];
    options.case_prefix(folderName_IS, false, "BFModule_Data");
    sprintf(num,"%d",case_num);
    strcat(folderName_IS, num);
    strcat(folderName_IS, "/");
    if (stat(folderName_IS, &statExist) != 0) {
            statMd = TB_MKDIR(folderName_IS);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }

    char folderName_DemoSync[300];
    options.case_prefix(folderName_DemoSync, false, "K2201SyncModule_Data");
    sprintf(num,"%d",case_num);
    strcat(folderName_DemoSync, num);
    strcat(folderName_DemoSync, "/");
    if (stat(folderName_DemoSync, &statExist) != 0) {
            statMd = TB_MKDIR(folderName_DemoSync);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }

    char folderName_Demo[300];
    options.case_prefix(folderName_Demo, false, "K2201DemodModule_Data");
    sprintf(num,"%d",case_num);
    strcat(folderName_Demo, num);
    strcat(folderName_Demo, "/");
    if (stat(folderName_Demo, &statExist) != 0) {
            statMd = TB_MKDIR(folderName_Demo);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }

    cfg1.Antenna = ant_num;
    cfg1.FFTLen = fft_len;
    cfg1.filterLen = FilterLen;
    cfg1.ifftLen = IFFTLen;//HCE
    cfg1.mode = GWAllRxMode;
    cfg1.BFMode = BFMode;

    cfg2.ant_num = ant_num;
    cfg2.fft_len = fft_len;
    cfg2.ifft_len = IFFTLen;
    cfg2.rx_d_win_i = ant_num;
    cfg2.gw_rxbf_mode = BFMode;
    cfg2.DSSS = 1;
    cfg2.bf_th = 4;
    cfg2.bf_alpha = 0.9;
    cfg2.slidedatanum = 2;//0:1,1:2,2:4,3,8,4:16

    // SYNC Setting
    cfg3.TOScope = TOScope;
    cfg3.Fs = Fs;
    cfg3.len_freq = len_freq;
    cfg3.freq_start = freq_start;
    cfg3.freq_inter = freq_inter;
    cfg3.dsss_num = dsss_num;
    cfg3.freq_drift_len = freq_drift_len;
    cfg3.freq_drift_s = freq_drift_s;
    cfg3.freq_drift_i = freq_drift_i;
    cfg3.Sym_Num_Data = Sym_Num_Data;
    cfg3.seg_num = seg_num;

    // Demod Setting
    cfg4.data_FDComp_Delta = data_FDComp_Delta;
    cfg4.data_FDComp_Num = data_FDComp_Num;
    cfg4.DSSSNum = dsss_num;
    cfg4.CRC_EN = crc_en;
    cfg4.FskaddLen = FskaddLen;
    cfg4.FsRx = Fs;
    cfg4.SymNumData = Sym_Num_Data;

    int DemodWinDowNum = WindowNum-1;
    int TmpDemodLen = 0;
    int data_cnt = 0;
    int SumCRCResult = 0;
    int m_rx_demod_en = 0;
    int seg_cnt = 0;
    int SumRxUserNum = 0;
    double TotalRxUserNum = 0;
    DemoBuffer = safe_new(DemoBuffer, GWAllRxUserNum, (DemodWinDowNum * IFFTLen + 512));
    Demo_in = safe_new(Demo_in,(DemodWinDowNum * IFFTLen + 512));
    in = safe_new(in, 8, fft_len * WindowNum);
    m_APreambleFft3_Last = safe_new(m_APreambleFft3_Last, 8, fft_len);
    m_APreambleFftESum3_Last = safe_new(m_APreambleFftESum3_Last, fft_len);
    ANoiseLast = safe_new(ANoiseLast,8);
    GWRxDemodDataBuffer = safe_new(GWRxDemodDataBuffer,GWAllRxUserNum,1024);

    FrameNum = options.frames;
    int SetTxUserNum[10] = {64,64,96,96,96,96,64,32,16,16};//bingfa
    //int SetTxUserNum[10] = {1,1,1,1,1,1,1,1,1,1};//lingmindu
    //int SetTxUserNum[10] = {2,2,2,2,2,2,2,2,2,2};//lingmindu
    //int SetTxUserNum[10] = {4,4,4,4,4,4,4,4,4,4};//lingmindu
    int SaveBFSelectIndex[16][128];
    for(int t1 = 0; t1 < FrameNum; t1++){

        char filenameNew[4096];
        options.case_prefix(filenameNew, true, "Cmodel_DataIn");
        sprintf(num,"%d",case_num);
        strcat(filenameNew, num);
        strcat(filenameNew, "/");
        strcat(filenameNew, "UD_InData");
        sprintf(num,"%d",t1);
        strcat(filenameNew, num);
        strcat(filenameNew, ".txt");

        ifstream fw1;
        fw1.open(filenameNew);
        if (!fw1) { std::cerr << "Cannot open input: " << filenameNew << '\n'; return 3; }

        m_BF.configure(cfg1);
        m_BF.init(folderName_IS);

        m_BFSelect.configure(cfg2);
        m_BFSelect.init(folderName_IS);

        m_ud.m_slide_num = 0;
        TmpDemodLen = 0;

        for(cnt = 0; cnt < fft_len * WindowNum; cnt++) {
            fw1 >> in_I;            in[0][cnt].I = in_I;            fw1 >> in_Q;            in[0][cnt].Q = in_Q;
            fw1 >> in_I;            in[1][cnt].I = in_I;            fw1 >> in_Q;            in[1][cnt].Q = in_Q;
            fw1 >> in_I;            in[2][cnt].I = in_I;            fw1 >> in_Q;            in[2][cnt].Q = in_Q;
            fw1 >> in_I;            in[3][cnt].I = in_I;            fw1 >> in_Q;            in[3][cnt].Q = in_Q;
            fw1 >> in_I;            in[4][cnt].I = in_I;            fw1 >> in_Q;            in[4][cnt].Q = in_Q;
            fw1 >> in_I;            in[5][cnt].I = in_I;            fw1 >> in_Q;            in[5][cnt].Q = in_Q;
            fw1 >> in_I;            in[6][cnt].I = in_I;            fw1 >> in_Q;            in[6][cnt].Q = in_Q;
            fw1 >> in_I;            in[7][cnt].I = in_I;            fw1 >> in_Q;            in[7][cnt].Q = in_Q;
        }
        if (!fw1) { std::cerr << "Truncated input: " << filenameNew << '\n'; return 3; }
        fw1.close();

        for (int ck0 = 0; ck0 < fft_len * WindowNum; ck0++)
        {
            m_ud.m_in_1 = in[0][ck0];            m_ud.m_in_2 = in[1][ck0];            m_ud.m_in_3 = in[2][ck0];            m_ud.m_in_4 = in[3][ck0];
            m_ud.m_in_5 = in[4][ck0];            m_ud.m_in_6 = in[5][ck0];            m_ud.m_in_7 = in[6][ck0];            m_ud.m_in_8 = in[7][ck0];

            m_ud.go(t1,m_APreambleFft3_Last,m_APreambleFftESum3_Last,ANoiseLast);

            if(m_ud.m_slide_num>1 && m_ud.m_BFStartFlag==1)
            {
                //LOG2DEV << m_ud.m_slide_num << endl;
                if(m_ud.m_slide_num==5){
                    m_ud.m_slide_num = 5;
                }
                for (int ck1 = 0; ck1 < m_ud.m_fft_len; ck1++)
                {
                    m_BF.m_in[0][ck1] = m_ud.m_out[0][ck1];                    m_BF.m_in[1][ck1] = m_ud.m_out[1][ck1];
                    m_BF.m_in[2][ck1] = m_ud.m_out[2][ck1];                    m_BF.m_in[3][ck1] = m_ud.m_out[3][ck1];
                    m_BF.m_in[4][ck1] = m_ud.m_out[4][ck1];                    m_BF.m_in[5][ck1] = m_ud.m_out[5][ck1];
                    m_BF.m_in[6][ck1] = m_ud.m_out[6][ck1];                    m_BF.m_in[7][ck1] = m_ud.m_out[7][ck1];
                }
                mem_copy(m_BF.m_HC,m_ud.m_HC,m_BF.m_filterLen);
                for(int it=0;it<m_ud.m_GWRxAllUser;it++)
                {
                    if(it==7 && t1==1){
                        it = 7;
                    }
                    //LOG2DEV << it << endl;
                    for(int ck1 = 0; ck1 < m_ud.m_ant_num;ck1++)
                    {
                        m_BF.m_AH[ck1] = m_ud.m_AHNorm_o[ck1][it];
                        m_BFSelect.m_AHIn[ck1] = m_ud.m_ah_o[ck1][it];
                    }
                    m_BF.go(m_ud.m_freq_offset_o[it],m_ud.m_GWRxSampleRate);

                    mem_copy(&m_BFSelect.m_DataIn[0][0], &m_BF.m_SigRxOut[0][0], (m_ud.m_ant_num * m_BF.m_ifftLen));
                    m_BFSelect.go();
                    for(int ck1 = 0; ck1 < m_BF.m_ifftLen; ck1++){
                        DemoBuffer[it][ck1+TmpDemodLen] = m_BFSelect.m_out[ck1];
                    }
                    SaveBFSelectIndex[m_ud.m_slide_num-2][it] = m_BFSelect.m_bf_o_index;
                }
                TmpDemodLen = TmpDemodLen + m_BF.m_ifftLen;
                m_ud.m_BFStartFlag = 0;
            }
        }

        SumRxUserNum = 0;

        for(int it = 0; it < m_ud.m_GWRxAllUser; it++){
            //LOG2DEV << it << endl;
            m_sync_FX.configure(cfg3);
            m_sync_FX.init(folderName_DemoSync);
            phy2_1_rx_dmod.config(cfg4);
            phy2_1_rx_dmod.init(folderName_Demo);

            data_cnt = 0;
            m_rx_demod_en = 0;
            seg_cnt = 0;
            SumCRCResult = 0;
            for(int ck1 = 0; ck1 < (DemodWinDowNum * IFFTLen + 512); ck1++){
                m_data_in = DemoBuffer[it][ck1];
                m_sync_FX.go(m_data_in);

                if (m_sync_FX.m_out_valid == 1)
                {
                    //WLOG2DEV<<cnt++<<endl;
                    Demo_in[data_cnt++] = m_sync_FX.m_out;
                    //LOG2DEV << ck1 << endl;
                    if (data_cnt == (int)m_sync_FX.m_sync_out_len)
                    {
                        //LOG2DEV << ck1 << endl;
                        data_cnt = 0;
                        m_rx_demod_en = 1;
                    }
                    if(m_rx_demod_en)
                    {
                        phy2_1_rx_dmod.m_avg_power = m_sync_FX.m_avg_power;
                        //LOG2DEV<<m_demod.m_avg_power<<endl;
                        mem_copy(correctionTiming, m_sync_FX.m_correctionTiming, 4);
                        phy2_1_rx_dmod.go(Demo_in, correctionTiming);
                        SumCRCResult = SumCRCResult + phy2_1_rx_dmod.m_crc_error;
                        //cout << phy2_1_rx_dmod.m_crc_error <<endl;
                        m_rx_demod_en = 0;
                        seg_cnt = seg_cnt + 1;
                        if(seg_cnt == cfg3.seg_num)
                            break;
                    }
                }
            }
            #ifdef Print_Log
            LOG2DEV <<"CRC:"<<phy2_1_rx_dmod.m_crc_error<<"  Index:"<<SaveBFSelectIndex[0][it]<<" "<<SaveBFSelectIndex[1][it]<<"  snr_fsk:"<<phy2_1_rx_dmod.snr_est_fsk<<"  snr_dqpsk:"<<phy2_1_rx_dmod.snr_est_dqpsk<<" Freq:"<<m_ud.m_freq_offset_o[it]<<endl;
            #endif // Print_Log
            if(seg_cnt == cfg3.seg_num && SumCRCResult==0){
                LOG2DEV << "RX_DECODE frame=" << t1 << " candidate=" << it
                        << " segments=" << seg_cnt << " crc_errors=" << SumCRCResult
                        << " bytes=" << phy2_1_rx_dmod.RxByteLen << std::endl;
                if(SumRxUserNum==0){
                    mem_copy(&GWRxDemodDataBuffer[SumRxUserNum][0],phy2_1_rx_dmod.RxByte_o,phy2_1_rx_dmod.RxByteLen);
                    for (int ant = 0; ant < ant_num; ++ant)
                        GWRX2TXPare.AH[ant][SumRxUserNum] = m_ud.m_ah_o[ant][it];
                    mem_copy(&GWRX2TXPare.ANoise[0],m_ud.m_ant_anoise,m_ud.m_ant_num);
                    GWRX2TXPare.GWRxPilotPower[SumRxUserNum] = m_ud.m_pilot_power_o[it];
                    GWRX2TXPare.TxFreq[SumRxUserNum] = m_ud.m_freq_offset_o[it];
                    //LOG2DEV <<"UserID:"<<GWRxDemodDataBuffer[SumRxUserNum][0]<<GWRxDemodDataBuffer[SumRxUserNum][1]<<endl;
                    SumRxUserNum++;
                    #ifdef Print_Log
                    //LOG2DEV <<"CRC:"<<phy2_1_rx_dmod.m_crc_error<<"  Index:"<<SaveBFSelectIndex[0][it]<<" "<<SaveBFSelectIndex[1][it]<<"  snr_fsk:"<<phy2_1_rx_dmod.snr_est_fsk<<"  snr_dqpsk:"<<phy2_1_rx_dmod.snr_est_dqpsk<<endl;
                    #endif // Print_Log
                }else{
                    int DifDataNum = 0;
                    for(int ck1 = 0; ck1 < SumRxUserNum; ck1++){
                        if(DataArraysCompare(&GWRxDemodDataBuffer[ck1][0],phy2_1_rx_dmod.RxByte_o,phy2_1_rx_dmod.RxByteLen)){
                            if(m_ud.m_pilot_power_o[it]>GWRX2TXPare.GWRxPilotPower[ck1]){
                                for (int ant = 0; ant < ant_num; ++ant)
                                    GWRX2TXPare.AH[ant][ck1] = m_ud.m_ah_o[ant][it];
                                GWRX2TXPare.GWRxPilotPower[ck1] = m_ud.m_pilot_power_o[it];
                                GWRX2TXPare.TxFreq[ck1] = m_ud.m_freq_offset_o[it];
                            }
                        }else{
                            DifDataNum++;
                        }
                    }
                    if(DifDataNum==SumRxUserNum){
                        mem_copy(&GWRxDemodDataBuffer[SumRxUserNum][0],phy2_1_rx_dmod.RxByte_o,phy2_1_rx_dmod.RxByteLen);
                        for (int ant = 0; ant < ant_num; ++ant)
                            GWRX2TXPare.AH[ant][SumRxUserNum] = m_ud.m_ah_o[ant][it];
                        GWRX2TXPare.GWRxPilotPower[SumRxUserNum] = m_ud.m_pilot_power_o[it];
                        GWRX2TXPare.TxFreq[SumRxUserNum] = m_ud.m_freq_offset_o[it];
                        //LOG2DEV <<"UserID:"<<GWRxDemodDataBuffer[SumRxUserNum][0]<<GWRxDemodDataBuffer[SumRxUserNum][1]<<endl;
                        SumRxUserNum++;
                        #ifdef Print_Log
                        //LOG2DEV <<"CRC:"<<phy2_1_rx_dmod.m_crc_error<<"  Index:"<<SaveBFSelectIndex[0][it]<<" "<<SaveBFSelectIndex[1][it]<<"  snr_fsk:"<<phy2_1_rx_dmod.snr_est_fsk<<"  snr_dqpsk:"<<phy2_1_rx_dmod.snr_est_dqpsk<<endl;
                        #endif // Print_Log
                    }
                }
            }
            //LOG2DEV << data_cnt << endl;
        }
        LOG2DEV << "FreamIndex:" << t1 << " RxUserNum:"<< SumRxUserNum << endl;
        fwlog << "FreamIndex:" << t1 << " RxUserNum:"<< SumRxUserNum << endl;
        TotalRxUserNum = TotalRxUserNum + SumRxUserNum;
        //m_BF.file_o_in_Tmp1.close();
        //m_BF.file_SVDIn_R.close();
        //m_BF.file_SVDOut_G.close();
        //m_BF.file_SVDOut_V.close();
        mem_copy(&m_APreambleFft3_Last[0][0],&m_ud.m_APreambleFft2[0][0],m_ud.m_ant_num*m_ud.m_fft_len);
        mem_copy(m_APreambleFftESum3_Last,m_ud.m_APreambleFftESum2,m_ud.m_fft_len);
        mem_copy(ANoiseLast,m_ud.m_ant_anoise,m_ud.m_ant_num);

        //SumRxUserNum = 0;
        GWRX2TXPare.TxUserNum = SumRxUserNum;
        std::string rxPath = std::string(folderName_Demo) + "rx_frame" + std::to_string(t1) + ".txt";
        std::ofstream rxOutput(rxPath.c_str());
        if (!rxOutput) { std::cerr << "Cannot open RX results\n"; return 3; }
        rxOutput << std::setprecision(17);
        for (int user = 0; user < SumRxUserNum; ++user) {
            rxOutput << user << ' ' << phy2_1_rx_dmod.RxByteLen << ' '
                     << GWRX2TXPare.TxFreq[user] << ' ' << GWRX2TXPare.GWRxPilotPower[user];
            for (int ant = 0; ant < ant_num; ++ant)
                rxOutput << ' ' << GWRX2TXPare.AH[ant][user].I << ' ' << GWRX2TXPare.AH[ant][user].Q;
            for (int byte = 0; byte < phy2_1_rx_dmod.RxByteLen; ++byte)
                rxOutput << ' ' << GWRxDemodDataBuffer[user][byte];
            rxOutput << '\n';
        }
        rxOutput.close();
        if (!rxOutput) return 3;
        MatlabTxInputs matlabTx;
        const bool importedTx = !options.tx_reference_file.empty();
        if (importedTx) {
            if (!matlabTx.load(options.tx_reference_file,TxMode)) {
                std::cerr << "Invalid MATLAB TX reference file\n"; return 3;
            }
            GWRX2TXPare.TxUserNum = static_cast<int>(matlabTx.users.size());
            for (int ant=0;ant<8;++ant) {
                GWRX2TXPare.ANoise[ant]=matlabTx.noise[ant];
                CalibrateFactor[2*ant]=matlabTx.calibration[2*ant];
                CalibrateFactor[2*ant+1]=matlabTx.calibration[2*ant+1];
            }
            for (int user=0;user<GWRX2TXPare.TxUserNum;++user) {
                GWRX2TXPare.TxFreq[user]=matlabTx.users[user].frequency;
                GWRX2TXPare.GWRxPilotPower[user]=matlabTx.users[user].pilot_power;
                for (int ant=0;ant<8;++ant) {
                    GWRX2TXPare.AH[ant][user].I=matlabTx.users[user].ah[2*ant];
                    GWRX2TXPare.AH[ant][user].Q=matlabTx.users[user].ah[2*ant+1];
                }
            }
            LOG2DEV << "TX_INPUT_SOURCE=MATLAB users=" << GWRX2TXPare.TxUserNum << std::endl;
        }
        if(GWRX2TXPare.TxUserNum>0){
            TxUserNum = GWRX2TXPare.TxUserNum;

            options.case_prefix(folderName, false, "GWTXModule");
            sprintf(num, "%d", case_num);
            strcat(folderName, num);
            strcat(folderName, "/");

            m_encode_cfg.CR = CodeRate;
            m_encode_cfg.crc_en = CRC_En;
            m_encode_cfg.len_tx = SymNumData*CodeRate/2;
            m_encode_cfg.Blocknum = TxBlockNum;

            m_mod_cfg.BlockNum = TxBlockNum;
            m_mod_cfg.DSSSNum = DSSSNUM;
            m_mod_cfg.LenSymPhase = 16;
            m_mod_cfg.len_infobit = m_encode_cfg.len_tx * 8;
            //m_mod_cfg.len_infobit = 224;
            m_mod_cfg.reg_agc_len =  AGCLen / (1e6 / DOUBLE_POW2[64 + UpSampleNum + 1]);
            m_mod_cfg.reg_agc_num = 0;
            m_mod_cfg.SymNumData = SymNumData;
            m_mod_cfg.TransRatio = TransRatio;

            m_us_cfg.BlockNum = m_mod_cfg.BlockNum;
            m_us_cfg.data_times = UpSampleNum;
            m_us_cfg.tx_data_len_cfg = (m_encode_cfg.len_tx * 8 / 4 * 1.0 / m_encode_cfg.CR  + 12) * m_mod_cfg.LenSymPhase *  m_mod_cfg.DSSSNum + m_mod_cfg.reg_agc_len;
            m_us_cfg.tx_samplingrate = SampleRate_reg[(int)GWRxSampleRate];

            m_beamforming_cfg.user_num_i = TxUserNum;
            m_beamforming_cfg.reg_total_pow = TotalTxPower;
            m_beamforming_cfg.reg_pow_diff1 = PowerDif1;
            m_beamforming_cfg.reg_pow_diff2 = PowerDif2;
            m_beamforming_cfg.reg_pow_diff3 = PowerDif3;
            m_beamforming_cfg.GWTxPowerFlag = TxPowerFlag;
            m_beamforming_cfg.RecipracalCaliFlag = RecipracalCaliFlag;
            m_beamforming_cfg.reg_mmse = MMSE;
            m_tx_beamforming.m_umode_tx = TxMode;
            m_tx_beamforming.configure(m_beamforming_cfg);
            m_tx_beamforming.init(folderName);
            for (int ant = 0; ant < ant_num; ++ant) {
                m_tx_beamforming.m_anoise_sqrt_i[ant] = sqrt(GWRX2TXPare.ANoise[ant]);
                m_tx_beamforming.m_cali_factor[ant].I = CalibrateFactor[2 * ant];
                m_tx_beamforming.m_cali_factor[ant].Q = CalibrateFactor[2 * ant + 1];
            }
            for(int ck0 = 0; ck0 < m_beamforming_cfg.user_num_i;ck0++){
                for(int ck1 = 0; ck1 < m_ud.m_ant_num;ck1++){
                    m_tx_beamforming.m_ah_i[ck1][ck0].I = GWRX2TXPare.AH[ck1][ck0].I;
                    m_tx_beamforming.m_ah_i[ck1][ck0].Q = GWRX2TXPare.AH[ck1][ck0].Q;
                }
                m_tx_beamforming.m_freq_off[ck0] = GWRX2TXPare.TxFreq[ck0];
                m_tx_beamforming.m_pilot_power_i[ck0] = GWRX2TXPare.GWRxPilotPower[ck0];
                m_tx_beamforming.m_txdata_power[ck0] = importedTx ? matlabTx.users[ck0].tx_power : 50;
            }

            if (importedTx) {
                std::ofstream used((std::string(folderName)+"tx_inputs_used.txt").c_str());
                used << std::setprecision(17) << "MATLAB_TX_V1 " << TxUserNum << " 8\n";
                for(int ant=0;ant<8;++ant) used << GWRX2TXPare.ANoise[ant] << ' ';
                used << '\n';
                for(int k=0;k<16;++k) used << CalibrateFactor[k] << ' ';
                used << '\n';
                for(int user=0;user<TxUserNum;++user) {
                    used << matlabTx.users[user].id << ' ' << TxMode << ' '
                         << m_tx_beamforming.m_freq_off[user] << ' ' << m_tx_beamforming.m_pilot_power_i[user]
                         << ' ' << m_tx_beamforming.m_txdata_power[user];
                    for(int ant=0;ant<8;++ant) used << ' ' << m_tx_beamforming.m_ah_i[ant][user].I
                                                << ' ' << m_tx_beamforming.m_ah_i[ant][user].Q;
                    used << '\n';
                }
                used.close();
                if (!used) { std::cerr << "Cannot verify imported TX inputs\n"; return 3; }
            }

            m_tx_encode.configure(m_encode_cfg);
            m_tx_encode.init(folderName);
            m_tx_mod.config(m_mod_cfg);
            m_tx_mod.init(folderName);
            m_us_cfg.tx_data_agc_cfg = 0;
            m_tx_us.config(m_us_cfg);
            m_tx_us.init(folderName);

            double          us_in = 0;
            double          m_user_phase[128];
            int             cnt_us_in = 0;
            int             cnt_us_out = 0;
            int             us_output_len = 0;
            int             m_block_start = 0;
            Complex<double> sum_tmp = 0;
            Complex<ModelScalar> m_sum_fxp;
            Complex<ModelScalar> m_mult_fxp;

            int             *in_user_tmp;
            double          *us_in_buff;
            int             **TxIn;
            int             *fsk;
            int             *dqpsk;
            double          *out;
            double          *sigtx;
            Complex<double> *sigtx_IQ;
            Complex<double> **GW_Tx_MutiAntennaSignal;

            in_user_tmp = NULL;
            us_in_buff = NULL;
            TxIn = NULL;
            fsk = NULL;
            dqpsk = NULL;
            out = NULL;
            sigtx = NULL;
            sigtx_IQ = NULL;
            GW_Tx_MutiAntennaSignal = NULL;

            in_user_tmp = safe_new(in_user_tmp, m_encode_cfg.len_tx * 8);
            us_in_buff = safe_new(us_in_buff , m_us_cfg.tx_data_len_cfg);
            TxIn = safe_new(TxIn, 128, 32768);
            if (!options.tx_bits_file.empty()) {
                std::ifstream txBits(options.tx_bits_file.c_str());
                std::vector<int> sourceIds;
                if (!options.tx_user_ids_file.empty()) {
                    std::ifstream ids(options.tx_user_ids_file.c_str());
                    int id;
                    while (ids >> id) {
                        if (id < 1 || id > 128 || std::find(sourceIds.begin(),sourceIds.end(),id) != sourceIds.end()) {
                            std::cerr << "Invalid/duplicate TX user ID\n"; return 3;
                        }
                        sourceIds.push_back(id);
                    }
                    if (!ids.eof() || sourceIds.empty()) { std::cerr << "Missing/invalid TX user IDs\n"; return 3; }
                }
                const int bitsPerUser = m_encode_cfg.len_tx * 8 * TxBlockNum;
                if (bitsPerUser <= 0 || bitsPerUser > 32768) return 3;
                const int sourceCount = sourceIds.empty() ? TxUserNum : static_cast<int>(sourceIds.size());
                std::vector<std::vector<int> > bank(sourceCount,std::vector<int>(bitsPerUser));
                for (int row=0; row<sourceCount; ++row)
                    for (int bit=0; bit<bitsPerUser; ++bit)
                        if (!(txBits >> bank[row][bit]) || (bank[row][bit]!=0 && bank[row][bit]!=1)) {
                            std::cerr << "Missing/invalid TX bits\n"; return 3;
                        }
                for (int user = 0; user < TxUserNum; ++user)
                {
                    int row=user;
                    if (!sourceIds.empty()) {
                        int id=importedTx ? matlabTx.users[user].id :
                            GWRxDemodDataBuffer[user][0]*255+GWRxDemodDataBuffer[user][1];
                        std::vector<int>::iterator found=std::find(sourceIds.begin(),sourceIds.end(),id);
                        if(found==sourceIds.end()) { std::cerr << "No TX payload for RX user " << id << '\n'; return 3; }
                        row=static_cast<int>(found-sourceIds.begin());
                        LOG2DEV << "TX_PAYLOAD_MAP rx_index=" << user << " user_id=" << id << " bank_row=" << row << std::endl;
                    }
                    for(int bit=0; bit<bitsPerUser; ++bit) TxIn[user][bit]=bank[row][bit];
                }
                std::string extra;
                if (txBits >> extra) { std::cerr << "Excess TX bits\n"; return 3; }
            }
            if (importedTx) {
                std::ofstream usedBits((std::string(folderName)+"tx_bits_used.txt").c_str());
                for(int user=0;user<TxUserNum;++user)
                    for(int bit=0;bit<m_encode_cfg.len_tx*8*TxBlockNum;++bit)
                        usedBits << TxIn[user][bit] << '\n';
                usedBits.close();
                if(!usedBits) return 3;
            }
            fsk = safe_new(fsk, 32768);
            dqpsk = safe_new(dqpsk, 32768);
            out = safe_new(out , 327680);
            sigtx = safe_new(sigtx , 327680);
            sigtx_IQ = safe_new(sigtx_IQ, 327680);
            GW_Tx_MutiAntennaSignal = safe_new(GW_Tx_MutiAntennaSignal , 8 , 327680);

            fix_set(m_sum_fxp, 12, -15, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sum_fxp, "m_sum_fxp", 1, 1);
            fix_set(m_mult_fxp, 12, -15, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_mult_fxp, "m_mult_fxp", 1, 1);

            // Without an explicit payload file, preserve the legacy zero test payload.
            m_tx_beamforming.go();
            std::ofstream weights((std::string(folderName) + "weights_frame" + std::to_string(t1) + ".txt").c_str());
            if (!weights) return 3;
            weights << std::setprecision(17);
            for (int user = 0; user < TxUserNum; ++user) {
                for (int ant = 0; ant < 8; ++ant)
                    weights << m_tx_beamforming.m_beamforming_matrix_tx_o[user][ant].I << ' '
                            << m_tx_beamforming.m_beamforming_matrix_tx_o[user][ant].Q << (ant == 7 ? '\n' : ' ');
            }
            weights.close();
            if (!weights) return 3;

            for(int ck1 = 0;ck1 < m_mod_cfg.BlockNum; ck1++) {
                if(ck1 > 0) {
                    m_tx_mod.m_agc_len = 0;
                }

                if(ck1 == m_mod_cfg.BlockNum - 1) {
                    m_tx_mod.m_tone_len = 0;
                } else {
                    m_tx_mod.m_tone_len = 0;
                }

                for(int ck0 = 0 ; ck0 < m_beamforming_cfg.user_num_i;ck0++)
                {
                    for (int ck2 = 0; ck2 < m_encode_cfg.len_tx * 8; ck2++) {
                        in_user_tmp[ck2] = TxIn[ck0][ck2 + ck1 * m_encode_cfg.len_tx * 8];
                    }
                    m_tx_encode.go(in_user_tmp, dqpsk, fsk);
                    m_tx_mod.m_iblock = 0;
                    m_tx_mod.go(fsk,dqpsk,out);
                    mem_copy(&us_in_buff[0], &out[0], m_tx_mod.m_outlen);

                    m_tx_us.m_cfg.tx_data_len_cfg = m_tx_mod.m_outlen;
                    m_tx_us.m_i_in_en = 1;
                    m_tx_us.m_freq_in = round(m_tx_beamforming.m_freq_off[ck0]);
                    m_tx_us.reset(2);
                    if(ck1 == 0) {
                        m_tx_us.m_ddc_o_init = 0;
                        m_tx_us.reset(2);
                    } else {
                        m_tx_us.m_ddc_o_init = m_user_phase[ck0];
                        m_tx_us.reset(3);
                    }

                    while(1)
                    {
                        if(cnt_us_in < m_tx_mod.m_outlen) {

                            us_in = us_in_buff[cnt_us_in];
                            cnt_us_in = cnt_us_in + 1;
                        } else {
                            m_tx_us.m_i_in_en = 0;
                        }

                        m_tx_us.go(us_in);

                        if(m_tx_us.m_out_en == 1 || m_tx_us.m_finished_o == 1)
                        {
                            sigtx[cnt_us_out]= m_tx_us.m_out;
                            sigtx_IQ[cnt_us_out] = m_tx_us.m_out_iq;
                            cnt_us_out = cnt_us_out + 1;
                        }

                        if(m_tx_us.m_finished_o == 1)
                        {
                            m_user_phase[ck0] = m_tx_us.m_ddc_output;
                            cnt_us_in = 0;
                            cnt_us_out = 0;
                            break;
                        }
                    }

                    m_tx_encode.reset();
                    m_tx_mod.reset();
                    if(ck1 == 0)
                        us_output_len = m_tx_us.m_output_len;
                    else
                        us_output_len = m_tx_us.m_output_len - 1;
                    for(int ck2 = 0;ck2 < 8 ;ck2++)
                    {
                        for(int ck3 = 0;ck3 < us_output_len;ck3++)
                        {
                            m_mult_fxp = m_tx_beamforming.m_beamforming_matrix_tx_o[ck0][ck2] * sigtx_IQ[ck3] / 1024.0 / 1024.0 / 8.0;
                            sum_tmp = GW_Tx_MutiAntennaSignal[ck2][m_block_start + ck3] + m_mult_fxp.todouble();
                            m_sum_fxp = sum_tmp;
                            GW_Tx_MutiAntennaSignal[ck2][m_block_start + ck3] = m_sum_fxp;
                        }
                    }
                }
                m_block_start = m_block_start + us_output_len;
                //LOG2DEV << m_block_start << endl;
            }
            std::string txPath = std::string(folderName) + "tx_frame" + std::to_string(t1) + ".txt";
            std::ofstream txOutput(txPath.c_str());
            if (!txOutput) { std::cerr << "Cannot open TX output\n"; return 3; }
            double energy = 0;
            txOutput << std::setprecision(17);
            for (int sample = 0; sample < m_block_start; ++sample) {
                for (int ant = 0; ant < 8; ++ant) {
                    Complex<double> value = GW_Tx_MutiAntennaSignal[ant][sample];
                    if (!std::isfinite(value.I) || !std::isfinite(value.Q)) return 4;
                    energy += value.power();
                    txOutput << value.I << ' ' << value.Q << (ant == 7 ? '\n' : ' ');
                }
            }
            txOutput.close();
            if (!txOutput || !std::isfinite(energy) || energy <= 0 || m_block_start <= 0) return 4;
            LOG2DEV << "TX_FRAME frame=" << t1 << " users=" << TxUserNum
                    << " samples=" << m_block_start << " energy=" << energy << std::endl;
            delete[] in_user_tmp;
            delete[] us_in_buff;
            free_n(TxIn);
            delete[] fsk;
            delete[] dqpsk;
            delete[] out;
            delete[] sigtx;
            delete[] sigtx_IQ;
            free_n(GW_Tx_MutiAntennaSignal);
        }
    }

    double LostRate;
    LostRate = 1 - TotalRxUserNum/(FrameNum * SetTxUserNum[GWAllRxMode-1]);
    LOG2DEV << LostRate << endl;
    #ifdef Fix
    fwlog << "[Fix_Sim]Case:" << case_num << " TotalRxNum:" << TotalRxUserNum <<" SimNum:" << FrameNum << "  LostRate:" << fixed << setprecision(10) << LostRate << "\t"<<endl;
    LOG2DEV << "[Fix_Sim]Case:" << case_num << " TotalRxNum:" << TotalRxUserNum <<" SimNum:" << FrameNum << "  LostRate:" << fixed << setprecision(10) << LostRate << "\t"<<endl;
    #else
    fwlog << "[Float_Sim]Case:" << case_num << "TotalRxNum" << TotalRxUserNum <<" SimNum:" << FrameNum << "  LostRate:" << fixed << setprecision(10) << LostRate << "\t"<<endl;
    LOG2DEV << "[Float_Sim]Case:" << case_num << "TotalRxNum" << TotalRxUserNum <<" SimNum:" << FrameNum << "  LostRate:" << fixed << setprecision(10) << LostRate << "\t"<<endl;
    #endif // Fix

    fwlog.close();
    free_n(in);
    free_n(DemoBuffer);
    delete[] Demo_in;
    free_n(m_APreambleFft3_Last);
    delete[] m_APreambleFftESum3_Last;
    delete[] ANoiseLast;
    free_n(GWRxDemodDataBuffer);
    std::cout<< "RxFinished"<<std::endl;

   	return 0;
}
