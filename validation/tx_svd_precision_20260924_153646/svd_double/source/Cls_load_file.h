/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File          : Cls_load_file.h
**  Created By    : Darren.Ren
**  Created On    : 17/08/2020
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**  Notice: Load filed test file
**
****************************************************************************/


#ifndef __Cls_load_file
#define __Cls_load_file

#include "common_type.h"
#include "Lib_complex.h"
#include "Lib_utility.h"
#include <iostream>
#include <fstream>
#include <sys/stat.h>
//#include <direct.h>

template <class T>
class Cls_load_file
{
	public:
		Cls_load_file() {

		}
		~Cls_load_file() {

		}
		void			init(CONF_cap cap_cfg);
		bool			go();
		void			reset();
		Complex<double> m_out;
		Complex<double> m_out_1;
		Complex<double> m_out_2;
		Complex<double> m_out_3;
		Complex<double> m_out_4;
		Complex<double> m_out_5;
		Complex<double> m_out_6;
		Complex<double> m_out_7;
		Complex<double> m_out_8;
	private:
		int				m_offset_len;// = 0;
		int				m_load_len;// = 0;
		Data_Format		m_data_type;// = Data_Format::OTHER;
		Complex<double>	m_in;
		unsigned int	m_cnt_offset;// = 0;
		unsigned int    m_cnt_len;// = 0;
		bool			m_is_file_over;// = 0;
		int 			m_ant_num;
		bool			hcap_bin2dec_if(FILE* fp, int& out);
		bool			data_from_matlab(FILE* fp, Complex<double>& out);
		bool			hcap_bin2dec_8(FILE* fp, int& out);
		FILE			*file_in;
		FILE			*file_in_1;
		FILE			*file_in_2;
		FILE			*file_in_3;
		FILE			*file_in_4;
		FILE			*file_in_5;
		FILE			*file_in_6;
		FILE			*file_in_7;
		FILE			*file_in_8;
};

