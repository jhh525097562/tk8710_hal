/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File          : Lib_utility.h
**  Created By    : Alan.Niu
**  Created On    : 11/25/2019
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**  Notice:  ���ڸ����⺯��
****************************************************************************/


#ifndef __LIB_utility_H
#define __LIB_utility_H

#include <iostream>
#include <fstream>
#include "stdlib.h"
#include "Lib_complex.h"
#include "Lib_fixpoint.h"
#include "Lib_random.h"

//<-------------  dump func list ------------------
inline int max( fxp *in, int len);
inline int max( double *in, int len);
inline int max( int *in, int len);

inline void  fix_set(fxp& in, int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){
             in.set(wlen,iwlen,e,q,o);
}
inline void  fix_set(Complex<fxp>& in, int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){
             in.I.set(wlen,iwlen,e,q,o);
             in.Q.set(wlen,iwlen,e,q,o);
}
inline void  statistic_set(fxp& in,  char* name, bool flag, int len) {
    in.statistic_set(name, flag, len);
}
inline void  statistic_set(Complex<fxp>& in,  char* name, bool flag, int len) {
    in.I.statistic_set(name, flag, len);
    in.Q.statistic_set(name, flag, len);
}
inline void  fix_set(double in, int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){}
inline void  fix_set(bool   in, int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){}
inline void  fix_set(int    in, int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){}
inline void  fix_set(Complex<double> in, int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){}
inline void  fix_set(Complex<bool  > in, int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){}
inline void  fix_set(Complex<int   > in, int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){}
inline void  statistic_set(double in,  char* name, bool flag, int len) {}
inline void  statistic_set(bool in,  char* name, bool flag, int len) {}
inline void  statistic_set(int in,  char* name, bool flag, int len) {}
inline void  statistic_set(Complex<double> in,  char* name, bool flag, int len) {}
inline void  statistic_set(Complex<bool> in,  char* name, bool flag, int len) {}
inline void  statistic_set(Complex<int> in,  char* name, bool flag, int len) {}



inline void  fix_set_group(fxp *in, int group_len, int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){
    int ck0;
    for (ck0=0;ck0<group_len;ck0++)
    {
         in[ck0].set(wlen,iwlen,e,q,o);
    }
}
inline void  fix_set_group(Complex<fxp> *in, int group_len,int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){
    int ck0;
    for (ck0=0;ck0<group_len;ck0++)
    {
        in[ck0].I.set(wlen,iwlen,e,q,o);
        in[ck0].Q.set(wlen,iwlen,e,q,o);
    }
}
inline void  fix_set_group(double *in, int group_len,int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){}
inline void  fix_set_group(bool   *in, int group_len,int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){}
inline void  fix_set_group(int    *in, int group_len,int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){}
inline void  fix_set_group(Complex<double> *in, int group_len,int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){}
inline void  fix_set_group(Complex<bool  > *in, int group_len,int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){}
inline void  fix_set_group(Complex<int   > *in, int group_len,int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT){}
inline void  fix_set_group(Complex<fxp> *in, int group_len,double &in_fxp){}
inline void  fix_set_group(Complex<double> *in, int group_len,double &in_fxp){}
inline void  fix_set_group(double *in, int group_len,double &in_fxp){}

inline void  fix_set_group(Complex<double> *in, int group_len,fxp &in_fxp)
{
	for (int i=0;i<group_len;i++)
	{
		in_fxp=in[i].I;
		in[i].I=in_fxp;

		in_fxp=in[i].Q;
		in[i].Q=in_fxp;
	}
}

inline void  fix_set_group(Complex<fxp> *in, int group_len,fxp &in_fxp)
{
	for (int i=0;i<group_len;i++)
	{
		in_fxp=in[i].I;
		in[i].I=in_fxp;

		in_fxp=in[i].Q;
		in[i].Q=in_fxp;
	}
}

inline void  fix_set_group(double *in, int group_len,fxp &in_fxp)
{
	for (int i=0;i<group_len;i++)
	{
		in_fxp=in[i];
		in[i]=in_fxp;
	}
}

inline int sign(double in){    return ((in>0)?1:-1);}
inline int sign(int in)   {    return ((in>0)?1:-1);}

