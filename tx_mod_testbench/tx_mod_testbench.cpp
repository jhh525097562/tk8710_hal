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
#include "iostream"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include "sys/stat.h"
#include "../Cls_tx_mod.h"

int main(int argc, char *argv[])
{

    Cls_tx_mod<fxp>      m_tx;
    TX_MOD_CFG              m_cfg;
    int						case_num;
    double                  TransRatio;

    int                    	statMd = 0;
    int                     in_fsk[528];
    int                     in_dqpsk[528];
    double                  *out;
    struct 					stat statExist;
    ifstream                file_in;
    ofstream                file_out;
	char 					num[10] = {0};
    char            		folderName[300] = {0};
    char                    fileName[300] = {0};
    out = NULL;
//    in_fsk = NULL;
//    in_dqpsk = NULL;
    out = safe_new(out, 16384);
//    in_fsk = safe_new(in_fsk, 66*8);
//    in_dqpsk = safe_new(in_dqpsk, 66*8);

    sscanf(argv[1],  "%d", &case_num);
    sscanf(argv[2],  "%d", &m_cfg.BlockNum);
    sscanf(argv[3],  "%d", &m_cfg.DSSSNum);
    sscanf(argv[4],  "%d", &m_cfg.len_infobit);
    sscanf(argv[5],  "%d", &m_cfg.LenSymPhase);
    sscanf(argv[6],  "%d", &m_cfg.SymNumData);
    sscanf(argv[7],  "%lf", &TransRatio);
    sscanf(argv[8],  "%d", &m_cfg.reg_agc_len);
    sscanf(argv[9],  "%d", &m_cfg.reg_agc_num);

    strcpy(fileName, "/home/data/K2301/tx_mod/c_data/in_");
    sprintf(num, "%d", case_num);
    strcat(fileName, num);
    strcat(fileName, ".txt");

    file_in.open(fileName);
    if(!file_in)
    {
        cout << "no input file" << endl;
        exit(100);
    }

    sprintf(num, "%d", case_num);
    strcpy(folderName, "./data/");
    if (stat(folderName, &statExist) != 0) {
            statMd = mkdir(folderName,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    strcpy(folderName, "./data/case");
    strcat(folderName, num); strcat(folderName, "/");
    if (stat(folderName, &statExist) != 0) {
            statMd = mkdir(folderName,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }

    // m_cfg.BlockNum = 1;
    // m_cfg.DSSSNum = 2;
    // m_cfg.len_infobit = 224;
    // m_cfg.LenSymPhase = 16;
//    m_cfg.reg_agc_len = 0;
//    m_cfg.reg_agc_num = 0;
    // m_cfg.SymNumData = 112;
    m_cfg.TransRatio = TransRatio;

    m_tx.m_iblock = 0;
	m_tx.config(m_cfg);
	m_tx.init(folderName);
    for (int i = 0; i < m_cfg.BlockNum; i++)
    {
        double tmp_in;
        for (int ck0 = 0; ck0 < m_cfg.len_infobit * 2; ck0++)
        {   file_in >> tmp_in;
//            LOG2DEV << tmp_in;
            if(ck0 < m_cfg.len_infobit)
                in_fsk[ck0] = tmp_in;
            else
                in_dqpsk[ck0 - m_cfg.len_infobit] = tmp_in;
        }

        m_tx.m_iblock = i;
	    m_tx.go(in_fsk, in_dqpsk,out);
    }

	// m_tx.go(in_fsk, in_dqpsk,out);

    std::cout<< "Finished"<<std::endl;
    free(out);
//    free(in_fsk);
//    free(in_dqpsk);
    file_in.close();
//    system("pause");
   	return 0;


}
