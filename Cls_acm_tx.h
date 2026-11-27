/****************************************************************************
**
**  Copyright  (c)  2023-2028,  All Rights Reserved.
**
**  File          : Cls_acm_tx.h
**  Created By    :
**  Created On    :
**
**  History     :
**  Programmer    	Date        Rev       	Description
**  --------------- ---------- 	-------- ------------------------------
**
**  Notice: Interference Suppression
**
****************************************************************************/
#ifndef __Cls_is
#define __Cls_is

#define DUMP_MATLAB_ACM_TX
#define DUMP_RTL
#include "common_type.h"
#include "Cls_ddc.h"
#include "/home/qswang/work/K2301/sim/tb_fft8192_dit/c/fft.h"

#include "Lib_dump.h"
#include "Lib_fixpoint.h"
#include "Lib_random.h"
typedef struct ACM_TX_CFG
{
    //Read Write
    int     reg_sampling_rate; //0 125kHz, 1 250kHz, 2 500kHz
    int     reg_rx_fft_len;
    int     reg_gain[16];
    int     reg_channel_choose;//
    int     reg_rx_fft_len_t;//
    int     reg_antenna_num[8];
    int     reg_rx_acm_num;
    int     reg_tone_switch;
    int     reg_freq[16];
    int     reg_phase[16];
    int     debug_tx1_rx8;//bitmatch use

}ACM_TX_CFG;

template <class T>
class Cls_acm_tx
{
	public:
		Cls_acm_tx() {
            m_in = NULL;
		}
		~Cls_acm_tx() {
            free(m_in);
#ifdef DUMP_MATLAB_ACM_TX
            m_file_o.close();
            m_file_ddc_1.close();
            m_file_ddc_2.close();
            m_file_ddc_3.close();
            m_file_ddc_4.close();
            m_file_ddc_5.close();
            m_file_ddc_6.close();
            m_file_ddc_7.close();
            m_file_ddc_8.close();
            m_file_ddc_9.close();
            m_file_ddc_10.close();
            m_file_ddc_11.close();
            m_file_ddc_12.close();
            m_file_ddc_13.close();
            m_file_ddc_14.close();
            m_file_ddc_15.close();
            m_file_ddc_16.close();
#endif
#ifdef DUMP_RTL
            fp_data_in.close();
            fp_reg_in.close();
            fp_data_out.close();
	    fp_data_out0.close();
	    fp_data_out1.close();
	    fp_data_out2.close();
	    fp_data_out3.close();
	    fp_data_out4.close();
	    fp_data_out5.close();
	    fp_data_out6.close();
	    fp_data_out7.close();

            fp_out_test0.close();
            fp_out_test1.close();
	    fp_out_test2.close();
	    fp_log.close();

#endif

		}
		int         bitrev(int in , int bitwidth);
		void			init(char* module_name);
		void			go();
		void 			reset();
		void			configure(ACM_TX_CFG cfg);
	private:
        ACM_TX_CFG      m_cfg;
        Complex<T>      *m_in;
        Cls_ddc<T>      m_ddc_1;
        Cls_ddc<T>      m_ddc_2;
        Cls_ddc<T>      m_ddc_3;
        Cls_ddc<T>      m_ddc_4;
        Cls_ddc<T>      m_ddc_5;
        Cls_ddc<T>      m_ddc_6;
        Cls_ddc<T>      m_ddc_7;
        Cls_ddc<T>      m_ddc_8;
        Cls_ddc<T>      m_ddc_9;
        Cls_ddc<T>      m_ddc_10;
        Cls_ddc<T>      m_ddc_11;
        Cls_ddc<T>      m_ddc_12;
        Cls_ddc<T>      m_ddc_13;
        Cls_ddc<T>      m_ddc_14;
        Cls_ddc<T>      m_ddc_15;
        Cls_ddc<T>      m_ddc_16;
        Complex<double> m_ddc_o[16];
        Complex<T>      m_out;
        T               m_phase[16];
        bool            m_tone_en[16];
        int             m_fft_len;
        int             m_freq[16];
        T               m_gain;
        double          m_gain_d[16];
        T               m_phaset;//
        T               m_freqt;//
        T               m_dump_t;

        Complex<T>      m_test0;
        Complex<T>      m_test1;
        Complex<T>      m_test2;
        Complex<T>      m_log;

