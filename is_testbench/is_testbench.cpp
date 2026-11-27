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
#include "../Cls_GWRxBFModule.h"

int main(int argc, char *argv[])
{
    Cls_GWRxBFModule<fxp>   m_tx;
    GWRXBFMODULE_CFG        m_cfg;
    int						case_num;

    int                    	statMd = 0;
    double                  in_I;
    double                  in_Q;
    Complex<double>         **in1;
    Complex<double>         *in2;
    Complex<double>         *out;
    struct 					stat statExist;
	char 					num[10] = {0};
    char            		folderName[300] = {0};
    char                    fileName[300] = {0};
    char                    fileName2[300] = {0};
    ifstream                file_in;
    ifstream                file_in1;
    double                  freqOffsetCh;
    int                     Antenna;
    int                     FFTLen;
    int                     mode;
    int                     filterLen;
    int                     ifftLen;
    int                     GWRxSampleRate;
    int                     GWRXBFMode;
    int                     HCE;

    out = NULL;
    out = safe_new(out, 65536 * 10);
    in1 = NULL;
    in1 = safe_new(in1, 8, 8193);
    in2 = NULL;
    in2 = safe_new(in2, 8);
    sscanf(argv[1],  "%d" , &case_num);
    sscanf(argv[2],  "%lf", &freqOffsetCh);
    sscanf(argv[3],  "%d" , &Antenna);
    sscanf(argv[4],  "%d" , &FFTLen);
    sscanf(argv[5],  "%d" , &mode);
    sscanf(argv[6],  "%d" , &filterLen);
    sscanf(argv[7],  "%d" , &ifftLen);
    sscanf(argv[8],  "%d" , &GWRxSampleRate);
    sscanf(argv[9],  "%d" , &GWRXBFMode);
    sscanf(argv[10], "%d" , &HCE);

    sprintf(num, "%d", case_num);
    strcpy(fileName, "/home/data/K2301/is_testbench/c_data/case");  strcat(fileName, num);
    strcpy(fileName2, fileName);strcat(fileName2, "/in_1.txt");     file_in.open(fileName2);
    strcpy(fileName2, fileName);strcat(fileName2, "/in_2.txt");     file_in1.open(fileName2);
    strcat(fileName, num);
    strcat(fileName, "_1.txt");

    if(!file_in)
    {
        cout << "no in_1.txt" << endl;
        exit(100);
    }

    if(!file_in1)
    {
        cout << "no in_1.txt" << endl;
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

    m_cfg.freqOffsetCh = freqOffsetCh;
    m_cfg.Antenna = Antenna;
    m_cfg.FFTLen = FFTLen;
    m_cfg.mode = mode;
    m_cfg.filterLen = filterLen;
    m_cfg.ifftLen = ifftLen;
    m_cfg.GWRxSampleRate = GWRxSampleRate;
    m_cfg.GWRXBFMode = GWRXBFMode;
    m_cfg.HCE = HCE;

    m_tx.configure(m_cfg);
    m_tx.init(folderName);
    for(int ck0 = 0; ck0 < FFTLen; ck0++)
    {
        file_in >> in_I >> in_Q;
        in1[0][ck0].I = in_I;in1[0][ck0].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[1][ck0].I = in_I;in1[1][ck0].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[2][ck0].I = in_I;in1[2][ck0].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[3][ck0].I = in_I;in1[3][ck0].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[4][ck0].I = in_I;in1[4][ck0].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[5][ck0].I = in_I;in1[5][ck0].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[6][ck0].I = in_I;in1[6][ck0].Q = in_Q;
        file_in >> in_I >> in_Q;
        in1[7][ck0].I = in_I;in1[7][ck0].Q = in_Q;
        m_tx.m_in[0][ck0] = in1[0][ck0];
        m_tx.m_in[1][ck0] = in1[1][ck0];
        m_tx.m_in[2][ck0] = in1[2][ck0];
        m_tx.m_in[3][ck0] = in1[3][ck0];
        m_tx.m_in[4][ck0] = in1[4][ck0];
        m_tx.m_in[5][ck0] = in1[5][ck0];
        m_tx.m_in[6][ck0] = in1[6][ck0];
        m_tx.m_in[7][ck0] = in1[7][ck0];
    }

    for(int ck0 = 0; ck0 < 8; ck0++)
    {
        file_in1 >> in_I >> in_Q;
        in2[ck0].I = in_I;in2[ck0].Q = in_Q;
        m_tx.m_AH[ck0] = in2[ck0];
    }

    m_tx.go();

    free(out);
    file_in.close();
    file_in1.close();
    free(in1);
    free(in2);
    std::cout<< "Finished"<<std::endl;
   	return 0;
}
