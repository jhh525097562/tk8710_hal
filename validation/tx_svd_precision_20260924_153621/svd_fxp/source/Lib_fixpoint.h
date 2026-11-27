/****************************************************************************
**
**  Copyright  (c)  2019,  All Rights Reserved.
**
**  Subsystem    :
**  File             : Lib_fixpoint.h
**  Created By   : Alan Niu
**  Created On   : 11/25/2019
**
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**
****************************************************************************/

#ifndef __FXP_H
#define __FXP_H
#include <string>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <stdint.h>

#define FXP_TC       0
#define FXP_US       1

#define FXP_TRN      0
#define FXP_RND      1
#define FXP_RND_INF  2
#define FXP_TRN_ZERO 3

#define FXP_SAT      0
#define FXP_SAT_SYM  1
#define FXP_WRAP     2

#define MAXWL        62
#define DEFAULT_WL    62
#define DEFAULT_IWL   31

//double inline log2(double in)
//{
//	return (log(in)/log(2.0));
//}

//#ifdef _MSC_VER
//    typedef __int64 word64;
//	typedef  unsigned int U32;
//    typedef  unsigned __int64 U64;
//#else
    typedef  long long word64;
    typedef  unsigned int U32;
    typedef  unsigned long long U64;
//#endif

//! Table for fast multiplication or division by 2^n
const word64 UINT64_POW2[64] = {
    word64(1),     word64(1)<<1,  word64(1)<<2,  word64(1)<<3,  word64(1)<<4,
    word64(1)<<5,  word64(1)<<6,  word64(1)<<7,  word64(1)<<8,  word64(1)<<9,
    word64(1)<<10, word64(1)<<11, word64(1)<<12, word64(1)<<13, word64(1)<<14,
    word64(1)<<15, word64(1)<<16, word64(1)<<17, word64(1)<<18, word64(1)<<19,
    word64(1)<<20, word64(1)<<21, word64(1)<<22, word64(1)<<23, word64(1)<<24,
    word64(1)<<25, word64(1)<<26, word64(1)<<27, word64(1)<<28, word64(1)<<29,
    word64(1)<<30, word64(1)<<31, word64(1)<<32, word64(1)<<33, word64(1)<<34,
    word64(1)<<35, word64(1)<<36, word64(1)<<37, word64(1)<<38, word64(1)<<39,
    word64(1)<<40, word64(1)<<41, word64(1)<<42, word64(1)<<43, word64(1)<<44,
    word64(1)<<45, word64(1)<<46, word64(1)<<47, word64(1)<<48, word64(1)<<49,
    word64(1)<<50, word64(1)<<51, word64(1)<<52, word64(1)<<53, word64(1)<<54,
    word64(1)<<55, word64(1)<<56, word64(1)<<57, word64(1)<<58, word64(1)<<59,
    word64(1)<<60, word64(1)<<61, word64(1)<<62, word64(1)<<63
};

  //! Table for fast multiplication by 2^(n-64)
  const double DOUBLE_POW2[128] = {
    0.5/(double)UINT64_POW2[63],    1.0/(double)UINT64_POW2[63], 1.0/(double)UINT64_POW2[62], 1.0/(double)UINT64_POW2[61],
    1.0/(double)UINT64_POW2[60],    1.0/(double)UINT64_POW2[59], 1.0/(double)UINT64_POW2[58], 1.0/(double)UINT64_POW2[57],
    1.0/(double)UINT64_POW2[56],    1.0/(double)UINT64_POW2[55], 1.0/(double)UINT64_POW2[54], 1.0/(double)UINT64_POW2[53],
    1.0/(double)UINT64_POW2[52],    1.0/(double)UINT64_POW2[51], 1.0/(double)UINT64_POW2[50], 1.0/(double)UINT64_POW2[49],
    1.0/(double)UINT64_POW2[48],    1.0/(double)UINT64_POW2[47], 1.0/(double)UINT64_POW2[46], 1.0/(double)UINT64_POW2[45],
    1.0/(double)UINT64_POW2[44],    1.0/(double)UINT64_POW2[43], 1.0/(double)UINT64_POW2[42], 1.0/(double)UINT64_POW2[41],
    1.0/(double)UINT64_POW2[40],    1.0/(double)UINT64_POW2[39], 1.0/(double)UINT64_POW2[38], 1.0/(double)UINT64_POW2[37],
    1.0/(double)UINT64_POW2[36],    1.0/(double)UINT64_POW2[35], 1.0/(double)UINT64_POW2[34], 1.0/(double)UINT64_POW2[33],
    1.0/(double)UINT64_POW2[32],    1.0/(double)UINT64_POW2[31], 1.0/(double)UINT64_POW2[30], 1.0/(double)UINT64_POW2[29],
    1.0/(double)UINT64_POW2[28],    1.0/(double)UINT64_POW2[27], 1.0/(double)UINT64_POW2[26], 1.0/(double)UINT64_POW2[25],
    1.0/(double)UINT64_POW2[24],    1.0/(double)UINT64_POW2[23], 1.0/(double)UINT64_POW2[22], 1.0/(double)UINT64_POW2[21],
    1.0/(double)UINT64_POW2[20],    1.0/(double)UINT64_POW2[19], 1.0/(double)UINT64_POW2[18], 1.0/(double)UINT64_POW2[17],
    1.0/(double)UINT64_POW2[16],    1.0/(double)UINT64_POW2[15], 1.0/(double)UINT64_POW2[14], 1.0/(double)UINT64_POW2[13],
    1.0/(double)UINT64_POW2[12],    1.0/(double)UINT64_POW2[11], 1.0/(double)UINT64_POW2[10], 1.0/(double)UINT64_POW2[9],
    1.0/(double)UINT64_POW2[8],     1.0/(double)UINT64_POW2[7],  1.0/(double)UINT64_POW2[6],  1.0/(double)UINT64_POW2[5],
    1.0/(double)UINT64_POW2[4],     1.0/(double)UINT64_POW2[3],  1.0/(double)UINT64_POW2[2],  1.0/(double)UINT64_POW2[1],
    1.0,                            1.0*(double)UINT64_POW2[1],  1.0*(double)UINT64_POW2[2],  1.0*(double)UINT64_POW2[3],
    1.0*(double)UINT64_POW2[4],     1.0*(double)UINT64_POW2[5],  1.0*(double)UINT64_POW2[6],  1.0*(double)UINT64_POW2[7],
    1.0*(double)UINT64_POW2[8],     1.0*(double)UINT64_POW2[9],  1.0*(double)UINT64_POW2[10], 1.0*(double)UINT64_POW2[11],
    1.0*(double)UINT64_POW2[12],    1.0*(double)UINT64_POW2[13], 1.0*(double)UINT64_POW2[14], 1.0*(double)UINT64_POW2[15],
    1.0*(double)UINT64_POW2[16],    1.0*(double)UINT64_POW2[17], 1.0*(double)UINT64_POW2[18], 1.0*(double)UINT64_POW2[19],
    1.0*(double)UINT64_POW2[20],    1.0*(double)UINT64_POW2[21], 1.0*(double)UINT64_POW2[22], 1.0*(double)UINT64_POW2[23],
    1.0*(double)UINT64_POW2[24],    1.0*(double)UINT64_POW2[25], 1.0*(double)UINT64_POW2[26], 1.0*(double)UINT64_POW2[27],
    1.0*(double)UINT64_POW2[28],    1.0*(double)UINT64_POW2[29], 1.0*(double)UINT64_POW2[30], 1.0*(double)UINT64_POW2[31],
    1.0*(double)UINT64_POW2[32],    1.0*(double)UINT64_POW2[33], 1.0*(double)UINT64_POW2[34], 1.0*(double)UINT64_POW2[35],
    1.0*(double)UINT64_POW2[36],    1.0*(double)UINT64_POW2[37], 1.0*(double)UINT64_POW2[38], 1.0*(double)UINT64_POW2[39],
    1.0*(double)UINT64_POW2[40],    1.0*(double)UINT64_POW2[41], 1.0*(double)UINT64_POW2[42], 1.0*(double)UINT64_POW2[43],
    1.0*(double)UINT64_POW2[44],    1.0*(double)UINT64_POW2[45], 1.0*(double)UINT64_POW2[46], 1.0*(double)UINT64_POW2[47],
    1.0*(double)UINT64_POW2[48],    1.0*(double)UINT64_POW2[49], 1.0*(double)UINT64_POW2[50], 1.0*(double)UINT64_POW2[51],
    1.0*(double)UINT64_POW2[52],    1.0*(double)UINT64_POW2[53], 1.0*(double)UINT64_POW2[54], 1.0*(double)UINT64_POW2[55],
    1.0*(double)UINT64_POW2[56],    1.0*(double)UINT64_POW2[57], 1.0*(double)UINT64_POW2[58], 1.0*(double)UINT64_POW2[59],
    1.0*(double)UINT64_POW2[60],    1.0*(double)UINT64_POW2[61], 1.0*(double)UINT64_POW2[62], 1.0*(double)UINT64_POW2[63]
  };