inline void ones(double *in, int len)
{
    int ck0;
    for (ck0=0;ck0<len;ck0++)
    {
        in[ck0] = 1.0;
    }
}
inline void ones(Complex<double> *in, int len)
{
    int ck0;
    for (ck0=0;ck0<len;ck0++)
    {
        in[ck0] = 1.0;
    }
}

inline void ones_val(Complex<double> **in, int row, int col, double val_set)
{
    int ck0, ck1;
    for (ck0 = 0; ck0 < row; ck0++)
    {
        for (ck1 = 0; ck1 < col; ck1++)
        {
            in[ck0][ck1].I = val_set;
        }
    }
}

inline void ones_val(Complex<fxp> **in, int row, int col, double val_set)
{
    int ck0, ck1;
    for (ck0 = 0; ck0 < row; ck0++)
    {
        for (ck1 = 0; ck1 < col; ck1++)
        {
            in[ck0][ck1].I = val_set;
        }
    }
}


inline void ones(int *in, int len)
{
    int ck0;
    for (ck0=0;ck0<len;ck0++)
    {
        in[ck0] = 1;
    }
}

inline void zeros(Complex<double> *in , int len)
{
    int ck0;
    for (ck0=0;ck0<len;ck0++)
    {
        in[ck0] = 0.0;
    }
}
inline void zeros(Complex<double> **in , int row,int col)
{
    for (int i=0;i<row;i++)
    {
	for(int j=0;j<col;j++)
        	in[i][j] = 0.0;
    }
}
inline void zeros(double *in, int len)
{
    int ck0;
    for (ck0=0;ck0<len;ck0++)
    {
        in[ck0] = 0.0;
    }
}
inline void zeros(bool* in, int len)
{
    int ck0;
    for (ck0 = 0; ck0 < len; ck0++)
    {
        in[ck0] = 0.0;
    }
}
inline void zeros(double **in , int row,int col)
{
    for (int i=0;i<row;i++)
    {
	for(int j=0;j<col;j++)
        	in[i][j] = 0;
    }
}
inline void zeros(fxp **in , int row,int col)
{
    for (int i=0;i<row;i++)
    {
	for(int j=0;j<col;j++)
        	in[i][j] = 0;
    }
}
inline void zeros(int * in, int len)
{
    int ck0;
    for (ck0=0;ck0<len;ck0++)
    {
        in[ck0] = 0;
    }
}
inline void zeros(int **in , int row,int col)
{
    for (int i=0;i<row;i++)
    {
	for(int j=0;j<col;j++)
        	in[i][j] = 0;
    }
}
inline void zeros(Complex<fxp> *in, int len)
{
    int ck0;
    for (ck0=0;ck0<len;ck0++)
    {
        in[ck0] = 0.0;
    }
}
inline void zeros(Complex<fxp> **in , int row,int col)
{
    for (int i=0;i<row;i++)
    {
	for(int j=0;j<col;j++)
        	in[i][j] = 0.0;
    }
}
inline void zeros(fxp * in, int len)
{
    int ck0;
    for (ck0=0;ck0<len;ck0++)
    {
        in[ck0] = 0.0;
    }
}

inline void zeros(unsigned char* in, int len)
{
    int ck0;
    for (ck0 = 0; ck0 < len; ck0++)
    {
        in[ck0] = 0;
    }
}

inline void eye(Complex<double> **in, int len)
{
    int ck0;
    for (ck0 = 0; ck0 < len; ck0++)
    {
        in[ck0][ck0].I = 1;
    }
}

inline void eye(Complex<fxp> **in, int len)
{
    int ck0;
    for (ck0 = 0; ck0 < len; ck0++)
    {
        in[ck0][ck0].I = 1;
    }
}

