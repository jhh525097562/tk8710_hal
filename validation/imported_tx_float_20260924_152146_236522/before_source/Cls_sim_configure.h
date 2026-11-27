/****************************************************************************
**
**  Copyright  (c)  2023-2025,  All Rights Reserved.
**
**  File          : Cls_sim_configure.h
**  Created By    : Darren.Ren
**  Created On    : 16/10/2023
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**  Notice: 配置层, 主要功能实体，把配置从命令行或者文件读到channel和tx/rx中
**
****************************************************************************/

#ifndef __sim_configure
#define __sim_configure

#include "common_type.h"
#include "Cls_load_file.h"
#include <iostream>
#include <fstream>
#include <sys/stat.h>
//#include <direct.h>

class Cls_sim_configure
{
	public:
		void init(int argc, char *argv[]);
		void go(){;} // Notice: it is nothing to do
		//void reset();
		//CONF_rx					m_rx;
		//CONF_tx					m_tx;
		//CONF_cap				m_cap;
		CONF					m_cfg;
		Cls_load_file<double>	m_load_file;
		int             m_sim_seeds;
		long long		m_load_length ;
	private:
		char			m_channel_type[300];// = {};
		long long		m_simulation_point;// = 0;
		char			m_data_in_mother_folder[300];// = {0};
		char			m_data_in_sub_folder[300];// = {0};
		char			m_data_in_file_name[300];// = {0};
		int				m_load_offset ;//= 0;

		int		        m_data_format;// = Data_Format::OTHER;//TC or US or other format
		double			m_fs ;//= 0;
		int				m_bw;//	= 1;// 11, 22, 33 ,depend on system
		int				m_rx_mode;
		int 			m_tx_mode;
		double			m_doppler_Hz;// = 0;
		double			m_snr_dB ;//= 0;
		int				m_osr_TX;// = 1;
		double			m_if_MHz;// = 1;
		int				m_ver;// = 1;
		int 			m_case_num;
		int 			m_ant_num;

		void			creat_dir(char *argv[]);
};

