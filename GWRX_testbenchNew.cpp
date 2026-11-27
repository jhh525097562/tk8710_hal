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

#include "../Cls_ud.h"
#include "../Cls_GWRxBFModule.h"
#include "../Cls_BFSelect.h"
#include "../Cls_rx_data_phy2_1.h"
#include "../Cls_Rx_Sync_2_1.h"
#include "../Cls_FIR.h"
#include "../Cls_RX_DS_FLT.h"
#include "../Cls_CIC.h"
#include <sys/stat.h>
#include <string.h>
#include <sstream>
#include <iostream>

/******************Tx********************/
#include "../Cls_tx_encode.h"
#include "../Cls_tx_beamforming.h"
#include "../Cls_tx_mod.h"
#include "../Cls_tx_us_2_1.h"
#include "../Cls_TX_US.h"
#include "../Cls_sdm_tx.h"

#define Fix
#define TXFix
#define SIM_1MHZ

//#define RTL_BITMATCH_USE
//#define Print_Log

int shift_cal(Complex<double> **in, int ant_num, int len) {
    double max_val = 0;
    for (int ck0 = 0; ck0 < ant_num; ck0++) {
        for (int ck1 = 0; ck1 < len; ck1++){
            if (max_val < fabs(in[ck0][ck1].I))
                max_val = fabs(in[ck0][ck1].I);
            if (max_val < fabs(in[ck0][ck1].Q))
                max_val = fabs(in[ck0][ck1].Q);
        }
    }
    return (ceil(log2(max_val)));
}

#ifdef RTL_BITMATCH_USE
int dump_cnt = 0;
int rtl_dump( int antenne, int user_number, int data_times, int us_len_out, int us_out_cnt, std::ofstream& file, Complex<fxp> *in,fxp fxp_temp);
#endif

typedef struct GWRx2TXCFG
{
    //Read Write
    int                 TxUserNum;
    Complex<double>     AH[8][128];
    double              TxFreq[128];
    double              GWRxPilotPower[128];
    double              ANoise[8];
    Complex<double>     CalibrateFactor[8];
    double              GWRXSNR[128];
    int                 Sbit[20][128];
    int                 Fbit[20][128];
}GWRx2TXCFG;


bool DataArraysCompare(int *arr1,int *arr2,int Size){
    for(int i = 0; i < Size; i++){
        if(arr1[i] != arr2[i]){
            return false;
        }
    }
    return true;
}

double AntennaMaxVal(Complex<double> in,double Max){
    double tmp = fabs(in.I);

    double TmpMax;
    TmpMax = Max;
    if(tmp > Max)
        TmpMax = tmp;

    tmp = fabs(in.Q);

    if(tmp > Max)
        TmpMax = tmp;

    return TmpMax;
}

Complex<double> fixModule(Complex<double> in,double RSSI,double MaxVal){

    Complex<double> out;
    double RSSIReference = -100;
    int BitGain = 12;
    int MaxBitLen = 14;
    int tmp0;
    int tmp1;
    int k;
    k = BitGain + floor( (RSSI - RSSIReference)/6 );
    if(k<=6){
        //k = 6;
        k = 8;
    }
    k = 14;
    tmp0 =  DOUBLE_POW2[64 + k - 1] - 1;
    tmp1 = DOUBLE_POW2[64 + MaxBitLen - 1];

    out = in/MaxVal;
    out = out * tmp0;

    if(k>14){
        if(out.I > (tmp1 - 1)){
            out.I = tmp1 - 1;
        }
        if(out.Q > (tmp1 - 1)){
            out.Q = tmp1 - 1;
        }
        if(out.I < (-tmp1 + 1)){
            out.I = -tmp1 + 1;
        }
        if(out.Q < (-tmp1 + 1)){
            out.Q = -tmp1 + 1;
        }
    }
    out = out / tmp1;
    //LOG2DEV << k << out.I << " " << out.Q << MaxVal << endl;
    return out;
}

