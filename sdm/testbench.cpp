/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File          : Downsampling.cpp (main program to run simulation)
**  Created By    : Darren
**  Created On    : 08/01/2024
**
**  History     :
**  Programmer      Date            Rev         Description
**  Darren          08/01/2023      V1.0        Initial
**  --------------- ---------- -------- ------------------------------
**
**  Notice:
****************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include "../Cls_sdm_tx.h"
using namespace std;

int main(int argc, char *argv[])
{
    Cls_sdm_tx<fxp>                 m_sdm_tx;
    double                          m_data_in;
    Complex<double>                 m_in;
    int                             case_num;
    struct                          stat statExist;
    int                             statMd = 0;
	ifstream                        file_in;
	ofstream                        file_out;
    char                            fileName[300] = { 0 };
    char                            num[10] = {0};
    char                            folderName[300] = {0};

    sscanf(argv[1], "%d" , &case_num);
    strcpy(fileName, "/home/data/K2301/tx_sdm/in_");
    sprintf(num,"%d",case_num);
    strcat(fileName, num);
    strcat(fileName, ".txt");

	file_in.open(fileName);
    if (!file_in)
	{
		cout << "no input file" << endl;
		exit(100);
	}

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

    m_sdm_tx.init(folderName);

    while(!file_in.eof())
    {
        file_in >> m_data_in;
        m_in.I = m_data_in;
        file_in >> m_data_in;
        m_in.Q = m_data_in;
        m_sdm_tx.go(m_in);
        //LOG2DEV<<m_data_in<<endl;
    }
    cout << "Finished" << endl;
}