        DDC_Cfg         m_ddc_cfg;
        ClsFFT<fxp>	    m_fft;
        double          m_times;
#ifdef DUMP_MATLAB_ACM_TX
        ofstream        m_file_o;
        ofstream        m_file_fft_in;
        ofstream        m_file_fft_out;
        ofstream        m_file_ddc_1;
        ofstream        m_file_ddc_2;
        ofstream        m_file_ddc_3;
        ofstream        m_file_ddc_4;
        ofstream        m_file_ddc_5;
        ofstream        m_file_ddc_6;
        ofstream        m_file_ddc_7;
        ofstream        m_file_ddc_8;
        ofstream        m_file_ddc_9;
        ofstream        m_file_ddc_10;
        ofstream        m_file_ddc_11;
        ofstream        m_file_ddc_12;
        ofstream        m_file_ddc_13;
        ofstream        m_file_ddc_14;
        ofstream        m_file_ddc_15;
        ofstream        m_file_ddc_16;
#endif
#ifdef DUMP_RTL
        std::ofstream           fp_data_in   ;
        std::ofstream           fp_reg_in    ;
        std::ofstream           fp_data_out  ;
        std::ofstream           fp_data_out0 ;
        std::ofstream           fp_data_out1 ;
        std::ofstream           fp_data_out2 ;
        std::ofstream           fp_data_out3 ;
        std::ofstream           fp_data_out4 ;
        std::ofstream           fp_data_out5 ;
        std::ofstream           fp_data_out6 ;
        std::ofstream           fp_data_out7 ;

        std::ofstream           fp_out_test0 ;
        std::ofstream           fp_out_test1 ;
        std::ofstream           fp_out_test2 ;
	std::ofstream           fp_log ;
#endif
};

template <class T>
void Cls_acm_tx<T>::configure(ACM_TX_CFG cfg)
{
	m_cfg = cfg;
	if(m_cfg.reg_sampling_rate == 0)
        m_times = 2;
    else if(m_cfg.reg_sampling_rate == 1)
        m_times = 4;
    else if(m_cfg.reg_sampling_rate == 2)
        m_times = 8;

    for(int ck0 = 0; ck0 < 16; ck0++)
    {
        if(m_cfg.reg_freq[ck0] > 1024)
            m_freq[ck0] = m_cfg.reg_freq[ck0] - 2048;
        else
            m_freq[ck0] = m_cfg.reg_freq[ck0];
    }

    for(int ck0 = 0; ck0 < 16; ck0++) {
        m_tone_en[ck0] = (m_cfg.reg_tone_switch >> ck0) & 0x1;
        m_gain = m_cfg.reg_gain[ck0] * 1.0 / 65536;
        m_gain_d[ck0] = m_gain;
    }

    if(m_cfg.reg_rx_fft_len == 0)
        m_fft_len = 256;
    else if(m_cfg.reg_rx_fft_len == 1)
        m_fft_len = 512;
    else if(m_cfg.reg_rx_fft_len == 2)
        m_fft_len = 1024;
    else if(m_cfg.reg_rx_fft_len == 3)
        m_fft_len = 2048;
    else if(m_cfg.reg_rx_fft_len == 4)
        m_fft_len = 4096;
    else if(m_cfg.reg_rx_fft_len == 5)
        m_fft_len = 8192;
	
}


