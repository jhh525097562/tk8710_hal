/****************************************************************************
**
**  Copyright  (c)  2023-2028,  All Rights Reserved.
**
**  File          : Cls_BF_beamf.h
**  Created By    :
**  Created On    :
**
**  History     :
**  Programmer    	Date        Rev       	Description
**  --------------- ---------- 	-------- ------------------------------
**
**  Notice: Beamforming
**
****************************************************************************/
#ifndef __Cls_BF_beamf
#define __Cls_BF_beamf

#define DUMP_MATLAB_BF_beamf

#include "common_type.h"
#include "Cls_GWRxBFModule.h"
#include "Cls_bf.h"
#include "Cls_Rx_Sync_2_1.h"
#include "Cls_rx_data_phy2_1.h"

typedef struct BF_beamf_CFG
{
    BF_CFG beamf_cfg;
    GWRXBFMODULE_CFG BF_cfg;
    RX_PHY2_1_SYNC_CFG SYNC_cfg;
    RX_DATA_2_1_CFG rx_data_cfg;
    int     user_num;
    int     slid_num;
    int     mode;
}BF_beamf_CFG;

template <class T>
class Cls_BF_beamf
{
	public:
		Cls_BF_beamf() {
            m_in = NULL;
            m_ifft = NULL;
            ah_in = NULL;
            p0_in = NULL;
            e0_in= NULL;
            BfMatrix_in= NULL;
            m_data_ram = NULL;;
            m_AH_ram = NULL;
            m_Freq_ram = NULL;
            m_out = NULL;
            m_sync_in = NULL;
            data_in = NULL;
            p0_buff = NULL;
            e0_buff = NULL;
            in_ram = NULL;
            AH_ram = NULL;
            m_shift = NULL;
		}
		~Cls_BF_beamf() {
            free(m_in);
            free(m_ifft);
            free(ah_in);
            free(p0_in);
            free(e0_in);
            free(BfMatrix_in);
            free(m_data_ram);
            free(m_AH_ram);
            free(m_Freq_ram);
            free(m_out);
            free(m_shift);
            free(m_sync_in);
            free(data_in);
            free(p0_buff);
            free(e0_buff);
            free(in_ram);
            free(AH_ram);
            fp_bf_modulation.close();
            fp_AH.close();
#ifdef DUMP_MATLAB_BF_beamf
        m_file_in_1.close();
        BF_in.close();
        BF_HA_in.close();
#endif
		}
		void			init(char* module_name);
		void			go();
		void 			reset();
		void			configure(BF_beamf_CFG cfg);
        Complex<double> m_in_1;
        Complex<double> m_in_2;
        Complex<double> m_in_3;
        Complex<double> m_in_4;
        Complex<double> m_in_5;
        Complex<double> m_in_6;
        Complex<double> m_in_7;
        Complex<double> m_in_8;
        bool            m_i_d_v;
        Complex<double> m_i_ah_1;
        Complex<double> m_i_ah_2;
        Complex<double> m_i_ah_3;
        Complex<double> m_i_ah_4;
        Complex<double> m_i_ah_5;
        Complex<double> m_i_ah_6;
        Complex<double> m_i_ah_7;
        Complex<double> m_i_ah_8;
        double          m_i_freq;
        bool            m_i_u_v;
        Complex<double> **m_out;
        int             **m_shift;
        Cls_GWRxBFModule<T> m_BF;
        Cls_bf<T>           m_beamf;
        Cls_Rx_Sync_2_1<T>  m_rx_sync;
        Cls_rx_data_phy_2_1<T> m_demod;
	private:
        ifstream 		fp_bf_modulation;
        ifstream 		fp_AH;
        BF_beamf_CFG    m_cfg;
        Complex<T>      *m_in;
        double          *m_ifft;
        double          *ah_in;
        double          *p0_in;
        double          *e0_in;
        double          *BfMatrix_in;
        int             m_en;
        Complex<double> **m_data_ram;
        Complex<double> **m_AH_ram;
        double          *m_Freq_ram;
        int             m_cnt_d;
        int             m_cnt_v;
        int             m_user_i;
        int             m_slid_i;
        Complex<double> *m_sync_in;
        int             m_RxDemod_2_1_en;
        Complex<double> *data_in;
        void            shift_sel();
        double          **p0_buff;
        double          **e0_buff;
        Complex<double> **in_ram;
        Complex<double> **AH_ram;

