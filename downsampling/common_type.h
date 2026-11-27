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

const int INBUFFER_LEN_VALID_BIT = 254;
const int INBUFFER_LEN = 32;
//using namespace std;
//typedef  int64 word64;
typedef  long long word64;
typedef  unsigned int U32;
typedef  unsigned long long U64;
typedef	 unsigned char U8;

enum   Data_In_Mode{ SELF_TX = 0, CAPTURE = 1, RTL_BITMATCH = 2};
enum   FX_Mode     { floatpoint = 10, fixpoint = 20, both_float_fix = 30};
enum   IN_Mode     { data_in    = 0, file_in  = 1};
enum   OUT_Mode    { file_out = 12, none = 22 };
enum   RAM_AB      { ram_a = 14, ram_b = 24, ram_none = 34, ram_all = 44};
enum   BW          { Mode0 = 128 >> 0, Mode1 = 128 >> 1, Mode2 = 128 >> 2, Mode3 = 128 >> 3, Mode4 = 128 >> 4, Mode5 = 128 >> 5, Mode6 = 128 >> 6, Mode7 = 128 >> 7 };
enum   TX_RX		{ TX = 0, RX = 1};
enum Data_Format	{ CHIP = 0, MATLAB = 1, CHIP_N = 2};
enum 	TX_GEN		{ BCN = 0, DATA = 1 };
enum 	PHY_VER		{ PHY1 = 0, PHY2 = 1, PHY2_1 = 2};
enum 	BCN_VER		{ BCN1 = 0, BCN_2_1 = 1};
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
	char		filename[300];// = { 0 };
};

struct CONF {
	int mode;
	int mode_phy2;
	int mode_phy2_1;
	int rs_polor;
	int bcn_mode;
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
char g_case_folder[300] = "";
char g_configure_channel[300] = "../../../configure/channel/";
char g_pattern[300] = "../../../configure/pattern/";
const char Pattern_folder[300] = "C:\\my_earth\\wuhan\\c_model\\sim_platform_narrowband\\code_version\\work_version\\data_in\\";
char g_tmp_fileName[300] ="";

const double PI = atan(1.0)*4;
std::ofstream   g_log_output;
double      g_fix_or_float;
double      g_mode;
int         g_snr_or_sen;
double      g_case_num;
int         g_data_format;
/*char* szString(const char* newstr) {
    int slen = int(strlen(newstr));
    char* str = new char[slen + 1];
    if (str)
        strcpy(str, (slen + 1), newstr);
    return  str;
}
*/


#endif