void Cls_sim_configure::init(int argc, char *argv[])
{
	int temp;
	char char_temp[300];
	char num[10] = {0};
	sscanf(argv[1],"%d",&temp);
	m_cfg.source_type = (enum Data_In_Mode)temp;

  	m_cfg.rx.bw		= m_bw;
	m_cfg.rx.fs_MHz = m_fs;
	m_cfg.rx.if_MHz = m_if_MHz;

	switch(m_cfg.source_type){
		case CAPTURE:
             if (argc < 11){
                std::cerr << "Usage: " << argv[0]
				          << "<SimMode>"
						  << "<DataIn mother folder>"
						  << "<subfolder>"
                          << "<data_in file name>"
						  << "<jump_offset>"
						  << "<sim_length> "
						  << "<data_format> "
						  << "<sampling frequence> "
                          << "<if_MHz> "
						  << "<bandwidth_MHz>"
						  << "<global_sim_debug>\n";
				system("pause");
				exit(100);
            } else {
				strcpy(m_data_in_mother_folder	, argv[2]);
				strcpy(m_data_in_sub_folder		, argv[3]);
            	strcpy(m_data_in_file_name		, argv[4]);        //agc out is Rx input
				sscanf(argv[5],	"%d"		    , &m_load_offset);
				sscanf(argv[6],	"%lld"		    , &m_load_length);
				sscanf(argv[7], "%d"		    , &m_rx_mode);
				sscanf(argv[8], "%d"		    , &m_tx_mode);
				strcpy(char_temp, m_data_in_mother_folder); strcat(char_temp, SLASH);
				strcat(char_temp, m_data_in_sub_folder);strcat(char_temp, SLASH);
				strcat(char_temp, m_data_in_file_name);
				strcpy(m_data_in_file_name, char_temp);
				//sscanf(argv[7],	"%d"		, &m_data_format	);    //decide if it is two's complement
				//sscanf(argv[8],	"%lf"		, &m_fs				);
				//sscanf(argv[9],	"%lf"		, &m_if_MHz			);
				//sscanf(argv[10], "%d"		, &m_bw				);
            }
            break;
        case RTL_BITMATCH :
			if (argc < 4) {
				std::cerr << "Usage: " << argv[0]
                    << "<SimMode>"
                    << "<Multipath_profile>"
                    << "<doppler>"
                    << "<snr> "
                    << "<run_offset>"
                    << "<data_length>"
                    << "<sampling frequence> "
                    << "<if_MHz> "
                    << "sim_cfg"
                    << "mode_idx"
                    << "<global_sim_debug>\n";
				system("pause");
				exit(100);
            } else {
				sscanf(argv[2],	"%d"		    , &m_case_num);
				sscanf(argv[3], "%d"		    , &m_rx_mode);
				sscanf(argv[4], "%d"		    , &m_tx_mode);
				sscanf(argv[5], "%d"		    , &m_ant_num);
				m_data_format = 1;
				m_load_offset = 0;
				m_load_length = -1;
			}
			break;
        default :
                std::cerr << "sim_mode is out of range [SELF_TX CAPTURE].\n " ; return;
    }
	creat_dir(argv);

	if (m_cfg.source_type == CAPTURE)
	{
		strcpy(m_cfg.cap.filename, m_data_in_file_name);
		m_cfg.cap.data_format = (Data_Format)m_data_format;
		m_cfg.cap.load_offset = m_load_offset;
		m_cfg.cap.load_length = int(m_load_length);
		LOG2DEV << " Load from " << m_data_in_file_name <<std::endl;
	}else if (m_cfg.source_type == RTL_BITMATCH){
		m_cfg.cap.data_format = (Data_Format)m_data_format;
		m_cfg.cap.load_offset = m_load_offset;
		m_cfg.cap.load_length = int(m_load_length);
		m_cfg.cap.ant_num = m_ant_num;
		strcpy(m_data_in_file_name, "./c_data/case");
		sprintf(num, "%d", m_case_num);
		strcat(m_data_in_file_name, num);
		strcat(m_data_in_file_name, SLASH);
		strcpy(m_cfg.cap.filename, m_data_in_file_name);
		strcpy(m_data_in_file_name, "./rtl_c_bitmatch/");
		int statMd = 0;
    	struct stat statExist;
    	if (stat(m_data_in_file_name, &statExist) != 0) {
				statMd = mkdir(m_data_in_file_name,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
				if (statMd)
					LOG2DEV << "Cannot create folder.\n";
		}
		strcpy(m_data_in_file_name, "./rtl_c_bitmatch/case");
		strcat(m_data_in_file_name, num); strcat(m_data_in_file_name, "/");
		if (stat(m_data_in_file_name, &statExist) != 0) {
				statMd = mkdir(m_data_in_file_name,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
				if (statMd)
					LOG2DEV << "Cannot create folder.\n";
		}
		strcpy(m_data_in_file_name, "./rtl_c_bitmatch/case");
		sprintf(num, "%d", m_case_num);
		strcat(m_data_in_file_name, num);
		strcat(m_data_in_file_name, SLASH);
		strcpy(m_cfg.cap.dump_folder, m_data_in_file_name);
	}
	m_cfg.rx_mode = m_rx_mode;
	m_cfg.tx_mode = m_tx_mode;
	LOG2DEV << "log folder : " << g_log_folder << std::endl;
}

void Cls_sim_configure::creat_dir(char *argv[])
{
	char data_out_path[300] = {0};
	struct stat statExist;
	int statMd = 0;
	if (stat(g_log_folder, &statExist) != 0) {
		statMd = mkdir(g_log_folder,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);//,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
		if (!statMd)
			LOG2DEV << "Folder created, start to log ... \n";
		else
			LOG2DEV << "Cannot creat foldder.\n";
	}
	strcat(g_log_folder, "/");
	if (m_cfg.source_type == RTL_BITMATCH){
		/*strcpy(data_out_path,argv[1]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[2]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[3]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[4]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[5]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[6]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[7]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[8]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[9]);strcat(data_out_path,"_");
		strcat(data_out_path, argv[10]); strcat(data_out_path, "_");
		strcat(data_out_path, argv[11]); strcat(data_out_path, "_");
		strcat(data_out_path, argv[12]); strcat(data_out_path, "_");
		strcat(data_out_path, argv[13]); strcat(data_out_path, "_");
		strcat(data_out_path,argv[14]); strcat(data_out_path, "_");
		strcat(data_out_path, argv[15]);// strcat(data_out_path, "_");
		*/
	}
	else if (m_cfg.source_type == CAPTURE){
		strcpy(data_out_path,argv[4]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[5]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[6]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[7]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[8]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[9]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[10]);strcat(data_out_path,"_");
		strcat(data_out_path,argv[11]);
	}
	strcat(g_log_folder,data_out_path);

	if (stat(g_log_folder, &statExist)!=0){
		statMd = mkdir(g_log_folder,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);//,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
		if (!statMd)
			LOG2DEV << "Folder created, start to log ... \n";
		else
			LOG2DEV << "Cannot creat foldder.\n";
	}
	strcat(g_log_folder, SLASH);
}

#endif
