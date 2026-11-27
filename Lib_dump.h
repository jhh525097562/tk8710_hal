/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File              : lib_random.h
**  Created By    : Alan.Niu
**  Created On    : 11/25/2019
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**  Notice:  ÊôÓÚ¸½Êô¿âº¯Êý
****************************************************************************/


#ifndef __LIB_dump_H
#define __LIB_dump_H

#include <iostream>
#include <fstream>
#include <iomanip>
#include "stdlib.h"
#include "Lib_complex.h"
#include "Lib_fixpoint.h"
using namespace std;
//<-------------  dump func list ------------------
inline void  dump(std::ofstream& file, Complex<double> *in, int len, int enter = 1);
inline void  dump(std::ofstream& file, Complex<fxp   > *in, int len, int enter = 1);
inline void  dump(std::ofstream& file, double          *in, int len, int enter = 1);
inline void  dump(std::ofstream& file, fxp             *in, int len, int enter = 1);

inline void  dump(std::ofstream& file, Complex<double> *in, int len, fxp fxp_temp, int enter = 1);
inline void  dump(std::ofstream& file, Complex<fxp   > *in, int len, fxp fxp_temp, int enter = 1);
inline void  dump(std::ofstream& file, double          *in, int len, fxp fxp_temp, int enter = 1);
inline void  dump(std::ofstream& file, fxp             *in, int len, fxp fxp_temp, int enter = 1);

inline void  dump(std::ofstream& file, Complex<double> *in, int len, int enter)
{
    int ck0;
    if(enter == 1)
    {
        for (ck0=0;ck0<len-1;ck0++)
            file << fixed << setprecision(26) << in[ck0].I << "\t" << in[ck0].Q << "\n";
        if (len>0)
            file << fixed << setprecision(26) << in[len-1].I << "\t" << in[len-1].Q << std::endl;
    }
    else
    {
        for (ck0=0;ck0<len-1;ck0++)
            file << fixed << setprecision(26) << in[ck0].I << "\t" << in[ck0].Q;
        if (len>0)
            file << fixed << setprecision(26) << in[len-1].I << "\t" << in[len-1].Q;
    }

}

inline void  dump(std::ofstream& file, Complex<fxp   > *in, int len, int enter)
{
    int ck0;
    if(enter == 1)
    {
        for (ck0=0;ck0<len-1;ck0++)
            file << fixed << setprecision(26) << in[ck0].I << "\t" << in[ck0].Q << "\n";
        if (len>0)
            file << fixed << setprecision(26) << in[len-1].I << "\t" << in[len-1].Q << std::endl;
    }
    else
    {
        for (ck0=0;ck0<len-1;ck0++)
            file << fixed << setprecision(26) << in[ck0].I << "\t" << in[ck0].Q;
        if (len>0)
            file << fixed << setprecision(26) << in[len-1].I << "\t" << in[len-1].Q;
    }

}
inline void  dump(std::ofstream& file, double *in, int len, int enter)
{
    int ck0;
    if(enter == 1)
    {
        for (ck0=0;ck0<len-1;ck0++)
            file << fixed << setprecision(26) << in[ck0]<<  "\n";
        if (len>0)
            file << fixed << setprecision(26) << in[len-1] << std::endl;
    }
    else
    {
        for (ck0=0;ck0<len-1;ck0++)
            file << fixed << setprecision(26) << in[ck0];//<<  "\n";
        if (len>0)
            file << fixed << setprecision(26) << in[len-1];// << std::endl;
    }

}

inline void  dump(std::ofstream& file, fxp    *in, int len, int enter)
{
    int ck0;
    if(enter == 1)
    {
        for (ck0=0;ck0<len-1;ck0++)
            file << fixed << setprecision(26) << in[ck0]  << "\n";
        if (len>0)
            file << fixed << setprecision(26) << in[len-1] << std::endl;
    }
    else
    {
        for (ck0=0;ck0<len-1;ck0++)
            file << fixed << setprecision(26) << in[ck0];//  << "\n";
        if (len>0)
            file << fixed << setprecision(26) << in[len-1];// << std::endl;
    }
}

inline void  dump(std::ofstream& file, Complex<double   > *in, int len, fxp fxp_temp, int enter)
{
    int ck0;
    if(enter == 1)
    {
        for (ck0=0;ck0<len;ck0++){
            fxp_temp = in[ck0].I;
            fxp_temp.write(file);
            fxp_temp = in[ck0].Q;
            fxp_temp.write(file);
            if (ck0<len-1) file << "\n";
            else              file << std::endl;
        }
    }
    else
    {
        for (ck0=0;ck0<len;ck0++){
            fxp_temp = in[ck0].I;
            fxp_temp.write(file);
            fxp_temp = in[ck0].Q;
            fxp_temp.write(file);
            //if (ck0<len-1) file << "\n";
            //else              file << std::endl;
        }
    }

}

inline void  dump(std::ofstream& file, Complex<fxp   > *in, int len, fxp fxp_temp, int enter)
{
    int ck0;
    if(enter == 1)
    {
        for (ck0=0;ck0<len;ck0++){
            fxp_temp = in[ck0].I;
            fxp_temp.write(file);
            fxp_temp = in[ck0].Q;
            fxp_temp.write(file);
            if (ck0<len-1) file << "\n";
            else              file << std::endl;
        }
    }
    else
    {
        for (ck0=0;ck0<len;ck0++){
            fxp_temp = in[ck0].I;
            fxp_temp.write(file);
            fxp_temp = in[ck0].Q;
            fxp_temp.write(file);
            //if (ck0<len-1) file << "\n";
            //else              file << std::endl;
        }
    }
}

inline void  dump(std::ofstream& file, double *in, int len, fxp fxp_temp, int enter)
{
    int ck0;
    if(enter == 1)
    {
        for (ck0=0;ck0<len;ck0++){
            fxp_temp = in[ck0];
            fxp_temp.write(file);
            if (ck0<len-1) file << "\n";
            else file << std::endl;
        }
    }
    else
    {
        for (ck0=0;ck0<len;ck0++){
            fxp_temp = in[ck0];
            fxp_temp.write(file);
           // if (ck0<len-1) file << "\n";
            //else file << std::endl;
        }
    }

}

inline void  dump(std::ofstream& file, fxp *in, int len, fxp fxp_temp, int enter)
{
    int ck0;
    if(enter== 1)
    {
        for (ck0=0;ck0<len;ck0++){
            fxp_temp = in[ck0];
            fxp_temp.write(file);
            if (ck0<len-1) file << "\n";
            else              file << std::endl;
        }
    }
    else
    {
        for (ck0=0;ck0<len;ck0++){
            fxp_temp = in[ck0];
            fxp_temp.write(file);
            //if (ck0<len-1) file << "\n";
            //else              file << std::endl;
        }
    }
}
#endif