template <class T>
void Cls_load_file<T>::init(CONF_cap cap_cfg)
{
	int ck0 = 0;
	int temp = 0;
	Complex<double> temp_cplx;
	char filename[300] = { 0 };
    switch (cap_cfg.ant_num) {
		case 8:
			strcpy(filename, cap_cfg.filename); strcat(filename, "ant_5.txt"); file_in_5 = fopen(filename, "rb"); if (file_in_5 == NULL) { std::cout << "Cannot open ant_5.txt!" << std::endl; system("pause");exit(0); }
			strcpy(filename, cap_cfg.filename); strcat(filename, "ant_6.txt"); file_in_6 = fopen(filename, "rb"); if (file_in_6 == NULL) { std::cout << "Cannot open ant_6.txt!" << std::endl; system("pause");exit(0); }
			strcpy(filename, cap_cfg.filename); strcat(filename, "ant_7.txt"); file_in_7 = fopen(filename, "rb"); if (file_in_7 == NULL) { std::cout << "Cannot open ant_7.txt!" << std::endl; system("pause");exit(0); }
			strcpy(filename, cap_cfg.filename); strcat(filename, "ant_8.txt"); file_in_8 = fopen(filename, "rb"); if (file_in_8 == NULL) { std::cout << "Cannot open ant_8.txt!" << std::endl; system("pause");exit(0); }
		case 4:
			strcpy(filename, cap_cfg.filename); strcat(filename, "ant_3.txt"); file_in_3 = fopen(filename, "rb"); if (file_in_3 == NULL) { std::cout << "Cannot open ant_3.txt!" << std::endl; system("pause");exit(0); }
			strcpy(filename, cap_cfg.filename); strcat(filename, "ant_4.txt"); file_in_4 = fopen(filename, "rb"); if (file_in_4 == NULL) { std::cout << "Cannot open ant_4.txt!" << std::endl; system("pause");exit(0); }
		case 2:
			strcpy(filename, cap_cfg.filename); strcat(filename, "ant_2.txt"); file_in_2 = fopen(filename, "rb"); if (file_in_2 == NULL) { std::cout << "Cannot open ant_2.txt!" << std::endl; system("pause");exit(0); }
		case 1:
			strcpy(filename, cap_cfg.filename); strcat(filename, "ant_1.txt"); file_in_1 = fopen(filename, "rb"); if (file_in_1 == NULL) { std::cout << "Cannot open ant_1.txt!" << std::endl; system("pause");exit(0); }
		default:
			break;
    }
	m_data_type = cap_cfg.data_format;
	m_offset_len = cap_cfg.load_offset;
	m_load_len = cap_cfg.load_length;
	m_ant_num = cap_cfg.ant_num;
	if (m_data_type == CHIP)
	{
		for (ck0 = 0; ck0 < m_offset_len; ck0++) {
			m_is_file_over = hcap_bin2dec_if(file_in, temp);
			if (m_is_file_over) { LOG2DEV << "Input file ends." << "\n"; fclose(file_in); }
		}
	}
	else if (m_data_type == MATLAB)
	{
		for (ck0 = 0; ck0 < m_offset_len; ck0++) {
			switch (cap_cfg.ant_num) {
				case 8:
					m_is_file_over = data_from_matlab(file_in_5, temp_cplx); if (m_is_file_over) { LOG2DEV << "Input file ends." << "\n"; fclose(file_in_5); system("pause");exit(0);}
					m_is_file_over = data_from_matlab(file_in_6, temp_cplx); if (m_is_file_over) { LOG2DEV << "Input file ends." << "\n"; fclose(file_in_6); system("pause");exit(0);}
					m_is_file_over = data_from_matlab(file_in_7, temp_cplx); if (m_is_file_over) { LOG2DEV << "Input file ends." << "\n"; fclose(file_in_7); system("pause");exit(0);}
					m_is_file_over = data_from_matlab(file_in_8, temp_cplx); if (m_is_file_over) { LOG2DEV << "Input file ends." << "\n"; fclose(file_in_8); system("pause");exit(0);}
				case 4:
					m_is_file_over = data_from_matlab(file_in_3, temp_cplx); if (m_is_file_over) { LOG2DEV << "Input file ends." << "\n"; fclose(file_in_3); system("pause");exit(0);}
					m_is_file_over = data_from_matlab(file_in_4, temp_cplx); if (m_is_file_over) { LOG2DEV << "Input file ends." << "\n"; fclose(file_in_4); system("pause");exit(0);}
				case 2:
					m_is_file_over = data_from_matlab(file_in_2, temp_cplx); if (m_is_file_over) { LOG2DEV << "Input file ends." << "\n"; fclose(file_in_2); system("pause");exit(0);}
				case 1:
					m_is_file_over = data_from_matlab(file_in_1, temp_cplx); if (m_is_file_over) { LOG2DEV << "Input file ends." << "\n"; fclose(file_in_1); system("pause");exit(0);}
				default:
					break;
			}
		}
	}
}