class fxp {
public:
    int        iwl      ;// = 0          ;// integer word-length
    int        wl       ;//= 0           ;// total word-length = integer word-length + fraction word-length
    int        e_mode   ;//= 0       ;// TC is Two's complement, US is unsigned;
    int        q_mode   ;//= 0       ;// RND is round to plus infinity; TRN is copy the remaining bits
    int        o_mode   ;//= 0       ;// SAT is saturation; SAT_SYM is symmetrical saturation; WRAP is wrap
    bool       o_flag   ;//= 0       ;// flag; 0 is in range, 1 is over_flow
    double     rvalue   ;//= 0       ;// real value that this object represents
    int        rvalue_int;// = 0   ;
	int        exp_lmt  ;//= 0      ;
	int        expVal   ;//= 0       ;
	int        exp_wl   ;//= 0       ;

public:
    fxp();
    fxp(int wlen, int iwlen,int e_mode = FXP_TC, int q_mode=FXP_TRN, int o_mode=FXP_SAT,int exp_lmt=0);
    fxp(double d, int wlen=DEFAULT_WL, int iwlen=DEFAULT_IWL, int e_mode = FXP_TC, int q_mode=FXP_TRN, int o_mode=FXP_SAT,int exp_lmt=0);
    ~fxp();
    double assign_value(double d, int wl, int iwl, int e_mode, int q_mode,int o_mode,int expValue);
    operator double() const;
	void element_print();
	int fast_log2(double data);

