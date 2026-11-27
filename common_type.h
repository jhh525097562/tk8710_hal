/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File          : common_type.h
**  Created By    : Alan.Niu
**  Created On    : 11/24/2019
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**  Notice: ���������Զ�����������
**
****************************************************************************/

#ifndef __common_type
#define __common_type
#include <cmath>
#include <string.h>
#include <iostream>
#include <fstream>
#include "sys/stat.h"
#include "Lib_complex.h"
//#include "Lib_complex_float.h"
#include "Lib_utility.h"
#include "Lib_dump.h"

const int INBUFFER_LEN_VALID_BIT = 254;
const int INBUFFER_LEN = 32;
using namespace std;
//typedef  int64 word64;
typedef  long long word64;
typedef  unsigned int U32;
typedef  unsigned long long U64;
typedef	 unsigned char U8;

double w_op1[8] = {0.058824, 0.176471, 0.294118, 0.411765, 0.529412, 0.647059, 0.764706, 0.882353};
double w_op2[4] = {0.111111, 0.333333, 0.555556, 0.777778};
double w_op3[4] = {0.270741, 0.436767, 0.672605, 0.879998};

enum 	DATA_RATE	{ DR_0_78125 = 0, DR_1_5625 = 1, DR_3_125 = 2, DR_6_25 = 3, DR_12_5 = 4, DR_25 = 5, DR_50 = 6};
enum   	OPTION_TYPE { OPTION1 = 1, OPTION2 = 2, OPTION3 = 3};
enum   	PB_TYPE 	{ PB_16 = 0, PB_40 = 1, PB_72 = 2, PB_136 = 3, PB_264 = 4, PB_520 = 5};
enum   	CODE_RATE   { CR_1_2 = 0, CR_4_5 = 2};
enum  	CONSTEL		{ BPSK = 0, QPSK = 1, QAM16 = 2};
enum   	CRC_TYPE 	{ CRC_24 = 1};
enum   	Data_In_Mode{ RTL_BITMATCH = 0, CAPTURE = 1};
enum   	FX_Mode     { floatpoint = 10, fixpoint = 20, both_float_fix = 30};
enum   	IN_Mode     { data_in    = 0, file_in  = 1};
enum   	OUT_Mode    { file_out = 12, none = 22 };
enum   	RAM_AB      { ram_a = 14, ram_b = 24, ram_none = 34, ram_all = 44};
enum   	BW          { Mode0 = 128 >> 0, Mode1 = 128 >> 1, Mode2 = 128 >> 2, Mode3 = 128 >> 3, Mode4 = 128 >> 4, Mode5 = 128 >> 5, Mode6 = 128 >> 6, Mode7 = 128 >> 7 };
enum   	TX_RX		{ TX = 0, RX = 1};
enum 	Data_Format	{ CHIP = 0, MATLAB = 1, CHIP_N = 2};
enum 	FFT_LEN		{ FFT_1024 = 0, FFT_2048 = 1, FFT_4096 = 2, FFT_8192 = 3};
typedef struct Module_config {
    FX_Mode			fx_mode     ;//=   floatpoint;          // fixpoint mode;
    IN_Mode			in_mode     ;//=   OTHER;          // data in mode;
    TX_RX			tx_rx_flag  ;//=   OTHER;
	Data_In_Mode	source_type ;//=	Data_OTHER;
	bool			by_pass_en  ;//= 0;
}Module_config;
Module_config    default_module_config;

struct CONF_rx {
	double	if_MHz;// = 0;
	double	fs_MHz;// = 0;
	double	bw;// = 0;
};

struct CONF_tx {
	double	if_MHz;// = 0;
	double	fs_MHz;// = 0;
	int		osr;// = 0;
	int		frame_duration;// = 0; // unit is second;
	int		ver;// = 0;
};

struct CONF_cap {
	Data_Format	data_format;// = Data_Format::OTHER;
	int			load_offset;// = 0;
	int			load_length;// = 0;
	int 		ant_num;
	char		filename[300];// = { 0 };
	char 		dump_folder[300];
};

struct CONF {
	int rx_mode;
	int tx_mode;
	int mode_phy2;
	int rs_polor;
	double m_snr_start;
	Data_In_Mode source_type;// = Data_OTHER;
	CONF_rx					rx;
	CONF_tx					tx;
	CONF_cap				cap;
};



#define SLASH "/"
#define LOG2DEV std::cout

double g_sim_debug1=0;
char g_log_folder[300] = "log";
char g_configure_channel[300] = "../../../configure/channel/";
char g_pattern[300] = "../../../configure/pattern/";
const char Pattern_folder[300] = "C:\\my_earth\\wuhan\\c_model\\sim_platform_narrowband\\code_version\\work_version\\data_in\\";
char g_tmp_fileName[300] ="";

const double PI = atan(1.0)*4;
std::ofstream   g_log_output;
double    g_fix_or_float;
/*char* szString(const char* newstr) {
    int slen = int(strlen(newstr));
    char* str = new char[slen + 1];
    if (str)
        strcpy(str, (slen + 1), newstr);
    return  str;
}
*/


#endif