inline void mem_copy(Complex<fxp> *dest,Complex<double> *sour,int len)
{
    int ck0;
    for(ck0=0;ck0<len;ck0++)
    {
        dest[ck0] = sour[ck0];
    }
}
inline void mem_copy(Complex<double> *dest,Complex<int> *sour,int len)
{
	int ck0;
	for(ck0=0;ck0<len;ck0++)
	{
		dest[ck0].I = sour[ck0].I;
		dest[ck0].Q = sour[ck0].Q;
	}
}
inline void mem_copy(Complex<double> **dest, Complex<double> **sour, int row, int col)
{
	int ck0, ck1;
	for(ck0 = 0; ck0 < row; ck0++) {
        for(ck1 = 0; ck1 < col; ck1++) {
            dest[ck0][ck1].I = sour[ck0][ck1].I;
            dest[ck0][ck1].Q = sour[ck0][ck1].Q;
        }
	}
}
inline void mem_copy(Complex<fxp> **dest, Complex<fxp> **sour, int row, int col)
{
	int ck0, ck1;
	for(ck0 = 0; ck0 < row; ck0++) {
        for(ck1 = 0; ck1 < col; ck1++) {
            dest[ck0][ck1].I = sour[ck0][ck1].I;
            dest[ck0][ck1].Q = sour[ck0][ck1].Q;
        }
	}
}
inline void mem_copy(double **dest, double **sour, int row, int col)
{
	int ck0, ck1;
	for(ck0 = 0; ck0 < row; ck0++) {
        for(ck1 = 0; ck1 < col; ck1++) {
            dest[ck0][ck1] = sour[ck0][ck1];
            dest[ck0][ck1] = sour[ck0][ck1];
        }
	}
}
inline void mem_copy(fxp **dest, fxp **sour, int row, int col)
{
	int ck0, ck1;
	for(ck0 = 0; ck0 < row; ck0++) {
        for(ck1 = 0; ck1 < col; ck1++) {
            dest[ck0][ck1] = sour[ck0][ck1];
            dest[ck0][ck1] = sour[ck0][ck1];
        }
	}
}
inline void mem_copy(Complex<double> **dest,Complex<double> **sour, int row, int start, int len)
{
    int ck0, ck1;
    for(ck0 = 0; ck0 < row ;ck0++)
    {
        for (ck1 = 0; ck1 < len; ck1++)
        {
            dest[ck0][ck1] = sour[ck0][ck1 + start];
        }
    }
}
inline void mem_copy(double **dest, double **sour, int row, int start, int len)
{
    int ck0, ck1;
    for(ck0 = 0; ck0 < row ;ck0++)
    {
        for (ck1 = 0; ck1 < len; ck1++)
        {
            dest[ck0][ck1] = sour[ck0][ck1 + start];
        }
    }
}
inline void mem_copy(fxp **dest, fxp **sour, int row, int start, int len)
{
    int ck0, ck1;
    for(ck0 = 0; ck0 < row ;ck0++)
    {
        for (ck1 = 0; ck1 < len; ck1++)
        {
            dest[ck0][ck1] = sour[ck0][ck1 + start];
        }
    }
}
inline void mem_copy(Complex<fxp> **dest,Complex<double> **sour, int row, int start, int len)
{
    int ck0, ck1;
    for(ck0 = 0; ck0 < row ;ck0++)
    {
        for (ck1 = 0; ck1 < len; ck1++)
        {
            dest[ck0][ck1] = sour[ck0][ck1 + start];
        }
    }
}

inline void mem_copy(Complex<fxp> **dest,Complex<fxp> **sour, int row, int start, int len)
{
    int ck0, ck1;
    for(ck0 = 0; ck0 < row ;ck0++)
    {
        for (ck1 = 0; ck1 < len; ck1++)
        {
            dest[ck0][ck1] = sour[ck0][ck1 + start];
        }
    }
}
inline void mem_copy(Complex<double> *dest,Complex<fxp> *sour,int len)
{
    int ck0;
    for(ck0=0;ck0<len;ck0++)
    {
        dest[ck0] = sour[ck0];
    }
}
inline void mem_copy(Complex<double> *dest,Complex<double> *sour,int len)
{
    int ck0;
    for(ck0=0;ck0<len;ck0++)
    {
        dest[ck0] = sour[ck0];
    }
}
inline void mem_copy(Complex<double> *dest,int *sour,int len,int mod)
{
    int ck0;
	if (mod==1)
        for(ck0=0;ck0<len;ck0++)
		{
			dest[ck0].I = sour[ck0];
		}
	else if (mod==2)
		for(ck0=0;ck0<len;ck0++)
		{
			dest[ck0].Q = sour[ck0];
		}
}
inline void mem_copy(Complex<fxp> *dest,Complex<fxp> *sour,int len)
{
    int ck0;
    for(ck0=0;ck0<len;ck0++)
    {
        dest[ck0] = sour[ck0];
    }
}
inline void mem_copy(int *dest,int *sour,int len)
{
    int ck0;
    for(ck0=0;ck0<len;ck0++)
    {
        dest[ck0] = sour[ck0];
    }
}
inline void mem_copy(double *dest,int *sour,int len)
{
    int ck0;
    for(ck0=0;ck0<len;ck0++)
    {
        dest[ck0] = sour[ck0];
    }
}