    // assignment operator
    fxp& operator = (const fxp& x);
    fxp& operator = (double d);

    double operator + ();    // unary '+' returns itself
    double operator - ();

    void set(int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o=FXP_SAT, int exp_lmt = 0);
	void configure(int wlen, int iwlen, int e=FXP_TC, int q=FXP_TRN, int o= FXP_SAT, int exp_lmt=0);
    void statistic_set(char *name, bool flag, int buffer_len){
        strcpy(m_name,name);
        m_buffer_len = buffer_len;
        m_statistic_flag = flag;
        if ((m_buffer_len>0)&&(m_statistic_flag))
        {
            if (m_buffer!=NULL) delete [] m_buffer;
            //if (m_fileH.is_open())  m_fileH.close();

            m_buffer = new double [m_buffer_len];
            //m_fileH.open(fileName);
            for (m_buffer_cnt=0;m_buffer_cnt<m_buffer_len;m_buffer_cnt++)
                m_buffer[m_buffer_cnt] = 0.0;
        }
        m_buffer_cnt = 0;
    }
    //void write(std::ostream& s);
    std::ostream& write(std::ostream& s, int mode =0);
    bool toString(char *outChar, int len , int mode=1);

    friend double operator + (const fxp& x, const fxp& y);
    friend double operator + (const fxp& x, double y);
    friend double operator + (double x, const fxp& y);

    friend double operator - (const fxp& x, const fxp& y);
    friend double operator - (const fxp& x, double y);
    friend double operator - (double x, const fxp& y);

    friend double operator * (const fxp& x, const fxp& y);
    friend double operator * (const fxp& x, double d);
    friend double operator * (double d, const fxp& x);

    friend double operator / (const fxp& x, const fxp& y);
    friend double operator / (const fxp& x, double d);
    friend double operator / (double d, const fxp& x);

    friend double operator << (const fxp& x, short b);
    friend double operator >> (const fxp& x, short b);

    void operator += (const fxp& x);
    void operator += (double d);
    void operator -= (const fxp& x);
    void operator -= (double d);

    void operator *= (const fxp&);
    void operator *= (double d);
    void operator /= (const fxp& x);
    void operator /= (double d);

    void operator <<= (short b);
    void operator >>= (short b);

    friend bool operator == (const fxp& x, const fxp& y);
    friend bool operator == (const fxp& x, double y);
    friend bool operator == (double x, const fxp& y);
    friend bool operator == (const fxp& x, int y);
    friend bool operator == (int x, const fxp& y);

    friend bool operator != (const fxp& x, const fxp& y);
    friend bool operator != (const fxp& x, double y);
    friend bool operator != (double x, const fxp& y);
    friend bool operator != (const fxp& x, int y);
    friend bool operator != (int x, const fxp& y);

    friend bool operator >= (const fxp& x, const fxp& y);
    friend bool operator >= (const fxp& x, double y);
    friend bool operator >= (double x, const fxp& y);
    friend bool operator >= (const fxp& x, int y);
    friend bool operator >= (int x, const fxp& y);

    friend bool operator <= (const fxp& x, const fxp& y);
    friend bool operator <= (const fxp& x, double y);
    friend bool operator <= (double x, const fxp& y);
    friend bool operator <= (const fxp& x, int y);
    friend bool operator <= (int x, const fxp& y);

    friend bool operator > (const fxp& x, const fxp& y);
    friend bool operator > (const fxp& x, double y);
    friend bool operator > (double x, const fxp& y);
    friend bool operator > (const fxp& x, int y);
    friend bool operator > (int x, const fxp& y);