int main(int argc, char *argv[])
{
    int						case_num;
    int                     Iter;
    int                     ant_num;
    int                     fft_len;
    int                    	statMd = 0;
    FFT_LEN                 reg_fft_len;
    UD_CFG                  cfg;
    GWRXBFMODULE_CFG        cfg1;
    BFSelect_CFG            cfg2;
    RX_PHY2_1_SYNC_CFG      cfg3;
    RX_DATA_2_1_CFG         cfg4;
    RX_DS_FLT_Cfg           cfg5;
    CIC_CLASS_CFG			cfg6;
    struct 					stat statExist;
	char 					num[10] = {0};
    double                  in_I;
    double                  in_Q;
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
    int                     SetFreqRx[128];
    int                     HCE;
    int                     FilterLen;
    int                     IFFTLen;
    int                     BFMode;
    GWRx2TXCFG              GWRX2TXPare;

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
	int                     CIC_RX_RR = 0;
	int                     HalfBandNum = 0;
    Complex<double>		    m_data_in = 0.0;
    double                  GWRX_RSSI = -100.0;
    double                  RXDigAGCTime = 1000;
    double                  RXDigGain = 1.25;
    //int                     FrameNum;
    Complex<double>         **in;

    #ifdef Fix
    Cls_ud<fxp>             m_ud;
    Cls_GWRxBFModule<fxp>   m_BF;
    Cls_BFSelect<fxp>       m_BFSelect;
    Cls_rx_data_phy_2_1<fxp> phy2_1_rx_dmod;
    Cls_Rx_Sync_2_1<fxp>	m_sync_FX;
    Cls_RX_DS_FLT<double>   halfbandfilter0;
    Cls_RX_DS_FLT<double>   halfbandfilter1;
    Cls_RX_DS_FLT<double>   halfbandfilter2;
    Cls_RX_DS_FLT<double>   halfbandfilter3;
    Cls_RX_DS_FLT<double>   halfbandfilter4;
    Cls_RX_DS_FLT<double>   halfbandfilter5;
    Cls_RX_DS_FLT<double>   halfbandfilter6;
    Cls_RX_DS_FLT<double>   halfbandfilter7;
    Cls_CIC<fxp>     		m_CIC0;
    Cls_CIC<fxp>		    m_CIC1;
    Cls_CIC<fxp>		    m_CIC2;
    Cls_CIC<fxp>		    m_CIC3;
    Cls_CIC<fxp>		    m_CIC4;
    Cls_CIC<fxp>		    m_CIC5;
    Cls_CIC<fxp>		    m_CIC6;
    Cls_CIC<fxp>		    m_CIC7;
    #else
    Cls_ud<double>          m_ud;
    Cls_GWRxBFModule<double>   m_BF;
    Cls_BFSelect<double>       m_BFSelect;
    Cls_rx_data_phy_2_1<double> phy2_1_rx_dmod;
    Cls_Rx_Sync_2_1<double>	m_sync_FX;
    Cls_RX_DS_FLT<double>   halfbandfilter0;
    Cls_RX_DS_FLT<double>   halfbandfilter1;
    Cls_RX_DS_FLT<double>   halfbandfilter2;
    Cls_RX_DS_FLT<double>   halfbandfilter3;
    Cls_RX_DS_FLT<double>   halfbandfilter4;
    Cls_RX_DS_FLT<double>   halfbandfilter5;
    Cls_RX_DS_FLT<double>   halfbandfilter6;
    Cls_RX_DS_FLT<double>   halfbandfilter7;
    Cls_CIC<double>     	m_CIC0;
    Cls_CIC<double>		    m_CIC1;
    Cls_CIC<double>		    m_CIC2;
    Cls_CIC<double>		    m_CIC3;
    Cls_CIC<double>		    m_CIC4;
    Cls_CIC<double>		    m_CIC5;
    Cls_CIC<double>		    m_CIC6;
    Cls_CIC<double>		    m_CIC7;
    #endif

    Complex<double>         **DemoBuffer;
    int                     **GWRxDemodDataBuffer;
    int                     *TmpDemodDataBuffer;
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
    TmpDemodDataBuffer          = NULL;

        /****************************Tx****************************/
    #ifdef TXFix
    Cls_tx_encode<fxp>      m_tx_encode;
    Cls_tx_beamforming<fxp> m_tx_beamforming;
    Cls_tx_mod<fxp>         m_tx_mod;
    Cls_tx_us_2_1<fxp>      m_tx_us;
    Cls_TX_US<double>       m_tx_ushalfbandfilter;
    Cls_sdm_tx<fxp>         m_sdm_tx;//m_sdm_tx.go();
    Cls_CIC<fxp>     		m_tx_CIC;
    #else
    Cls_tx_encode<double>      m_tx_encode;
    Cls_tx_beamforming<double> m_tx_beamforming;
    Cls_tx_mod<double>         m_tx_mod;
    Cls_tx_us_2_1<double>      m_tx_us;
    Cls_TX_US<double>          m_tx_ushalfbandfilter;
    Cls_sdm_tx<double>         m_sdm_tx;//m_sdm_tx.go();
    Cls_TX_US<double>          m_tx_us_h;
    Cls_CIC<double>     	   m_tx_CIC;
    #endif
    TX_ENCODE_CFG              m_encode_cfg;
    TX_MOD_CFG                 m_mod_cfg;
    TX_BEAMFORMING_CFG         m_beamforming_cfg;
    TX_US_2_1_Cfg              m_us_cfg;
    TX_US_2_Cfg                m_txhalfband_cfg;
    CIC_CLASS_CFG			   m_txCIC_cfg;
    int                  TxMode;
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
    double               TmpBWOverlop;
    int                  halfbandUpNum;
    int                  CIC_TX_RR;

    sscanf(argv[44], "%lf", &CalibrateFactor[0]);    sscanf(argv[45], "%lf", &CalibrateFactor[1]);    sscanf(argv[46], "%lf", &CalibrateFactor[2]);    sscanf(argv[47], "%lf", &CalibrateFactor[3]);
    sscanf(argv[48], "%lf", &CalibrateFactor[4]);    sscanf(argv[49], "%lf", &CalibrateFactor[5]);    sscanf(argv[50], "%lf", &CalibrateFactor[6]);    sscanf(argv[51], "%lf", &CalibrateFactor[7]);
    sscanf(argv[52], "%lf", &CalibrateFactor[8]);    sscanf(argv[53], "%lf", &CalibrateFactor[9]);    sscanf(argv[54], "%lf", &CalibrateFactor[10]);    sscanf(argv[55], "%lf", &CalibrateFactor[11]);
    sscanf(argv[56], "%lf", &CalibrateFactor[12]);    sscanf(argv[57], "%lf", &CalibrateFactor[13]);    sscanf(argv[58], "%lf", &CalibrateFactor[14]);    sscanf(argv[59], "%lf", &CalibrateFactor[15]);


    int fft_reg[4] = {1024, 2048, 4096, 8192};
    double SampleRate_reg[6] = {15625.0, 31250.0, 62500.0, 125000.0, 250000.0, 500000.0};

    sscanf(argv[1],  "%d", &case_num);
    sscanf(argv[2],  "%d", &Iter);
    char filenameGWRXPara[4096] = "/home/data/K2301/K2301PHY_CaseList_Data/Cmodel_DataIn/case"; //
    sprintf(num,"%d",case_num);
    strcat(filenameGWRXPara, num);
    strcat(filenameGWRXPara, "/");
    strcat(filenameGWRXPara, "GWRXPara.txt");
    ifstream fw1;
    fw1.open(filenameGWRXPara);

    char filenameGWTXPara[4096] = "/home/data/K2301/K2301PHY_CaseList_Data/Cmodel_DataIn/case"; //
    sprintf(num,"%d",case_num);
    strcat(filenameGWTXPara, num);
    strcat(filenameGWTXPara, "/");
    strcat(filenameGWTXPara, "GWTXPara.txt");
    ifstream fw2;
    fw2.open(filenameGWTXPara);

    char filenameCalibrateFactor[4096] = "/home/data/K2301/K2301PHY_CaseList_Data/Cmodel_DataIn/case"; //
    sprintf(num,"%d",case_num);
    strcat(filenameCalibrateFactor, num);
    strcat(filenameCalibrateFactor, "/");
    strcat(filenameCalibrateFactor, "CalibrateFactor.txt");
    ifstream fw3;
    fw3.open(filenameCalibrateFactor);

    fw1 >> case_num;
    while (!fw1.eof()){
        fw1 >> ant_num;     fw1 >> reg_fft_len_s;     fw1 >> WindowNum;     fw1 >> GWRxSampleRate;     fw1 >> anoise_the1;      fw1 >> anoise_the2;
        fw1 >> UDSwitch;    fw1 >> P1_the;            fw1 >> p1_para1;      fw1 >> p1_para2;           fw1 >> p2_the;           fw1 >> p3_the;
        fw1 >> p6_the;      fw1 >> GWAllRxUserNum;    fw1 >> GWAllRxMode;   fw1 >> SetFreqRxFlag;      fw1 >> SetFreqRxUserNum; fw1 >> HCE;
        fw1 >> FilterLen;   fw1 >> IFFTLen;           fw1 >> BFMode;        fw1 >> TOScope;            fw1 >> Fs;               fw1 >> len_freq;
        fw1 >> freq_start;  fw1 >> freq_inter;        fw1 >> dsss_num;      fw1 >> freq_drift_len;     fw1 >> freq_drift_s;     fw1 >> freq_drift_i;
        fw1 >> seg_num;     fw1 >> Sym_Num_Data;      fw1 >> DqpskaddLen;   fw1 >> FskaddLen;          fw1 >> data_FDComp_Num;  fw1 >> data_FDComp_Delta;
        fw1 >> crc_en;      fw1 >> CIC_RX_RR;         fw1 >> HalfBandNum;   fw1 >> GWRX_RSSI;          fw1 >> RXDigAGCTime;     fw1 >> RXDigGain;
    }

    while (!fw2.eof()){
        fw2 >> TxMode;      fw2 >> TxPowerFlag;       fw2 >> RecipracalCaliFlag;   fw2 >> MMSE;        fw2 >> TotalTxPower;     fw2 >> PowerDif1;
        fw2 >> PowerDif2;   fw2 >> PowerDif3;         fw2 >> AGCLen;               fw2 >> ToneLen;     fw2 >> CodeRate;         fw2 >> CRC_En;
        fw2 >> DSSSNUM;     fw2 >> TransRatio;        fw2 >> TxBlockNum;           fw2 >> Fbit[0];     fw2 >> Fbit[1];          fw2 >> Fbit[2];
        fw2 >> Fbit[3];     fw2 >> Fbit[4];           fw2 >> Fbit[5];              fw2 >> Fbit[6];     fw2 >> Fbit[7];          fw2 >> Sbit[0];
        fw2 >> Sbit[1];     fw2 >> Sbit[2];           fw2 >> Sbit[3];              fw2 >> Sbit[4];     fw2 >> Sbit[5];          fw2 >> Sbit[6];
        fw2 >> Sbit[7];     fw2 >> SymNumData;        fw2 >> UpSampleNum;          fw2 >> TmpBWOverlop;fw2 >> halfbandUpNum;    fw2 >> CIC_TX_RR;
    }

    while (!fw3.eof()){
        fw3 >> CalibrateFactor[0];      fw3 >> CalibrateFactor[1];       fw3 >> CalibrateFactor[2];      fw3 >> CalibrateFactor[3];
        fw3 >> CalibrateFactor[4];      fw3 >> CalibrateFactor[5];       fw3 >> CalibrateFactor[6];      fw3 >> CalibrateFactor[7];
        fw3 >> CalibrateFactor[8];      fw3 >> CalibrateFactor[9];       fw3 >> CalibrateFactor[10];     fw3 >> CalibrateFactor[11];
        fw3 >> CalibrateFactor[12];     fw3 >> CalibrateFactor[13];      fw3 >> CalibrateFactor[14];     fw3 >> CalibrateFactor[15];
    }

    //LOG2DEV << "CaseNum:" << case_num << " Mode:" << GWAllRxMode <<endl;
    if(SetFreqRxFlag==1){
        char filenameFreq[4096] = "/home/data/K2301/K2301PHY_CaseList_Data/Cmodel_DataIn/case"; //
        sprintf(num,"%d",case_num);
        strcat(filenameFreq, num);
        strcat(filenameFreq, "/");
        strcat(filenameFreq, "UD_SetFreq.txt");
        ifstream fw0;
        fw0.open(filenameFreq);
        for(cnt=0;cnt<128;cnt++){
            fw0 >> in_I;
            SetFreqRx[cnt] = in_I;
        }
    }

    char folderName_Log[300] = { "/home/data/K2301/K2301PHY_CaseList_Data/LostRate/case" };
    sprintf(num,"%d",case_num);
    strcat(folderName_Log, num);
    strcat(folderName_Log, "/");
    strcat(folderName_Log, "log.txt");
    ofstream fwlog;
    //fwlog.open(folderName_Log);
    fwlog.open(folderName_Log,std::ios::app);

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
    char folderName_UD[300] = { "/home/data/K2301/K2301PHY_CaseList_Data/UDModule_Data/case" };
    sprintf(num,"%d",case_num);
    strcat(folderName_UD, num);
    strcat(folderName_UD, "/");
    if (stat(folderName_UD, &statExist) != 0) {
            statMd = mkdir(folderName_UD,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    m_ud.init(folderName_UD);

    char folderName_IS[300] = { "/home/data/K2301/K2301PHY_CaseList_Data/BFModule_Data/case" };
    sprintf(num,"%d",case_num);
    strcat(folderName_IS, num);
    strcat(folderName_IS, "/");
    if (stat(folderName_IS, &statExist) != 0) {
            statMd = mkdir(folderName_IS,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }

    char folderName_DemoSync[300] = { "/home/data/K2301/K2301PHY_CaseList_Data/K2201SyncModule_Data/case" };
    sprintf(num,"%d",case_num);
    strcat(folderName_DemoSync, num);
    strcat(folderName_DemoSync, "/");
    if (stat(folderName_DemoSync, &statExist) != 0) {
            statMd = mkdir(folderName_DemoSync,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }

    char folderName_Demo[300] = { "/home/data/K2301/K2301PHY_CaseList_Data/K2201DemodModule_Data/case" };
    sprintf(num,"%d",case_num);
    strcat(folderName_Demo, num);
    strcat(folderName_Demo, "/");
    if (stat(folderName_Demo, &statExist) != 0) {
            statMd = mkdir(folderName_Demo,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }

    char folderName_FilterDownSample[300] = { "/home/data/K2301/K2301PHY_CaseList_Data/K2201FilterDownSample_Data/case" };
    sprintf(num,"%d",case_num);
    strcat(folderName_FilterDownSample, num);
    strcat(folderName_FilterDownSample, "/");
    if (stat(folderName_FilterDownSample, &statExist) != 0) {
            statMd = mkdir(folderName_FilterDownSample,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }

    char folderName_GWTXModule[300] = { "/home/data/K2301/K2301PHY_CaseList_Data/GWTXModule/case" };
    sprintf(num,"%d",case_num);
    strcat(folderName_GWTXModule, num);
    strcat(folderName_GWTXModule, "/");
    if (stat(folderName_GWTXModule, &statExist) != 0) {
            statMd = mkdir(folderName_GWTXModule,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
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

    //FilterDownSample Setting
    cfg5.mode = HalfBandNum;

    //int fs3 = 8*cfg.rx_sampling_rate;
    //if(8*cfg.rx_sampling_rate > 1000000){
    //    fs3 = 1000000;
    //}
	//CIC Configuration
	cfg6.RR				    =	CIC_RX_RR;
	cfg6.MM				    =	1;
	cfg6.NN				    =	3;
	cfg6.Fs				    =	32e6;
	cfg6.alpha			    =	0.25;
	cfg6.bypass			    =	0;
	cfg6.agc_gain			=	1;

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
    in = safe_new(in, 8, fft_len * WindowNum * 2 * 2);
    m_APreambleFft3_Last = safe_new(m_APreambleFft3_Last, 8, fft_len);
    m_APreambleFftESum3_Last = safe_new(m_APreambleFftESum3_Last, fft_len);
    ANoiseLast = safe_new(ANoiseLast,8);
    GWRxDemodDataBuffer = safe_new(GWRxDemodDataBuffer,GWAllRxUserNum,1024);
    TmpDemodDataBuffer = safe_new(TmpDemodDataBuffer,1024);

    //FrameNum = 1;
    //int SetTxUserNum[10] = {64,64,96,96,96,96,64,32,16,16};//bingfa
    //int SetTxUserNum[10] = {1,1,1,1,1,1,1,1,1,1};//lingmindu
    //int SetTxUserNum[10] = {2,2,2,2,2,2,2,2,2,2};//lingmindu
    //int SetTxUserNum[10] = {4,4,4,4,4,4,4,4,4,4};//lingmindu
    int SaveBFSelectIndex[80][128];

    ofstream        file_o_in_Tmp1;
    char filenameTmp[300] = {};
    strcpy(filenameTmp, folderName_FilterDownSample); strcat(filenameTmp, "RXFilter.dat"); file_o_in_Tmp1.open(filenameTmp,std::ios::trunc);

    //for(int t1 = 0; t1 < FrameNum; t1++){
#ifdef SIM_1MHZ
        int cnt1 = 0;
        char filenameNew[4096] = "/home/data/K2301/K2301PHY_CaseList_Data/Cmodel_DataIn/case"; //
        sprintf(num,"%d",case_num);
        strcat(filenameNew, num);
        strcat(filenameNew, "/");
        strcat(filenameNew, "UD_InData");
        sprintf(num,"%d",Iter);
        strcat(filenameNew, num);
        strcat(filenameNew, ".txt");

        ifstream fw9;
        fw9.open(filenameNew);

//        m_BF.configure(cfg1);
//        m_BF.init(folderName_IS);

//        m_BFSelect.configure(cfg2);
//        m_BFSelect.init(folderName_IS);

        m_ud.m_slide_num = 0;
        TmpDemodLen = 0;

        for(cnt = 0; cnt < fft_len * WindowNum; cnt++) {
            fw9 >> in_I;            in[0][cnt].I = in_I;            fw9 >> in_Q;            in[0][cnt].Q = in_Q;
            fw9 >> in_I;            in[1][cnt].I = in_I;            fw9 >> in_Q;            in[1][cnt].Q = in_Q;
            fw9 >> in_I;            in[2][cnt].I = in_I;            fw9 >> in_Q;            in[2][cnt].Q = in_Q;
            fw9 >> in_I;            in[3][cnt].I = in_I;            fw9 >> in_Q;            in[3][cnt].Q = in_Q;
            fw9 >> in_I;            in[4][cnt].I = in_I;            fw9 >> in_Q;            in[4][cnt].Q = in_Q;
            fw9 >> in_I;            in[5][cnt].I = in_I;            fw9 >> in_Q;            in[5][cnt].Q = in_Q;
            fw9 >> in_I;            in[6][cnt].I = in_I;            fw9 >> in_Q;            in[6][cnt].Q = in_Q;
            fw9 >> in_I;            in[7][cnt].I = in_I;            fw9 >> in_Q;            in[7][cnt].Q = in_Q;
        }
        fw9.close();
        #else
        char filenameNew[4096] = "/home/data/K2301/K2301PHY_CaseList_Data/Cmodel_DataIn/case"; //
        sprintf(num,"%d",case_num);
        strcat(filenameNew, num);
        strcat(filenameNew, "/");
        //strcat(filenameNew, "FilterDataIn");
        strcat(filenameNew, "CICDataIn");
        sprintf(num,"%d",Iter);
        strcat(filenameNew, num);
        //strcat(filenameNew, ".txt");
        strcat(filenameNew, ".bin");

        m_CIC0.m_tx_rx_flag = RX;       m_CIC1.m_tx_rx_flag = RX;       m_CIC2.m_tx_rx_flag = RX;       m_CIC3.m_tx_rx_flag = RX;
        m_CIC4.m_tx_rx_flag = RX;       m_CIC5.m_tx_rx_flag = RX;       m_CIC6.m_tx_rx_flag = RX;       m_CIC7.m_tx_rx_flag = RX;

        m_CIC0.config(cfg6);     m_CIC1.config(cfg6);     m_CIC2.config(cfg6);     m_CIC3.config(cfg6);
        m_CIC4.config(cfg6);     m_CIC5.config(cfg6);     m_CIC6.config(cfg6);     m_CIC7.config(cfg6);

        m_CIC0.init(folderName_FilterDownSample);     m_CIC1.init(folderName_FilterDownSample);     m_CIC2.init(folderName_FilterDownSample);     m_CIC3.init(folderName_FilterDownSample);
        m_CIC4.init(folderName_FilterDownSample);     m_CIC5.init(folderName_FilterDownSample);     m_CIC6.init(folderName_FilterDownSample);     m_CIC7.init(folderName_FilterDownSample);

        halfbandfilter0.config(cfg5);       halfbandfilter1.config(cfg5);       halfbandfilter2.config(cfg5);       halfbandfilter3.config(cfg5);
        halfbandfilter4.config(cfg5);       halfbandfilter5.config(cfg5);       halfbandfilter6.config(cfg5);       halfbandfilter7.config(cfg5);

        halfbandfilter0.init(folderName_FilterDownSample);      halfbandfilter1.init(folderName_FilterDownSample);      halfbandfilter2.init(folderName_FilterDownSample);      halfbandfilter3.init(folderName_FilterDownSample);
        halfbandfilter4.init(folderName_FilterDownSample);      halfbandfilter5.init(folderName_FilterDownSample);      halfbandfilter6.init(folderName_FilterDownSample);      halfbandfilter7.init(folderName_FilterDownSample);

        ifstream file_CIC_in;
        file_CIC_in.open(filenameNew,std::ios::binary);

        int cnt1 = 0;
        int cnt0 = 0;
        double AntennaMax[8];
        zeros(AntennaMax,8);
        if(!file_CIC_in)
        {
            std::cout<<"Cannot open file"<<std::endl;
            exit(0);
        }
        int8_t in_It;
        while (!file_CIC_in.eof()) {
            cnt0++;
            //file_CIC_in >> in_I;    m_data_in.I = in_I;     file_CIC_in >> in_Q;    m_data_in.Q = in_Q;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.I = in_It;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.Q = in_It;
            m_CIC0.go(m_data_in);
            if(m_CIC0.m_cic_out_en){
                halfbandfilter0.m_in = m_CIC0.m_out;
                halfbandfilter0.go();
                if(halfbandfilter0.m_hf_en==1){
                    in[0][cnt1] = halfbandfilter0.m_hf_o;
                    AntennaMax[0] = AntennaMaxVal(in[0][cnt1],AntennaMax[0]);
                }
                //file_o_in_Tmp1 << fixed << setprecision(26) << m_CIC0.m_out.I << "\t" << endl;
                //file_o_in_Tmp1 << fixed << setprecision(26) << m_CIC0.m_out.Q << "\t" << endl;
            }

            //file_CIC_in >> in_I;    m_data_in.I = in_I;     file_CIC_in >> in_Q;    m_data_in.Q = in_Q;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.I = in_It;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.Q = in_It;
            m_CIC1.go(m_data_in);
            if(m_CIC1.m_cic_out_en){
                halfbandfilter1.m_in = m_CIC1.m_out;
                halfbandfilter1.go();
                if(halfbandfilter1.m_hf_en==1){
                    in[1][cnt1] = halfbandfilter1.m_hf_o;
                    AntennaMax[1] = AntennaMaxVal(in[1][cnt1],AntennaMax[1]);
                }
            }

            //file_CIC_in >> in_I;    m_data_in.I = in_I;     file_CIC_in >> in_Q;    m_data_in.Q = in_Q;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.I = in_It;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.Q = in_It;
            m_CIC2.go(m_data_in);
            if(m_CIC2.m_cic_out_en){
                halfbandfilter2.m_in = m_CIC2.m_out;
                halfbandfilter2.go();
                if(halfbandfilter2.m_hf_en==1){
                    in[2][cnt1] = halfbandfilter2.m_hf_o;
                    AntennaMax[2] = AntennaMaxVal(in[2][cnt1],AntennaMax[2]);
                }
                file_o_in_Tmp1 << fixed << setprecision(26) << halfbandfilter2.m_hf_o.I << "\t" << endl;//m_CIC2.m_out.I   halfbandfilter2.m_hf_o
                file_o_in_Tmp1 << fixed << setprecision(26) << halfbandfilter2.m_hf_o.Q << "\t" << endl;//m_CIC2.m_out.Q
            }

            //file_CIC_in >> in_I;    m_data_in.I = in_I;     file_CIC_in >> in_Q;    m_data_in.Q = in_Q;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.I = in_It;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.Q = in_It;
            m_CIC3.go(m_data_in);
            if(m_CIC3.m_cic_out_en){
                halfbandfilter3.m_in = m_CIC3.m_out;
                halfbandfilter3.go();
                if(halfbandfilter3.m_hf_en==1){
                    in[3][cnt1] = halfbandfilter3.m_hf_o;
                    AntennaMax[3] = AntennaMaxVal(in[3][cnt1],AntennaMax[3]);
                }
            }

            //file_CIC_in >> in_I;    m_data_in.I = in_I;     file_CIC_in >> in_Q;    m_data_in.Q = in_Q;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.I = in_It;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.Q = in_It;
            m_CIC4.go(m_data_in);
            if(m_CIC4.m_cic_out_en){
                halfbandfilter4.m_in = m_CIC4.m_out;
                halfbandfilter4.go();
                if(halfbandfilter4.m_hf_en==1){
                    in[4][cnt1] = halfbandfilter4.m_hf_o;
                    AntennaMax[4] = AntennaMaxVal(in[4][cnt1],AntennaMax[4]);
                }
            }

            //file_CIC_in >> in_I;    m_data_in.I = in_I;     file_CIC_in >> in_Q;    m_data_in.Q = in_Q;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.I = in_It;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.Q = in_It;
            m_CIC5.go(m_data_in);
            if(m_CIC5.m_cic_out_en){
                halfbandfilter5.m_in = m_CIC5.m_out;
                halfbandfilter5.go();
                if(halfbandfilter5.m_hf_en==1){
                    in[5][cnt1] = halfbandfilter5.m_hf_o;
                    AntennaMax[5] = AntennaMaxVal(in[5][cnt1],AntennaMax[5]);
                }
            }

            //file_CIC_in >> in_I;    m_data_in.I = in_I;     file_CIC_in >> in_Q;    m_data_in.Q = in_Q;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.I = in_It;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.Q = in_It;
            m_CIC6.go(m_data_in);
            if(m_CIC6.m_cic_out_en){
                halfbandfilter6.m_in = m_CIC6.m_out;
                halfbandfilter6.go();
                if(halfbandfilter6.m_hf_en==1){
                    in[6][cnt1] = halfbandfilter6.m_hf_o;
                    AntennaMax[6] = AntennaMaxVal(in[6][cnt1],AntennaMax[6]);
                }
            }

            //file_CIC_in >> in_I;    m_data_in.I = in_I;     file_CIC_in >> in_Q;    m_data_in.Q = in_Q;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.I = in_It;
            file_CIC_in.read(reinterpret_cast<char*>(&in_It),sizeof(int8_t));m_data_in.Q = in_It;
            m_CIC7.go(m_data_in);
            if(m_CIC7.m_cic_out_en){
                halfbandfilter7.m_in = m_CIC7.m_out;
                halfbandfilter7.go();
                //LOG2DEV << cnt1 << endl;
                if(halfbandfilter7.m_hf_en==1){
                    in[7][cnt1] = halfbandfilter7.m_hf_o;
                    AntennaMax[7] = AntennaMaxVal(in[7][cnt1],AntennaMax[7]);
                    cnt1++;
                    //if(cnt1%10000==0){
                    //    LOG2DEV << cnt1 << endl;
                    //}
                }
            }
        }
        file_o_in_Tmp1.close();
        //LOG2DEV << cnt1 << endl;
        file_CIC_in.close();
        #endif // SIM_1MHZ

        m_BF.configure(cfg1);
        m_BF.init(folderName_IS);

        m_BFSelect.configure(cfg2);
        m_BFSelect.init(folderName_IS);

        m_ud.m_slide_num = 0;
        TmpDemodLen = 0;
        //Iter = 0;
        if(Iter>=1){
            ifstream        file_o_in0;
            ifstream        file_o_in1;
            ifstream        file_o_in2;
            cnt1 = 0;
            char filenameIn[300] = {};
            strcpy(filenameIn, folderName_UD); strcat(filenameIn, "LastFFT.dat"); file_o_in0.open(filenameIn);
            strcpy(filenameIn, folderName_UD); strcat(filenameIn, "LastFFTESum.dat"); file_o_in1.open(filenameIn);
            strcpy(filenameIn, folderName_UD); strcat(filenameIn, "ANoiseLast.dat"); file_o_in2.open(filenameIn);
            while(!file_o_in0.eof()){
                file_o_in0>>in_I;   file_o_in0>>in_Q;   m_data_in.I = in_I;   m_data_in.Q = in_Q;   m_APreambleFft3_Last[0][cnt1] = m_data_in;
                file_o_in0>>in_I;   file_o_in0>>in_Q;   m_data_in.I = in_I;   m_data_in.Q = in_Q;   m_APreambleFft3_Last[1][cnt1] = m_data_in;
                file_o_in0>>in_I;   file_o_in0>>in_Q;   m_data_in.I = in_I;   m_data_in.Q = in_Q;   m_APreambleFft3_Last[2][cnt1] = m_data_in;
                file_o_in0>>in_I;   file_o_in0>>in_Q;   m_data_in.I = in_I;   m_data_in.Q = in_Q;   m_APreambleFft3_Last[3][cnt1] = m_data_in;
                file_o_in0>>in_I;   file_o_in0>>in_Q;   m_data_in.I = in_I;   m_data_in.Q = in_Q;   m_APreambleFft3_Last[4][cnt1] = m_data_in;
                file_o_in0>>in_I;   file_o_in0>>in_Q;   m_data_in.I = in_I;   m_data_in.Q = in_Q;   m_APreambleFft3_Last[5][cnt1] = m_data_in;
                file_o_in0>>in_I;   file_o_in0>>in_Q;   m_data_in.I = in_I;   m_data_in.Q = in_Q;   m_APreambleFft3_Last[6][cnt1] = m_data_in;
                file_o_in0>>in_I;   file_o_in0>>in_Q;   m_data_in.I = in_I;   m_data_in.Q = in_Q;   m_APreambleFft3_Last[7][cnt1] = m_data_in;
                file_o_in1>>in_I;   m_APreambleFftESum3_Last[cnt1] = in_I;
                cnt1++;
            }
            for(int ck0 = 0; ck0<m_ud.m_ant_num; ck0++){
                file_o_in2>>in_I;   ANoiseLast[ck0] = in_I;
            }
            file_o_in0.close();
            file_o_in1.close();
            file_o_in2.close();
        }

        for (int ck0 = 0; ck0 < fft_len * WindowNum; ck0++)
        {
        #ifdef SIM_1MHZ
            m_ud.m_in_1 = in[0][ck0];            m_ud.m_in_2 = in[1][ck0];            m_ud.m_in_3 = in[2][ck0];            m_ud.m_in_4 = in[3][ck0];
            m_ud.m_in_5 = in[4][ck0];            m_ud.m_in_6 = in[5][ck0];            m_ud.m_in_7 = in[6][ck0];            m_ud.m_in_8 = in[7][ck0];
        #else
            m_ud.m_in_1 = in[0][ck0+15];            m_ud.m_in_2 = in[1][ck0+15];            m_ud.m_in_3 = in[2][ck0+15];            m_ud.m_in_4 = in[3][ck0+15];
            m_ud.m_in_5 = in[4][ck0+15];            m_ud.m_in_6 = in[5][ck0+15];            m_ud.m_in_7 = in[6][ck0+15];            m_ud.m_in_8 = in[7][ck0+15];
        #endif
            m_ud.go(Iter,m_APreambleFft3_Last,m_APreambleFftESum3_Last,ANoiseLast);

            if(m_ud.m_slide_num>1 && m_ud.m_BFStartFlag==1)
            {
                //LOG2DEV << m_ud.m_slide_num << endl;
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
                    /*if(it==7 && t1==1){
                        it = 7;
                    }*/
                    //LOG2DEV << it << endl;
                    for(int ck1 = 0; ck1 < m_ud.m_ant_num;ck1++)
                    {
                        m_BF.m_AH[ck1] = m_ud.m_AHNorm_o[ck1][it];
                        m_BFSelect.m_AHIn[ck1] = m_ud.m_ah_o[ck1][it];
                    }
                    //m_BF.m_freqOffsetChGrid = (int)(m_ud.m_fft_len*(m_ud.m_freq_offset_o[it]+m_ud.m_GWRxSampleRate)/m_ud.m_GWRxSampleRate);
                    m_BF.go(m_ud.m_freq_offset_o[it],SampleRate_reg[(int)GWRxSampleRate]);

                    mem_copy(&m_BFSelect.m_DataIn[0][0], &m_BF.m_SigRxOut[0][0], (m_ud.m_ant_num * m_BF.m_ifftLen));
                    m_BFSelect.go();
                    for(int ck1 = 0; ck1 < m_BF.m_ifftLen; ck1++){
                        DemoBuffer[it][ck1+TmpDemodLen] = m_BFSelect.m_out[ck1];
                    }
                    SaveBFSelectIndex[m_ud.m_slide_num-2][it] = m_BFSelect.m_bf_o_index;
                    //SaveBFSelectIndex[m_ud.m_slide_num-2][it] = m_BFSelect.m_shift;
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

                        mem_copy(&TmpDemodDataBuffer[0+phy2_1_rx_dmod.RxByteLen*seg_cnt],phy2_1_rx_dmod.RxByte_o,phy2_1_rx_dmod.RxByteLen);
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
            if(SumCRCResult==0){
                if(SumRxUserNum==0){
                    mem_copy(&GWRxDemodDataBuffer[SumRxUserNum][0],&TmpDemodDataBuffer[0],phy2_1_rx_dmod.RxByteLen*cfg3.seg_num);
                    for(int i1=0;i1<m_ud.m_ant_num;i1++){
                        GWRX2TXPare.AH[i1][SumRxUserNum] = m_ud.m_ah_o[i1][it];
                    }
                    mem_copy(&GWRX2TXPare.ANoise[0],m_ud.m_ant_anoise,m_ud.m_ant_num);
                    GWRX2TXPare.GWRxPilotPower[SumRxUserNum] = m_ud.m_pilot_power_o[it];
                    GWRX2TXPare.TxFreq[SumRxUserNum] = m_ud.m_freq_offset_o[it];
                    GWRX2TXPare.GWRXSNR[SumRxUserNum] = phy2_1_rx_dmod.snr_est_fsk;
                    GWRX2TXPare.Fbit[0][SumRxUserNum] = phy2_1_rx_dmod.Fbit;
                    GWRX2TXPare.Sbit[0][SumRxUserNum] = phy2_1_rx_dmod.Sbit;
                    //LOG2DEV <<"UserID:"<<GWRxDemodDataBuffer[SumRxUserNum][0]<<GWRxDemodDataBuffer[SumRxUserNum][1]<<endl;
                    SumRxUserNum++;
                    #ifdef Print_Log
                    //LOG2DEV <<"CRC:"<<phy2_1_rx_dmod.m_crc_error<<"  Index:"<<SaveBFSelectIndex[0][it]<<" "<<SaveBFSelectIndex[1][it]<<"  snr_fsk:"<<phy2_1_rx_dmod.snr_est_fsk<<"  snr_dqpsk:"<<phy2_1_rx_dmod.snr_est_dqpsk<<endl;
                    #endif // Print_Log
                }else{
                    int DifDataNum = 0;
                    for(int ck1 = 0; ck1 < SumRxUserNum; ck1++){
                        if(DataArraysCompare(&GWRxDemodDataBuffer[ck1][0],&TmpDemodDataBuffer[0],phy2_1_rx_dmod.RxByteLen*cfg3.seg_num)){
                            if(m_ud.m_pilot_power_o[it]>GWRX2TXPare.GWRxPilotPower[ck1]){
                                for(int i1=0;i1<m_ud.m_ant_num;i1++){
                                    GWRX2TXPare.AH[i1][ck1] = m_ud.m_ah_o[i1][it];
                                }
                                GWRX2TXPare.GWRxPilotPower[ck1] = m_ud.m_pilot_power_o[it];
                                GWRX2TXPare.TxFreq[ck1] = m_ud.m_freq_offset_o[it];
                            }
                        }else{
                            DifDataNum++;
                        }
                    }
                    if(DifDataNum==SumRxUserNum){
                        mem_copy(&GWRxDemodDataBuffer[SumRxUserNum][0],&TmpDemodDataBuffer[0],phy2_1_rx_dmod.RxByteLen*cfg3.seg_num);
                        for(int i1=0;i1<m_ud.m_ant_num;i1++){
                            GWRX2TXPare.AH[i1][SumRxUserNum] = m_ud.m_ah_o[i1][it];
                        }
                        GWRX2TXPare.GWRxPilotPower[SumRxUserNum] = m_ud.m_pilot_power_o[it];
                        GWRX2TXPare.TxFreq[SumRxUserNum] = m_ud.m_freq_offset_o[it];
                        GWRX2TXPare.GWRXSNR[SumRxUserNum] = phy2_1_rx_dmod.snr_est_fsk;
                        GWRX2TXPare.Fbit[0][SumRxUserNum] = phy2_1_rx_dmod.Fbit;
                        GWRX2TXPare.Sbit[0][SumRxUserNum] = phy2_1_rx_dmod.Sbit;

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
        for(int i1 = 0; i1 < m_ud.m_ant_num; i1++){
            GWRX2TXPare.ANoise[i1] = m_ud.m_ant_anoise[i1];
            GWRX2TXPare.CalibrateFactor[i1].I = CalibrateFactor[i1*2];
            GWRX2TXPare.CalibrateFactor[i1].Q = CalibrateFactor[i1*2 + 1];
        }
        LOG2DEV << "CaseNum:" << case_num << " FreamIndex:" << Iter << " RxUserNum:"<< SumRxUserNum << endl;
        fwlog << "CaseNum:"  << case_num  << " FreamIndex:" << Iter << " RxUserNum:"<< SumRxUserNum << endl;
        TotalRxUserNum = TotalRxUserNum + SumRxUserNum;
        //m_BF.file_o_in_Tmp1.close();
        //m_BF.file_SVDIn_R.close();
        //m_BF.file_SVDOut_G.close();
        //m_BF.file_SVDOut_V.close();
        ofstream        file_o_out0;
        ofstream        file_o_out1;
        ofstream        file_o_out2;
        ofstream        file_o_out3;
        ofstream        file_o_out4;
        ofstream        file_o_out5;
        ofstream        file_o_out6;
        ofstream        file_o_out7;
        ofstream        file_o_out8;
        cnt1 = 0;
        char filenameIn[300] = {};
        strcpy(filenameIn, folderName_UD); strcat(filenameIn, "LastFFT.dat"); file_o_out0.open(filenameIn,std::ios::trunc);
        strcpy(filenameIn, folderName_UD); strcat(filenameIn, "LastFFTESum.dat"); file_o_out1.open(filenameIn,std::ios::trunc);
        strcpy(filenameIn, folderName_UD); strcat(filenameIn, "ANoiseLast.dat"); file_o_out2.open(filenameIn,std::ios::trunc);
        strcpy(filenameIn, folderName_Demo); strcat(filenameIn, "GWRXUserNum.dat"); file_o_out3.open(filenameIn,std::ios::trunc);
        strcpy(filenameIn, folderName_Demo); strcat(filenameIn, "GWRXSNR_Freq.dat"); file_o_out4.open(filenameIn,std::ios::trunc);
        strcpy(filenameIn, folderName_Demo); strcat(filenameIn, "GWRXAH.dat"); file_o_out5.open(filenameIn,std::ios::trunc);
        strcpy(filenameIn, folderName_Demo); strcat(filenameIn, "GWRXPilotPower.dat"); file_o_out6.open(filenameIn,std::ios::trunc);
        strcpy(filenameIn, folderName_Demo); strcat(filenameIn, "S_F_Bit.dat"); file_o_out7.open(filenameIn,std::ios::trunc);
        strcpy(filenameIn, folderName_Demo); strcat(filenameIn, "GWRXData.dat"); file_o_out8.open(filenameIn,std::ios::trunc);
        for(int i1 = 0; i1 < m_ud.m_fft_len; i1++){
            for(int i2 = 0; i2 < m_ud.m_ant_num; i2++){
                file_o_out0 << fixed << setprecision(26) << m_ud.m_APreambleFft2[i2][i1].I<<"\t"<<endl;
                file_o_out0 << fixed << setprecision(26) << m_ud.m_APreambleFft2[i2][i1].Q<<"\t"<<endl;
            }
            file_o_out1 << fixed << setprecision(26) << m_ud.m_APreambleFftESum2[i1]<<"\t"<<endl;
        }
        for(int i1 = 0; i1<m_ud.m_ant_num; i1++){
            file_o_out2 << fixed << setprecision(26) << m_ud.m_ant_anoise[i1]<<"\t"<<endl;
        }

        file_o_out3 << SumRxUserNum << "\t" << endl;
        file_o_out3 << cfg3.seg_num << "\t" << endl;
        file_o_out3 << phy2_1_rx_dmod.RxByteLen << "\t" << endl;

        if(SumRxUserNum>0){
            for(int i1 = 0; i1 < SumRxUserNum ;i1++){
                file_o_out4 << fixed << setprecision(26) << GWRX2TXPare.GWRXSNR[i1] << "\t" << endl;
                file_o_out4 << fixed << setprecision(26) << GWRX2TXPare.TxFreq[i1] << "\t" << endl;
                for(int i2 = 0; i2 < m_ud.m_ant_num; i2++){
                    file_o_out5 << fixed << setprecision(26) << GWRX2TXPare.AH[i2][i1].I << "\t" << endl;
                    file_o_out5 << fixed << setprecision(26) << GWRX2TXPare.AH[i2][i1].Q << "\t" << endl;
                }
                file_o_out6 << fixed << setprecision(26) << GWRX2TXPare.GWRxPilotPower[i1] << "\t" << endl;
                for(int i2 = 0; i2 < cfg3.seg_num; i2++){
                    file_o_out7 << GWRX2TXPare.Sbit[i2][i1] << "\t" << endl;
                    file_o_out7 << GWRX2TXPare.Fbit[i2][i1] << "\t" << endl;
                }
                for(int i2 = 0; i2 < phy2_1_rx_dmod.RxByteLen*cfg3.seg_num ;i2++){
                    file_o_out8 << GWRxDemodDataBuffer[i1][i2] << "\t" << endl;
                }
            }
        }
        file_o_out0.close();
        file_o_out1.close();
        file_o_out2.close();
        file_o_out3.close();
        file_o_out4.close();
        file_o_out5.close();
        file_o_out6.close();
        file_o_out7.close();
        file_o_out8.close();
    //}

    fwlog.close();
    //file_o_in_Tmp1.close();
    free(in);
    free(DemoBuffer);
    free(Demo_in);
    free(m_APreambleFft3_Last);
    free(m_APreambleFftESum3_Last);
    free(ANoiseLast);
    free(TmpDemodDataBuffer);
    //free(GWRxDemodDataBuffer);

    //SumRxUserNum = 0;
    GWRX2TXPare.TxUserNum = SumRxUserNum;
    if(SumRxUserNum>0){
        //LOG2DEV << 1 << endl;

        ofstream        file_o_in_Tmp2;
        ofstream        file_o_in_Tmp3;
        ofstream        file_o_in_Tmp4;
        ofstream        file_o_in_Tmp5;
        char filenameTmp1[300] = {};
        strcpy(filenameTmp1, folderName_GWTXModule); strcat(filenameTmp1, "GW_Tx_MutiAntennaSignal.dat"); file_o_in_Tmp2.open(filenameTmp1,std::ios::trunc);
        strcpy(filenameTmp1, folderName_GWTXModule); strcat(filenameTmp1, "GW_Tx_halfbandOut.dat"); file_o_in_Tmp3.open(filenameTmp1,std::ios::trunc);
        strcpy(filenameTmp1, folderName_GWTXModule); strcat(filenameTmp1, "GW_Tx_CIC_SDM_I.dat"); file_o_in_Tmp4.open(filenameTmp1,std::ios::trunc);
        strcpy(filenameTmp1, folderName_GWTXModule); strcat(filenameTmp1, "GW_Tx_CIC_SDM_Q.dat"); file_o_in_Tmp5.open(filenameTmp1,std::ios::trunc);

#ifdef RTL_BITMATCH_USE
        //Complex<double>                 **use_mem;
        Complex<fxp>                    m_test_fxp;
        ofstream                   fp_ddc_freq;
        ofstream                   up_bf_mult0;
        ofstream                   up_bf_mult1;
        ofstream                   up_bf_mult2;
        ofstream                   up_bf_mult3;
        ofstream                   up_bf_mult4;
        ofstream                   up_bf_mult5;
        ofstream                   up_bf_mult6;
        ofstream                   up_bf_mult7;
        ofstream                   sum_add;
        ofstream                   up_bf_mult_shift;
        ofstream                   fp_test_dump;
        ofstream                   fp_half_band_ant0;
        ofstream                   fp_half_band_ant1;
        ofstream                   fp_half_band_ant2;
        ofstream                   fp_half_band_ant3;
        ofstream                   fp_half_band_ant4;
        ofstream                   fp_half_band_ant5;
        ofstream                   fp_half_band_ant6;
        ofstream                   fp_half_band_ant7;
        ofstream                   fp_test;
        //use_mem  = NULL;
        //use_mem = safe_new(use_mem , 128 , 327680);
        fix_set(m_test_fxp, 25, 25, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sum_fxp, "m_sum_fxp", 1, 1);

        char filename_mat[300] = {};
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"up_bf_mult0.dat"); up_bf_mult0.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"up_bf_mult1.dat"); up_bf_mult1.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"up_bf_mult2.dat"); up_bf_mult2.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"up_bf_mult3.dat"); up_bf_mult3.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"up_bf_mult4.dat"); up_bf_mult4.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"up_bf_mult5.dat"); up_bf_mult5.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"up_bf_mult6.dat"); up_bf_mult6.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"up_bf_mult7.dat"); up_bf_mult7.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"sum_add.dat"); sum_add.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"up_bf_mult_shift.dat"); up_bf_mult_shift.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"ddc_freq.dat"); fp_ddc_freq.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"main_test.dat"); fp_test_dump.open(filename_mat);

        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"half_band_ant0.dat"); fp_half_band_ant0.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"half_band_ant1.dat"); fp_half_band_ant1.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"half_band_ant2.dat"); fp_half_band_ant2.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"half_band_ant3.dat"); fp_half_band_ant3.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"half_band_ant4.dat"); fp_half_band_ant4.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"half_band_ant5.dat"); fp_half_band_ant5.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"half_band_ant6.dat"); fp_half_band_ant6.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"half_band_ant7.dat"); fp_half_band_ant7.open(filename_mat);
        strcpy(filename_mat, folderName_GWTXModule); strcat(filename_mat,"main_test.dat"); fp_test.open(filename_mat);