        void        get_input_msg();
        void        GWRxBFModule();
        void        GWRxbeamforming();
        void        RxDemod_2_1(Complex<double> *in);
        int         m_shift_in;
        ofstream        m_file_in_1;
        ofstream        BF_in;
        ofstream        BF_HA_in;
        ofstream        freqOffsetCh;

#ifdef DUMP_MATLAB_BF_beamf_beamf
        //ofstram       m_file_in;
#endif
};

template <class T>
void Cls_BF_beamf<T>::configure(BF_beamf_CFG cfg)
{
    m_cfg = cfg;
    //m_cfg.slid_num = slid_num;
	m_BF.configure(cfg.BF_cfg);
    m_beamf.configure(cfg.beamf_cfg);
    int rx_phy2_1_mode = cfg.mode;
    if(rx_phy2_1_mode == 1)
    {
        m_cfg.SYNC_cfg.len_FreqComp = 32;
        m_cfg.SYNC_cfg.TOScope = 32;
        m_cfg.SYNC_cfg.Fs = 7812.5;
        m_cfg.SYNC_cfg.sync_len = 12 * 16 + 32;
        m_cfg.SYNC_cfg.len_freq = 32;
        m_cfg.SYNC_cfg.freq_start = -77.5;
        m_cfg.SYNC_cfg.freq_inter = 5;
        m_cfg.SYNC_cfg.dsss_num = 4;
        m_cfg.SYNC_cfg.freq_drift_s = -15;
        m_cfg.SYNC_cfg.freq_drift_i = 30;
        m_cfg.SYNC_cfg.freq_drift_len = 2;
        m_cfg.SYNC_cfg.Sym_Num_Data = 112;
        m_cfg.SYNC_cfg.seg_num = 1;
        m_rx_sync.configure(m_cfg.SYNC_cfg);

        m_cfg.rx_data_cfg.SymNumData = 112;
        m_cfg.rx_data_cfg.FsRx = 7812.5;
        m_cfg.rx_data_cfg.data_FDComp_Num = 8;
        m_cfg.rx_data_cfg.data_FDComp_Delta = 8;
        m_cfg.rx_data_cfg.CRC_EN = 1;
        m_cfg.rx_data_cfg.DSSSNum = 4;
        m_cfg.rx_data_cfg.FskaddLen = 0;
        m_demod.config(m_cfg.rx_data_cfg);
    }
    if(rx_phy2_1_mode == 2)
    {
        m_cfg.SYNC_cfg.len_FreqComp = 64;
        m_cfg.SYNC_cfg.TOScope = 32;
        m_cfg.SYNC_cfg.Fs = 7812.5;
        m_cfg.SYNC_cfg.sync_len = 12 * 16 + 512;
        m_cfg.SYNC_cfg.len_freq = 64;
        m_cfg.SYNC_cfg.freq_start = -315;
        m_cfg.SYNC_cfg.freq_inter = 10;
        m_cfg.SYNC_cfg.dsss_num = 2;
        m_cfg.SYNC_cfg.freq_drift_s = 0;
        m_cfg.SYNC_cfg.freq_drift_i = 0;
        m_cfg.SYNC_cfg.freq_drift_len = 1;
        m_cfg.SYNC_cfg.Sym_Num_Data = 112;
        m_cfg.SYNC_cfg.seg_num = 1;
        m_rx_sync.configure(m_cfg.SYNC_cfg);

        m_cfg.rx_data_cfg.SymNumData = 112;
        m_cfg.rx_data_cfg.FsRx = 7812.5;
        m_cfg.rx_data_cfg.data_FDComp_Num = 8;
        m_cfg.rx_data_cfg.data_FDComp_Delta = 50;
        m_cfg.rx_data_cfg.CRC_EN = 1;
        m_cfg.rx_data_cfg.DSSSNum = 2;
        m_cfg.rx_data_cfg.FskaddLen = 0;
        m_demod.config(m_cfg.rx_data_cfg);
    }
    if(rx_phy2_1_mode == 3)
    {
        m_cfg.SYNC_cfg.len_FreqComp = 16;
        m_cfg.SYNC_cfg.TOScope = 32;
        m_cfg.SYNC_cfg.Fs = 7812.5;
        m_cfg.SYNC_cfg.sync_len = 12 * 16 + 512;
        m_cfg.SYNC_cfg.len_freq = 16;
        m_cfg.SYNC_cfg.freq_start = -300;
        m_cfg.SYNC_cfg.freq_inter = 40;
        m_cfg.SYNC_cfg.dsss_num = 1;
        m_cfg.SYNC_cfg.freq_drift_s = 0;
        m_cfg.SYNC_cfg.freq_drift_i = 0;
        m_cfg.SYNC_cfg.freq_drift_len = 1;
        m_cfg.SYNC_cfg.Sym_Num_Data = 112;
        m_cfg.SYNC_cfg.seg_num = 1;
        m_rx_sync.configure(m_cfg.SYNC_cfg);

        m_cfg.rx_data_cfg.SymNumData = 112;
        m_cfg.rx_data_cfg.FsRx = 7812.5;
        m_cfg.rx_data_cfg.data_FDComp_Num = 2;
        m_cfg.rx_data_cfg.data_FDComp_Delta = 300;
        m_cfg.rx_data_cfg.CRC_EN = 1;
        m_cfg.rx_data_cfg.DSSSNum = 1;
        m_cfg.rx_data_cfg.FskaddLen = 0;
        m_demod.config(m_cfg.rx_data_cfg);
    }
    if(rx_phy2_1_mode == 4)
    {
        m_cfg.SYNC_cfg.len_FreqComp = 16;
        m_cfg.SYNC_cfg.TOScope = 64;
        m_cfg.SYNC_cfg.Fs = 15625;
        m_cfg.SYNC_cfg.sync_len = 12 * 16 + 512;
        m_cfg.SYNC_cfg.len_freq = 16;
        m_cfg.SYNC_cfg.freq_start = -300;
        m_cfg.SYNC_cfg.freq_inter = 40;
        m_cfg.SYNC_cfg.dsss_num = 1;
        m_cfg.SYNC_cfg.freq_drift_s = 0;
        m_cfg.SYNC_cfg.freq_drift_i = 0;
        m_cfg.SYNC_cfg.freq_drift_len = 1;
        m_cfg.SYNC_cfg.Sym_Num_Data = 112;
        m_cfg.SYNC_cfg.seg_num = 1;
        m_rx_sync.configure(m_cfg.SYNC_cfg);

        m_cfg.rx_data_cfg.SymNumData = 112;
        m_cfg.rx_data_cfg.FsRx = 15625;
        m_cfg.rx_data_cfg.data_FDComp_Num = 2;
        m_cfg.rx_data_cfg.data_FDComp_Delta = 300;
        m_cfg.rx_data_cfg.CRC_EN = 1;
        m_cfg.rx_data_cfg.DSSSNum = 1;
        m_cfg.rx_data_cfg.FskaddLen = 0;
        m_demod.config(m_cfg.rx_data_cfg);
    }
    if(rx_phy2_1_mode == 5)
    {
        m_cfg.SYNC_cfg.len_FreqComp = 16;
        m_cfg.SYNC_cfg.TOScope = 32;
        m_cfg.SYNC_cfg.Fs = 31250;
        m_cfg.SYNC_cfg.sync_len = 12 * 16 + 512;
        m_cfg.SYNC_cfg.len_freq = 16;
        m_cfg.SYNC_cfg.freq_start = -300;
        m_cfg.SYNC_cfg.freq_inter = 40;
        m_cfg.SYNC_cfg.dsss_num = 1;
        m_cfg.SYNC_cfg.freq_drift_s = 0;
        m_cfg.SYNC_cfg.freq_drift_i = 0;
        m_cfg.SYNC_cfg.freq_drift_len = 1;
        m_cfg.SYNC_cfg.Sym_Num_Data = 112;
        m_cfg.SYNC_cfg.seg_num = 1;
        m_rx_sync.configure(m_cfg.SYNC_cfg);

        m_cfg.rx_data_cfg.SymNumData = 112;
        m_cfg.rx_data_cfg.FsRx = 31250;
        m_cfg.rx_data_cfg.data_FDComp_Num = 2;
        m_cfg.rx_data_cfg.data_FDComp_Delta = 150;
        m_cfg.rx_data_cfg.CRC_EN = 1;
        m_cfg.rx_data_cfg.DSSSNum = 1;
        m_cfg.rx_data_cfg.FskaddLen = 0;
        m_demod.config(m_cfg.rx_data_cfg);
    }
    if(rx_phy2_1_mode == 6)
    {
        m_cfg.SYNC_cfg.len_FreqComp = 16;
        m_cfg.SYNC_cfg.TOScope = 256;
        m_cfg.SYNC_cfg.Fs = 62500;
        m_cfg.SYNC_cfg.sync_len = 12 * 16 + 512;
        m_cfg.SYNC_cfg.len_freq = 16;
        m_cfg.SYNC_cfg.freq_start = -300;
        m_cfg.SYNC_cfg.freq_inter = 40;
        m_cfg.SYNC_cfg.dsss_num = 1;
        m_cfg.SYNC_cfg.freq_drift_s = 0;
        m_cfg.SYNC_cfg.freq_drift_i = 0;
        m_cfg.SYNC_cfg.freq_drift_len = 1;
        m_cfg.SYNC_cfg.Sym_Num_Data = 112;
        m_cfg.SYNC_cfg.seg_num = 1;
        m_rx_sync.configure(m_cfg.SYNC_cfg);

        m_cfg.rx_data_cfg.SymNumData = 112;
        m_cfg.rx_data_cfg.FsRx = 62500;
        m_cfg.rx_data_cfg.data_FDComp_Num = 2;
        m_cfg.rx_data_cfg.data_FDComp_Delta = 300;
        m_cfg.rx_data_cfg.CRC_EN = 1;
        m_cfg.rx_data_cfg.DSSSNum = 1;
        m_cfg.rx_data_cfg.FskaddLen = 0;
        m_demod.config(m_cfg.rx_data_cfg);
    }
    if(rx_phy2_1_mode == 7)
    {
        m_cfg.SYNC_cfg.len_FreqComp = 16;
        m_cfg.SYNC_cfg.TOScope = 256;
        m_cfg.SYNC_cfg.Fs = 125000;
        m_cfg.SYNC_cfg.sync_len = 12 * 16 + 512;
        m_cfg.SYNC_cfg.len_freq = 16;
        m_cfg.SYNC_cfg.freq_start = -300;
        m_cfg.SYNC_cfg.freq_inter = 40;
        m_cfg.SYNC_cfg.dsss_num = 1;
        m_cfg.SYNC_cfg.freq_drift_s = 0;
        m_cfg.SYNC_cfg.freq_drift_i = 0;
        m_cfg.SYNC_cfg.freq_drift_len = 1;
        m_cfg.SYNC_cfg.Sym_Num_Data = 112;
        m_cfg.SYNC_cfg.seg_num = 1;
        m_rx_sync.configure(m_cfg.SYNC_cfg);

        m_cfg.rx_data_cfg.SymNumData = 112;
        m_cfg.rx_data_cfg.FsRx = 125000;
        m_cfg.rx_data_cfg.data_FDComp_Num = 0;
        m_cfg.rx_data_cfg.data_FDComp_Delta = 0;
        m_cfg.rx_data_cfg.CRC_EN = 1;
        m_cfg.rx_data_cfg.DSSSNum = 1;
        m_cfg.rx_data_cfg.FskaddLen = 0;
        m_demod.config(m_cfg.rx_data_cfg);
    }
    if(rx_phy2_1_mode == 8)
    {
        m_cfg.SYNC_cfg.len_FreqComp = 16;
        m_cfg.SYNC_cfg.TOScope = 512;
        m_cfg.SYNC_cfg.Fs = 250000;
        m_cfg.SYNC_cfg.sync_len = 12 * 16 + 512;
        m_cfg.SYNC_cfg.len_freq = 16;
        m_cfg.SYNC_cfg.freq_start = -300;
        m_cfg.SYNC_cfg.freq_inter = 40;
        m_cfg.SYNC_cfg.dsss_num = 1;
        m_cfg.SYNC_cfg.freq_drift_s = 0;
        m_cfg.SYNC_cfg.freq_drift_i = 0;
        m_cfg.SYNC_cfg.freq_drift_len = 1;
        m_cfg.SYNC_cfg.Sym_Num_Data = 104;
        m_cfg.SYNC_cfg.seg_num = 1;
        m_rx_sync.configure(m_cfg.SYNC_cfg);

        m_cfg.rx_data_cfg.SymNumData = 104;
        m_cfg.rx_data_cfg.FsRx = 250000;
        m_cfg.rx_data_cfg.data_FDComp_Num = 0;
        m_cfg.rx_data_cfg.data_FDComp_Delta = 0;
        m_cfg.rx_data_cfg.CRC_EN = 1;
        m_cfg.rx_data_cfg.DSSSNum = 1;
        m_cfg.rx_data_cfg.FskaddLen = 0;
        m_demod.config(m_cfg.rx_data_cfg);
    }
        if(rx_phy2_1_mode == 9)
    {
        m_cfg.SYNC_cfg.len_FreqComp = 16;
        m_cfg.SYNC_cfg.TOScope = 512;
        m_cfg.SYNC_cfg.Fs = 500000;
        m_cfg.SYNC_cfg.sync_len = 12 * 16 + 512;
        m_cfg.SYNC_cfg.len_freq = 16;
        m_cfg.SYNC_cfg.freq_start = -300;
        m_cfg.SYNC_cfg.freq_inter = 40;
        m_cfg.SYNC_cfg.dsss_num = 1;
        m_cfg.SYNC_cfg.freq_drift_s = 0;
        m_cfg.SYNC_cfg.freq_drift_i = 0;
        m_cfg.SYNC_cfg.freq_drift_len = 1;
        m_cfg.SYNC_cfg.Sym_Num_Data = 104;
        m_cfg.SYNC_cfg.seg_num = 1;
        m_rx_sync.configure(m_cfg.SYNC_cfg);

        m_cfg.rx_data_cfg.SymNumData = 104;
        m_cfg.rx_data_cfg.FsRx = 500000;
        m_cfg.rx_data_cfg.data_FDComp_Num = 2;
        m_cfg.rx_data_cfg.data_FDComp_Delta = 300;
        m_cfg.rx_data_cfg.CRC_EN = 1;
        m_cfg.rx_data_cfg.DSSSNum = 1;
        m_cfg.rx_data_cfg.FskaddLen = 0;
        m_demod.config(m_cfg.rx_data_cfg);
    }
        if(rx_phy2_1_mode == 10)
    {
        m_cfg.SYNC_cfg.len_FreqComp = 16;
        m_cfg.SYNC_cfg.TOScope = 512;
        m_cfg.SYNC_cfg.Fs = 500000;
        m_cfg.SYNC_cfg.sync_len = 12 * 16 + 512;
        m_cfg.SYNC_cfg.len_freq = 16;
        m_cfg.SYNC_cfg.freq_start = -300;
        m_cfg.SYNC_cfg.freq_inter = 40;
        m_cfg.SYNC_cfg.dsss_num = 1;
        m_cfg.SYNC_cfg.freq_drift_s = 0;
        m_cfg.SYNC_cfg.freq_drift_i = 0;
        m_cfg.SYNC_cfg.freq_drift_len = 1;
        m_cfg.SYNC_cfg.Sym_Num_Data = 104;
        m_cfg.SYNC_cfg.seg_num = 1;
        m_rx_sync.configure(m_cfg.SYNC_cfg);

        m_cfg.rx_data_cfg.SymNumData = 104;
        m_cfg.rx_data_cfg.FsRx = 500000;
        m_cfg.rx_data_cfg.data_FDComp_Num = 2;
        m_cfg.rx_data_cfg.data_FDComp_Delta = 300;
        m_cfg.rx_data_cfg.CRC_EN = 1;
        m_cfg.rx_data_cfg.DSSSNum = 1;
        m_cfg.rx_data_cfg.FskaddLen = 104;
        m_demod.config(m_cfg.rx_data_cfg);
    }
}