    friend bool operator < (const fxp& x, const fxp& y);
    friend bool operator < (const fxp& x, double y);
    friend bool operator < (double x, const fxp& y);
    friend bool operator < (const fxp& x, int y);
    friend bool operator < (int x, const fxp& y);

    //friend ostream& operator << (ostream& s, const fxp& x);
    char m_outChar[64] ;//= {0}        ;
    char m_name[64] ;//= {0}           ;

    double  *m_buffer               ;
    int     m_buffer_len;// = 0        ;
    int     m_buffer_cnt;// = 0        ;
    int     m_exp_max;// = 0           ;
    int     m_exp_min;// = 0           ;
    bool    m_statistic_flag;// = 0    ;
    double  m_max;// = 0               ;
    double  m_fix_snr;// = 0           ;
    double  m_fix_snr_min;// = 0       ;
    double  m_fabs_d ;//= 0            ;
    double  m_power_d;// = 0           ;
    double  m_abs_avg;// = 0           ;
    double  m_min;// = 0               ;
    double  m_power_error;// = 0       ;


};

inline void fxp::operator += (const fxp& x)    {  *this = *this + x;}
inline void fxp::operator += (double d)        {  *this = *this + d;}
inline void fxp::operator -= (const fxp& x)    {  *this = *this - x;}
inline void fxp::operator -= (double d)        {  *this = *this - d;}
inline void fxp::operator *= (const fxp& x)    {  *this = *this * x;}
inline void fxp::operator *= (double d)        {  *this = *this * d;}
inline void fxp::operator /= (const fxp& x)    {  *this = *this / x;}
inline void fxp::operator /= (double d)        {  *this = *this / d;}
inline void fxp::operator <<= (short b)        {  *this = *this << b;}
inline void fxp::operator >>= (short b)        {  *this = *this >> b;}
inline double fxp::operator + ()               {  return (rvalue);}
inline double fxp::operator - ()               {  return (-1*rvalue);}

inline bool operator == (const fxp& x, const fxp& y){    return (double(x) == double(y));}
inline bool operator == (const fxp& x, double y)    {    return (double(x) == double(y));}
inline bool operator == (double x, const fxp& y)    {    return (double(x) == double(y));}
inline bool operator == (const fxp& x, int y)       {    return (double(x) == double(y));}
inline bool operator == (int x, const fxp& y)       {    return (double(x) == double(y));}

inline bool operator != (const fxp& x, const fxp& y){    return (double(x) != double(y));}
inline bool operator != (const fxp& x, double y)    {    return (double(x) != double(y));}
inline bool operator != (double x, const fxp& y)    {    return (double(x) != double(y));}
inline bool operator != (const fxp& x, int y)       {    return (double(x) != double(y));}
inline bool operator != (int x, const fxp& y)       {    return (double(x) != double(y));}
inline bool operator >= (const fxp& x, const fxp& y){    return (double(x) >= double(y));}
inline bool operator >= (const fxp& x, double y)    {    return (double(x) >= double(y));}
inline bool operator >= (double x, const fxp& y)    {    return (double(x) >= double(y));}
inline bool operator >= (const fxp& x, int y)       {    return (double(x) >= double(y));}
inline bool operator >= (int x, const fxp& y)       {    return (double(x) >= double(y));}
inline bool operator <= (const fxp& x, const fxp& y){    return (double(x) <= double(y));}
inline bool operator <= (const fxp& x, double y)    {    return (double(x) <= double(y));}
inline bool operator <= (double x, const fxp& y)    {    return (double(x) <= double(y));}
inline bool operator <= (const fxp& x, int y)       {    return (double(x) <= double(y));}
inline bool operator <= (int x, const fxp& y)       {    return (double(x) <= double(y));}
inline bool operator > (const fxp& x, const fxp& y){    return (double(x) > double(y));}
inline bool operator > (const fxp& x, double y)    {    return (double(x) > double(y));}
inline bool operator > (double x, const fxp& y)    {    return (double(x) > double(y));}
inline bool operator > (const fxp& x, int y)       {    return (double(x) > double(y));}
inline bool operator > (int x, const fxp& y)       {    return (double(x) > double(y));}
inline bool operator < (const fxp& x, const fxp& y){    return (double(x) < double(y));}
inline bool operator < (const fxp& x, double y)    {    return (double(x) < double(y));}
inline bool operator < (double x, const fxp& y)    {    return (double(x) < double(y));}
inline bool operator < (const fxp& x, int y)       {    return (double(x) < double(y));}
inline bool operator < (int x, const fxp& y)       {    return (double(x) < double(y));}

