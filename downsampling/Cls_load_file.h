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
			//fclose(file_in);
		}
		void			init(CONF_cap cap_cfg);
		bool			go();
		void			reset();
		Complex<double> m_out;
	private:
		int				m_offset_len;// = 0;
		int				m_load_len;// = 0;
		Data_Format		m_data_type;// = Data_Format::OTHER;
		Complex<double>	m_in;
		unsigned int	m_cnt_offset;// = 0;
		unsigned int    m_cnt_len;// = 0;
		bool			m_is_file_over;// = 0;
		bool			hcap_bin2dec_if(FILE* fp, int& out);
		bool			data_from_matlab(FILE* fp, Complex<double>& out);
		bool			hcap_bin2dec_8(FILE* fp, int& out);
		FILE			*file_in;
};

template <class T>
void Cls_load_file<T>::init(CONF_cap cap_cfg)
{
	int ck0 = 0;
	int temp = 0;
	Complex<double> temp_cplx;
	char filename[300] = { 0 };
	strcpy(filename, cap_cfg.filename);
	m_data_type = cap_cfg.data_format;
	m_offset_len = cap_cfg.load_offset;
	m_load_len = cap_cfg.load_length;
	file_in =fopen(filename, "rb");
	if (file_in == NULL) { std::cout << "cann't open the write file!" << std::endl; exit(0); }
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
			m_is_file_over = data_from_matlab(file_in, temp_cplx);
			if (m_is_file_over) { LOG2DEV << "Input file ends." << "\n"; fclose(file_in); }
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
			m_is_file_over = data_from_matlab(file_in, m_out);
			if (m_is_file_over) { LOG2DEV << "Input file ends." << "\n"; fclose(file_in); }
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
