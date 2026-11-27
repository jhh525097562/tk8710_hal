/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File              : Lib_load_file.h
**  Created By    : Alan.Niu
**  Created On    : 11/25/2019
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**  Notice:  ÊôÓÚ¸½Êô¿âº¯Êý
****************************************************************************/

#ifndef __Cls_load_file_H
#define __Cls_load_file_H

#include <stdio.h>
#include <ctime>

class Cls_load_file
{
    public:
		Cls_load_file(){
			p_data_in_fileH = NULL;
		}
		~Cls_load_file(){
			free(p_out);
			if (p_data_in_fileH.is_open())  p_data_in_fileH.close();
		}

		void init(char *fileName, int source_mode, int load_format, long long load_offset, long long load_length);
		void reset(){
			m_sample_cnt = 0;
		}
             void go(Complex<double> &out);
	private:
		char                m_data_in_file_name[300];
		Data_In_Mode		m_source_mode;
		int                 m_load_format;
		long long           m_load_offset;
		long long           m_load_length;
		FILE                *p_data_in_fileH;
		bool                m_file_is_over;
		long long           m_sample_cnt;
		int                 m_timer_recorder;
		
}

void Cls_load_file::init(char *file_name, Data_In_Mode source_mode, int load_format, long long load_offset, long long load_length)
{

	  m_timer_recorder			= double(time(NULL));
	  m_data_in_file_name		= file_name; 
	  m_source_mode				= source_mode;
	  m_load_format				= load_format;
	  m_load_offset				= load_offset;
	  m_load_length				= load_length;

	  if (m_source_mode == SELF_TX)
	  	return;

	  if (m_load_format == 1){
	  	p_data_in_fileH = fopen(m_data_in_file_name,"r");
		fseek(p_data_in_fileH,long(m_load_offset*4),SEEK_CUR);
	  }
	  else if (m_load_format == 2){
	  	p_data_in_fileH = fopen(m_data_in_file_name,"rb");
		fseek(p_data_in_fileH,long(m_load_offset*2),SEEK_CUR);
	  }
	 
}

void Cls_load_file::go(Complex<double> &out)
{
      int temp;
	if (m_load_format == 1){ // ASCII - TC
		if (fscanf(p_data_in_fileH,"%x",&temp)!=-1){
			temp = temp > 511 ? temp - 1024: temp;
			out = (double)temp/512;
		}else{
			fclose(p_data_in_fileH);
			m_file_is_over = true;
		}
	}else if (m_load_format ==2){  // ASCII - US
		if (fscanf(p_data_in_fileH,"%x",&temp)!=-1){
			out = (double)temp/512 - 1.0;
		}else{
			fclose(p_data_in_fileH);
			m_file_is_over = true;			
		}
	}else if (m_load_format == 3) { // Binary - TC(16,16)
		if (fread(&temp,2*sizeof(char),1,p_data_in_fileH)==1){
			out.I = (temp>32768-1)? temp-65536:temp;
			fread(&temp,2*sizeof(char),1,p_data_in_fileH);
			out.Q = (temp>32768-1)? temp-65536:temp;
			out = out/32768.0;
		}else{
			fclose(p_data_in_fileH);
			m_file_is_over = true;
		}
	}

	if (m_file_is_over){
		LOG2DEV << "Load file ends.\n";
		LOG2DEV << "Simulation takes " << time(NULL) - m_timer_recorder << "seconds." << std::endl;
		exit(0);
	}else{
		if ((m_sample_cnt % 1e5) ==0){
			LOG2DEV << "Load samples : " << m_sample_cnt *1.0/ 1e5 << " points\n";
		}
		if (m_sample_cnt > m_load_length){
			LOG2DEV << "Simulation is done by  " << m_load_length << " points.\n".
			LOG2DEV << "This simulation takes "  << time(NULL) - m_timer_recorder <<  "seconds." << std::endl;
			exit(0);
		}
	}
		

}


#endif



