template <class T>
void Cls_BF_beamf<T>::init(char* module_name)
{
    m_BF.init(module_name);
    m_beamf.init(module_name);
    m_rx_sync.init(module_name);
    m_demod.init(module_name);

    m_data_ram = safe_new(m_data_ram , m_cfg.BF_cfg.Antenna , m_cfg.BF_cfg.FFTLen);
    m_AH_ram = safe_new(m_AH_ram , m_cfg.BF_cfg.Antenna , m_cfg.user_num);
    m_Freq_ram = safe_new(m_Freq_ram , m_cfg.user_num);

    m_ifft = safe_new(m_ifft , m_cfg.beamf_cfg.ifft_len * 2 * 8);
    ah_in = safe_new(ah_in , m_cfg.beamf_cfg.ant_num * 2);
    p0_in = safe_new(p0_in , m_cfg.beamf_cfg.ant_num);
    e0_in = safe_new(e0_in , m_cfg.beamf_cfg.ant_num);
    BfMatrix_in = safe_new(BfMatrix_in, m_cfg.beamf_cfg.ant_num);
    m_out = safe_new(m_out , m_cfg.user_num , m_cfg.beamf_cfg.ifft_len * m_cfg.slid_num);
    m_shift = safe_new(m_shift , m_cfg.user_num , m_cfg.slid_num);
    m_sync_in = safe_new(m_sync_in , m_cfg.beamf_cfg.ifft_len * (m_cfg.slid_num - 1));
    data_in = safe_new(data_in , m_rx_sync.m_sync_out_len);
    p0_buff = safe_new(p0_buff , m_cfg.user_num , 8);
    e0_buff = safe_new(e0_buff , m_cfg.user_num , 8);
    in_ram = safe_new(in_ram,8,m_cfg.beamf_cfg.ifft_len*10);
    AH_ram = safe_new(AH_ram , 8 , 10);

    m_i_d_v = true;
    m_i_u_v = true;
    m_en = 0;
    m_cnt_d = 0;
    m_cnt_v = 0;
    m_user_i = 0;
    m_slid_i = 0;
    m_RxDemod_2_1_en = 0;


#ifdef DUMP_MATLAB_BF_beamf
    char filename_mat[300] = {};
    strcpy(filename_mat, module_name); strcat(filename_mat, "out_user.txt");        m_file_in_1.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "BF_in.txt");        BF_in.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "BF_HA_in.txt");        BF_HA_in.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "freqOffsetCh.txt");        freqOffsetCh.open(filename_mat);