#endif

        m_encode_cfg.CR = CodeRate;
        m_encode_cfg.crc_en = CRC_En;
        m_encode_cfg.len_tx = SymNumData*CodeRate/2;

        m_mod_cfg.BlockNum = TxBlockNum;
        m_mod_cfg.DSSSNum = DSSSNUM;
        m_mod_cfg.LenSymPhase = 16;
        //m_mod_cfg.len_infobit = 224;
        m_mod_cfg.reg_agc_len =  AGCLen / (1e6 / DOUBLE_POW2[64 + UpSampleNum + 1]);
        m_mod_cfg.reg_agc_num = 0;
        m_mod_cfg.SymNumData = SymNumData;
        m_mod_cfg.TransRatio = TransRatio;

        m_us_cfg.BlockNum = m_mod_cfg.BlockNum;
        m_us_cfg.data_times = UpSampleNum;
        m_us_cfg.tx_data_len_cfg = (m_encode_cfg.len_tx * 8 / 4 * 1.0 / m_encode_cfg.CR  + 12) * m_mod_cfg.LenSymPhase *  m_mod_cfg.DSSSNum + m_mod_cfg.reg_agc_len;
        m_us_cfg.tx_samplingrate = SampleRate_reg[(int)GWRxSampleRate];

        m_beamforming_cfg.user_num_i = GWRX2TXPare.TxUserNum;
        m_beamforming_cfg.reg_total_pow = TotalTxPower;
        m_beamforming_cfg.reg_pow_diff1 = PowerDif1;
        m_beamforming_cfg.reg_pow_diff2 = PowerDif2;
        m_beamforming_cfg.reg_pow_diff3 = PowerDif3;
        m_beamforming_cfg.GWTxPowerFlag = TxPowerFlag;
        m_beamforming_cfg.RecipracalCaliFlag = RecipracalCaliFlag;
        m_beamforming_cfg.reg_mmse = MMSE;
        m_beamforming_cfg.reg_bw_overlop_w = TmpBWOverlop;

        m_txCIC_cfg.RR				    =	CIC_TX_RR;
        m_txCIC_cfg.MM				    =	1;
        m_txCIC_cfg.NN				    =	3;
        m_txCIC_cfg.Fs				    =	32e6;
        m_txCIC_cfg.alpha			    =	0.25;
        m_txCIC_cfg.bypass			    =	0;
        m_txCIC_cfg.agc_gain			=	1;

        m_tx_CIC.m_tx_rx_flag = TX;
        m_tx_CIC.init(folderName_GWTXModule);
        m_tx_CIC.config(m_txCIC_cfg);
        m_sdm_tx.init(folderName_GWTXModule);

        m_tx_beamforming.m_umode_tx = TxMode;
        m_tx_beamforming.configure(m_beamforming_cfg);
        m_tx_beamforming.init(folderName_GWTXModule);

        for(int ck0 = 0; ck0 < m_beamforming_cfg.user_num_i;ck0++){
            for(int ck1 = 0; ck1 < m_ud.m_ant_num;ck1++){
                m_tx_beamforming.m_ah_i[ck1][ck0].I = GWRX2TXPare.AH[ck1][ck0].I;
                m_tx_beamforming.m_ah_i[ck1][ck0].Q = GWRX2TXPare.AH[ck1][ck0].Q;
            }
            m_tx_beamforming.m_freq_off[ck0] = GWRX2TXPare.TxFreq[ck0];
            m_tx_beamforming.m_pilot_power_i[ck0] = GWRX2TXPare.GWRxPilotPower[ck0];
            m_tx_beamforming.m_txdata_power[ck0] = 41;//0:powerallocation + 0dB,1:powerallocation + 1dB 0 41
        }
        for(int i1 = 0; i1 < m_ud.m_ant_num; i1++){
            m_tx_beamforming.m_anoise_sqrt_i[i1] = GWRX2TXPare.ANoise[i1];
            m_tx_beamforming.m_cali_factor[i1] = GWRX2TXPare.CalibrateFactor[i1];
        }

        m_tx_encode.configure(m_encode_cfg);
        m_tx_encode.init(folderName_GWTXModule);
        m_tx_mod.config(m_mod_cfg);
        m_tx_mod.init(folderName_GWTXModule);
        m_us_cfg.tx_data_agc_cfg = 0;
        m_tx_us.config(m_us_cfg);
        m_tx_us.init(folderName_GWTXModule);

        m_txhalfband_cfg.osr = 1;
        m_txhalfband_cfg.osr_cfg = halfbandUpNum;

        //LOG2DEV << 2 << " " << halfbandUpNum << " " << m_encode_cfg.len_tx << " " << m_us_cfg.tx_data_len_cfg <<endl;
        //m_tx_ushalfbandfilter.config(m_txhalfband_cfg);
        //m_tx_ushalfbandfilter.init(1);

        double          us_in;
        double          m_user_phase[128];
        int             cnt_us_in = 0;
        int             cnt_us_out = 0;
        int             us_output_len = 0;
        int             m_block_start = 0;
        int             halfbandout_len = 0;
        int             halfbandout_len1 = 0;
        int             halfbandout_len2 = 0;
        int             shift;
        Complex<double> sum_tmp = 0;
        Complex<fxp>    m_sum_fxp;
        Complex<fxp>    m_mult_fxp;
        Complex<fxp>    gw_up_in_fxp;

        int             *in_user_tmp;
        double          *us_in_buff;
        int             **TxIn;
        int             *fsk;
        int             *dqpsk;
        double          *out;
        double          *sigtx;
        Complex<double> *sigtx_IQ;
        Complex<double> **GW_Tx_MutiAntennaSignal;
        Complex<double> **gw_up_in;
        Complex<double> **sigtxhalfband;
        Complex<double> us_in1;
        Complex<double> **CIC_UpsampleOut;
        in_user_tmp = NULL;
        us_in_buff = NULL;
        TxIn = NULL;
        fsk = NULL;
        dqpsk = NULL;
        out = NULL;
        sigtx = NULL;
        sigtx_IQ = NULL;
        GW_Tx_MutiAntennaSignal = NULL;
        gw_up_in = NULL;
        sigtxhalfband = NULL;
        CIC_UpsampleOut = NULL;

        //LOG2DEV << 3 << endl;
        in_user_tmp = safe_new(in_user_tmp, m_encode_cfg.len_tx * 8);
        us_in_buff = safe_new(us_in_buff , m_us_cfg.tx_data_len_cfg);
        TxIn = safe_new(TxIn, 128, 32768);
        fsk = safe_new(fsk, 32768);
        dqpsk = safe_new(dqpsk, 32768);
        out = safe_new(out , 327680);
        sigtx = safe_new(sigtx , 327680);
        sigtx_IQ = safe_new(sigtx_IQ, 327680);
        GW_Tx_MutiAntennaSignal = safe_new(GW_Tx_MutiAntennaSignal , 8 , 327680);
        gw_up_in = safe_new(gw_up_in , 8 , 327680);
        sigtxhalfband = safe_new(sigtxhalfband , 8 , 32768000);
        //CIC_UpsampleOut = safe_new(CIC_UpsampleOut, 8 ,32768000*16);
        #ifdef TXFix
        fix_set(m_sum_fxp, 16, 2, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sum_fxp, "m_sum_fxp", 1, 1);
        fix_set(m_mult_fxp, 16, 2, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_mult_fxp, "m_mult_fxp", 1, 1);
        fix_set(gw_up_in_fxp, 12, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sum_fxp, "m_sum_fxp", 1, 1);
        #else
        #endif
        //LOG2DEV << 4 << endl;
        m_tx_beamforming.go();

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
                for (int ck2 = 0; ck2 < m_encode_cfg.len_tx; ck2++) {
                    //in_user_tmp[ck2] = TxIn[ck0][ck2 + ck1 * m_encode_cfg.len_tx * 8];GWRxDemodDataBuffer[0][1]
                    for(int t0 = 0; t0 < 8; t0++){
                        int TmpData = GWRxDemodDataBuffer[ck0][ck2 + ck1 * m_encode_cfg.len_tx];
                        in_user_tmp[ck2 * 8 + 7 - t0] = (TmpData>>t0 & 0x1);
                        //LOG2DEV << TmpData << " " << in_user_tmp[ck2 * 8 + 7 - t0] << endl;
                    }
                }
                in_user_tmp[m_encode_cfg.len_tx * 8 -16] = GWRX2TXPare.Sbit[ck1][ck0];
                in_user_tmp[m_encode_cfg.len_tx * 8 -15] = GWRX2TXPare.Fbit[ck1][ck0];

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

