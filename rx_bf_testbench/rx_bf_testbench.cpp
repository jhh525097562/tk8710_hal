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
#include "../Cls_bf.h"

int main(int argc, char *argv[])
{
    Cls_bf<fxp>             m_bf;
    int						case_num;
    double                  I;
    double                  Q;
    BF_CFG                  cfg;
    int                    	statMd = 0;
    struct 					stat statExist;
    ifstream                file_in;
    ifstream                file_in1;
    ifstream                file_in2;
	char 					num[10] = {0};
    char            		folderName[300] = {0};
    char                    fileName1[300] = {0};
    char                    fileName2[300] = {0};
    int                     ant_num     ;
    int                     fft_len     ;
    int                     ifft_len    ;
    int                     rx_d_win_i  ;
    int                     gw_rxbf_mode;
    int                     DSSS        ;
    double                  *ifft_in;
    Complex<double>         **ifft_in_buff;
    double                  *an_in;
    Complex<double>         **an_in_buff;
    double                  *po;
    double                  **po_buff;
    double                  *eo;
    double                  **eo_buff;
    double                  *BfMatrix;
    int                     slide_num;
    int                     user_num;
    int                     th;
    double                  alpha;
    int                     slidedatanum;

    sscanf(argv[1],  "%d", &case_num);
    sscanf(argv[2],  "%d", &ant_num);
    sscanf(argv[3],  "%d", &fft_len);
    sscanf(argv[4],  "%d", &ifft_len);
    sscanf(argv[5],  "%d", &rx_d_win_i);
    sscanf(argv[6],  "%d", &gw_rxbf_mode);
    sscanf(argv[7],  "%d", &DSSS);
    sscanf(argv[8],  "%d", &th);
    sscanf(argv[9],  "%lf", &alpha);
    sscanf(argv[10],  "%d", &slidedatanum);

    slide_num = 1;
    user_num = 1;
    cfg.ant_num = ant_num;
    cfg.fft_len = fft_len;
    cfg.ifft_len = ifft_len;
    cfg.rx_d_win_i = rx_d_win_i;
    cfg.gw_rxbf_mode = gw_rxbf_mode;
    cfg.DSSS = DSSS;
    cfg.slidedatanum = slidedatanum;
    cfg.bf_alpha = alpha;
    cfg.bf_th = th;

    ifft_in = NULL;
    ifft_in_buff = NULL;
    an_in = NULL;
    an_in_buff = NULL;
    po = NULL;
    po_buff = NULL;
    eo = NULL;
    eo_buff = NULL;
    BfMatrix = NULL;
    ifft_in = safe_new(ifft_in, ant_num * ifft_len * 2);
    ifft_in_buff = safe_new(ifft_in_buff, ant_num,slide_num * user_num * ifft_len * 2);
    an_in = safe_new(an_in, ant_num * 2);
    an_in_buff = safe_new(an_in_buff, ant_num,slide_num * user_num * 2);
    po = safe_new(po, ant_num );
    po_buff = safe_new(po_buff, ant_num,user_num );
    eo = safe_new(eo, ant_num );
    eo_buff = safe_new(eo_buff, ant_num,user_num );
    BfMatrix = safe_new(BfMatrix, ant_num );

    strcpy(fileName1, "/home/data/K2301/rx_bf_testbench/c_data/case");
    sprintf(num, "%d", case_num);
    strcat(fileName1, num);
    strcat(fileName2, fileName1);
    strcat(fileName1, "/in_1.txt");
    strcat(fileName2, "/in_2.txt");

    file_in.open(fileName1);
    file_in1.open(fileName1);
    file_in2.open(fileName2);
    if(!file_in1 && !file_in2 )
    {
        cout << "no input file" << endl;
        exit(100);
    }

    sprintf(num, "%d", case_num);
    strcpy(folderName, "../data/");
    if (stat(folderName, &statExist) != 0) {
            statMd = mkdir(folderName, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    strcpy(folderName, "../data/case");
    strcat(folderName, num); strcat(folderName, "/");
    if (stat(folderName, &statExist) != 0) {
            statMd = mkdir(folderName, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }

    for (int ck0 = 0; ck0 < ifft_len * slide_num*user_num; ck0++)
    {
        for (int ck1 = 0; ck1 < ant_num; ck1++)
        {
            file_in1 >> I >> Q;
            ifft_in_buff[ck1][ck0].I = I;
            ifft_in_buff[ck1][ck0].Q = Q;

        }
    }
    for(int ck0 = 0;ck0 < slide_num * user_num;ck0++)
    {
        for (int ck1 = 0; ck1 < ant_num; ck1++)
        {
            file_in2 >> I >> Q;
            an_in_buff[ck1][ck0].I = I;
            an_in_buff[ck1][ck0].Q = Q;
        }
    }

    m_bf.configure(cfg);
    m_bf.init(folderName);
    for(int ck0 = 0;ck0 < slide_num;ck0++)
    {
        for(int ck1 = 0;ck1 < user_num;ck1++)
        {
            for(int ck2 = 0; ck2 <  ant_num;ck2++)
            {
                for(int ck3 = 0 ; ck3 < ifft_len;ck3++)
                {
                    ifft_in[2*(ck2 * ifft_len + ck3) ] = ifft_in_buff[ck2][ck3 + (ck0*user_num + ck1) * ifft_len].I;
                    ifft_in[2*(ck2 * ifft_len + ck3) + 1] = ifft_in_buff[ck2][ck3 + (ck0*user_num + ck1) * ifft_len].Q;
                }
            }

            for(int ck2 = 0;ck2 < ant_num;ck2++)
            {
               an_in[ck2 * 2] = an_in_buff[ck2][ck0 * user_num + ck1].I;
               an_in[ck2 * 2 + 1] = an_in_buff[ck2][ck0 * user_num + ck1].Q;
               BfMatrix[ck2] = 1;
            }
            m_bf.go(ifft_in,an_in,BfMatrix);
        }
    }

    free(ifft_in);
    free(an_in);
    free(po);
    free(eo);
    free(BfMatrix);
    file_in.close();
    file_in1.close();
    file_in2.close();
    std::cout<< "Finished"<<std::endl;
   	return 0;
}
