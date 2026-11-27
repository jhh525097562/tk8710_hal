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
#include "../Cls_bf.h"
#include "../Cls_tx_encode.h"

using namespace std;

int main(int argc, char *argv[])
{

    Cls_tx_encode<fxp>             m_tx;
    TX_ENCODE_CFG                  m_cfg;
    int						case_num;

    int                    	statMd = 0;
    int                     len_o;
    int                     *in;
    int                     *dqpsk;
    int                     *fsk;
    struct 					stat statExist;
    ifstream                file_in;
    ofstream                file_out;
	char 					num[10] = {0};
    char            		folderName[300] = {0};
    char                    fileName[300] = {0};
    double                  CR;
    int                     crc_en;
    int                     len_tx;

    in = NULL;
    fsk = NULL;
    dqpsk = NULL;
    in = safe_new(in, 32768);
    fsk = safe_new(fsk, 32768);
    dqpsk = safe_new(dqpsk, 32768);


    sscanf(argv[1],  "%d", &case_num);
    sscanf(argv[2],  "%f", &CR);
    sscanf(argv[3],  "%d", &crc_en);
    sscanf(argv[4],  "%d", &len_tx);

    strcpy(fileName, "./c_data/in_");
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

    int in_tmp;
    m_cfg.CR = CR;
    m_cfg.crc_en = crc_en;
    m_cfg.len_tx = len_tx;
    for (int ck0 = 0; ck0 < m_cfg.len_tx * 8; ck0++)
    {
        file_in >> in_tmp;
        in[ck0] = in_tmp;
    }

	m_tx.configure(m_cfg);
	m_tx.init(folderName);
	m_tx.go(in, dqpsk, fsk);


    free(in);
    free(dqpsk);
    free(fsk);


    cout<< "Finished"<<std::endl;
    cout<<"hello word"<<endl;

    system("pause");
   	return 0;
}