#ifdef RTL_BITMATCH_USE
	   m_test_fxp.I = round(m_tx_beamforming.m_freq_off[ck0]);//**********
	   dump(fp_ddc_freq,&m_test_fxp.I,1,m_test_fxp.I);
#endif
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
                /*if(ck0==0){
                    for(int ck3=0;ck3 < m_tx_us.m_output_len;ck3++){
                        file_o_in_Tmp2 << fixed << setprecision(26) << sigtx_IQ[ck3].I << "\t" << endl;//sigtx
                    }
                }*/
                m_tx_encode.reset();
                m_tx_mod.reset();
                if(ck1 == 0)
                    us_output_len = m_tx_us.m_output_len;
                else
                    us_output_len = m_tx_us.m_output_len - 1;
                //LOG2DEV << ck0 << " " << us_output_len << " " << m_tx_us.m_freq_in <<endl;
                for(int ck2 = 0;ck2 < 8 ;ck2++)
                {
                    for(int ck3 = 0;ck3 < us_output_len;ck3++)
                    {
                        m_mult_fxp = m_tx_beamforming.m_beamforming_matrix_tx_o[ck0][ck2] * sigtx_IQ[ck3];// / 1024.0 / 1024.0 / 8.0;
                        sum_tmp = GW_Tx_MutiAntennaSignal[ck2][m_block_start + ck3] + m_mult_fxp.todouble();
                        m_sum_fxp = sum_tmp;
                        GW_Tx_MutiAntennaSignal[ck2][m_block_start + ck3] = m_sum_fxp;
#ifdef RTL_BITMATCH_USE
                        if(ck2 == 0)
                           rtl_dump(ck2,ck0,m_us_cfg.data_times,us_output_len,ck3,up_bf_mult0,&m_sum_fxp,m_sum_fxp.I);
                        if(ck2 == 1)
                           rtl_dump(ck2,ck0,m_us_cfg.data_times,us_output_len,ck3,up_bf_mult1,&m_sum_fxp,m_sum_fxp.I);
                        if(ck2 == 2)
                           rtl_dump(ck2,ck0,m_us_cfg.data_times,us_output_len,ck3,up_bf_mult2,&m_sum_fxp,m_sum_fxp.I);
                        if(ck2 == 3)
                           rtl_dump(ck2,ck0,m_us_cfg.data_times,us_output_len,ck3,up_bf_mult3,&m_sum_fxp,m_sum_fxp.I);
                        if(ck2 == 4)
                           rtl_dump(ck2,ck0,m_us_cfg.data_times,us_output_len,ck3,up_bf_mult4,&m_sum_fxp,m_sum_fxp.I);
                        if(ck2 == 5)
                           rtl_dump(ck2,ck0,m_us_cfg.data_times,us_output_len,ck3,up_bf_mult5,&m_sum_fxp,m_sum_fxp.I);
                        if(ck2 == 6)
                           rtl_dump(ck2,ck0,m_us_cfg.data_times,us_output_len,ck3,up_bf_mult6,&m_sum_fxp,m_sum_fxp.I);
                        if(ck2 == 7)
                           rtl_dump(ck2,ck0,m_us_cfg.data_times,us_output_len,ck3,up_bf_mult7,&m_sum_fxp,m_sum_fxp.I);
#endif
                    }
                }
            }
            for(int ck2 = 0;ck2 < 8 ;ck2++){
                //LOG2DEV << "halfband Start: " << us_output_len <<endl;
                //tx halfband
                m_txhalfband_cfg.osr = 1;
                m_txhalfband_cfg.osr_cfg = halfbandUpNum;
                m_txhalfband_cfg.tx_len_cfg = us_output_len;

                m_tx_ushalfbandfilter.config(m_txhalfband_cfg);
                m_tx_ushalfbandfilter.init(1);
                m_tx_ushalfbandfilter.m_i_in_en = 1;
                //m_tx_ushalfbandfilter.reset(1);
                while(1)
                {
                    if(cnt_us_in < us_output_len) {
                        us_in1 = GW_Tx_MutiAntennaSignal[ck2][cnt_us_in];
                        cnt_us_in = cnt_us_in + 1;
                    } else {
                        //m_tx_us.m_i_in_en = 0;
                        m_tx_ushalfbandfilter.m_i_in_en = 0;
                    }
                    m_tx_ushalfbandfilter.go(us_in1);
                    //m_tx_us.go(us_in);
                    if(m_tx_ushalfbandfilter.m_out_en == 1 || m_tx_ushalfbandfilter.m_finished_o == 1)
                    {
                        sigtxhalfband[ck2][halfbandout_len2+cnt_us_out]= m_tx_ushalfbandfilter.m_out;
                        cnt_us_out = cnt_us_out + 1;
                    }
                    if(m_tx_ushalfbandfilter.m_finished_o == 1 && ck2==0)
                    {
                        halfbandout_len = halfbandout_len + cnt_us_out;
                    }
                    if(m_tx_ushalfbandfilter.m_finished_o == 1)
                    {
                        //halfbandout_len1 = halfbandout_len1 + cnt_us_out;
                        //halfbandout_len = halfbandout_len + cnt_us_out;
                        //LOG2DEV << "halfband end: " << halfbandout_len <<endl;
                        cnt_us_in = 0;
                        cnt_us_out = 0;
                        //m_tx_ushalfbandfilter.reset(1);
                        break;
                    }
                }
                //halfbandout_len1 = 0;
            }
                //cic upsample
            halfbandout_len2 = halfbandout_len;
            m_block_start = m_block_start + us_output_len;
            //LOG2DEV << m_block_start << " " << halfbandout_len2 << endl;
        }
        /*for(int ck0 = 0; ck0<m_block_start; ck0++){
            for(int ck1=0;ck1 < 8;ck1++){
                file_o_in_Tmp2 << fixed << setprecision(26) << GW_Tx_MutiAntennaSignal[ck1][ck0].I << "\t" << endl;
                file_o_in_Tmp2 << fixed << setprecision(26) << GW_Tx_MutiAntennaSignal[ck1][ck0].Q << "\t" << endl;
            }
        }*/