inline void mem_copy(double *dest,fxp *sour,int len)
{
    int ck0;
    for(ck0=0;ck0<len;ck0++)
    {
        dest[ck0] = sour[ck0];
    }
}
inline void mem_copy(double *dest,double *sour,int len)
{
    int ck0;
    for(ck0=0;ck0<len;ck0++)
    {
        dest[ck0] = sour[ck0];
    }
}
inline void mem_copy(fxp *dest,double *sour,int len)
{
    int ck0;
    for(ck0=0;ck0<len;ck0++)
    {
        dest[ck0] = sour[ck0];
    }
}
inline void mem_copy(fxp *dest,fxp *sour,int len)
{
    int ck0;
    for(ck0=0;ck0<len;ck0++)
    {
        dest[ck0] = sour[ck0];
    }
}

inline void mem_copy(Complex<double> *dest,double *sour,int len)
{
    int ck0;
    for(ck0=0;ck0<len;ck0++)
    {
        dest[ck0] = sour[ck0];
    }
}
inline void mem_copy(double *dest,Complex<double> *sour,int len)
{
    int ck0;
    for(ck0=0;ck0<len;ck0++)
    {
        dest[ck0] = sour[ck0].I;
    }
}
inline void mem_copy(bool *dest,bool *sour,int len)
{
    int ck0;
    for(ck0=0;ck0<len;ck0++)
    {
        dest[ck0] = sour[ck0];
    }
}
inline int simple_mode(int in, int mode)
{
    int out;
    out = (in > (mode-1))? in - mode: in;
    out = (out < 0)? out + mode:out;
    return out;
}
inline double power( double *in, int len)
{
    double power;
    int ck0;
    power = 0;
    for (ck0=0;ck0<len;ck0++)    power += in[ck0] * in[ck0];
    power /= len;
    return (power);
}
inline double power( fxp *in, int len)
{
    double power;
    int ck0;
    power = 0;
    for (ck0=0;ck0<len;ck0++)    power = power + in[ck0] * in[ck0];
    power /= len;
    return (power);
}
inline double power( Complex<double> *in, int len)
{
    double power;
    int ck0;
    power = 0;
    for (ck0=0;ck0<len;ck0++)    power = power + in[ck0].power();
    power /= len;
    return (power);
}
inline double power( Complex<fxp> *in, int len)
{
    double power;
    int ck0;
    power = 0;
    for (ck0=0;ck0<len;ck0++)    power = power + in[ck0].power();
    power /= len;
    return (power);
}
inline int max( fxp *in, int len)
{
    fxp max_value = 0;
    int ck0;
    int max_index;
    max_value = in[0];
    max_index = 0;
    for (ck0=1;ck0<len;ck0++)
    {
        max_value = (max_value<in[ck0])?in[ck0]:max_value;
        max_index = (max_value==in[ck0])?ck0:max_index;
    }
    return (max_index);
}
inline int max( double *in, int len)
{
    double max_value = 0;
    int ck0;
    int max_index;
    max_value = in[0];
    max_index = 0;
    for (ck0=1;ck0<len;ck0++)
    {
        max_value = (max_value<in[ck0])?in[ck0]:max_value;
        max_index = (max_value==in[ck0])?ck0:max_index;
    }
    return (max_index);
}
inline int max( int *in, int len)
{
    double max_value = 0;
    int ck0;
    int max_index;
    max_value = in[0];
    max_index = 0;
    for (ck0=1;ck0<len;ck0++)
    {
        max_value = (max_value<in[ck0])?in[ck0]:max_value;
        max_index = (max_value==in[ck0])?ck0:max_index;
    }
    return (max_index);
}
inline int min( fxp *in, int len)
{
    fxp min_value = 0;
    int ck0;
    int min_index;
    min_value = in[0];
    min_index = 0;
    for (ck0=1;ck0<len;ck0++)
    {
        min_value = (min_value>in[ck0])?in[ck0]:min_value;
        min_index = (min_value==in[ck0])?ck0:min_index;
    }
    return (min_index);
}
inline int min( double *in, int len)
{
    double min_value = 0;
    int ck0;
    int min_index;
    min_value = in[0];
    min_index = 0;
    for (ck0=1;ck0<len;ck0++)
    {
        min_value = (min_value>in[ck0])?in[ck0]:min_value;
        min_index = (min_value==in[ck0])?ck0:min_index;
    }
    return (min_index);
}
inline double *safe_new(double *p, int len)
{
    if (p!=NULL) delete []p;
    if (len==0)
        p = NULL;
    else
        p = new double [len];
    zeros(p, len);
    return (p);
}
inline fxp *safe_new(fxp *p, int len)
{
    if (p!=NULL) delete []p;
    if (len==0)
        p = NULL;
    else
        p = new fxp [len];
    zeros(p, len);
    return (p);
}
inline int *safe_new(int *p, int len)
{
    if (p!=NULL) delete []p;
    if (len==0)
        p = NULL;
    else
        p = new int [len];
    zeros(p, len);
    return (p);
}
inline bool* safe_new(bool* p, int len)
{
    if (p != NULL) delete[]p;
    if (len == 0)
        p = NULL;
    else
        p = new bool[len];
    zeros(p, len);
    return (p);
}
inline short *safe_new(short *p, int len)
{
    if (p!=NULL) delete []p;
    if (len==0)
        p = NULL;
    else
        p = new short [len];

    return (p);
}
inline char *safe_new(char *p, int len)
{
    if (p!=NULL) delete []p;
    if (len==0)
        p = NULL;
    else
        p = new char [len];

    return (p);
}
inline Complex<double> *safe_new(Complex<double> *p, int len)
{
    if (p!=NULL) delete []p;
    if (len==0)
        p = NULL;
    else
        p = new Complex<double> [len];
    zeros(p, len);
    return (p);
}
inline Complex<fxp> *safe_new(Complex<fxp> *p, int len)
{
    if (p!=NULL) delete []p;
    if (len==0)
        p = NULL;
    else
        p = new Complex<fxp> [len];
    zeros(p, len);
    return (p);
}
inline unsigned char* safe_new(unsigned char* p, int len)
{
    if (p != NULL) delete[]p;
    if (len == 0)
        p = NULL;
    else
        p = new unsigned char[len];
    zeros(p, len);
    return (p);
}
inline void free(Complex<double> *&p)
{
    if (p!=NULL) {delete [] p; p = NULL;}
}
inline void free(Complex<fxp> *&p)
{
    if (p!=NULL) {delete [] p; p = NULL;}
}
inline void free(double *&p)
{
    if (p!=NULL) {delete [] p; p = NULL;}
}
inline void free(fxp *&p)
{
    if (p!=NULL) {delete [] p; p = NULL;}
}
inline void free(int *&p)
{
    if (p!=NULL) {delete [] p; p = NULL;}
}
inline void free(short *&p)
{
    if (p!=NULL) {delete [] p; p = NULL;}
}