template <class T>
void Cls_acm_tx<T>::init(char* module_name)
{
    //fix_set(m_NB_fxp, 8, 7, FXP_US, FXP_TRN, FXP_SAT_SYM);

    fix_set(m_out, 12, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_gain, 16, 0, FXP_US, FXP_RND, FXP_SAT_SYM);

    fix_set_group(m_phase, 16, 18, 3, FXP_TC, FXP_RND, FXP_SAT_SYM);
    //fix_set(m_test0 ,21, 3, FXP_TC, FXP_RND, FXP_SAT_SYM);//dump pi
    fix_set(m_test0 ,14, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_test1 ,14, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_test2 ,14, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);

    fix_set(m_phaset, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
    fix_set(m_freqt, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
    fix_set(m_dump_t, 21, 3, FXP_TC, FXP_RND, FXP_WRAP);


    for(int ck0 = 0; ck0 < 16; ck0++)
    {
        if(m_cfg.reg_phase[ck0] > 131072)
            m_phase[ck0] = (m_cfg.reg_phase[ck0] - 262144) * DOUBLE_POW2[64 - 15];
        else
            m_phase[ck0] = m_cfg.reg_phase[ck0] * DOUBLE_POW2[64 - 15];
    }

    double phase_d;
    phase_d = m_phase[0];  m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[0] * 1.0 / m_fft_len * 2 / m_times; m_ddc_1.configure(m_ddc_cfg);
    phase_d = m_phase[1];  m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[1] * 1.0 / m_fft_len * 2 / m_times;  m_ddc_2.configure(m_ddc_cfg);
    phase_d = m_phase[2];  m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[2] * 1.0 / m_fft_len * 2 / m_times;  m_ddc_3.configure(m_ddc_cfg);
    phase_d = m_phase[3];  m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[3] * 1.0 / m_fft_len * 2 / m_times;  m_ddc_4.configure(m_ddc_cfg);
    phase_d = m_phase[4];  m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[4] * 1.0 / m_fft_len * 2 / m_times;  m_ddc_5.configure(m_ddc_cfg);
    phase_d = m_phase[5];  m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[5] * 1.0 / m_fft_len * 2 / m_times;  m_ddc_6.configure(m_ddc_cfg);
    phase_d = m_phase[6];  m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[6] * 1.0 / m_fft_len * 2 / m_times;  m_ddc_7.configure(m_ddc_cfg);
    phase_d = m_phase[7];  m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[7] * 1.0 / m_fft_len * 2 / m_times;  m_ddc_8.configure(m_ddc_cfg);
    phase_d = m_phase[8];  m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[8] * 1.0 / m_fft_len * 2 / m_times;  m_ddc_9.configure(m_ddc_cfg);
    phase_d = m_phase[9];  m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[9] * 1.0 / m_fft_len * 2 / m_times;  m_ddc_10.configure(m_ddc_cfg);
    phase_d = m_phase[10]; m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[10] * 1.0 / m_fft_len * 2 / m_times; m_ddc_11.configure(m_ddc_cfg);
    phase_d = m_phase[11]; m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[11] * 1.0 / m_fft_len * 2 / m_times; m_ddc_12.configure(m_ddc_cfg);
    phase_d = m_phase[12]; m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[12] * 1.0 / m_fft_len * 2 / m_times; m_ddc_13.configure(m_ddc_cfg);
    phase_d = m_phase[13]; m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[13] * 1.0 / m_fft_len * 2 / m_times; m_ddc_14.configure(m_ddc_cfg);
    phase_d = m_phase[14]; m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[14] * 1.0 / m_fft_len * 2 / m_times; m_ddc_15.configure(m_ddc_cfg);
    phase_d = m_phase[15]; m_ddc_cfg.ang_init = phase_d * 205887 * DOUBLE_POW2[64 - 16];  m_ddc_cfg.ang_delta = m_freq[15] * 1.0 / m_fft_len * 2 / m_times; m_ddc_16.configure(m_ddc_cfg);

    m_ddc_1.init();
    m_ddc_2.init();
    m_ddc_3.init();
    m_ddc_4.init();
    m_ddc_5.init();
    m_ddc_6.init();
    m_ddc_7.init();
    m_ddc_8.init();
    m_ddc_9.init();
    m_ddc_10.init();
    m_ddc_11.init();
    m_ddc_12.init();
    m_ddc_13.init();
    m_ddc_14.init();
    m_ddc_15.init();
    m_ddc_16.init();

    //mkdir Folder
#ifdef DUMP_MATLAB_ACM_TX
    struct stat statExist;
    int statMd = 0;
    if (stat(module_name, &statExist) != 0) {
            statMd = mkdir(module_name,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
            if (statMd)
                LOG2DEV << "Cannot create folder.\n";
    }
    char filename_mat[300] = {};
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_o.txt");       m_file_o.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_fft_in.txt");      m_file_fft_in.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_fft_out.txt");     m_file_fft_out.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_1.txt");       m_file_ddc_1.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_2.txt");       m_file_ddc_2.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_3.txt");       m_file_ddc_3.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_4.txt");       m_file_ddc_4.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_5.txt");       m_file_ddc_5.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_6.txt");       m_file_ddc_6.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_7.txt");       m_file_ddc_7.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_8.txt");       m_file_ddc_8.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_9.txt");       m_file_ddc_9.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_10.txt");      m_file_ddc_10.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_11.txt");      m_file_ddc_11.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_12.txt");      m_file_ddc_12.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_13.txt");      m_file_ddc_13.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_14.txt");      m_file_ddc_14.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_15.txt");      m_file_ddc_15.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "m_ddc_16.txt");      m_file_ddc_16.open(filename_mat);
#endif
#ifdef DUMP_RTL
    char filename_rtl[300] = {};
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "data_in.dat");         fp_data_in.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "reg_in.dat");          fp_reg_in.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "data_out.dat");        fp_data_out.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "data_out0.dat");       fp_data_out0.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "data_out1.dat");       fp_data_out1.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "data_out2.dat");       fp_data_out2.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "data_out3.dat");       fp_data_out3.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "data_out4.dat");       fp_data_out4.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "data_out5.dat");       fp_data_out5.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "data_out6.dat");       fp_data_out6.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "data_out7.dat");       fp_data_out7.open(filename_rtl);

    strcpy(filename_rtl, module_name); strcat(filename_rtl, "test0_out.dat");       fp_out_test0.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "test1_out.dat");       fp_out_test1.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "test2_out.dat");       fp_out_test2.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "debug.log");           fp_log.open(filename_rtl);