#endif
	reset();
}

template <class T>
void Cls_BF_beamf<T>::reset()
{
    //zeros(m_rs_o_bit, m_len_rs_o_bit);
}

template <class T>
void Cls_BF_beamf<T>::go()
{
    get_input_msg();

    if(m_en && m_slid_i >= m_cfg.slid_num)
        m_en = 0;
    if(m_en && m_slid_i == 0)
    {
        m_slid_i++;
        m_en = 0;
    }
    if(m_en)
    {
        for (m_user_i = 0; m_user_i < m_cfg.user_num; m_user_i++)
        {
            //GWRxBFModule
            GWRxBFModule();

            GWRxbeamforming();

            if(m_RxDemod_2_1_en)
            {
                LOG2DEV << "user:"<<m_user_i + 1 << "\t" ;
                RxDemod_2_1(m_sync_in);
                LOG2DEV << "crc:"<<m_demod.m_crc_error<<endl;
            }
        }
        m_en = 0;
        m_slid_i++;
    }

}

template <class T>
void Cls_BF_beamf<T>::get_input_msg()
{
    if (!m_en)
    {
        if(m_i_d_v)
        {
            m_data_ram[0][m_cnt_d] = m_in_1;
            m_data_ram[1][m_cnt_d] = m_in_2;
            m_data_ram[2][m_cnt_d] = m_in_3;
            m_data_ram[3][m_cnt_d] = m_in_4;
            m_data_ram[4][m_cnt_d] = m_in_5;
            m_data_ram[5][m_cnt_d] = m_in_6;
            m_data_ram[6][m_cnt_d] = m_in_7;
            m_data_ram[7][m_cnt_d] = m_in_8;
            m_cnt_d++;
        }

        if(m_i_u_v)
        {
            m_AH_ram[0][m_cnt_v] = m_i_ah_1;
            m_AH_ram[1][m_cnt_v] = m_i_ah_2;
            m_AH_ram[2][m_cnt_v] = m_i_ah_3;
            m_AH_ram[3][m_cnt_v] = m_i_ah_4;
            m_AH_ram[4][m_cnt_v] = m_i_ah_5;
            m_AH_ram[5][m_cnt_v] = m_i_ah_6;
            m_AH_ram[6][m_cnt_v] = m_i_ah_7;
            m_AH_ram[7][m_cnt_v] = m_i_ah_8;

            m_Freq_ram[m_cnt_v] = m_i_freq;
            m_cnt_v++;
        }

        if(m_cnt_d ==  m_cfg.BF_cfg.FFTLen)
        {
            m_cnt_d = 0;
            m_cnt_v = 0;
            m_en = 1;
        }
    }
}

