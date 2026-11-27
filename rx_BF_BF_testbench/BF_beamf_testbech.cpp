/****************************************************************************
**
**  Copyright  (c)  2023-2025,  All Rights Reserved.
**
**  File          : is_testbench.cpp (main program to run simulation)
**  Created By    : 
**  Created On    : 
**
**  History     :
**  Programmer      Date        Rev      Description
**  --------------- ----------  -------- ------------------------------
**  
**  Notice: 主函数入口
****************************************************************************/

#include <iostream>
using namespace std;
#include "../Cls_BF_beamf.h"

int main(int argc, char *argv[])
{
    
    Cls_BF_beamf<double>    m_tx;
    BF_beamf_CFG            m_cfg;
    int						case_num;

    int                    	statMd = 0;
    int                     len_o;
    uint64_t                reg_phr;
    double                  in_I;
    double                  in_Q;
    Complex<double>         **in1;
    Complex<double>         **in2;
    double                  *in3;
    Complex<double>         **out;
    int                     cnt = 0;
    struct 					stat statExist;
	char 					num[10] = {0};
    char            		folderName[300] = {0};
    char                    fileName[300] = {0};
    char                    fileName1[300] = {0};
    char                    fileName2[300] = {0};
    ifstream                file_in;
    ifstream                file_in1;
    ifstream                file_in2;
    int                     user_num ;
    

    out = NULL;
    out = safe_new(out, 128,8192);
    in1 = NULL;
    in1 = safe_new(in1, 8, 8193);
    in2 = NULL;
    in2 = safe_new(in2, 8 , 128);
    in3 = NULL;
    in3 = safe_new(in3 , 128);
    sscanf(argv[1],  "%d", &case_num);

    strcpy(fileName, "./c_data/case1/in1_");
    sprintf(num, "%d", case_num);
    strcat(fileName, num);
    strcat(fileName, ".txt");
    file_in.open(fileName);
    if(!file_in)
    {
        cout << "no input file" << endl;
        exit(100);
    }
    strcat(fileName1, "./c_data/case1/in1_2.txt");
    file_in1.open(fileName1);
    strcat(fileName2, "./c_data/case1/in1_3.txt");
    file_in2.open(fileName2);

    sprintf(num, "%d", case_num);
    strcpy(folderName, "./data/");
    if (stat(folderName, &statExist) != 0) {
            statMd = mkdir(folderName);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    strcpy(folderName, "./data/case");
    strcat(folderName, num); strcat(folderName, "/");
    if (stat(folderName, &statExist) != 0) {
            statMd = mkdir(folderName);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }

    m_cfg.user_num = 5;
    m_cfg.BF_cfg.freqOffsetCh = 48141.47949218750;
    m_cfg.BF_cfg.Antenna = 8;
    m_cfg.BF_cfg.FFTLen = 2048;
    m_cfg.BF_cfg.SymbolRate = 15.625;
    m_cfg.BF_cfg.filterLen = 320;
    m_cfg.BF_cfg.ifftLen = 1024;
    m_cfg.BF_cfg.GWRxSampleRate = 500000;
    m_cfg.BF_cfg.GWRXBFMode = 0;
    m_cfg.BF_cfg.HCE = 9;

    m_cfg.beamf_cfg.slide_num_i = 1;
    m_cfg.beamf_cfg.ant_num = 8;
    m_cfg.beamf_cfg.fft_len = 2048;
    m_cfg.beamf_cfg.ifft_len = 1024;
    m_cfg.beamf_cfg.rx_d_win_i = 2;
    m_cfg.beamf_cfg.gw_rxbf_mode = 0;
    m_cfg.beamf_cfg.mfsk = 4;

    m_tx.configure(m_cfg);
    m_tx.init(folderName);
    while( !file_in.eof())
    {
        file_in >> in_I >> in_Q;
        in1[0][cnt].I = in_I;in1[0][cnt].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[1][cnt].I = in_I;in1[1][cnt].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[2][cnt].I = in_I;in1[2][cnt].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[3][cnt].I = in_I;in1[3][cnt].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[4][cnt].I = in_I;in1[4][cnt].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[5][cnt].I = in_I;in1[5][cnt].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[6][cnt].I = in_I;in1[6][cnt].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[7][cnt].I = in_I;in1[7][cnt].Q = in_Q;
        cnt++;
    }

    for(int ck0 = 0; ck0 < user_num; ck0++)
    {
        file_in1 >> in_I >> in_Q;
        in2[0][ck0].I = in_I;in2[0][ck0].Q = in_Q;
        file_in1 >> in_I >> in_Q;
        in2[1][ck0].I = in_I;in2[1][ck0].Q = in_Q;
        file_in1 >> in_I >> in_Q;
        in2[2][ck0].I = in_I;in2[2][ck0].Q = in_Q;
        file_in1 >> in_I >> in_Q;
        in2[3][ck0].I = in_I;in2[3][ck0].Q = in_Q;
        file_in1 >> in_I >> in_Q;
        in2[4][ck0].I = in_I;in2[4][ck0].Q = in_Q;
        file_in1 >> in_I >> in_Q;
        in2[5][ck0].I = in_I;in2[5][ck0].Q = in_Q;
        file_in1 >> in_I >> in_Q;
        in2[6][ck0].I = in_I;in2[7][ck0].Q = in_Q;
        file_in1 >> in_I >> in_Q;
        in2[7][ck0].I = in_I;in2[7][ck0].Q = in_Q;
    }

    for(int ck0 = 0; ck0 < user_num; ck0++)
    {
        file_in2 >> in_I ;
        in3[ck0] = in_I;
    }
    
    int cnt_d = 0 , cnt_u = 0;
    while (!m_tx.m_i_d_v || !m_tx.m_i_u_v)
    {
        if(!m_tx.m_i_d_v)
        {
            m_tx.m_in_1 = in1[0][cnt_d];
            m_tx.m_in_2 = in1[1][cnt_d];
            m_tx.m_in_3 = in1[2][cnt_d];
            m_tx.m_in_4 = in1[3][cnt_d];
            m_tx.m_in_5 = in1[4][cnt_d];
            m_tx.m_in_6 = in1[5][cnt_d];
            m_tx.m_in_7 = in1[6][cnt_d];
            m_tx.m_in_8 = in1[7][cnt_d];
            cnt_d++;
        }

        if (!m_tx.m_i_u_v)
        {
            m_tx.m_i_ah_1 = in2[0][cnt_u];
            m_tx.m_i_ah_2 = in2[1][cnt_u];
            m_tx.m_i_ah_3 = in2[2][cnt_u];
            m_tx.m_i_ah_4 = in2[3][cnt_u];
            m_tx.m_i_ah_5 = in2[4][cnt_u];
            m_tx.m_i_ah_6 = in2[5][cnt_u];
            m_tx.m_i_ah_7 = in2[6][cnt_u];
            m_tx.m_i_ah_8 = in2[7][cnt_u];
            m_tx.m_i_freq = in3[cnt_u];
            cnt_u++;        
        }
        m_tx.go();
    }
    
    // for(int user_i = 0; user_i < user_num; user_i++)
    // {
    //     for(int ck0 = 0 ;ck0 < cnt - 1 ;ck0++)
    //     {

    //     }
    //     // for (int ck0 = 0; ck0 < m_cfg.beamf_cfg.ifft_len; ck0++)
    //     // {
    //     //     out[user_i][ck0].I = m_tx.m_beamf.m_RxBeamforming_out[2 * ck0];
    //     //     out[user_i][ck0].Q = m_tx.m_beamf.m_RxBeamforming_out[2 * ck0 + 1];
    //     // }
        
    //     m_tx.m_BF.m_in2_finish = 0;
    // }

    std::cout<< "Finished"<<std::endl;
    system("pause");
    free(out);
    file_in.close();
    file_in1.close();
   	return 0;
    

}