inline double operator + (const fxp& x, const fxp& y)  { return (x.rvalue + y.rvalue);}
inline double operator + (const fxp& x, double y)      { return (x.rvalue + y       );}
inline double operator + (double x, const fxp& y)      { return (x + y.rvalue       );}
inline double operator - (const fxp& x, const fxp& y)  { return (x.rvalue - y.rvalue);}
inline double operator - (const fxp& x, double y)      { return (x.rvalue - y       );}
inline double operator - (double x, const fxp& y)      { return (x - y.rvalue       );}
inline double operator * (const fxp& x, const fxp& y)  { return (x.rvalue * y.rvalue);}
inline double operator * (const fxp& x, double d)      { return (x.rvalue * d       );}
inline double operator * (double d, const fxp& x)      { return (x.rvalue * d       );}
inline double operator * (int    d, const fxp& x)      { return (x.rvalue * d       );}
inline double operator / (const fxp& x, const fxp& y)  { return (x.rvalue / y.rvalue);}
inline double operator / (const fxp& x, double d)      { return (x.rvalue / d       );}
inline double operator / (double d, const fxp& x)      { return (d / x.rvalue       );}
inline double operator >> (const fxp& x, short b)      { return (x.rvalue * DOUBLE_POW2[-b+64]);}
inline double operator << (const fxp& x, short b)      { return (x.rvalue * DOUBLE_POW2[b+64]);}

inline fxp::operator double() const {return rvalue;}
inline fxp& fxp::operator = (const fxp& x){    *this = x.rvalue;
return *this;}
inline int fxp::fast_log2(double data)
{
    unsigned long long inData;
    memcpy(&inData, &data, sizeof(double));
    return ((((inData) >> 52) & 0x7ff) - 1022);
}
fxp& fxp::operator = (double d) {
    int exp_t;
	double abs_d;
	double t_d = 0;

	expVal = 0;
	t_d = d;
	if (exp_lmt>0){
		t_d = assign_value(d, wl+exp_lmt+1,iwl, e_mode, FXP_TRN, FXP_SAT, 0);
	}
	abs_d = (t_d<0)?-t_d:t_d;

	if (abs_d < 1e-30 || exp_lmt ==0){
		expVal = 0;
	}
	else{
		exp_t = fast_log2(t_d);
		double d_tmp = log2(abs_d);
	    if (d < 0&& ((d_tmp - int(d_tmp)) == 0) && (o_mode!=FXP_SAT_SYM))
			expVal = (iwl - exp_t - (e_mode==FXP_US?0:0));
		else
			expVal = (iwl - exp_t - (e_mode==FXP_US?0:1));
		expVal = expVal<0 ? 0: expVal;
		expVal = expVal>exp_lmt ? exp_lmt : expVal;
	}

	rvalue = assign_value(t_d, wl, iwl, e_mode, q_mode, o_mode, expVal);
	if (fabs(rvalue)<1e-30) expVal = 0;

    if (m_statistic_flag)
    {
        m_buffer[m_buffer_cnt++] = d; //rvalue;
        m_fabs_d = (d<0)? -d :d ;
        m_power_d = m_power_d + d*d;
        m_abs_avg = m_abs_avg + m_fabs_d;
        m_power_error = m_power_error + (d - rvalue)*(d - rvalue);
        m_max = (m_fabs_d < 1e-30)? m_max : (m_max < m_fabs_d)? m_fabs_d:m_max;
        m_min = (m_fabs_d < 1e-30)? m_min : (m_min > m_fabs_d)? m_fabs_d:m_min;

		m_exp_max = (m_fabs_d < 1e-30) ? m_exp_max : expVal > m_exp_max ? expVal : m_exp_max;
		m_exp_min = (m_fabs_d < 1e-30) ? m_exp_min : expVal < m_exp_min ? expVal : m_exp_min;

        if (m_buffer_cnt == m_buffer_len)
        {
            m_buffer_cnt = 0;
            m_fix_snr = m_power_d == 0 ? 2000 : 10*log10(m_power_d / m_power_error);
            m_fix_snr_min = (m_fix_snr < m_fix_snr_min)?m_fix_snr:m_fix_snr_min;
            if(m_fix_snr < 20){
                //if(d > 1e-4){
                    int a = 1;
                //}
            }

            std::cout << " +++  " << m_name << " snr = " << m_fix_snr_min << "\t";
            int max_width = m_max == 0 ? -100 : (log2(m_max) > 0) ? int(log2(m_max)) + 1 : int(log2(m_max));
			int min_width = m_min == 0 ? -100 : (log2(m_min) > 0) ? int(log2(m_min)) + 1 : int(log2(m_min));
            std::cout << " max = " << max_width << "\t min = " << min_width << "\t";
			std::cout << " m_exp_max = " << m_exp_max << "\t" << "m_exp_min = " << m_exp_min << "\t";

            int abs_avg_width = m_abs_avg == 0 ? -100 : (log2(m_abs_avg/m_buffer_len) > 0) ? int(log2(m_abs_avg/m_buffer_len)) + 1 : int(log2(m_abs_avg/m_buffer_len));
            std::cout << " avg_abs = " << abs_avg_width << "  +++" << std::endl;
            m_power_d = 0;
            m_power_error = 0;
            m_abs_avg = 0;
        }
    }
      return *this;
}