template <class T>
bool Cls_load_file<T>::go()
{
	int temp = 0;
	m_cnt_len++;
	if (m_load_len == -1)
	{
		if (m_data_type == CHIP)
		{
			m_is_file_over = hcap_bin2dec_if(file_in, temp);
			m_in.I = temp >> 12;
			m_in.Q = temp % (int)pow(2, 12);
			m_out.I = ((m_in.I > 2048) ? m_in.I - 4096 : m_in.I) * 1.0 / pow(2, 11);
			m_out.Q = ((m_in.Q > 2048) ? m_in.Q - 4096 : m_in.Q) * 1.0 / pow(2, 11);
			if (m_is_file_over) { LOG2DEV << "Input file ends." << "\n"; fclose(file_in); }
		}
		else if (m_data_type == CHIP_N)
		{
			m_is_file_over = hcap_bin2dec_8(file_in, temp);
			m_in.I = temp >> 12;
			m_in.Q = temp % (int)pow(2, 12);
			m_out.I = ((m_in.I > 2048) ? m_in.I - 4096 : m_in.I) * 1.0 / pow(2, 11);
			m_out.Q = ((m_in.Q > 2048) ? m_in.Q - 4096 : m_in.Q) * 1.0 / pow(2, 11);
			//LOG2DEV << m_cnt_len << "\t" << m_out.I << "\t" << m_out.Q << std::endl;
			if (m_is_file_over) { LOG2DEV << "Input file ends." << "\n"; fclose(file_in); }
		}
		else if (m_data_type == MATLAB)
		{
			switch (m_ant_num) {
				case 8:
					m_is_file_over = data_from_matlab(file_in_5, m_out_5); if (m_is_file_over) { LOG2DEV << "ant_5 file ends." << "\n"; fclose(file_in_5); }
					m_is_file_over = data_from_matlab(file_in_6, m_out_6); if (m_is_file_over) { LOG2DEV << "ant_6 file ends." << "\n"; fclose(file_in_6); }
					m_is_file_over = data_from_matlab(file_in_7, m_out_7); if (m_is_file_over) { LOG2DEV << "ant_7 file ends." << "\n"; fclose(file_in_7); }
					m_is_file_over = data_from_matlab(file_in_8, m_out_8); if (m_is_file_over) { LOG2DEV << "ant_8 file ends." << "\n"; fclose(file_in_8); }
				case 4:
					m_is_file_over = data_from_matlab(file_in_3, m_out_3); if (m_is_file_over) { LOG2DEV << "ant_3 file ends." << "\n"; fclose(file_in_3); }
					m_is_file_over = data_from_matlab(file_in_4, m_out_4); if (m_is_file_over) { LOG2DEV << "ant_4 file ends." << "\n"; fclose(file_in_4); }
				case 2:
					m_is_file_over = data_from_matlab(file_in_2, m_out_2); if (m_is_file_over) { LOG2DEV << "ant_2 file ends." << "\n"; fclose(file_in_2); }
				case 1:
					m_is_file_over = data_from_matlab(file_in_1, m_out_1); if (m_is_file_over) { LOG2DEV << "ant_1 file ends." << "\n"; fclose(file_in_1); }
				default:
					break;
			}
		}
	}
	else if (int(m_cnt_len) > m_load_len)
		m_is_file_over = 1;
	return m_is_file_over;
}

template <class T>
bool Cls_load_file<T>::hcap_bin2dec_if(FILE* fp, int& out)
{
	static int cnt = 0;
	static U32 temp_32[8];
	bool is_file_over = false;
	if (cnt == 0)
	{
		is_file_over = (fread(temp_32, sizeof(U32), 8, fp) != 8);
	}
	out = temp_32[cnt++];
	cnt = (cnt == 8) ? 0 : cnt;
	return is_file_over;
}

template <class T>
bool Cls_load_file<T>::data_from_matlab(FILE* fp, Complex<double>& out)
{
	bool is_file_over	= false;
	int return_fsc = 0;
	double I = 0;
	double Q = 0;
	if (!feof(fp))
	{
		return_fsc = fscanf(fp, "%lf\t%lf\n", &I, &Q);
		if(return_fsc == 0)
			return 1;
		out.I = I;
		out.Q = Q;
	}
	else
		is_file_over = true;
	return is_file_over;
}

template <class T>
bool Cls_load_file<T>::hcap_bin2dec_8(FILE* fp, int& out)
{
	static int cnt = 0;
	static U8 temp_8[3];
	bool is_file_over = false;
	if (cnt == 0)
	{
		is_file_over = (fread(temp_8, sizeof(U8), 3, fp)!=3);
	}
	out = ((int)temp_8[2] << 16) + ((int)temp_8[1] << 8) + (int)temp_8[0];
	//LOG2DEV << (int)temp_8[2] << "\t" << (int)temp_8[1] << "\t" << (int)temp_8[0] << "\t" << out << endl;
	return is_file_over;
}

#endif