#ifdef RTL_BITMATCH_USE
    int us_output_lentt = us_output_len;
    for(int ck1 = 0; ck1 < m_block_start; ck1++) {
	    for(int ck0 = 7;ck0 >= 0 ;ck0--) {
	    if(ck0 == 0 ){
	      dump(sum_add,&GW_Tx_MutiAntennaSignal[ck0][ck1],1,m_sum_fxp.I);
              if( ck1 == us_output_lentt){
	         us_output_lentt = us_output_len;
	         break;
	      }
	    }
	    else
	      dump(sum_add,&GW_Tx_MutiAntennaSignal[ck0][ck1],1,m_sum_fxp.I,0);
        }
    }
#endif

    shift = shift_cal(GW_Tx_MutiAntennaSignal, 8, us_output_len);//m_tx_us.m_output_len);
//    cout << "shift_out" << "\t"<<shift<<endl;

    for(int ck0 = 0;ck0 < 8 ;ck0++) {
        for(int ck1 = 0; ck1 < m_block_start; ck1++) {
            gw_up_in_fxp = GW_Tx_MutiAntennaSignal[ck0][ck1] * DOUBLE_POW2[64 - shift];
            gw_up_in[ck0][ck1] = gw_up_in_fxp;
        }
    }
    for(int ck0 = 0; ck0<m_block_start; ck0++){
        for(int ck1=0;ck1 < 8;ck1++){
            file_o_in_Tmp2 << fixed << setprecision(26) << gw_up_in[ck1][ck0].I*1.0 << "\t" << endl;
            file_o_in_Tmp2 << fixed << setprecision(26) << gw_up_in[ck1][ck0].Q*1.0 << "\t" << endl;
        }
    }