double fxp:: assign_value(double d, int wl, int iwl, int e_mode, int q_mode, int o_mode, int expValue)
{
//        double norm = pow(2., wl-iwl+expValue);
		rvalue = d * DOUBLE_POW2[wl-iwl+expValue+64];

		switch (q_mode){
		case FXP_RND:			rvalue = floor(rvalue +0.5);			                        break;
		case FXP_TRN:			rvalue = floor(rvalue);			                                break;
		case FXP_RND_INF:		rvalue = (rvalue>0)?int64_t(rvalue+0.5) : int64_t(rvalue-0.5);	break;
		case FXP_TRN_ZERO:		rvalue = int64_t(rvalue);			                            break;
		default:			    rvalue = 0;			                                            break;
		}

		if (e_mode == FXP_TC){	  // signed
			switch (o_mode){
				case FXP_SAT:
					o_flag = ((rvalue >= DOUBLE_POW2[64+wl-1])||(rvalue < -1.0*DOUBLE_POW2[64+wl-1]));
					if (o_flag){
						rvalue = (rvalue >= DOUBLE_POW2[64+wl-1])?DOUBLE_POW2[64+wl-1]-1:-1*DOUBLE_POW2[64+wl-1];
					}
					break;
				case FXP_SAT_SYM:
					o_flag = ((rvalue >= DOUBLE_POW2[64+wl-1])||(rvalue <= -1.0*DOUBLE_POW2[64+wl-1]));
					if (o_flag){
						rvalue = (rvalue >= DOUBLE_POW2[64+wl-1])?DOUBLE_POW2[64+wl-1]-1:-1*DOUBLE_POW2[64+wl-1]+1;
					}
					break;
				case FXP_WRAP:
					o_flag = ((rvalue >= DOUBLE_POW2[64+wl-1])||(rvalue < -1.0*DOUBLE_POW2[64+wl-1]));
					if (o_flag){
						rvalue = fmod(rvalue,DOUBLE_POW2[wl+64]);
						rvalue = (rvalue<-DOUBLE_POW2[wl-1+64])?rvalue+DOUBLE_POW2[wl-1+64+1]:rvalue;
						rvalue = (rvalue>=DOUBLE_POW2[wl-1+64])?rvalue-DOUBLE_POW2[wl-1+64+1]:rvalue;
					}
					break;
				default:
					o_flag = 0;
					rvalue = 0;
					break;
			}
		}
		else if (e_mode == FXP_US){  // unsigned
			switch (o_mode){
				case FXP_SAT:
				case FXP_SAT_SYM:
					o_flag = ((rvalue >= DOUBLE_POW2[64+wl])||(rvalue < -1.0*DOUBLE_POW2[64+wl]));
					if (o_flag){
						rvalue = DOUBLE_POW2[64+wl]-1;
					}
					rvalue = (rvalue<0)?rvalue + DOUBLE_POW2[64+wl]:rvalue ;
					break;
				case FXP_WRAP:
					o_flag = ((rvalue >= (double)UINT64_POW2[wl])||(rvalue < 0.0));
					if (o_flag){
						rvalue = fmod(rvalue,DOUBLE_POW2[wl+64]);
						rvalue = (rvalue<0)?rvalue+DOUBLE_POW2[wl-1+64+1]:rvalue;
					}
					break;
				default:
					o_flag = 0;
					rvalue = 0;
					break;
			}
		}
		else {
			o_flag = 0;
			rvalue = 0;
		}
		//rvalue_int = int(rvalue);
		rvalue = rvalue / DOUBLE_POW2[wl-iwl+64];
        return rvalue;
}

fxp::fxp()
{
    iwl = DEFAULT_IWL;
    wl = DEFAULT_WL;
	exp_lmt = 0;
	expVal = 0;
	exp_wl = 0;
    rvalue = 0;
    rvalue_int = 0;
    e_mode = FXP_TC;
    q_mode = FXP_TRN;
    o_mode = FXP_SAT;
    o_flag = 0;
    m_statistic_flag = false;
    m_buffer = NULL;
    m_buffer_cnt = 0;
    m_buffer_len = 0;
    m_max = 0;
	m_min = 1099511627776;
    m_fix_snr = 0;
    m_fix_snr_min = 1099511627776;
    m_fabs_d = 0;
    m_power_d = 0;
    m_abs_avg = 0;
    m_power_error = 0;
	m_exp_max = 0;
	m_exp_min = 40;


}