template <class T>
void Cls_BF_beamf<T>::GWRxBFModule()
{
    if(m_user_i == 0)
    {
        for (int ck0 = 0; ck0 < 8; ck0++)
        {
            for (int ck1 = 0; ck1 < m_cfg.BF_cfg.FFTLen; ck1++)
            {
                m_BF.m_in[ck0][ck1] = m_data_ram[ck0][ck1];
                BF_in << fixed<<setprecision(30) << m_data_ram[ck0][ck1].I <<endl;
                BF_in << fixed<<setprecision(30) << m_data_ram[ck0][ck1].Q << endl;
            }
        }
    }

    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        m_BF.m_AH[ck0] = m_AH_ram[ck0][m_user_i];
        BF_HA_in << fixed<<setprecision(30) << m_BF.m_AH[ck0].I <<endl;
        BF_HA_in << fixed<<setprecision(30) << m_BF.m_AH[ck0].Q << endl;
    }

    m_BF.m_freqOffsetCh = m_Freq_ram[m_user_i];
    freqOffsetCh << m_Freq_ram[m_user_i] << endl;

    m_BF.go();
}

template <class T>
void Cls_BF_beamf<T>::GWRxbeamforming()
{
    for (int ck0 = 0; ck0 < m_cfg.beamf_cfg.ant_num; ck0++)
    {
        for (int ck1 = 0; ck1 < m_cfg.beamf_cfg.ifft_len; ck1++)
        {
            m_ifft[2 * ck0 * m_cfg.beamf_cfg.ifft_len + 2 * ck1] = m_BF.m_SigRxOut[ck0][ck1].I;//m_BF.m_SigRxOut[ck0][ck1  + m_cfg.beamf_cfg.ifft_len * (5 * (m_slid_i - 1)+ m_user_i)].I;
            m_ifft[2 * ck0 * m_cfg.beamf_cfg.ifft_len + 2 * ck1 + 1] = m_BF.m_SigRxOut[ck0][ck1].Q;//m_BF.m_SigRxOut[ck0][ck1  + m_cfg.beamf_cfg.ifft_len * (5 * (m_slid_i - 1)+ m_user_i)].Q;
        }
    }

    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        ah_in[2 * ck0] = m_AH_ram[ck0][ m_user_i].I;
        ah_in[2 * ck0 + 1] = m_AH_ram[ck0][ m_user_i].Q;
        BfMatrix_in[ck0] = 1;
    }

    m_beamf.go(m_ifft, ah_in , BfMatrix_in);

    for (int ck0 = 0; ck0 < m_cfg.beamf_cfg.ifft_len; ck0++)
    {
        m_out[m_user_i][(m_slid_i - 1) * m_cfg.beamf_cfg.ifft_len + ck0].I = m_beamf.m_RxBeamforming_out[2 * ck0];
        m_out[m_user_i][(m_slid_i - 1) * m_cfg.beamf_cfg.ifft_len + ck0].Q = m_beamf.m_RxBeamforming_out[2 * ck0 + 1];
                m_shift[m_user_i][m_slid_i] = m_beamf.m_shift;
        m_file_in_1 << m_beamf.m_RxBeamforming_out[2 * ck0] << endl;
        m_file_in_1 << m_beamf.m_RxBeamforming_out[2 * ck0 + 1] << endl;
    }

    if(m_slid_i == m_cfg.slid_num - 1)
    {
        m_RxDemod_2_1_en = 1;
        shift_sel();
    }

    if(m_RxDemod_2_1_en)
    {
        for(int ck0 = 0;ck0 < (m_cfg.slid_num - 1) * m_cfg.beamf_cfg.ifft_len ; ck0++)
        {
            m_sync_in[ck0] = m_out[m_user_i][ + ck0] * DOUBLE_POW2[64 - m_shift_in];
        }
    }

}