#ifdef RTL_BITMATCH_USE
    us_output_lentt = us_output_len;
    for(int ck1 = 0; ck1 < m_block_start; ck1++) {
	    for(int ck0 = 7;ck0 >= 0 ;ck0--) {
	    if(ck0 == 0 ){
	      dump(up_bf_mult_shift ,&gw_up_in[ck0][ck1],1,gw_up_in_fxp.I);
              if( ck1 == us_output_lentt){
	         us_output_lentt = us_output_len;
	         break;
	      }
	    }
	    else
	      dump(up_bf_mult_shift,&gw_up_in[ck0][ck1],1,gw_up_in_fxp.I,0);
        }
    }
#endif

        Complex<double> TmpSDMIn,TmpSDMOut;
        for(int ck0 = 0; ck0<halfbandout_len; ck0++){
            for(int ck1=0;ck1 < 8;ck1++){
                file_o_in_Tmp3 << fixed << setprecision(26) << sigtxhalfband[ck1][ck0].I*4.0 << "\t" << endl;
                file_o_in_Tmp3 << fixed << setprecision(26) << sigtxhalfband[ck1][ck0].Q*4.0 << "\t" << endl;
            }
        }
#ifdef SIM_1MHZ

#else
        int cic_count = 0;
        int RR_all[7]  = {8, 16, 32, 64, 128, 256, 512}; // DSP Decm Ratio
        for(int ck0 = 0; ck0 < 8; ck0++){
            m_tx_CIC.init(folderName_GWTXModule);
            m_tx_CIC.config(m_txCIC_cfg);
            m_sdm_tx.reset();
            for(int ck1 = 0;ck1 < halfbandout_len;ck1++){
                if(ck0==2){
                    //LOG2DEV << "CIC_IN: " << sigtxhalfband[ck0][ck1].I << " " << sigtxhalfband[ck0][ck1].Q << endl;
                }
                m_tx_CIC.go(sigtxhalfband[ck0][ck1]);
                for(int ck2 = 0; ck2 < RR_all[CIC_TX_RR]; ck2++){
                    TmpSDMIn = m_tx_CIC.m_cic_o[ck2];
                    m_sdm_tx.go(TmpSDMIn);
                    TmpSDMOut = m_sdm_tx.m_out;
                    file_o_in_Tmp4 << TmpSDMOut.I << "\t" << endl;//TmpSDMIn  TmpSDMOut
                    file_o_in_Tmp5 << TmpSDMOut.Q << "\t" << endl;//
                    cic_count++;
                    if(ck0==2){
                        //LOG2DEV << "CIC_OUT: " << ck1 << " " << ck2 << " "<< TmpSDMIn.I << " " << TmpSDMIn.Q << endl;
                    }
                }
            }
        }
        //LOG2DEV << halfbandout_len << " " << cic_count << " " << CIC_TX_RR << endl;
        file_o_in_Tmp2.close();
        file_o_in_Tmp3.close();
        file_o_in_Tmp4.close();
        file_o_in_Tmp5.close();