inline void free_n(Complex<double> **p)
{
    if (p!=NULL)
    {
        if (p[0]!=NULL) delete [] p[0];
        if (p!=NULL) delete [] p;
    }
}

inline Complex<double> **safe_new(Complex<double> **p, int row , int col)
{
    int ck0;
    free_n(p);
    if ((row==0)||(col==0))
        p = NULL;
    else
    {
        p        = new Complex<double> *[row];
        p[0]    = new Complex<double> [row*col];
        for (ck0=1;ck0<row;ck0++)
        {
            p[ck0] = p[ck0-1] + col;
        }
    }
    zeros(p, row, col);
    return p;
}


inline void free_n(Complex<fxp> **p)
{
    if (p!=NULL)
    {
        if (p[0]!=NULL) delete [] p[0];
        if (p!=NULL) delete [] p;
    }
}

inline Complex<fxp> **safe_new(Complex<fxp> **p, int row , int col)
{
    int ck0;
    free_n(p);
    if ((row==0)||(col==0))
        p = NULL;
    else
    {
        p        = new Complex<fxp> *[row];
        p[0]    = new Complex<fxp> [row*col];
        for (ck0=1;ck0<row;ck0++)
        {
            p[ck0] = p[ck0-1] + col;
        }
    }
    zeros(p, row, col);
    return p;
}

