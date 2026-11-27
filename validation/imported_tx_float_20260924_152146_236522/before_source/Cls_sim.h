/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File          : Cls_sim
**  Created By    : Darren.Ren
**  Created On    : 17/10/2023
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**  Notice: 这是配置层代码，一个仿真平台只有一个配置层
**          为了阅读简洁，成员变量没有加m_前缀
****************************************************************************/

#ifndef  __Cls_sim
#define  __Cls_sim

//#define DUMP_TO_MATLAB

//#include <statfs.h>
#include <ctime>
#include <iostream>
#include "Cls_sim_configure.h"
#include "Cls_module_bb_top.h"
#include "Cls_load_file.h"

class Cls_sim
{
public:
	Cls_sim() {
		//m_snr = NULL;
		//m_per = NULL;
		//m_ber = NULL;
	}
	~Cls_sim() {
		//free(m_snr);
		//free(m_per);
		//free(m_ber);
		//output.close();
		//output_log.close();
		//g_log_output.close();
	}
	int   init(int argc, char* argv[]);
	Cls_sim_configure   configure;
	void   reset();
	double go();

private:
	double					m_timer_recorder;
	void					rtl_bitmatch();
	void					capture_data();
	Cls_load_file<double>	device_load_file;
	Cls_module_bb_top       m_gw;
	bb_top_config           m_bb_cfg;
	/*Cls_tm_device			device_pure_tx;
	Cls_tm_device			device_pure_rx;
	Cls_channel				device_channel;

	Complex<double>			m_tx;
	bool					m_tx_valid;// = false;
	Complex<double>			m_rx;
	bool					m_rx_valid;// = false;

	void					ber_per_cal(double ber, double per,bool reset);
	double					m_ber_tmp;// = 0;
	double					m_per_tmp;// = 0;
	int						m_snr_times;// = 0;
	int						m_snr_comp;
	unsigned int			m_cnt;// = 0;
	double*					m_snr;
	double*					m_per;
	double*					m_ber;
	//std::ofstream           output;
	//std::ofstream           output_log;
*/
};

int Cls_sim::init(int argc, char* argv[])
{
	//char filename[300];
	/*m_snr_times = 1;
	m_snr = safe_new(m_snr, m_snr_times);
	m_ber = safe_new(m_ber, m_snr_times);
	m_per = safe_new(m_per, m_snr_times);*/
	configure.init(argc, argv);
	device_load_file.init(configure.m_cfg.cap);
	m_bb_cfg.rx_mode = configure.m_cfg.rx_mode;
	m_gw.configure(m_bb_cfg);
	m_gw.init(configure.m_cfg.cap.dump_folder);
	//air.init(configure.m_channel_1x1);
	//device_pure_tx.m_tx_rx_flag = TX;
	//device_pure_tx.init("device_tx", configure.m_cfg);
	//device_pure_tx.configure();
	//device_pure_rx.m_tx_rx_flag = RX;
	//device_pure_rx.init("device_rx", configure.m_cfg);
	//device_pure_rx.configure();
	//reset();
	//strcpy(filename, g_log_folder); strcat(filename, "err.log");output_log.open(filename);
	//strcpy(filename, g_log_folder); strcat(filename, "fer.log");g_log_output.open(filename);
	//output.open("data_channel.txt");
	//if (!output)
	//	LOG2DEV << "open file fail" << std::endl;
	return 1;
}

void Cls_sim::reset()
{
	//device_pure_tx.reset();
	//device_pure_rx.reset();
	//ber_per_cal(0, 0, 1);
}

double Cls_sim::go()
{
	m_timer_recorder = double(time(NULL));

	if (configure.m_cfg.source_type == RTL_BITMATCH)
	{
		rtl_bitmatch();
	}
	else if (configure.m_cfg.source_type == CAPTURE)
	{
		//capture_data();
	}

	LOG2DEV << "This simulation takes " << (double)time(NULL) - m_timer_recorder << " seconds." << std::endl;

	return 0.0;
}


void Cls_sim::rtl_bitmatch()
{
	while (1)
	{
		if (device_load_file.go())
		{
			break;
		}
		m_gw.m_IO.i_dig_iq_rx_1_i = device_load_file.m_out_1.I;
		m_gw.m_IO.i_dig_iq_rx_1_q = device_load_file.m_out_1.Q;
		m_gw.m_IO.i_dig_iq_rx_2_i = device_load_file.m_out_2.I;
		m_gw.m_IO.i_dig_iq_rx_2_q = device_load_file.m_out_2.Q;
		m_gw.m_IO.i_dig_iq_rx_3_i = device_load_file.m_out_3.I;
		m_gw.m_IO.i_dig_iq_rx_3_q = device_load_file.m_out_3.Q;
		m_gw.m_IO.i_dig_iq_rx_4_i = device_load_file.m_out_4.I;
		m_gw.m_IO.i_dig_iq_rx_4_q = device_load_file.m_out_4.Q;
		m_gw.m_IO.i_dig_iq_rx_5_i = device_load_file.m_out_5.I;
		m_gw.m_IO.i_dig_iq_rx_5_q = device_load_file.m_out_5.Q;
		m_gw.m_IO.i_dig_iq_rx_6_i = device_load_file.m_out_6.I;
		m_gw.m_IO.i_dig_iq_rx_6_q = device_load_file.m_out_6.Q;
		m_gw.m_IO.i_dig_iq_rx_7_i = device_load_file.m_out_7.I;
		m_gw.m_IO.i_dig_iq_rx_7_q = device_load_file.m_out_7.Q;
		m_gw.m_IO.i_dig_iq_rx_8_i = device_load_file.m_out_8.I;
		m_gw.m_IO.i_dig_iq_rx_8_q = device_load_file.m_out_8.Q;
		m_gw.m_IO.i_dig_iq_rx_v = 1;
		m_gw.go();
		//m_tx = device_load_file.m_out;
		//m_tx_valid = 1;
		//device_pure_rx.go(m_rx, m_rx_valid, m_tx, m_tx_valid);
	}
}

void Cls_sim::capture_data()
{
	/*
	while (1)
	{
		if (device_load_file.go())
		{
			break;
		}
		m_tx = device_load_file.m_out;
		m_tx_valid = 1;
		device_pure_rx.go(m_rx, m_rx_valid, m_tx, m_tx_valid);
	}
	*/
}
/*
void Cls_sim_turbomas::ber_per_cal(double ber, double per, bool reset)
{
	if (reset) {
		m_ber_tmp = 0;
		m_per_tmp = 0;
	}
	else {
		m_ber_tmp += ber;
		m_per_tmp += per;
	}
}
*/
/*void Cls_sim_turbomas::check_disk_space(char *folder_name)
{
	struct statfs fs1;
	if (statfs(folder_name,&fs1)<0){
		LOG2DEV << "folder space is wrong!" << std::endl;
		exit(0);
	}
	double spaceG = fs1.f_bfree/1024;
	if (spaceG < 10){
		LOG2DEV << "free space size is " << spaceG << "GB\n";
		exit(0);
	}
	return;
}
*/
#endif