#endif
#ifdef DUMP_RTL
    fp_reg_in <<  m_cfg.reg_tone_switch<<endl;
    for(int ck0 = 0; ck0 < 16; ck0++) {
       fp_reg_in<<m_cfg.reg_freq[ck0]<<endl;
    }
    for(int ck0 = 0; ck0 < 16; ck0++){
       fp_reg_in << m_cfg.reg_phase[ck0] <<endl;
    }
    fp_reg_in << m_cfg.reg_rx_fft_len  <<endl;
    for(int ck0 = 0; ck0 < 16; ck0++){
       fp_reg_in<< m_cfg.reg_gain[ck0] <<endl;
       
    }
    fp_reg_in << m_cfg.reg_sampling_rate<<endl;

    // m_test0 = 205887 * DOUBLE_POW2[64 - 16];
    // dump(fp_out_test0,&m_test0,1,m_test0);
for (int ck0 =0;ck0 < 16; ck0++) {
      phase_d  = m_phase[ck0];
      m_phaset = phase_d * 205887 * DOUBLE_POW2[64 - 16];
      m_freqt  = m_freq[ck0] * 1.0 / m_fft_len * 2 / m_times ;
      dump(fp_log, &m_phaset, 1, m_phaset);
      dump(fp_log, &m_freqt, 1, m_freqt);
}

phase_d  = m_phase[1];
m_dump_t = 205887 * DOUBLE_POW2[64 - 16];
m_phaset = phase_d * m_dump_t;
dump(fp_log, &m_phaset, 1, m_phaset);
dump(fp_log, &m_dump_t, 1, m_dump_t);

#endif

    m_fft.init((int)log2(m_cfg.reg_rx_fft_len_t), module_name,1,0);

	reset();
}

template <class T>
void Cls_acm_tx<T>::reset()
{
    zeros(m_ddc_o, 16);
}

template <class T>
int Cls_acm_tx<T>:: bitrev(int in , int bitwidth)
{
    int i   = 0;
    int tmp = 0;
    for (i = 0 ; i< bitwidth ;i ++){
        tmp <<= 1;
        tmp += in&0x1;
        in >>= 1;
    }
    return tmp;
}