template <class T>
void Cls_BF_beamf<T>:: shift_sel()
{
    m_shift_in = m_shift[m_user_i][0] + 1;
}

template <class T>
void Cls_BF_beamf<T>::RxDemod_2_1(Complex<double> *in)
{
    int sync_cnt = 0;
    int data_cnt = 0;
    int m_rx_demod_en = 0;
    Complex<double> correctionTiming[4] = {0};
    Complex<double> sync_in;
    m_rx_sync.reset();
    int seg_cnt = 0;
    while(1)
    {
        if(sync_cnt < (m_cfg.slid_num - 1) * m_cfg.beamf_cfg.ifft_len)
        {
            sync_in = in[sync_cnt];
        }
        else{
            sync_in.I = 0;
            sync_in.Q = 0;
        }
        m_rx_sync.go(sync_in);
        sync_cnt++;
        if(m_rx_sync.m_out_valid == 1)
        {
            data_in[data_cnt] = m_rx_sync.m_out;
            data_cnt++;

            if(data_cnt == (int)m_rx_sync.m_sync_out_len)
            {
                data_cnt = 0;
                m_rx_demod_en = 1;
            }
            if(m_rx_demod_en)
            {
                m_demod.m_avg_power = m_rx_sync.m_avg_power;
                mem_copy(correctionTiming , m_rx_sync.m_correctionTiming , 4);
                m_rx_sync.reset();
                m_demod.go(data_in , correctionTiming);

//                per = per + m_demod.m_crc_error;
                m_rx_demod_en = 0;
                seg_cnt = seg_cnt + 1;
                if(seg_cnt == m_cfg.SYNC_cfg.seg_num)
                    break;
            }
        }
    }
}


#endif
