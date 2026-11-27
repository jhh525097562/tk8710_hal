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

#include <stdint.h>
#include "../Cls_ud.h"
#include "../Cls_GWRxBFModule.h"
#include "../Cls_BFSelect.h"
#include "../Cls_rx_data_phy2_1.h"
#include "../Cls_Rx_Sync_2_1.h"
#define _WIN32

#ifdef _WIN32
#include <direct.h>
#define MKDIR(dir) _mkdir(dir)
#else
#include <sys/stat.h>
#define MKDIR(dir) mkdir(dir, 0777)
#endif

#define Fix
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
    int						case_num;
    int                     ant_num;
    int                     fft_len;
    int                    	statMd = 0;
    FFT_LEN                 reg_fft_len;
    UD_CFG                  cfg;
    GWRXBFMODULE_CFG        cfg1;
    BFSelect_CFG            cfg2;
    RX_PHY2_1_SYNC_CFG      cfg3;
    RX_DATA_2_1_CFG         cfg4;
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

    LOG2DEV << "CaseNum:" << case_num << " Mode:" << GWAllRxMode <<endl;
    if(SetFreqRxFlag==1){
        char filenameFreq[4096] = "D:/data/K2301/K2301PHY_CaseList/Cmodel_DataIn/case"; //
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

    char folderName_Log[300] = { "D:/data/K2301/K2301PHY_CaseList/LostRate/case" };
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
    char folderName_UD[300] = { "D:/data/K2301/K2301PHY_CaseList/UDModule_Data/case" };
    sprintf(num,"%d",case_num);
    strcat(folderName_UD, num);
    strcat(folderName_UD, "/");
    if (stat(folderName_UD, &statExist) != 0) {
            //statMd = mkdir(folderName_UD,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            statMd = MKDIR(folderName_UD);
            /*if (statMd)
                LOG2DEV << "Cannot create folder.\n";*/
    }
    m_ud.init(folderName_UD);

    char folderName_IS[300] = { "D:/data/K2301/K2301PHY_CaseList/BFModule_Data/case" };
    sprintf(num,"%d",case_num);
    strcat(folderName_IS, num);
    strcat(folderName_IS, "/");
    if (stat(folderName_IS, &statExist) != 0) {
            //statMd = mkdir(folderName_IS,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            statMd = MKDIR(folderName_IS);
            /*if (statMd)
                LOG2DEV << "Cannot create folder.\n";*/
    }

    char folderName_DemoSync[300] = { "D:/data/K2301/K2301PHY_CaseList/K2201SyncModule_Data/case" };
    sprintf(num,"%d",case_num);
    strcat(folderName_DemoSync, num);
    strcat(folderName_DemoSync, "/");
    if (stat(folderName_DemoSync, &statExist) != 0) {
            //statMd = mkdir(folderName_DemoSync,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            statMd = MKDIR(folderName_IS);
            /*if (statMd)
                LOG2DEV << "Cannot create folder.\n";*/
    }

    char folderName_Demo[300] = { "D:/data/K2301/K2301PHY_CaseList/K2201DemodModule_Data/case" };
    sprintf(num,"%d",case_num);
    strcat(folderName_Demo, num);
    strcat(folderName_Demo, "/");
    if (stat(folderName_Demo, &statExist) != 0) {
            //statMd = mkdir(folderName_Demo,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            statMd = MKDIR(folderName_IS);
            /*if (statMd)
                LOG2DEV << "Cannot create folder.\n";*/
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

    FrameNum = 100;
    int SetTxUserNum[10] = {64,64,96,96,96,96,64,32,16,16};//bingfa
    //int SetTxUserNum[10] = {1,1,1,1,1,1,1,1,1,1};//lingmindu
    //int SetTxUserNum[10] = {2,2,2,2,2,2,2,2,2,2};//lingmindu
    //int SetTxUserNum[10] = {4,4,4,4,4,4,4,4,4,4};//lingmindu
    int SaveBFSelectIndex[16][128];
    for(int t1 = 0; t1 < FrameNum; t1++){

        char filenameNew[4096] = "D:/data/K2301/K2301PHY_CaseList/Cmodel_DataIn/case"; //
        sprintf(num,"%d",case_num);
        strcat(filenameNew, num);
        strcat(filenameNew, "/");
        strcat(filenameNew, "UD_InData");
        sprintf(num,"%d",t1);
        strcat(filenameNew, num);
        strcat(filenameNew, ".txt");

        ifstream fw1;
        fw1.open(filenameNew);

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
                    //m_BF.m_freqOffsetChGrid = (int)(m_ud.m_fft_len*(m_ud.m_freq_offset_o[it]+m_ud.m_GWRxSampleRate)/m_ud.m_GWRxSampleRate);
                    m_BF.go(m_ud.m_freq_offset_o[it],SampleRate_reg[(int)GWRxSampleRate]);

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
            if(SumCRCResult==0){
                if(SumRxUserNum==0){
                    mem_copy(&GWRxDemodDataBuffer[SumRxUserNum][0],phy2_1_rx_dmod.RxByte_o,phy2_1_rx_dmod.RxByteLen);
                    mem_copy(&GWRX2TXPare.AH[0][SumRxUserNum],&m_ud.m_ah_o[0][it],m_ud.m_ant_num);
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
                                mem_copy(&GWRX2TXPare.AH[0][ck1],&m_ud.m_ah_o[0][it],m_ud.m_ant_num);
                                GWRX2TXPare.GWRxPilotPower[ck1] = m_ud.m_pilot_power_o[it];
                                GWRX2TXPare.TxFreq[ck1] = m_ud.m_freq_offset_o[it];
                            }
                        }else{
                            DifDataNum++;
                        }
                    }
                    if(DifDataNum==SumRxUserNum){
                        mem_copy(&GWRxDemodDataBuffer[SumRxUserNum][0],phy2_1_rx_dmod.RxByte_o,phy2_1_rx_dmod.RxByteLen);
                        mem_copy(&GWRX2TXPare.AH[0][SumRxUserNum],&m_ud.m_ah_o[0][it],m_ud.m_ant_num);
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
    free(in);
    free(DemoBuffer);
    free(Demo_in);
    free(m_APreambleFft3_Last);
    free(m_APreambleFftESum3_Last);
    free(ANoiseLast);
    std::cout<< "Finished"<<std::endl;
    system("pause");
   	return 0;
}