fxp::fxp(int wlen, int iwlen,int e, int q, int o, int exp_l)
{
      wl = wlen;
      iwl = iwlen;
	  exp_lmt = exp_l;
	  exp_wl = exp_lmt ==0 ? 0: fast_log2(exp_lmt);
      rvalue = 0;
      rvalue_int = 0;
    e_mode = e;
    q_mode = q;
    o_mode = o;
    o_flag = 0;
    m_statistic_flag = false;
    m_buffer = NULL;
    m_buffer_cnt = 0;
    m_buffer_len = 0;
    m_max = 0;
	m_min = 1099511627776;
    m_fix_snr = 0;
    m_fix_snr_min = 1099511627776;
    m_fabs_d = 0;
    m_power_d = 0;
    m_power_error = 0;
	m_exp_max = 0;
	m_exp_min = 0;

}

fxp::fxp(double d, int wlen, int iwlen, int e, int q, int o, int exp_l)
{
    wl = wlen;
    iwl = iwlen;
    e_mode = e;
    q_mode = q;
    o_mode = o;
    o_flag = 0;
	exp_lmt= exp_l;
	expVal = 0;
	exp_wl = exp_lmt ==0 ? 0: fast_log2(exp_lmt);

    m_statistic_flag = false;
    m_buffer = NULL;
    m_buffer_cnt = 0;
    m_buffer_len = 0;
    m_max = 0;
	m_min = 1099511627776;
    m_fix_snr = 0;
    m_fix_snr_min = 1099511627776;
    m_fabs_d = 0;
    m_power_d = 0;
    m_power_error = 0;
	m_exp_max = 0;
	m_exp_min = 40;

    rvalue = d * DOUBLE_POW2[wl-iwl+64+exp_lmt];

    switch (q_mode){
    case FXP_RND:
        rvalue = floor(rvalue +0.5);
        break;
    case FXP_TRN:
        rvalue = floor(rvalue);
        break;
    case FXP_RND_INF:
        rvalue = (rvalue>0)?int(rvalue+0.5) : int(rvalue-0.5);
        break;
    case FXP_TRN_ZERO:
        rvalue = int(rvalue);
        break;
    default:
        rvalue = 0;
        break;
    }

    if (e_mode == FXP_TC){    // signed
        switch (o_mode){
            case FXP_SAT:
                o_flag = ((rvalue >= (double)UINT64_POW2[wl-1])||(rvalue < -1.0*(double)UINT64_POW2[wl-1]));
                if (o_flag){
                    rvalue = (rvalue >= DOUBLE_POW2[64+wl-1])?DOUBLE_POW2[64+wl-1]-1:-1*DOUBLE_POW2[64+wl-1];
                }
                break;
            case FXP_SAT_SYM:
                o_flag = ((rvalue >= (double)UINT64_POW2[wl-1])||(rvalue <= -1.0*(double)UINT64_POW2[wl-1]));
                if (o_flag){
                    rvalue = (rvalue >= (double)UINT64_POW2[wl-1])?DOUBLE_POW2[64+wl-1]-1:-1*DOUBLE_POW2[64+wl-1]+1;
                }
                break;
            case FXP_WRAP:
                o_flag = ((rvalue >= (double)UINT64_POW2[wl-1])||(rvalue < -1.0*(double)UINT64_POW2[wl-1]));
                if (o_flag){
                    rvalue = fmod(rvalue,DOUBLE_POW2[wl+64]);
                    rvalue = (rvalue<-DOUBLE_POW2[wl-1+64])?rvalue+DOUBLE_POW2[wl-1+64+1]:rvalue;
                    rvalue = (rvalue>DOUBLE_POW2[wl-1+64])?rvalue-DOUBLE_POW2[wl-1+64+1]:rvalue;
                }
                break;
            default:
                o_flag = 0;
                rvalue = 0;
                break;
        }
    }
    else if (e_mode == FXP_US){  // unsigned
        switch (o_mode){
            case FXP_SAT:
            case FXP_SAT_SYM:
                o_flag = ((rvalue >= (double)UINT64_POW2[wl])||(rvalue < -1.0*(double)UINT64_POW2[wl]));
                if (o_flag){
                    rvalue = DOUBLE_POW2[64+wl]-1;
                }
                rvalue = (rvalue<0)?rvalue + DOUBLE_POW2[64+wl]:rvalue ;
                break;
            case FXP_WRAP:
                o_flag = ((rvalue >= (double)UINT64_POW2[wl])||(rvalue < 0.0));
                if (o_flag){
                    rvalue = fmod(rvalue,DOUBLE_POW2[wl+64]);
                    rvalue = (rvalue<0)?rvalue+DOUBLE_POW2[wl-1+64+1]:rvalue;
                }
                break;
            default:
                o_flag = 0;
                rvalue = 0;
                break;
        }
    }
    else {
        o_flag = 0;
        rvalue = 0;
    }
    rvalue_int = int(rvalue);
    rvalue = rvalue * DOUBLE_POW2[-wl+iwl+64-exp_lmt];
}