template <class T>
void Cls_acm_tx<T>::go()
{
    Complex<double> output;
    Complex<double> output0;////qswang
    Complex<double> output1;
    Complex<double> output2;
    Complex<double> output3;
    Complex<double> output4;
    Complex<double> output5;
    Complex<double> output6;
    Complex<double> output7;
    Complex<double> m_fft_in[32768];
    Complex<double> m_fft_out[32768];
    Complex<double> in;
    in.I = 1;
    in.Q = 0;
    double gain_times;
    int test_cnt=0;//qswang
    int  channel_num;//rx
    int  rx_fft_len;//rx debug
    Complex<double> ddc_remove;//ddc remove
    double mem_testi[8192];
    double mem_testq[8192];
    int adr = 0;
    int m_addr_reversed[8192];
    Complex<T> sum_8_1;//8tx->1rx
#ifdef DUMP_RTL

     if(m_cfg.reg_rx_acm_num == 9)
        channel_num = 1;
     else if(m_cfg.reg_channel_choose==0)
        channel_num = 1;
     else if(m_cfg.reg_channel_choose==1)
         channel_num = 2;
     else if(m_cfg.reg_channel_choose==2)
         channel_num = 4;
     else if(m_cfg.reg_channel_choose==3)
         channel_num = 6;
     else if(m_cfg.reg_channel_choose==4)
         channel_num = 8;
     else
         channel_num = 1;
    //cout << m_cfg.reg_channel_choose <<endl;
    fp_reg_in << m_cfg.reg_channel_choose  <<endl;
     if(m_cfg.reg_rx_fft_len_t==256)
        rx_fft_len = 0;
     else if(m_cfg.reg_rx_fft_len_t==512)
         rx_fft_len = 1;
     else if(m_cfg.reg_rx_fft_len_t==1024)
         rx_fft_len = 2;
     else if(m_cfg.reg_rx_fft_len_t==2048)
         rx_fft_len = 3;
     else if(m_cfg.reg_rx_fft_len_t==4096)
         rx_fft_len = 4;
     else if(m_cfg.reg_rx_fft_len_t==8192)
         rx_fft_len = 5;
    //cout << m_cfg.reg_rx_fft_len_t <<endl;
    fp_reg_in << rx_fft_len  <<endl;
    //8tx or 7tx
    int  tx_1antenna_num =  m_cfg.reg_antenna_num[0];
    int  tx_2antenna_num =  m_cfg.reg_antenna_num[1] + tx_1antenna_num;
    int  tx_3antenna_num =  m_cfg.reg_antenna_num[2] + tx_2antenna_num;
    int  tx_4antenna_num =  m_cfg.reg_antenna_num[3] + tx_3antenna_num;
    int  tx_5antenna_num =  m_cfg.reg_antenna_num[4] + tx_4antenna_num;
    int  tx_6antenna_num =  m_cfg.reg_antenna_num[5] + tx_5antenna_num;
    int  tx_7antenna_num =  m_cfg.reg_antenna_num[6] + tx_6antenna_num;
    int  tx_8antenna_num =  m_cfg.reg_antenna_num[7] + tx_7antenna_num;

    for (int i = 0;i < 8 ; i++){
        fp_reg_in << m_cfg.reg_antenna_num[i]  <<endl;
    }
    fp_reg_in << m_cfg.reg_rx_acm_num <<endl;
    fp_reg_in << m_cfg.debug_tx1_rx8 <<endl;
    //reverse
    for(int i=0;i<m_fft_len;i++){
            m_addr_reversed[i] = bitrev(i,(int)log2(m_cfg.reg_rx_fft_len_t));
    }
    fix_set(m_log ,14, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
 #endif

    while(1)
    {
        output = 0.0;
        gain_times = (m_tone_en[0] == 1)  ? m_gain_d[0] : 0; m_ddc_o[0] = gain_times * m_ddc_1.rot(in);   output = m_ddc_o[0] + output;
        gain_times = (m_tone_en[1] == 1)  ? m_gain_d[1] : 0; m_ddc_o[1] = gain_times * m_ddc_2.rot(in);   output = m_ddc_o[1] + output;
        gain_times = (m_tone_en[2] == 1)  ? m_gain_d[2] : 0; m_ddc_o[2] = gain_times * m_ddc_3.rot(in);   output = m_ddc_o[2] + output;
        gain_times = (m_tone_en[3] == 1)  ? m_gain_d[3] : 0; m_ddc_o[3] = gain_times * m_ddc_4.rot(in);   output = m_ddc_o[3] + output;
        gain_times = (m_tone_en[4] == 1)  ? m_gain_d[4] : 0; m_ddc_o[4] = gain_times * m_ddc_5.rot(in);   output = m_ddc_o[4] + output;
        gain_times = (m_tone_en[5] == 1)  ? m_gain_d[5] : 0; m_ddc_o[5] = gain_times * m_ddc_6.rot(in);   output = m_ddc_o[5] + output;
        gain_times = (m_tone_en[6] == 1)  ? m_gain_d[6] : 0; m_ddc_o[6] = gain_times * m_ddc_7.rot(in);   output = m_ddc_o[6] + output;
        gain_times = (m_tone_en[7] == 1)  ? m_gain_d[7] : 0; m_ddc_o[7] = gain_times * m_ddc_8.rot(in);   output = m_ddc_o[7] + output;
        gain_times = (m_tone_en[8] == 1)  ? m_gain_d[8] : 0; m_ddc_o[8] = gain_times * m_ddc_9.rot(in);   output = m_ddc_o[8] + output;
        gain_times = (m_tone_en[9] == 1)  ? m_gain_d[9] : 0; m_ddc_o[9] = gain_times * m_ddc_10.rot(in);  output = m_ddc_o[9] + output;
        gain_times = (m_tone_en[10] == 1) ? m_gain_d[10] : 0; m_ddc_o[10] = gain_times * m_ddc_11.rot(in); output = m_ddc_o[10] + output;
        gain_times = (m_tone_en[11] == 1) ? m_gain_d[11] : 0; m_ddc_o[11] = gain_times * m_ddc_12.rot(in); output = m_ddc_o[11] + output;
        gain_times = (m_tone_en[12] == 1) ? m_gain_d[12] : 0; m_ddc_o[12] = gain_times * m_ddc_13.rot(in); output = m_ddc_o[12] + output;
        gain_times = (m_tone_en[13] == 1) ? m_gain_d[13] : 0; m_ddc_o[13] = gain_times * m_ddc_14.rot(in); output = m_ddc_o[13] + output;
        gain_times = (m_tone_en[14] == 1) ? m_gain_d[14] : 0; m_ddc_o[14] = gain_times * m_ddc_15.rot(in); output = m_ddc_o[14] + output;
        gain_times = (m_tone_en[15] == 1) ? m_gain_d[15] : 0; m_ddc_o[15] = gain_times * m_ddc_16.rot(in); output = m_ddc_o[15] + output;
        m_out = output;
#ifdef DUMP_MATLAB_ACM_TX
        m_file_o << m_out.I << "\t" << m_out.Q << endl;
#endif // DUMP_MATLAB_ACM_TX

#ifdef DUMP_RTL

	/// 8tx or 7tx----------------------
	output0 = 0.0;output1 = 0.0;output2 = 0.0;output3 = 0.0;output4 = 0.0;
	output5 = 0.0;output6 = 0.0;output7 = 0.0;//qswag

        for(int i = 0; i < (tx_1antenna_num) ; i ++){
          output0 = m_ddc_o[i] + output0;
          //cout << i<<endl;
        }
        for(int i = tx_1antenna_num;i < tx_2antenna_num ;i++){
          output1 = m_ddc_o[i] + output1;
        }
        for(int i = tx_2antenna_num;i < tx_3antenna_num ;i++){
          output2 = m_ddc_o[i] + output2;
        }
        for(int i = tx_3antenna_num;i < tx_4antenna_num ;i++){
          output3 = m_ddc_o[i] + output3;
        }
        for(int i = tx_4antenna_num;i < tx_5antenna_num ;i++){
          output4 = m_ddc_o[i] + output4;
        }
        for(int i = tx_5antenna_num;i < tx_6antenna_num ;i++){
          output5 = m_ddc_o[i] + output5;
        }
        for(int i = tx_6antenna_num;i < tx_7antenna_num ;i++){
          output6 = m_ddc_o[i] + output6;
        }
        for(int i = tx_7antenna_num;i < tx_8antenna_num ;i++){
          output7 = m_ddc_o[i] + output7;
          //cout << i<<endl;
        }

        #endif

        ///------------------------
#ifdef DUMP_RTL
	//rx debug(1tx -> 8rx)-----------------
        if(m_cfg.reg_rx_acm_num  == 0 && m_cfg.debug_tx1_rx8  == 1 && test_cnt%2 == 1){
            m_test0 = m_out;
            dump(fp_out_test0,&m_test0,1,m_test0.I);
            m_fft_in[adr]  = m_test0;
            adr = adr + 1;
            if(adr == m_cfg.reg_rx_fft_len_t){
                ddc_remove.I = 0;
                ddc_remove.Q = 0;
                for (int ck1 = 0; ck1 < m_cfg.reg_rx_fft_len_t; ck1++) {
                    ddc_remove = m_fft_in[ck1] + ddc_remove; //ddc remove
                }
                m_test1 = ddc_remove * 1.0 / m_cfg.reg_rx_fft_len_t;
                ddc_remove = m_test1;
                adr = 0;

                for (int j=0 ; j < m_cfg.reg_rx_fft_len_t ; j ++ ){
                    m_fft_in[j] = m_fft_in[j] - ddc_remove;
                }

                for (int i = 0 ; i < channel_num ;i ++){
                    m_fft.fft_n(m_fft_in, m_fft_out, 1, 0, 1);//fft
                    for (int ck0 = 0; ck0 < m_fft_len ; ck0++) {
                        m_file_fft_in << m_fft_in[ck0].I << "\t" << m_fft_in[ck0].Q << endl;
                        m_file_fft_out << m_fft_out[ck0].I << "\t" << m_fft_out[ck0].Q << endl;
                    }
                    for (int j=0 ; j < m_cfg.reg_rx_fft_len_t ; j ++ ){ //fft in
                         m_test1 = m_fft_in[m_addr_reversed[j]];
                         dump(fp_out_test1,&m_test1,1,m_test1.I);
                    }
                }
            }
        }

        dump(fp_data_out ,&m_out,1,m_out.I);
///// 8tx -> 1rx------------------------------
	m_out = output0;
	sum_8_1 = m_out;//rx1
	dump(fp_data_out0 ,&m_out,1,m_out.I);
	m_out = output1;
	sum_8_1 =  sum_8_1 + m_out;//rx2
        dump(fp_data_out1 ,&m_out,1,m_out.I);
	m_out = output2;
	sum_8_1 = sum_8_1 + m_out;//rx3
        dump(fp_data_out2 ,&m_out,1,m_out.I);
	m_out = output3;
	sum_8_1 = sum_8_1 + m_out;//rx4
        dump(fp_data_out3 ,&m_out,1,m_out.I);
	m_out = output4;
	sum_8_1 = sum_8_1 + m_out;//rx5
        dump(fp_data_out4 ,&m_out,1,m_out.I);
	m_out = output5;
	sum_8_1 = sum_8_1 + m_out;//rx6
        dump(fp_data_out5 ,&m_out,1,m_out.I);
	m_out = output6;
	sum_8_1 = sum_8_1 + m_out;//rx7
        dump(fp_data_out6 ,&m_out,1,m_out.I);
	m_out = output7;
	sum_8_1 = sum_8_1 + m_out;//rx8
        dump(fp_data_out7 ,&m_out,1,m_out.I);
	m_out = sum_8_1;
        m_test2 = m_out;//rx

	dump(fp_out_test2,&m_test2,1,m_test2.I);//8tx -> 1rx

        if((m_cfg.reg_rx_acm_num != 0 || m_cfg.debug_tx1_rx8  == 0) && test_cnt%2 == 1){ // antenna = 1 --> amc antenna
	   m_fft_in[adr]  = m_test2;
	   adr = adr + 1;
	   if(adr == m_cfg.reg_rx_fft_len_t){
                ddc_remove.I = 0;
                ddc_remove.Q = 0;
                for (int ck1 = 0; ck1 < m_cfg.reg_rx_fft_len_t; ck1++) {
                    ddc_remove = m_fft_in[ck1] + ddc_remove; //ddc remove
                }
                m_test2 = ddc_remove * 1.0 / m_cfg.reg_rx_fft_len_t ;
                ddc_remove = m_test2;
                adr = 0;

               for (int j=0 ; j < m_cfg.reg_rx_fft_len_t ; j ++ ){
                  m_test2 = m_fft_in[j] - ddc_remove;
		  m_fft_in[j]  = m_test2;
               }

               for (int i = 0 ; i < channel_num ;i ++){
                   m_fft.fft_n(m_fft_in, m_fft_out, 1, 0, 1);//fft
                      for (int ck0 = 0; ck0 < m_fft_len ; ck0++) {
                          m_file_fft_in << m_fft_in[ck0].I << "\t" << m_fft_in[ck0].Q << endl;
                          m_file_fft_out << m_fft_out[ck0].I << "\t" << m_fft_out[ck0].Q << endl;
                      }
	           for (int j=0 ; j < m_cfg.reg_rx_fft_len_t ; j ++ ){
     	              m_test1 = m_fft_in[m_addr_reversed[j]];
	              dump(fp_out_test1,&m_test1,1,m_test1.I);
	           }
	       }
	   }
        }
       #endif

        //---------------
        test_cnt = test_cnt + 1;
           if(test_cnt == 65536)
               break;
     }
}

#endif

