/****************************************************************************
**
**  Copyright  (c)  2023-2025,  All Rights Reserved.
**
**  File          : svd_testbench.cpp (main program to run simulation)
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
#include "../Cls_svd_tx.h"

int main(int argc, char *argv[])
{
    Cls_svd<fxp >           m_svd;
    int						case_num;
    Complex<double>         **in;
    int                     ant_num;
    double                  I;
    double                  Q;
    SVD_CFG                 cfg;
    int                    	statMd = 0;
    struct 					stat statExist;
    ifstream                file_in;
    ofstream                file_out;
	char 					num[10] = {0};
    char            		folderName[300] = {0};
    char                    fileName[300] = {0};

    in = NULL;
    in = safe_new(in, 8, 8);

    sscanf(argv[1],  "%d", &case_num);
    sscanf(argv[2],  "%d", &ant_num);

    strcpy(fileName, "/home/data/K2301/svd/c_data/in_");//strcpy(fileName, "/home/data/K2301/svd/c_data/in_"); //strcpy(fileName, "./c_data/in_");
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

    for (int ck0 = 0; ck0 < ant_num; ck0++)
    {
        for (int ck1 = 0; ck1 < ant_num; ck1++)
        {
            file_in >> I >> Q;
            in[ck0][ck1].I = I;
            in[ck0][ck1].Q = Q;
        }
    }

    //cfg.ant_num = 8;
    m_svd.configure(cfg);
    m_svd.init(folderName);
    m_svd.go(in);
    std::cout<< "V2.3(fix) sqrt(all) Finished"<<std::endl;
    free(in);
    //system("pause");
   	return 0;
}