inline void free_n(double **p)
{
    if (p!=NULL)
    {
        if (p[0]!=NULL) delete [] p[0];
        if (p!=NULL) delete [] p;
    }
}

inline void free_n(fxp **p)
{
    if (p!=NULL)
    {
        if (p[0]!=NULL) delete [] p[0];
        if (p!=NULL) delete [] p;
    }
}

inline fxp **safe_new(fxp **p, int row , int col)
{
    int ck0;
    free_n(p);
    if ((row==0)||(col==0))
        p = NULL;
    else
    {
        p        = new fxp *[row];
        p[0]    = new fxp [row*col];
        for (ck0=1;ck0<row;ck0++)
        {
            p[ck0] = p[ck0-1] + col;
        }
    }
    zeros(p, row, col);
    return p;
}

inline double **safe_new(double **p, int row , int col)
{
    int ck0;
    free_n(p);
    if ((row==0)||(col==0))
        p = NULL;
    else
    {
        p        = new double *[row];
        p[0]    = new double [row*col];
        for (ck0=1;ck0<row;ck0++)
        {
            p[ck0] = p[ck0-1] + col;
        }
    }
    zeros(p, row, col);
    return p;
}

inline void free_n(short **p)
{
    if (p!=NULL)
    {
        if (p[0]!=NULL) delete [] p[0];
        if (p!=NULL) delete [] p;
    }
}

inline short **safe_new(short **p, int row , int col)
{
    int ck0;
    free_n(p);
    if ((row==0)||(col==0))
        p = NULL;
    else
    {
        p        = new short *[row];
        p[0]    = new short [row*col];
        for (ck0=1;ck0<row;ck0++)
        {
            p[ck0] = p[ck0-1] + col;
        }
    }
    return p;
}

inline void free_n(int **p)
{
    if (p!=NULL)
    {
        if (p[0]!=NULL) delete [] p[0];
        if (p!=NULL) delete [] p;
    }
}

inline int **safe_new(int **p, int row , int col)
{
    int ck0;
    free_n(p);
    if ((row==0)||(col==0))
        p = NULL;
    else
    {
        p        = new int *[row];
        p[0]    = new int [row*col];
        for (ck0=1;ck0<row;ck0++)
        {
            p[ck0] = p[ck0-1] + col;
        }
    }
    zeros(p, row, col);
    return p;
}
inline void free_n(char **p)
{
    if (p!=NULL)
    {
        if (p[0]!=NULL) delete [] p[0];
        if (p!=NULL) delete [] p;
    }
}
inline char **safe_new(char **p, int row , int col)
{
    int ck0;
    free_n(p);
    if ((row==0)||(col==0))
        p = NULL;
    else
    {
        p        = new char *[row];
        p[0]    = new char [row*col];
        for (ck0=1;ck0<row;ck0++)
        {
            p[ck0] = p[ck0-1] + col;
        }
    }
    return p;
}
inline void ErrorHangUp(char *errInfo)
{
    std::cout << errInfo ;
    exit(0);
    while (1) {}
}

inline int get_bit(int in, int bit);
inline int get_bit(int in, int bit)
{
    int t1 = int(pow(10.0, bit));
    return int(abs(in)/t1%10);
}
inline void closefile(std::ofstream &fp)
{
    if (fp.is_open()) fp.close();
}

inline void closefile(std::ifstream &fp)
{
    if (fp.is_open()) fp.close();
}
inline void closefile(std::fstream &fp)
{
    if (fp.is_open()) fp.close();
}
inline void dec2bin(int in, int bitwidth, int *out)
{
    int ck0;
    zeros(out,bitwidth);
    for (ck0 = 0; ck0<bitwidth; ck0++)
    {
        out[bitwidth-1-ck0] = in&0x1;
        in = in >>1;
    }
}
inline void bin2dec(int *in, int len, int &out)
{
    int ck0;
    out = 0;
    for (ck0 = 0; ck0<len; ck0++)
    {
        out += int(in[ck0]*DOUBLE_POW2[64+ck0]);
    }
}

