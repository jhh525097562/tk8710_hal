/****************************************************************************
**
**  Copyright  (c)  2023-2025,  All Rights Reserved.
**
**  File          : tx_encode_testbench.cpp (main program to run simulation)
**  Created By    :
**  Created On    :
**
**  History     :
**  Programmer      Date        Rev      Description
**  --------------- ----------  -------- ------------------------------
**
**  Notice: 主函数入口
****************************************************************************/
#include "iostream"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include "sys/stat.h"
#include "../Cls_tx_encode.h"
#include "../Cls_tx_beamforming.h"
#include "../Cls_tx_mod.h"
#include "../Cls_tx_us_2_1.h"
#include "../Cls_TX_US.h"

using namespace std;

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

int main(int argc, char *argv[])
{
    Cls_tx_encode<double>           m_tx_encode;
    Cls_tx_beamforming<fxp>         m_tx_beamforming;
    Cls_tx_mod<fxp>                 m_tx_mod;
    Cls_tx_us_2_1<double>           m_tx_us;
    Cls_TX_US<fxp>                  m_tx_us_h;
    TX_US_2_Cfg                     m_tx_us_cfg;
    TX_ENCODE_CFG                   m_encode_cfg;
    TX_MOD_CFG                      m_mod_cfg;
    TX_BEAMFORMING_CFG              m_beamforming_cfg;
    TX_US_2_1_Cfg                   m_us_cfg;
    int						        case_num;
    int                             reg_total_pow;
    double                          reg_pow_diff3;
    int                             model;
    int                             user_num;
    int                    	        statMd = 0;
    int                             **in;
    double                          *out;
    int                             *dqpsk;
    int                             *fsk;
    int                             shift;
    struct 					        stat statExist;
    ifstream                        file_in;
    ifstream                        file_in2;
    ifstream                        file_in3;
    ifstream                        file_in4;
    ifstream                        file_in5;
    ifstream                        file_in6;
    ofstream                        file_out;
	char 					        num[10] = {0};
	char                            model_num[10]= {0};
    char            		        folderName[300] = {0};
    char                            fileName[300] = {0};
    char                            fileName2[300] = {0};
    char                            fileName3[300] = {0};
    char                            fileName4[300] = {0};
    char                            fileName5[300] = {0};
    char                            fileName6[300] = {0};
    double                          *anoise;
    Complex<double>                 *factor;
    double                          *fre;
    double                          *pilotpower;
    Complex<double>                 **out2;
    double                          *sigtx;
    Complex<double>                 *sigtx_IQ;
    Complex<double>                 **sigtx_IQ_dump;
    Complex<double>                 **GW_Tx_MutiAntennaSignal;
    Complex<double>                 **gw_up_in;
    Complex<fxp>                    gw_up_in_fxp;
    double                          *us_in_buff;
    double                          reg_mmse;
    int                             agc_len;
    int                             GWTxPowerFlag;
    int                             RecipracalCaliFlag;
    int                             tone_len;
    int                             block_num;
    Complex<fxp>                    m_sum_fxp;
    Complex<fxp>                    m_mult_fxp;
    double                          m_user_phase[128];
    std::ofstream                   out_iq;
    std::ofstream                   out1;
    std::ofstream                   GW_Tx_out;
    std::ofstream                   mod_out;
    std::ofstream                   file_sig_user_o;
    std::ofstream                   file_mod_in;
    std::ofstream                   file_gw_up_in;
    std::ofstream                   file_hf_o;

    if(argc < 10) {
        LOG2DEV << "Lack Input Parameter" << endl;
        exit(0);
    }
    sscanf(argv[1],  "%d",  &case_num);
    sscanf(argv[2],  "%d",  &model);
    sscanf(argv[3],  "%d",  &user_num);
    sscanf(argv[4],  "%d",  &GWTxPowerFlag);
    sscanf(argv[5],  "%d",  &RecipracalCaliFlag);
    sscanf(argv[6],  "%d",  &reg_total_pow);
    sscanf(argv[7],  "%lf", &reg_pow_diff3);
    sscanf(argv[8],  "%lf", &reg_mmse);
    sscanf(argv[9],  "%d",  &block_num);

    in = NULL;
    fsk = NULL;
    dqpsk = NULL;
    out = NULL;
    us_in_buff = NULL;
    sigtx = NULL;
    anoise = NULL;
    factor = NULL;
    fre = NULL;
    pilotpower = NULL;
    out2 = NULL;
    sigtx_IQ = NULL;
    sigtx_IQ_dump = NULL;
    GW_Tx_MutiAntennaSignal = NULL;
    gw_up_in = NULL;



    anoise = safe_new(anoise , 8);
    factor = safe_new(factor , 8);
    fre = safe_new(fre , user_num);
    pilotpower = safe_new(pilotpower , user_num);
    out2 = safe_new(out2,user_num,8);

    in = safe_new(in, 128, 32768);
    fsk = safe_new(fsk, 32768);
    dqpsk = safe_new(dqpsk, 32768);
    out = safe_new(out , 327680);
    sigtx = safe_new(sigtx , 327680);
    sigtx_IQ = safe_new(sigtx_IQ, 327680);
    sigtx_IQ_dump = safe_new(sigtx_IQ_dump, 128, 327680);
    GW_Tx_MutiAntennaSignal = safe_new(GW_Tx_MutiAntennaSignal , 8 , 327680);
    gw_up_in = safe_new(gw_up_in , 8 , 327680);

    sprintf(num, "%d", case_num);
    sprintf(model_num, "%d", model);

    strcpy(fileName, "/home/data/K2301/tx_bf/c_data/case");strcat(fileName, num);
    strcpy(fileName2, fileName);strcat(fileName2, "/in_2.txt");
    strcpy(fileName3, fileName);strcat(fileName3, "/in_3.txt");
    strcpy(fileName4, fileName);strcat(fileName4, "/in_4.txt");
    strcpy(fileName5, fileName);strcat(fileName5, "/in_5.txt");
    strcpy(fileName6, fileName);strcat(fileName6, "/in_6.txt");
    strcat(fileName, "/in_1.txt");//strcat(fileName, num);


    file_in.open(fileName);
    file_in2.open(fileName2);
    file_in3.open(fileName3);
    file_in4.open(fileName4);
    file_in5.open(fileName5);
    file_in6.open(fileName6);
    if(!file_in)
    {
        cout << "no input file" << endl;
        exit(100);
    }

    sprintf(num, "%d", case_num);
    strcpy(folderName, "./data/");
    if (stat(folderName, &statExist) != 0) {
            statMd = mkdir(folderName, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    strcpy(folderName, "./data/case");
    strcat(folderName, num); strcat(folderName, "/");
    if (stat(folderName, &statExist) != 0) {
            statMd = mkdir(folderName, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }

    fix_set(m_sum_fxp, 12, -15, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sum_fxp, "m_sum_fxp", 1, 1);
    fix_set(m_mult_fxp, 12, -15, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sum_fxp, "m_sum_fxp", 1, 1);
    fix_set(gw_up_in_fxp, 12, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sum_fxp, "m_sum_fxp", 1, 1);

    char filename_mat[300] = {};
    strcpy(filename_mat, folderName); strcat(filename_mat, "out_iq.txt");           out_iq.open(filename_mat);
    strcpy(filename_mat, folderName); strcat(filename_mat, "out1.txt");             out1.open(filename_mat);
    strcpy(filename_mat, folderName); strcat(filename_mat, "GW_Tx_out.txt");        GW_Tx_out.open(filename_mat);
    strcpy(filename_mat, folderName); strcat(filename_mat, "mod_out.txt");          mod_out.open(filename_mat);
    strcpy(filename_mat, folderName); strcat(filename_mat, "sig_user_o.txt");       file_sig_user_o.open(filename_mat);
    strcpy(filename_mat, folderName); strcat(filename_mat, "mod_in.txt");           file_mod_in.open(filename_mat);
    strcpy(filename_mat, folderName); strcat(filename_mat, "up_in.txt");            file_gw_up_in.open(filename_mat);
    strcpy(filename_mat, folderName); strcat(filename_mat, "hf_o.txt");             file_hf_o.open(filename_mat);

    double in_tmp;
    switch (model)
    {
        case 11:
            agc_len = 5120 / (1e6 / 15625 * 2);
            m_encode_cfg.CR = 0.5;
            m_encode_cfg.crc_en = 1;
            m_encode_cfg.len_tx = 28;

            m_mod_cfg.BlockNum = block_num;
            m_mod_cfg.DSSSNum = 4;
            m_mod_cfg.LenSymPhase = 16;
            m_mod_cfg.len_infobit = 224;
            m_mod_cfg.reg_agc_len = agc_len;
            m_mod_cfg.reg_agc_num = 0;
            m_mod_cfg.SymNumData = 112;
            m_mod_cfg.TransRatio = 0.25;

            m_us_cfg.BlockNum = 1;//m_mod_cfg.BlockNum;
            m_us_cfg.data_times = 1;
            m_us_cfg.tx_data_len_cfg = (m_encode_cfg.len_tx * 8 / 4 * 1.0 / m_encode_cfg.CR  + 12) * m_mod_cfg.LenSymPhase *  m_mod_cfg.DSSSNum + agc_len;
            m_us_cfg.tx_samplingrate = 15625;
            break;
        case 12:
            agc_len = 5120 / (1e6 / 31250 * 4);
            m_encode_cfg.CR = 0.5;
            m_encode_cfg.crc_en = 1;
            m_encode_cfg.len_tx = 28;

            m_mod_cfg.BlockNum = block_num;
            m_mod_cfg.DSSSNum = 2;
            m_mod_cfg.LenSymPhase = 16;
            m_mod_cfg.len_infobit = 224;
            m_mod_cfg.reg_agc_len = agc_len;
            m_mod_cfg.reg_agc_num = 0;
            m_mod_cfg.SymNumData = 112;
            m_mod_cfg.TransRatio = 0.25;

            m_us_cfg.BlockNum = 1;//m_mod_cfg.BlockNum;
            m_us_cfg.data_times = 2;
            m_us_cfg.tx_data_len_cfg = (m_encode_cfg.len_tx * 8 / 4 * 1.0 / m_encode_cfg.CR  + 12) * m_mod_cfg.LenSymPhase *  m_mod_cfg.DSSSNum + agc_len;
            m_us_cfg.tx_samplingrate = 31250;
            break;
        case 13:
            agc_len = 5120 / (1e6 / 62500 * 8);
            m_encode_cfg.CR = 0.5;
            m_encode_cfg.crc_en = 1;
            m_encode_cfg.len_tx = 26;

            m_mod_cfg.BlockNum = block_num;
            m_mod_cfg.DSSSNum = 1;
            m_mod_cfg.LenSymPhase = 16;
            m_mod_cfg.len_infobit = 208;
            m_mod_cfg.reg_agc_len = agc_len;
            m_mod_cfg.reg_agc_num = 0;
            m_mod_cfg.SymNumData = 104;
            m_mod_cfg.TransRatio = 0.25;

            m_us_cfg.BlockNum = 1;//m_mod_cfg.BlockNum;
            m_us_cfg.data_times = 3;
            m_us_cfg.tx_data_len_cfg = (m_encode_cfg.len_tx * 8 / 4 * 1.0 / m_encode_cfg.CR  + 12) * m_mod_cfg.LenSymPhase *  m_mod_cfg.DSSSNum + agc_len;
            m_us_cfg.tx_ddc = -85852160;
            m_us_cfg.tx_samplingrate = 62500;
            break;
        case 14:
            agc_len = 3072 / (1e6 / 125000 * 8);
            m_encode_cfg.CR = 0.5;
            m_encode_cfg.crc_en = 1;
            m_encode_cfg.len_tx = 48;

            m_mod_cfg.BlockNum = block_num;
            m_mod_cfg.DSSSNum = 1;
            m_mod_cfg.LenSymPhase = 16;
            m_mod_cfg.len_infobit = 384;
            m_mod_cfg.reg_agc_len = agc_len;
            m_mod_cfg.reg_agc_num = 0;
            m_mod_cfg.SymNumData = 192;
            m_mod_cfg.TransRatio = 0.25;

            m_us_cfg.BlockNum = 1;//m_mod_cfg.BlockNum;
            m_us_cfg.data_times = 3;
            m_us_cfg.tx_data_len_cfg = (m_encode_cfg.len_tx * 8 / 4 * 1.0 / m_encode_cfg.CR  + 12) * m_mod_cfg.LenSymPhase *  m_mod_cfg.DSSSNum + agc_len;
            m_us_cfg.tx_samplingrate = 125000;
            break;
        case 15:
            agc_len = 3072 / (1e6 / 250000 * 8);
            m_encode_cfg.CR = 0.5;
            m_encode_cfg.crc_en = 1;
            m_encode_cfg.len_tx = 48;

            m_mod_cfg.BlockNum = block_num;
            m_mod_cfg.DSSSNum = 1;
            m_mod_cfg.LenSymPhase = 16;
            m_mod_cfg.len_infobit = 384;
            m_mod_cfg.reg_agc_len = agc_len;
            m_mod_cfg.reg_agc_num = 0;
            m_mod_cfg.SymNumData = 192;
            m_mod_cfg.TransRatio = 0.25;

            m_us_cfg.BlockNum = 1;
            m_us_cfg.data_times = 3;
            m_us_cfg.tx_data_len_cfg = (m_encode_cfg.len_tx * 8 / 4 * 1.0 / m_encode_cfg.CR  + 12) * m_mod_cfg.LenSymPhase *  m_mod_cfg.DSSSNum + agc_len;
            m_us_cfg.tx_samplingrate = 250000;
            break;
        case 16:
            agc_len = 2560 / (1e6 / 500000 * 8);
            m_encode_cfg.CR = 0.5;
            m_encode_cfg.crc_en = 1;
            m_encode_cfg.len_tx = 48;

            m_mod_cfg.BlockNum = block_num;
            m_mod_cfg.DSSSNum = 1;
            m_mod_cfg.LenSymPhase = 16;
            m_mod_cfg.len_infobit = 384;
            m_mod_cfg.reg_agc_len = agc_len;
            m_mod_cfg.reg_agc_num = 0;
            m_mod_cfg.SymNumData = 192;
            m_mod_cfg.TransRatio = 0.25;

            m_us_cfg.BlockNum = 1;
            m_us_cfg.data_times = 3;
            m_us_cfg.tx_data_len_cfg = (m_encode_cfg.len_tx * 8 / 4 * 1.0 / m_encode_cfg.CR  + 12) * m_mod_cfg.LenSymPhase *  m_mod_cfg.DSSSNum + agc_len;
            m_us_cfg.tx_samplingrate = 500000;
            break;
        case 17:
            agc_len = 1536 / (1e6 / 500000 * 4);
            m_encode_cfg.CR = 0.5;
            m_encode_cfg.crc_en = 1;
            m_encode_cfg.len_tx = 26;

            m_mod_cfg.BlockNum = block_num;
            m_mod_cfg.DSSSNum = 1;
            m_mod_cfg.LenSymPhase = 16;
            m_mod_cfg.len_infobit = 208;
            m_mod_cfg.reg_agc_len = agc_len;
            m_mod_cfg.reg_agc_num = 0;
            m_mod_cfg.SymNumData = 104;
            m_mod_cfg.TransRatio = 0.25;

            m_us_cfg.BlockNum = 1;//m_mod_cfg.BlockNum;
            m_us_cfg.data_times = 2;
            m_us_cfg.tx_data_len_cfg = (m_encode_cfg.len_tx * 8 / 4 * 1.0 / m_encode_cfg.CR  + 12) * m_mod_cfg.LenSymPhase *  m_mod_cfg.DSSSNum + agc_len;
            m_us_cfg.tx_samplingrate = 500000;
            break;
        case 18:
            agc_len = 1024 / (1e6 / 500000 * 2);
            m_encode_cfg.CR = 0.5;
            m_encode_cfg.crc_en = 1;
            m_encode_cfg.len_tx = 26;

            m_mod_cfg.BlockNum = block_num;
            m_mod_cfg.DSSSNum = 1;
            m_mod_cfg.LenSymPhase = 16;
            m_mod_cfg.len_infobit = 208;
            m_mod_cfg.reg_agc_len = agc_len;
            m_mod_cfg.reg_agc_num = 0;
            m_mod_cfg.SymNumData = 104;
            m_mod_cfg.TransRatio = 0.25;

            m_us_cfg.BlockNum = 1;
            m_us_cfg.data_times = 1;
            m_us_cfg.tx_data_len_cfg = (m_encode_cfg.len_tx * 8 / 4 * 1.0 / m_encode_cfg.CR  + 12) * m_mod_cfg.LenSymPhase *  m_mod_cfg.DSSSNum + agc_len;
            m_us_cfg.tx_samplingrate = 500000;
            break;
        case 19:
            agc_len = 512 / (1e6 / 500000 * 1);
            m_encode_cfg.CR = 0.5;
            m_encode_cfg.crc_en = 1;
            m_encode_cfg.len_tx = 26;

            m_mod_cfg.BlockNum = block_num;
            m_mod_cfg.DSSSNum = 1;
            m_mod_cfg.LenSymPhase = 16;
            m_mod_cfg.len_infobit = 208;
            m_mod_cfg.reg_agc_len = agc_len;
            m_mod_cfg.reg_agc_num = 0;
            m_mod_cfg.SymNumData = 104;
            m_mod_cfg.TransRatio = 0.25;

            m_us_cfg.BlockNum = 1;
            m_us_cfg.data_times = 0;
            m_us_cfg.tx_data_len_cfg = (m_encode_cfg.len_tx * 8 / 4 * 1.0 / m_encode_cfg.CR  + 12) * m_mod_cfg.LenSymPhase *  m_mod_cfg.DSSSNum + agc_len;
            m_us_cfg.tx_samplingrate = 500000;
            break;
        case 20:
            agc_len = 512 / (1e6 / 500000 * 1);
            m_encode_cfg.CR = 0.75;
            m_encode_cfg.crc_en = 1;
            m_encode_cfg.len_tx = 39;

            m_mod_cfg.BlockNum = block_num;
            m_mod_cfg.DSSSNum = 1;
            m_mod_cfg.LenSymPhase = 16;
            m_mod_cfg.len_infobit = 208;
            m_mod_cfg.reg_agc_len = agc_len;
            m_mod_cfg.reg_agc_num = 0;
            m_mod_cfg.SymNumData = 104;
            m_mod_cfg.TransRatio = 0.25;

            m_us_cfg.BlockNum = 1;
            m_us_cfg.data_times = 0;
            m_us_cfg.tx_data_len_cfg = (m_encode_cfg.len_tx * 8 / 4 * 1.0 / m_encode_cfg.CR  + 12) * m_mod_cfg.LenSymPhase *  m_mod_cfg.DSSSNum + agc_len;
            m_us_cfg.tx_samplingrate = 500000;
            break;
    }
    tone_len = 0;
    m_beamforming_cfg.user_num_i = user_num;
    m_beamforming_cfg.reg_total_pow = reg_total_pow;
    m_beamforming_cfg.reg_pow_diff3 = reg_pow_diff3;
    m_beamforming_cfg.GWTxPowerFlag = GWTxPowerFlag;
    m_beamforming_cfg.RecipracalCaliFlag = RecipracalCaliFlag;
    m_beamforming_cfg.reg_mmse = reg_mmse;
    m_tx_beamforming.m_umode_tx = model;
    m_tx_beamforming.configure(m_beamforming_cfg);
    m_tx_beamforming.init(folderName);
    double i_tmp,q_tmp;
    for(int ck0 = 0; ck0 < m_beamforming_cfg.user_num_i;ck0++) {
        for(int ck1 = 0;ck1 < 8;ck1++) {
            file_in2 >> i_tmp;file_in2 >> q_tmp;
            m_tx_beamforming.m_ah_i[ck1][ck0].I = i_tmp;
            m_tx_beamforming.m_ah_i[ck1][ck0].Q = q_tmp;
            //LOG2DEV<< m_tx_beamforming.m_ah_i[ck1][ck0].I << "\t" << m_tx_beamforming.m_ah_i[ck1][ck0].Q << endl;
        }
        file_in5 >> in_tmp; m_tx_beamforming.m_freq_off[ck0] = in_tmp;
        //LOG2DEV << m_tx_beamforming.m_freq_off[ck0] << endl;
        file_in6 >> in_tmp; m_tx_beamforming.m_pilot_power_i[ck0] = in_tmp;
        m_tx_beamforming.m_txdata_power[ck0] = 50;
    }

    for(int ck0 = 0;ck0 < 8;ck0++) {
        file_in3 >> in_tmp;
        m_tx_beamforming.m_anoise_sqrt_i[ck0] = sqrt(in_tmp);

        file_in4 >> i_tmp;file_in4 >> q_tmp;
        m_tx_beamforming.m_cali_factor[ck0].I = i_tmp;
        m_tx_beamforming.m_cali_factor[ck0].Q = q_tmp;
    }

	m_tx_beamforming.go();

	m_tx_encode.configure(m_encode_cfg);
	m_tx_encode.init(folderName);
    m_tx_mod.config(m_mod_cfg);
    m_tx_mod.init(folderName);
    m_us_cfg.tx_data_agc_cfg = 0;
    m_tx_us.config(m_us_cfg);
    m_tx_us.init(folderName);

    for(int ck2 = 0; ck2 < m_mod_cfg.BlockNum; ck2++) {
        for (int ck0 = 0; ck0 < m_beamforming_cfg.user_num_i; ck0++) {
            for (int ck1 = 0; ck1 < m_encode_cfg.len_tx * 8; ck1++) {
                file_in >> in_tmp;
                in[ck0][ck1 + m_encode_cfg.len_tx * 8 * ck2] = in_tmp;
                //LOG2DEV << ck1 + m_encode_cfg.len_tx * 8 * ck2 << "\t" << in_tmp<< endl;
                //if(ck0 == 1)
                    //LOG2DEV << in_tmp << endl;
            }
        }
    }

    double us_in;
    int     cnt_us_in = 0;
    int     cnt_us_out = 0;
    int     us_output_len = 0;
    int     m_block_start = 0;
    Complex<double>  sum_tmp = 0;
    int     *in_user_tmp;
    in_user_tmp = NULL;
    in_user_tmp = safe_new(in_user_tmp, m_encode_cfg.len_tx * 8);
    us_in_buff = safe_new(us_in_buff , m_us_cfg.tx_data_len_cfg);
    for(int ck1 = 0;ck1 < m_mod_cfg.BlockNum; ck1++) {
        if(ck1 > 0) {
            m_tx_mod.m_agc_len = 0;
        }

        if(ck1 == m_mod_cfg.BlockNum - 1) {
            m_tx_mod.m_tone_len = tone_len;
        } else {
            m_tx_mod.m_tone_len = 0;
        }

        for(int ck0 = 0 ; ck0 < m_beamforming_cfg.user_num_i;ck0++)
        {
            for (int ck2 = 0; ck2 < m_encode_cfg.len_tx * 8; ck2++) {
                in_user_tmp[ck2] = in[ck0][ck2 + ck1 * m_encode_cfg.len_tx * 8];
                file_mod_in << in_user_tmp[ck2] << endl;
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
                    mod_out << us_in_buff[cnt_us_in] << endl;
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
                    out_iq << sigtx_IQ[cnt_us_out].I<< "\t" << sigtx_IQ[cnt_us_out].Q <<endl;
                    out1 << m_tx_us.m_out <<endl;
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

    for(int ck0 = 0;ck0 < 8 ;ck0++) {
        for(int ck1 = 0; ck1 < m_block_start; ck1++) {
            GW_Tx_out << GW_Tx_MutiAntennaSignal[ck0][ck1].I << "\t" << GW_Tx_MutiAntennaSignal[ck0][ck1].Q <<endl;
            //LOG2DEV << m_tx_beamforming.m_beamforming_matrix_tx_o[ck0][ck1].I << "\t" << m_tx_beamforming.m_beamforming_matrix_tx_o[ck0][ck1].Q <<endl;
        }
    }
    m_tx_us_cfg.tx_len_cfg = m_block_start;
    m_tx_us_cfg.osr_cfg = 2;
    m_tx_us_cfg.osr = 1;
    m_tx_us_h.config(m_tx_us_cfg);
    m_tx_us_h.init(2);

    shift = shift_cal(GW_Tx_MutiAntennaSignal, 8, m_tx_us.m_output_len);

    for(int ck0 = 0;ck0 < 8 ;ck0++) {
        for(int ck1 = 0; ck1 < m_block_start; ck1++) {
            gw_up_in_fxp = GW_Tx_MutiAntennaSignal[ck0][ck1] * DOUBLE_POW2[64 - shift];
            gw_up_in[ck0][ck1] = gw_up_in_fxp;
            file_gw_up_in << gw_up_in[ck0][ck1].I << "\t" << gw_up_in[ck0][ck1].Q << endl;
        }
    }
    //int cnt = 0;
    for(int ck0 = 0;ck0 < 8 ;ck0++) {
        for(int ck1 = 0; ck1 < m_block_start; ck1++) {
            m_tx_us_h.m_i_in_en = 1;
            m_tx_us_h.go(gw_up_in[ck0][ck1]);
            file_hf_o<< m_tx_us_h.m_out.I << "\t" << m_tx_us_h.m_out.Q << endl;
        }
        //cnt = 0;
        while(!m_tx_us_h.m_finished_o) {
            m_tx_us_h.m_i_in_en = 0;
            m_tx_us_h.go(0);
            file_hf_o << m_tx_us_h.m_out.I << "\t" << m_tx_us_h.m_out.Q << endl;
            //LOG2DEV<<cnt++<<endl;
        }
        m_tx_us_h.reset(2);
    }

    for(int ck0 = 0; ck0 < m_beamforming_cfg.user_num_i; ck0++) {
        for(int ck1 = 0; ck1 < m_block_start; ck1++) {
            file_sig_user_o << sigtx_IQ_dump[ck0][ck1].I << "\t" << sigtx_IQ_dump[ck0][ck1].Q << endl;
        }
    }

    free(in);
    free(dqpsk);
    free(fsk);
    free(out);
    free(sigtx);
    free(anoise);
    free(factor);
    free(fre);
    free(pilotpower);
    free(out2);
    free(sigtx_IQ);
    free(GW_Tx_MutiAntennaSignal);
    free(us_in_buff);
    free(in_user_tmp);
    free(sigtx_IQ_dump);
    free(gw_up_in);
    file_in.close();
    file_in2.close();
    file_in3.close();
    file_in4.close();
    file_in5.close();
    file_in6.close();
    file_sig_user_o.close();
    out_iq.close();
    out1.close();
    GW_Tx_out.close();
    mod_out.close();
    file_mod_in.close();
    file_gw_up_in.close();
    file_hf_o.close();
    cout<< "Finished"<<std::endl;
   	return 0;
}
