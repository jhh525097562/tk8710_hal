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
**
**  Notice: 主函数入口
****************************************************************************/



using namespace std;
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include "sys/stat.h"
#include "../Cls_tx_beamforming.h"

int main(int argc, char *argv[])
{

    Cls_tx_beamforming<double> m_tx;
    TX_BEAMFORMING_CFG      m_cfg;
    int						case_num;

    int                    	statMd = 0;
    struct 					stat statExist;
    ifstream                file_in;
    ifstream                file_in1;
    ifstream                file_in2;
    ifstream                file_in3;
    ifstream                file_in4;
    ifstream                file_in5;
    ifstream                file_in6;
    ofstream                file_out;
	char 					num[10] = {0};
    char            		folderName[300] = {0};
    char                    fileName[300] = {0};
    char                    fileName1[300] = {0};
    char                    fileName2[300] = {0};
    char                    fileName3[300] = {0};
    char                    fileName4[300] = {0};
    char                    fileName5[300] = {0};
    char                    fileName6[300] = {0};
    int                     user_num_i;
    int                     *umode_tx;
    Complex<double>         **ah_i;
    double                  *anoise_i;
    Complex<double>         *cali_factor;
    double                  *freq_off;
    double                  *pilot_power_i;
    //Complex<double>         **beamforming_matrix_tx_o;


    umode_tx = NULL;
    ah_i = NULL;
    anoise_i = NULL;
    cali_factor = NULL;
    freq_off = NULL;
    pilot_power_i = NULL;
    //beamforming_matrix_tx_o = NULL;

    sscanf(argv[1],  "%d", &case_num);

    strcpy(fileName, "/home/data/K2301/tx_bf/c_data/case");
    sprintf(num, "%d", case_num);
    strcat(fileName, num);
    strcat(fileName1, fileName);
    strcat(fileName2, fileName);
    strcat(fileName3, fileName);
    strcat(fileName4, fileName);
    strcat(fileName5, fileName);
    strcat(fileName6, fileName);

    strcat(fileName, "/in_0.txt");
    strcat(fileName1, "/in_1.txt");
    strcat(fileName2, "/in_2.txt");
    strcat(fileName3, "/in_3.txt");
    strcat(fileName4, "/in_4.txt");
    strcat(fileName5, "/in_5.txt");
    strcat(fileName6, "/in_6.txt");

    file_in.open(fileName);
    file_in1.open(fileName1);
    file_in2.open(fileName2);
    file_in3.open(fileName3);
    file_in4.open(fileName4);
    file_in5.open(fileName5);
    file_in6.open(fileName6);
    if(!file_in && !file_in1 && !file_in2 && !file_in3 && !file_in4 && !file_in5 && !file_in6 )
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

    user_num_i = 48;
    int int_tmp ;
    double tmp,tmp_I,tmp_Q;
    umode_tx = safe_new(umode_tx, user_num_i);
    ah_i = safe_new(ah_i, 8, user_num_i);
    anoise_i = safe_new(anoise_i, 8);
    cali_factor = safe_new(cali_factor, 8);
    freq_off = safe_new(freq_off, user_num_i);
    pilot_power_i = safe_new(pilot_power_i, user_num_i);
    for(int ck0 = 0; ck0 < user_num_i; ck0++)
    {
        file_in >> int_tmp ; umode_tx[ck0] = int_tmp;
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            file_in1 >> tmp_I >> tmp_Q ;
            ah_i[ck1][ck0].I = tmp_I; ah_i[ck1][ck0].Q = tmp_Q;
        }
        file_in4 >> tmp ; freq_off[ck0] = tmp;
        file_in5 >> tmp ; pilot_power_i[ck0] = tmp;
    }
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        file_in2 >> tmp ; anoise_i[ck0] = tmp;
        file_in3 >> tmp_I >> tmp_Q ; cali_factor[ck0].I = tmp_I;cali_factor[ck0].Q = tmp_Q;
    }

    for (int ck0 = 0; ck0 < 48*8*8; ck0++)
    {
        file_in6 >> tmp_I >> tmp_Q ;
        m_tx.tmp_use[ck0].I = tmp_I;
        m_tx.tmp_use[ck0].Q = tmp_Q;
    }

    m_cfg.user_num_i = user_num_i;
    m_cfg.GWTxAHSelectFlag = 0;
    m_cfg.GWTxPowerFlag = 2;
    m_cfg.RecipracalCaliFlag = 1;
	m_tx.configure(m_cfg);
	m_tx.init(folderName);
	m_tx.go();//umode_tx, ah_i, anoise_i, cali_factor, freq_off, pilot_power_i, beamforming_matrix_tx_o);


    free(umode_tx);
    free(ah_i);
    file_in.close();
    file_in1.close();
    file_in2.close();
    file_in3.close();
    file_in4.close();
    file_in5.close();
    file_in6.close();
    std::cout<< "Finished"<<std::endl;
   	return 0;
}