#include <ios>
char* itoa(int num,char*str,int radix)
{
    char index[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    unsigned unum;
    int  i=0,j,k;

    if(radix==10&&num<0)
    {
        unum=(unsigned)-num;
        str[i++]='-';
    }
    else
        unum=(unsigned)num;

    do{
        str[i++] = index[unum%(unsigned)radix];
        unum    /= radix;
    }while(unum);
    str[i]='\0';

    if(str[0]=='-')
        k=1;
    else
        k=0;

    char temp;
    for(j=k;j<=(i-1)/2;j++)
    {
        temp = str[j];
        str[j] = str[i-1+k-j];
        str[i-1+k-j] = temp;
    }
    return str;
}


int blank(Complex<double> *in, int vld[], int len)
{
    int ck0;
    int cnt=0;
    for (ck0 =0;ck0< len;ck0++)
    {
        in[ck0] = vld[ck0]? 0: in[ck0];
        cnt = vld[ck0]? cnt+1:cnt;
    }
    return cnt;
}

void blank_self(Complex<double> *in, int blank_start, int blank_end)
{
    int ck0;
    for (ck0 = blank_start; ck0 < blank_end; ck0++)
    { in[ck0] = 0;}
}

int count_one(int *in,int len)
{
    int cnt=0;
    int ck0;
    for(ck0=0;ck0<len;ck0++)
        cnt = in[ck0] ? cnt+1 : cnt ;
    return cnt;
}

double mean(double *in, int len)
{
    double sum=0;
    for (int ck0=0;ck0<len;ck0++)
        sum += in[ck0];
    return (sum/len);
}

double mean_pow(Complex<double> *in, int len)
{
    double sum=0;
    for (int ck0 =0;ck0<len;ck0++)
        sum += in[ck0].power();
    return(sum/len);
}

/*double rsqrt(double number)
{
    int i;
    float x2, y;
    const float threehalfs = 1.5F;
    double out;

    x2 = number * 0.5F;
    y  = number;
    i  = * ( int * ) &y;                       // evil floating point bit level hacking
    i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
    y  = * ( float * ) &i;
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed
    y  = y * ( threehalfs - ( x2 * y * y ) );

    out = y;
    return out;
}*/

double get_rand(int wl, int iwl, int US)
{
    double m_int = int(DOUBLE_POW2[64+wl]);
    if (US==0)
        return (fmod(randx()*m_int,m_int)-m_int/2)/DOUBLE_POW2[64+wl-iwl];
    else
        return (fmod(randx()*m_int,m_int))/DOUBLE_POW2[64+wl-iwl];
}

int sum(int* in, unsigned int len)
{
    int out = 0;
    for (size_t i = 0; i < len; i++)
    {
        out += in[i];
    }
    return out;
}
void readhex(char const *fileName, int len, double* data_out, int WL)
{
    FILE* fp = NULL;
    fp = fopen(fileName, "r");
    //int tmp;
    if (fp == NULL) {
        std::cout << "error to open " << fileName << "\n"; exit(0);
    }
    int ck0 = 0;
    unsigned int temp;
    while (ck0 < len) {
        //tmp = fscanf(fp, "%x", &temp);
        fscanf(fp, "%x", &temp);
        data_out[ck0] = temp > DOUBLE_POW2[64+WL-1] ? temp - DOUBLE_POW2[64 + WL] : temp;
        ck0++;
    }
    fclose(fp);
}

void readhex(char const* fileName, int len, Complex<double>* data_out, int WL, double scale)
{
    FILE* fp = NULL;
    fp = fopen(fileName, "r");
    //int tmp;
    if (fp == NULL) {
        std::cout << "error to open " << fileName << "\n"; exit(0);
    }
    int ck0 = 0;
    unsigned int temp;
    int buf;
    while (ck0 < len) {
        //tmp = fscanf(fp, "%x", &temp);
        fscanf(fp, "%x", &temp);
        buf = temp >> WL;
        data_out[ck0].I = buf > DOUBLE_POW2[64 + WL - 1] ? buf - DOUBLE_POW2[64 + WL] : buf;
        buf = temp % (int)(DOUBLE_POW2[64 + WL]);
        data_out[ck0].Q = buf > DOUBLE_POW2[64 + WL - 1] ? buf - DOUBLE_POW2[64 + WL] : buf;
        data_out[ck0] = data_out[ck0] / scale;
        ck0++;
    }
    fclose(fp);
}

#endif