#endif

        free(in_user_tmp);
        free(us_in_buff);
        free(TxIn);
        free(fsk);
        free(dqpsk);
        free(out);
        free(sigtx);
        free(sigtx_IQ);
        free(GW_Tx_MutiAntennaSignal);
        free(gw_up_in);
        free(sigtxhalfband);
        free(CIC_UpsampleOut);

#ifdef RTL_BITMATCH_USE
        //free(use_mem);
        up_bf_mult0.close();
        up_bf_mult1.close();
        up_bf_mult2.close();
        up_bf_mult3.close();
        up_bf_mult4.close();
        up_bf_mult5.close();
        up_bf_mult6.close();
        up_bf_mult7.close();
        sum_add.close();
        up_bf_mult_shift.close();
        fp_half_band_ant0.close();
        fp_half_band_ant1.close();
        fp_half_band_ant2.close();
        fp_half_band_ant3.close();
        fp_half_band_ant4.close();
        fp_half_band_ant5.close();
        fp_half_band_ant6.close();
        fp_half_band_ant7.close();
        fp_test.close();
#endif
    }
    free(GWRxDemodDataBuffer);
    LOG2DEV << "TX Finished" << endl;
    //std::cout<< "Finished"<<std::endl;
    //system("pause");
   	return 0;
}