fxp::~fxp() {
     if (m_buffer != NULL) delete [] m_buffer;
}

void fxp::set(int wlen, int iwlen, int e, int q, int o, int exp_l)
{
    wl = wlen;
    iwl = iwlen;
    e_mode = e;
    q_mode = q;
    o_mode = o;
    o_flag = 0;
	exp_lmt = exp_l;
	exp_wl = exp_lmt ==0? 0: fast_log2(exp_lmt);
}

inline void fxp::configure(int wlen, int iwlen, int e , int q , int o , int exp_l)
{
    set(wlen,iwlen,e,q,o,exp_l);
}

std::ostream& fxp::write(std::ostream& s, int mode)
{
    int  outSize;
    switch (mode)
    {
        case 0 : outSize = (int)ceil(wl/4.0);break; // hex
        case 1: outSize = (int)ceil(wl/1.0);break;  // binary
        default : outSize = (int)ceil(wl/4.0);break; // hex
    }
    (*this).toString(m_outChar,outSize,mode);
    s.write(m_outChar,outSize);

    return s;
}

bool fxp::toString(char *outChar, int len , int mode)
{
    word64 temp;
    temp = word64 (rvalue * DOUBLE_POW2[64+wl-iwl+expVal]);
    temp = (temp<0)?word64(temp + DOUBLE_POW2[64+wl]): temp;
	temp = ((word64)expVal<<wl)|temp;

    int  outSize;
    switch (mode)
    {
        case 0 : outSize  = int(ceil(((word64)wl+exp_wl)/4.0));break; // hex
        case 1: outSize   = int(ceil(((word64)wl+exp_wl)/1.0));break;  // binary
        default : outSize = int(ceil(((word64)wl+exp_wl)/4.0));break; // hex
    }

    if (len < outSize)
    {
        printf("Warning ! fxp::toString input len is less than real length.");
        return 0;
    }

    int ck0;
    int frac;
    switch (mode)
    {
    case 0:
    for (ck0=outSize-1;ck0>=0;ck0--)
    {
        frac = int(temp&0xF);
        temp >>= 4;
        switch (frac){
            case 0: outChar[ck0] = 48 + 0;break;
            case 1: outChar[ck0] = 48 + 1;break;
            case 2: outChar[ck0] = 48 + 2;break;
            case 3: outChar[ck0] = 48 + 3;break;
            case 4: outChar[ck0] = 48 + 4;break;
            case 5: outChar[ck0] = 48 + 5;break;
            case 6: outChar[ck0] = 48 + 6;break;
            case 7: outChar[ck0] = 48 + 7;break;
            case 8: outChar[ck0] = 48 + 8;break;
            case 9: outChar[ck0] = 48 + 9;break;
            case 10: outChar[ck0] = 97 + 0;break;
            case 11: outChar[ck0] = 97 + 1;break;
            case 12: outChar[ck0] = 97 + 2;break;
            case 13: outChar[ck0] = 97 + 3;break;
            case 14: outChar[ck0] = 97 + 4;break;
            case 15: outChar[ck0] = 97 + 5;break;
            default:outChar[ck0] = 69;break;
        }
        }
        break;
    case 1:
        for (ck0=outSize-1;ck0>=0;ck0--)
        {
            frac = int(temp&0x1);
            temp >>= 1;
            switch (frac){
                case 0: outChar[ck0] = 48 + 0;break;
                case 1: outChar[ck0] = 48 + 1;break;
                default:outChar[ck0] = 69;break;
            }
        }
        break;
    default : printf("Error in .toString(mode), mode is out of range.\n");break;
    }
    return 1;
}

void fxp::element_print()
{
    std::cout << "this.wl = "      << wl                  << "\n";
	std::cout << "this.iwl = "     << iwl                 << "\n";
	std::cout << "this.dbVal = "   << double(rvalue)      << "\n";
	std::cout << "this.rtlVal = "  << double(rvalue_int)  << "\n";
	std::cout << "this.expVal = "  << double(expVal)      << "\n";
}

#endif    //__FXP_H






