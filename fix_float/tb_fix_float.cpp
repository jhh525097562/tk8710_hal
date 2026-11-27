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
#include "../Cls_float_fix.h"

int main(int argc, char *argv[])
{
    Cls_float_fix<fxp>      m_fix_float;
    int						case_num;
    double                  in_tmp;
    Complex<double>         in;
    int                    	statMd = 0;
    struct 					stat statExist;
    ifstream                file_in;
    ofstream                file_out;
	char 					num[10] = {0};
    char            		folderName[300] = {0};
    char                    fileName[300] = {0};

    sscanf(argv[1],  "%d", &case_num);

    strcpy(fileName, "/home/data/K2301/float_fix/c_data/in_");
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
    strcpy(folderName, "../data/");
    if (stat(folderName, &statExist) != 0) {
            statMd = mkdir(folderName,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    strcpy(folderName, "../data/case");
    strcat(folderName, num); strcat(folderName, "/");
    if (stat(folderName, &statExist) != 0) {
            statMd = mkdir(folderName,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }

	m_fix_float.init(folderName);

    while(!file_in.eof()) {
        file_in >> in_tmp ;
        in.I = in_tmp;
        //file_in >> in_tmp ;
        //in.Q = in_tmp;
        m_fix_float.go(in);
    }

    std::cout<< "Finished"<<std::endl;
    file_in.close();
   	return 0;
}