#ifdef RTL_BITMATCH_USE

int rtl_dump( int antenne, int user_number, int data_times, int us_len_out, int us_out_cnt, std::ofstream& file, Complex<fxp> *in,fxp fxp_temp){

          if(data_times == 0 ){
             dump(file,in,1,fxp_temp);
          }
          if(data_times == 1 ){
             if(dump_cnt == 0 && (us_out_cnt != us_len_out-1 ))
                 dump(file,in,1,fxp_temp,0);
             if(dump_cnt == 1 || (us_out_cnt == us_len_out-1))
                 dump(file,in,1,fxp_temp);
             if(us_out_cnt == us_len_out-1)
                 dump_cnt = 0;
            else
                 dump_cnt += 1;
             if(dump_cnt == 2 || (us_out_cnt == us_len_out-1))
                 dump_cnt = 0;
          }
          if(data_times == 2 ){
             if((dump_cnt != 3) && (us_out_cnt != us_len_out-1)) {
                 dump(file,in,1,fxp_temp,0);
             }
             if((dump_cnt == 3) || (us_out_cnt == us_len_out-1) ) {
                 dump(file,in,1,fxp_temp);
             }
             if(us_out_cnt == us_len_out-1 )
                 dump_cnt = 0;
             else
                 dump_cnt += 1;
             if((dump_cnt == 4) || (us_out_cnt == us_len_out-1) )
                 dump_cnt = 0;
          }
          if(data_times  == 3 ){
             if(dump_cnt != 7 && (us_out_cnt != us_len_out-1) )
	         dump(file,in,1,fxp_temp,0);
             if(dump_cnt == 7 || (us_out_cnt == us_len_out-1) )
	         dump(file,in,1,fxp_temp);
             if(us_out_cnt == us_len_out-1 )
                 dump_cnt = 0;
             else
                dump_cnt += 1;

             if(dump_cnt == 8 || (us_out_cnt == us_len_out-1))
                dump_cnt = 0;
          }
    return 0;
}
#endif
