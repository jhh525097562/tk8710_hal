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

#include "../Cls_acm_tx.h"

int main(int argc, char *argv[])
{
    Cls_acm_tx<fxp>         m_acm_tx;

    int						case_num;
    int                     tone_switch;
    int                     freq;
    int                     phase;
    int                     fft_len;
    int                     gain;
    int                     channel_choose;
    int                     rx_fft_len;
    int                     antenna_num;
    int                     rx_acm_num;
    int                     tx1_rx8;

    char                    num[10] = {0};
    ACM_TX_CFG              m_cfg;

    char     fileName[300] = {0};
    sscanf(argv[1],  "%d", &case_num);
    sscanf(argv[2],  "%x", &tone_switch);
    sscanf(argv[3],  "%d", &freq);    m_cfg.reg_freq[0] = freq;
    sscanf(argv[4],  "%d", &freq);    m_cfg.reg_freq[1] = freq;
    sscanf(argv[5],  "%d", &freq);    m_cfg.reg_freq[2] = freq;
    sscanf(argv[6],  "%d", &freq);    m_cfg.reg_freq[3] = freq;
    sscanf(argv[7],  "%d", &freq);    m_cfg.reg_freq[4] = freq;
    sscanf(argv[8],  "%d", &freq);    m_cfg.reg_freq[5] = freq;
    sscanf(argv[9],  "%d", &freq);    m_cfg.reg_freq[6] = freq;
    sscanf(argv[10],  "%d", &freq);   m_cfg.reg_freq[7] = freq;
    sscanf(argv[11],  "%d", &freq);   m_cfg.reg_freq[8] = freq;
    sscanf(argv[12],  "%d", &freq);   m_cfg.reg_freq[9] = freq;
    sscanf(argv[13],  "%d", &freq);   m_cfg.reg_freq[10] = freq;
    sscanf(argv[14],  "%d", &freq);   m_cfg.reg_freq[11] = freq;
    sscanf(argv[15],  "%d", &freq);   m_cfg.reg_freq[12] = freq;
    sscanf(argv[16],  "%d", &freq);   m_cfg.reg_freq[13] = freq;
    sscanf(argv[17],  "%d", &freq);   m_cfg.reg_freq[14] = freq;
    sscanf(argv[18],  "%d", &freq);   m_cfg.reg_freq[15] = freq;
    sscanf(argv[19],  "%d", &phase);  m_cfg.reg_phase[0] = phase;
    sscanf(argv[20],  "%d", &phase);  m_cfg.reg_phase[1] = phase;
    sscanf(argv[21],  "%d", &phase);  m_cfg.reg_phase[2] = phase;
    sscanf(argv[22],  "%d", &phase);  m_cfg.reg_phase[3] = phase;
    sscanf(argv[23],  "%d", &phase);  m_cfg.reg_phase[4] = phase;
    sscanf(argv[24],  "%d", &phase);  m_cfg.reg_phase[5] = phase;
    sscanf(argv[25],  "%d", &phase);  m_cfg.reg_phase[6] = phase;
    sscanf(argv[26],  "%d", &phase);  m_cfg.reg_phase[7] = phase;
    sscanf(argv[27],  "%d", &phase);  m_cfg.reg_phase[8] = phase;
    sscanf(argv[28],  "%d", &phase);  m_cfg.reg_phase[9] = phase;
    sscanf(argv[29],  "%d", &phase);  m_cfg.reg_phase[10] = phase;
    sscanf(argv[30],  "%d", &phase);  m_cfg.reg_phase[11] = phase;
    sscanf(argv[31],  "%d", &phase);  m_cfg.reg_phase[12] = phase;
    sscanf(argv[32],  "%d", &phase);  m_cfg.reg_phase[13] = phase;
    sscanf(argv[33],  "%d", &phase);  m_cfg.reg_phase[14] = phase;
    sscanf(argv[34],  "%d", &phase);  m_cfg.reg_phase[15] = phase;
    sscanf(argv[35],  "%d", &fft_len);m_cfg.reg_rx_fft_len = fft_len;
    sscanf(argv[36],  "%d", &channel_choose);   m_cfg.reg_channel_choose = channel_choose;
    sscanf(argv[37],  "%d", &rx_fft_len);   m_cfg.reg_rx_fft_len_t = rx_fft_len;
    sscanf(argv[38],  "%d", &antenna_num);   m_cfg.reg_antenna_num[0] = antenna_num;
    sscanf(argv[39],  "%d", &antenna_num);   m_cfg.reg_antenna_num[1] = antenna_num;
    sscanf(argv[40],  "%d", &antenna_num);   m_cfg.reg_antenna_num[2] = antenna_num;
    sscanf(argv[41],  "%d", &antenna_num);   m_cfg.reg_antenna_num[3] = antenna_num;
    sscanf(argv[42],  "%d", &antenna_num);   m_cfg.reg_antenna_num[4] = antenna_num;
    sscanf(argv[43],  "%d", &antenna_num);   m_cfg.reg_antenna_num[5] = antenna_num;
    sscanf(argv[44],  "%d", &antenna_num);   m_cfg.reg_antenna_num[6] = antenna_num;
    sscanf(argv[45],  "%d", &antenna_num);   m_cfg.reg_antenna_num[7] = antenna_num;
    sscanf(argv[46],  "%d", &rx_acm_num);   m_cfg.reg_rx_acm_num = rx_acm_num;
    sscanf(argv[47],  "%d", &tx1_rx8);      m_cfg.debug_tx1_rx8 = tx1_rx8;
    sscanf(argv[48],  "%d", &gain);         m_cfg.reg_gain[0] = gain;
    sscanf(argv[49],  "%d", &gain);         m_cfg.reg_gain[1] = gain;
    sscanf(argv[50],  "%d", &gain);         m_cfg.reg_gain[2] = gain;
    sscanf(argv[51],  "%d", &gain);         m_cfg.reg_gain[3] = gain;
    sscanf(argv[52],  "%d", &gain);         m_cfg.reg_gain[4] = gain;
    sscanf(argv[53],  "%d", &gain);         m_cfg.reg_gain[5] = gain;
    sscanf(argv[54],  "%d", &gain);         m_cfg.reg_gain[6] = gain;
    sscanf(argv[55],  "%d", &gain);         m_cfg.reg_gain[7] = gain;
    sscanf(argv[56],  "%d", &gain);         m_cfg.reg_gain[8] = gain;
    sscanf(argv[57],  "%d", &gain);         m_cfg.reg_gain[9] = gain;
    sscanf(argv[58],  "%d", &gain);         m_cfg.reg_gain[10] = gain;
    sscanf(argv[59],  "%d", &gain);         m_cfg.reg_gain[11] = gain;
    sscanf(argv[60],  "%d", &gain);         m_cfg.reg_gain[12] = gain;
    sscanf(argv[61],  "%d", &gain);         m_cfg.reg_gain[13] = gain;
    sscanf(argv[62],  "%d", &gain);         m_cfg.reg_gain[14] = gain;
    sscanf(argv[63],  "%d", &gain);         m_cfg.reg_gain[15] = gain;
    sprintf(num, "%d", case_num);
    strcat(fileName,num);
    strcat(fileName,"txt");
    char    folderName[300] = "../data/";
    struct  stat statExist;
    int     statMd = 0;


        if (stat(folderName, &statExist) != 0) {
            statMd = mkdir(folderName,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
   char    folderName_2[300] = {"../data/case"};

    strcat(folderName_2, num); strcat(folderName_2, "/");
    if (stat(folderName_2, &statExist) != 0) {
            statMd = mkdir(folderName_2,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    m_cfg.reg_sampling_rate = 0;
    m_cfg.reg_tone_switch = tone_switch;
    m_acm_tx.configure(m_cfg);
    m_acm_tx.init(folderName_2);
    m_acm_tx.go();
    std::cout<< "Finished"<<std::endl;
   	return 0;
}
