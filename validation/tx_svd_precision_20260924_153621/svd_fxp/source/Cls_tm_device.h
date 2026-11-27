/****************************************************************************
**
**  Copyright  (c)  2019-2024,  All Rights Reserved.
**
**  File          : Cls_tm_device
**  Created By    :
**  Created On    :
**
**  History     :
**  Programmer    Date         Rev       Description
**  Darren        07/08/2020   v1.0
**
**  Notice:
****************************************************************************/


#ifndef __M_BB_TOP_H
#define __M_BB_TOP_H

#include "module_bb_top.h"

class Cls_tm_device
{
public:
	Cls_tm_device() {
	}
	~Cls_tm_device() {
	}
	void init(char* module_name, CONF cfg);
	void reset();
	void go(Complex<double>& tx_one, bool& tx_valid, Complex<double> rx_one, bool rx_valid);

	Module_bb_top		m_bbu;
	TX_RX				m_tx_rx_flag;// = OTHER;
	bool				m_reset;// = false;
	bool				m_o_rx_done;// = false;
	bool				m_i_rx_done;// = false;
	Data_In_Mode		m_source_mode;// = Data_OTHER;
	int                 m_packet_cnt;// = 0;
	double				m_ber;// = 0;
	double   			m_per;// = 0;
private:
	int                 m_tx_file_in_cnt;// = 0;

};

void Cls_tm_device::init(char* module_name, CONF cfg)
{
	char fileName[300];
	strcpy(fileName, g_log_folder);
	strcat(fileName, module_name);
	m_bbu.CONF_cfg = cfg;
	m_bbu.init(strcat(fileName, "_bb"), m_tx_rx_flag);

}

void Cls_tm_device::reset()
{
	m_reset = 1;
	m_bbu.reset();
	m_i_rx_done = 0;
	m_o_rx_done = 0;

}

void Cls_tm_device::go(Complex<double>& tx_one, bool & tx_valid,Complex<double> rx_one, bool rx_valid)
{
	m_bbu.m_IO.i_dig_iq_rx_data_i	=	rx_one.I;
	m_bbu.m_IO.i_dig_iq_rx_data_q	=	rx_one.Q;
	m_bbu.m_IO.i_dig_iq_rx_data_v	=	rx_valid;

	m_bbu.m_IO.p_reset_n = m_reset;
	m_bbu.go();
	m_o_rx_done = m_bbu.m_IO.o_rx_data_done;
	m_ber = m_bbu.m_IO.o_rx_result.ber;
	m_per = m_bbu.m_IO.o_rx_result.per;

	tx_one.I = m_bbu.m_IO.o_dig_iq_tx_data_i;
	tx_one.Q = m_bbu.m_IO.o_dig_iq_tx_data_q;
	tx_valid = m_bbu.m_IO.o_dig_iq_tx_data_v;
}

#endif
