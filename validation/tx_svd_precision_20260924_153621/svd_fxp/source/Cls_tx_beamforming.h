/****************************************************************************
**
**  Copyright  (c)  2023-2028,  All Rights Reserved.
**
**  File          : Cls_tx_beamforming.h
**  Created By    :
**  Created On    :
**
**  History     :
**  Programmer    	Date        Rev       	Description
**  --------------- ---------- 	-------- ------------------------------
**
**  Notice: FFT & User Detection
**
****************************************************************************/
#ifndef __Cls_tx_beamforming
#define __Cls_tx_beamforming

//#define DUMP_MATLAB_tx_beamforming
//#define DUMP_RTL_tx_beamforming

#include "common_type.h"
#include <cmath>
#include "Lib_complex.h"
#include "Lib_fixpoint.h"
#include "Lib_random.h"
#include "Lib_utility.h"
#include "Cls_svd.h"

int power2_diff[33] = { 32768, 26029, 20675, 16423, 13045, 10362, 8231, 6538,
                    5193,  4125,  3277,  2603 , 2068,  1642 , 1305, 1036,
                    823,   654 ,  519,   413,   328,   260,   207 , 164,
                    130,   104,   82,    65,    52,    41,    33,   26,
                    21};

typedef struct TX_BEAMFORMING_CFG
{
    int user_num_i;
    int RecipracalCaliFlag;
    int GWTxPowerFlag;
    int reg_mmse;
    int reg_bw_overlop_w;
    int reg_total_pow;
    int reg_bw_overlop;
    double reg_pow_diff1;
    double reg_pow_diff2;
    double reg_pow_diff3;
}TX_BEAMFORMING_CFG;

template <class T>
class Cls_tx_beamforming
{
	public:
		Cls_tx_beamforming() {
            m_in = NULL;
            m_GWRxAH = NULL;
            m_GWRxPilotPower = NULL;
            m_Hhat = NULL;
            m_Freq = NULL;
            m_HNormal = NULL;
            m_BW = NULL;
            m_BFNumIndex = NULL;
            m_BeamformerMatrix = NULL;
            m_HNormalTemp = NULL;
            m_MMSEBeamformerTmp = NULL;
            m_pinv_in = NULL;
            m_MMSEBeamformer = NULL;
            m_PowerPerUser = NULL;
            m_PerPower1 = NULL;
            m_txdata_power = NULL;
            m_ah_i = NULL;
            m_anoise_sqrt_i = NULL;
            m_cali_factor = NULL;
            m_freq_off = NULL;
            m_pilot_power_i = NULL;
            m_beamforming_matrix_tx_o = NULL;
            m_TempTxPower = NULL;
            m_TxPower0 = NULL;
            m_TxPowerPer = NULL;
            m_TxPower1 = NULL;
            m_U_est = NULL;
            m_V_est = NULL;
            m_S_est = NULL;
		}
		~Cls_tx_beamforming() {
            free(m_TempTxPower);
            free(m_in);
            free(m_GWRxAH);
            free(m_GWRxPilotPower);
            free(m_Hhat);
            free(m_Freq);
            free(m_HNormal);
            free(m_BW);
            free(m_BFNumIndex);
            free(m_BeamformerMatrix);
            free(m_HNormalTemp);
            free(m_MMSEBeamformerTmp);
            free(m_pinv_in);
            free(m_MMSEBeamformer);
            free(m_PowerPerUser);
            free(m_PerPower1);
            free(m_txdata_power);
            free(m_ah_i);
            free(m_anoise_sqrt_i);
            free(m_cali_factor);
            free(m_freq_off);
            free(m_pilot_power_i);
            free(m_beamforming_matrix_tx_o);
            free(m_TxPower0);
            free(m_TxPowerPer);
            free(m_TxPower1);
            free(m_U_est);
            free(m_V_est);
            free(m_S_est);
#ifdef DUMP_MATLAB_tx_beamforming
            m_file_in.close();
            m_file_h_norm.close();
            m_file_h_anoise.close();
            m_file_hat.close();
            m_file_h_in.close();
            file_pinv_o.close();
            m_file_bf_matrix.close();
            file_pinv_out_o.close();
            m_file_bf_matrix_o.close();
#endif
#ifdef DUMP_RTL_tx_beamforming
        fp_pilot_power.close();
	fp_umode_tx.close();
	fp_txdata_power.close();
	fp_freq.close();
	fp_power0.close();
	fp_power1.close();
        fp_power2.close();
        fp_m_ah_i.close();
	fp_anoise.close();
	fp_cali_factor.close();
        fp_Hhat.close();
	fp_HNormal.close();
	bf_matrix.close();
	fp_pinv_in.close();
	fp_pinv_in_row.close();
	fp_pinv_out.close();

	fp_out.close();

	fp_reg.close();

	fp_test.close();
		fp_test1.close();
	fp_svd_in.close();


#endif
		}
		void			init(char* module_name);
		void			go();
		void 			reset();
		void			configure(TX_BEAMFORMING_CFG cfg);
        Complex<double> tmp_use[48*8*8];
        int             m_umode_tx;
        Complex<double> **m_ah_i;
        double          *m_anoise_sqrt_i;
        Complex<double> *m_cali_factor;
        double          *m_freq_off;
        double          *m_pilot_power_i;
        int             *m_txdata_power;
        Complex<double> **m_beamforming_matrix_tx_o;
	private:
        TX_BEAMFORMING_CFG    m_cfg;
        SVD_CFG               m_svd_cfg;
        Cls_svd<fxp>          m_svd;
        Complex<T>          *m_in;
        int                 m_user_num;
        int                 m_RecipracalCaliFlag;
        int                 m_GWTxPowerFlag;
        Complex<double>     **m_GWRxAH;
        double              *m_GWRxPilotPower;
        Complex<double>     **m_Hhat;
        double              *m_Freq;
        Complex<double>     **m_HNormal;
        int                 *m_BW;
        int                 *m_BFNumIndex;
        Complex<double>     **m_BeamformerMatrix;
        Complex<double>     **m_HNormalTemp;
        Complex<double>     **m_pinv_in;
        Complex<double>     **m_MMSEBeamformerTmp;
        Complex<double>     **m_MMSEBeamformer;
        double              *m_PowerPerUser;
        double              *m_PerPower1;
        double              *m_TempTxPower;
        double              *m_TxPower0;
        double              *m_TxPowerPer;
        double              *m_TxPower1;

        T                   m_anoise_sqrt_i_fxp;
        Complex<T>          m_ah_i_fxp;
        T                   m_pilot_power_i_fxp;
        Complex<T>          m_cali_factor_fxp;
        T                   m_power_tmp;
        Complex<T>          m_GWRxAH_fxp;
        Complex<T>          m_hat_tmp_fxp;
        Complex<T>          m_hat_fxp;
        T                   m_freq_fxp;
        T                   m_h_hat_abs_sum;
        T                   m_h_hat_pow;
        T                   m_hat_root_sqrt_fxp;
        T                   m_BW_fxp;
        T                   m_freq_abs;
        T                   m_mmse;
        T                   m_bw_overlop_w;
        T                   m_BeamformerMatrix_abs;
        T                   m_BeamformerMatrix_pow;
        T                   m_BeamformerMatrix_abs_sqrt;
        T                   m_total_power;
        T                   m_umode_fxp;
        T                   m_pow_fxp;
        T                   m_pp_fxp;
        T                   m_pp_sum_fxp;
        T                   m_pp_sum_frac;
        Complex<T>          m_HNormal_fxp;
        T                   m_bw_tmp;
        T                   m_fre_tmp;
        T                   m_per_fxp;
        T                   m_TxPower0_fxp;
        T                   m_TxPower0_sum;
        T                   m_TxPower0_sum_frac;
        T                   m_TxPowerPer_fxp;
        T                   m_PowerLimit;
        T                   m_TxPower2_sum;
        T                   m_TxPower2_sum_frac;
        T                   m_PowerPerUser_sqrt;
        T                   m_sqrt_hat_fxp;
        T                   m_pow_diff3;
        T                   m_BeamformerMatrix_abs_sqrt_fxp;
        Complex<T>          m_pinv_fxp;
        Complex<T>          m_MMSEBeamformer_tmp_fxp;
        T                   m_s_est_frac;
        int                 m_pinv_cnt;
        Complex<T>          m_v_tmp_fxp;
        Complex<T>          m_out_tmp_fxp;
        double              m_BW_tmp;
        double              m_BeamformerMatrix_tmp;
        Complex<T>          m_BeamformerMatrix_fxp;
        Complex<double>     **m_U_est;
        Complex<double>     **m_V_est;
        double              **m_S_est;
        T                   m_test;
        T                   m_test0;
	Complex<T>          m_test1;


        void            get_input_msg();
        void            Normalize_Hhat();
        void            generate_Beamformer();
        int             generate_BW(int tx_mod);
        void            pinvFunction(Complex<double> **in, Complex<double> **out);
        void            power_calculate();
        void            power_allocation();
        void            get_out_msg();
        void            dump_pinv(Complex<double> **in, Complex<double> **out);
        double          sqrt_bisection(double in, double epsilon);

#ifdef DUMP_MATLAB_tx_beamforming
        ofstream       m_file_in;
        ofstream       m_file_h_norm;
        ofstream       m_file_h_anoise;
        ofstream       m_file_hat;
        ofstream       m_file_h_in;
        ofstream       file_pinv_o;
        ofstream       file_pinv_out_o;
        ofstream       m_file_bf_matrix;
        ofstream       m_file_bf_matrix_o;
#endif
#ifdef DUMP_RTL_tx_beamforming
        ofstream      fp_pilot_power;
        ofstream      fp_umode_tx;
        ofstream      fp_txdata_power;
        ofstream      fp_freq;
        ofstream      fp_power0;
        ofstream      fp_power1;
        ofstream      fp_power2;
        ofstream      fp_m_ah_i;
        ofstream      fp_anoise;
        ofstream      fp_cali_factor;
        ofstream      fp_Hhat;
        ofstream      fp_HNormal;
        ofstream      bf_matrix;
	ofstream      fp_pinv_in;
	ofstream      fp_pinv_in_row;
	ofstream      fp_pinv_out;
        ofstream      fp_out;
        ofstream      fp_reg;
        ofstream      fp_test;
        ofstream      fp_test1;
	ofstream      fp_svd_in;
#endif
};

template <class T>
void Cls_tx_beamforming<T>::configure(TX_BEAMFORMING_CFG cfg)
{
    m_svd_cfg.ant_num = 8;
    m_svd.configure(m_svd_cfg);
	m_cfg = cfg;
    m_user_num = m_cfg.user_num_i;
    m_RecipracalCaliFlag = m_cfg.RecipracalCaliFlag;
    m_GWTxPowerFlag = m_cfg.GWTxPowerFlag;
    m_bw_overlop_w = 1.0;//m_cfg.reg_bw_overlop;
    m_total_power = m_cfg.reg_total_pow;


    m_TempTxPower = safe_new(m_TempTxPower , m_user_num);
    m_TxPower0 = safe_new(m_TxPower0 , m_user_num);
    m_GWRxAH = safe_new(m_GWRxAH, 8, m_user_num);
    m_GWRxPilotPower = safe_new(m_GWRxPilotPower, m_user_num);
    m_Hhat = safe_new(m_Hhat, 8, m_user_num);
    m_Freq = safe_new(m_Freq, m_user_num);
    m_HNormal = safe_new(m_HNormal , 8, m_user_num);
    m_BW = safe_new(m_BW , m_user_num);
    m_BFNumIndex = safe_new(m_BFNumIndex , m_user_num);
    m_BeamformerMatrix = safe_new(m_BeamformerMatrix , m_user_num, 8);
    m_HNormalTemp = safe_new(m_HNormalTemp , 8 , m_user_num);
    m_pinv_in = safe_new(m_pinv_in , 8 , 8);
    m_MMSEBeamformerTmp = safe_new(m_MMSEBeamformerTmp , 8 , 8);
    m_MMSEBeamformer = safe_new(m_MMSEBeamformer , m_user_num , 8);
    m_PowerPerUser = safe_new(m_PowerPerUser , m_user_num);
    m_PerPower1 = safe_new(m_PerPower1 , m_user_num);
    m_txdata_power = safe_new(m_txdata_power , m_user_num);
    m_ah_i = safe_new(m_ah_i , 8 , m_user_num);
    m_anoise_sqrt_i = safe_new(m_anoise_sqrt_i , 8);
    m_cali_factor = safe_new(m_cali_factor , 8);
    m_freq_off = safe_new(m_freq_off , m_user_num);
    m_pilot_power_i = safe_new(m_pilot_power_i , m_user_num);
    m_beamforming_matrix_tx_o = safe_new(m_beamforming_matrix_tx_o , m_user_num,8);
    m_TxPowerPer = safe_new(m_TxPowerPer , m_user_num);
    m_TxPower1 = safe_new(m_TxPower1 , m_user_num);
    m_V_est = safe_new(m_V_est, 8, 8);
    m_U_est = safe_new(m_U_est, 8, 8);
    m_S_est = safe_new(m_S_est, 8, 8);
}

template <class T>
void Cls_tx_beamforming<T>::init(char* module_name)
{
    m_svd.init(module_name);

    fix_set(m_umode_fxp, 8, 8, FXP_US, FXP_RND, FXP_SAT_SYM);
    fix_set(m_ah_i_fxp, 19, 13, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_ah_i_fxp, "m_ah_i_fxp", 1, 1);
    fix_set(m_anoise_sqrt_i_fxp, 32, 10, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_anoise_sqrt_i_fxp, "m_anoise_sqrt_i_fxp", 1, 1);
    fix_set(m_pilot_power_i_fxp, 28, 24, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_pilot_power_i_fxp, "m_pilot_power_i_fxp", 1, 1);
    fix_set(m_cali_factor_fxp, 18, 3, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_cali_factor_fxp, "m_pilot_power_i_fxp", 1, 1);
    fix_set(m_power_tmp, 21, 6, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_power_tmp, "m_power_tmp", 1, 1);
    fix_set(m_GWRxAH_fxp, 24, 12, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_GWRxAH_fxp, "m_GWRxAH_fxp", 1, 1);
    fix_set(m_hat_tmp_fxp, 21, 12, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_hat_tmp_fxp, "m_hat_tmp_fxp", 1, 1);
    fix_set(m_hat_fxp, 24, 12, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_hat_fxp, "m_hat_fxp", 1, 1);
    fix_set(m_freq_fxp, 25, 18, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_freq_fxp, "m_freq_fxp", 1, 1);
    fix_set(m_h_hat_abs_sum, 26, 23, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_h_hat_abs_sum, "m_h_hat_abs_sum", 1, 1);
    fix_set(m_h_hat_pow, 26, 21, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_h_hat_pow, "m_h_hat_pow", 1, 1);
    fix_set(m_hat_root_sqrt_fxp, 12, -5, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_hat_root_sqrt_fxp, "m_hat_root_sqrt_fxp", 1, 1);
    fix_set(m_HNormal_fxp, 25, 5, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_HNormal_fxp, "m_HNormal_fxp", 1, 1);
    fix_set(m_BW_fxp, 17, 17, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_BW_fxp, "m_BW_fxp", 1, 1);
    fix_set(m_freq_abs, 24, 17, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_BW_fxp, "m_BW_fxp", 1, 1);
    fix_set(m_BeamformerMatrix_pow, 23, 7, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_BeamformerMatrix_pow, "m_BeamformerMatrix_pow", 1, 1);
    fix_set(m_BeamformerMatrix_abs, 20, 8, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_BeamformerMatrix_abs, "m_BeamformerMatrix_abs", 1, 1);
    fix_set(m_BeamformerMatrix_abs_sqrt, 15, 6, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_BeamformerMatrix_abs_sqrt, "m_BeamformerMatrix_abs_sqrt", 1, 1);
    fix_set(m_total_power, 10, 10, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_BeamformerMatrix_abs_sqrt, "m_BeamformerMatrix_abs_sqrt", 1, 1);
    fix_set(m_pp_fxp, 20, 3, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_pp_fxp, "m_pp_fxp", 1, 1);
    fix_set(m_pp_sum_fxp, 20, 5, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_pp_sum, "m_pp_sum", 1, 1);
    fix_set(m_pp_sum_frac, 20, 5, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_pp_sum_frac, "m_pp_sum_frac", 1, 1);
    fix_set(m_per_fxp, 20, 2, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_per_fxp, "m_per_fxp", 1, 1);
    fix_set(m_TxPower0_fxp, 20, 10, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_TxPower0_fxp, "m_TxPower0_fxp", 1, 1);
    fix_set(m_bw_tmp, 18, 18, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_bw_tmp, "m_bw_tmp", 1, 1);
    fix_set(m_bw_overlop_w, 4, 2, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_bw_tmp, "m_bw_tmp", 1, 1);
    fix_set(m_TxPower0_sum, 20, 12, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_TxPower0_sum, "m_TxPower0_sum", 1, 1);
    fix_set(m_TxPower0_sum_frac, 15, -5, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_TxPower0_sum_frac, "m_TxPower0_sum_frac", 1, 1);
    fix_set(m_TxPowerPer_fxp, 18, 2, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_TxPowerPer_fxp, "m_TxPowerPer_fxp", 1, 1);
    fix_set(m_PowerLimit, 18, 4, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_PowerLimit, "m_PowerLimit", 1, 1);
    fix_set(m_TxPower2_sum, 18, 9, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_TxPower2_sum, "m_TxPower2_sum", 1, 1);
    fix_set(m_TxPower2_sum_frac, 12, -5, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_TxPower2_sum_frac, "m_TxPower2_sum_frac", 1, 1);
    fix_set(m_pow_fxp, 18, 9, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_pow_fxp, "m_pow_fxp", 1, 1);
    fix_set(m_PowerPerUser_sqrt, 11, 5, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_PowerPerUser_sqrt, "m_PowerPerUser_sqrt", 1, 1);
    fix_set(m_BeamformerMatrix_fxp, 24, 5, FXP_TC, FXP_RND, FXP_SAT_SYM);//(m_BeamformerMatrix_fxp, "m_BeamformerMatrix_fxp", 1, 1);
    fix_set(m_fre_tmp, 25, 18, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_fre_tmp, "m_fre_tmp", 1, 1);
    fix_set(m_sqrt_hat_fxp, 16, 12, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_sqrt_hat_fxp, "m_sqrt_hat_fxp", 1, 1);
    fix_set(m_pow_diff3, 16, 1, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_pow_diff3, "m_pow_diff3", 1, 1);
    fix_set(m_BeamformerMatrix_abs_sqrt_fxp, 12, 4, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_BeamformerMatrix_abs_sqrt_fxp, "m_BeamformerMatrix_abs_sqrt_fxp", 1, 1);
    fix_set(m_mmse, 9, 1, FXP_US, FXP_RND, FXP_SAT_SYM);//statistic_set(m_BeamformerMatrix_abs_sqrt_fxp, "m_BeamformerMatrix_abs_sqrt_fxp", 1, 1);
    fix_set(m_pinv_fxp, 28, 15, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_pinv_fxp, "m_pinv_fxp", 1, 1);
    fix_set(m_s_est_frac, 20, 11, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_s_est_frac, "m_s_est_frac", 1, 1);
    fix_set(m_v_tmp_fxp, 32, 9, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_v_tmp_fxp, "m_v_tmp_fxp", 1, 1);
    fix_set(m_out_tmp_fxp, 32, 9, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_out_tmp_fxp, "m_out_tmp_fxp", 1, 1);
    fix_set(m_MMSEBeamformer_tmp_fxp, 25, 5, FXP_TC, FXP_RND, FXP_SAT_SYM);//statistic_set(m_MMSEBeamformer_tmp_fxp, "m_MMSEBeamformer_tmp_fxp", 1, 1);
    fix_set(m_test, 17, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_test0, 50, 10, FXP_TC, FXP_RND, FXP_SAT_SYM);
    //fix_set(m_test1, 50, 10, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_test1, 24, 10, FXP_US, FXP_RND, FXP_SAT_SYM);

#ifdef DUMP_MATLAB_tx_beamforming
    char filename_mat[300] = {};
    strcpy(filename_mat, module_name); strcat(filename_mat, "out.txt");             m_file_in.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "h_norm.txt");          m_file_h_norm.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "h_anoise.txt");        m_file_h_anoise.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "h_hat.txt");           m_file_hat.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "h_in.txt");            m_file_h_in.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "pinv_in.txt");         file_pinv_o.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "pinv_out.txt");        file_pinv_out_o.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "bf_matrix.txt");       m_file_bf_matrix.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "bf_matrix_o.txt");     m_file_bf_matrix_o.open(filename_mat);
#endif
#ifdef DUMP_RTL_tx_beamforming
    char filename_rtl[300] = {};
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "pilot_power_out.dat");    fp_pilot_power.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "umode_tx_out.dat");       fp_umode_tx.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "./txdata_power_out.dat");       fp_txdata_power.open(filename_rtl);


    strcpy(filename_rtl, module_name); strcat(filename_rtl, "freq_out.dat");           fp_freq.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "power0_out.dat");           fp_power0.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "power1_out.dat");           fp_power1.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "power2_out.dat");           fp_power2.open(filename_rtl);

    strcpy(filename_rtl, module_name); strcat(filename_rtl, "ah_out.dat");           fp_m_ah_i.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "anoise_out.dat");           fp_anoise.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "cali_factor_out.dat");           fp_cali_factor.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "Hhat_out.dat");           fp_Hhat.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "HNormal_out.dat");           fp_HNormal.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "BF_matrix_out.dat");           bf_matrix.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "pinv_in.dat");           fp_pinv_in.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "pinv_in_row.dat");           fp_pinv_in_row.open(filename_rtl);

    strcpy(filename_rtl, module_name); strcat(filename_rtl, "pinv_out.dat");           fp_pinv_out.open(filename_rtl);

    strcpy(filename_rtl, module_name); strcat(filename_rtl, "tx_bf_out.dat");           fp_out.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "reg_out.dat");           fp_reg.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "test.dat");           fp_test.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "test1.dat");           fp_test1.open(filename_rtl);
    strcpy(filename_rtl, module_name); strcat(filename_rtl, "svd_in_c.txt");           fp_svd_in.open(filename_rtl);



    m_umode_fxp = m_user_num;
    dump(fp_reg,&m_umode_fxp,1,m_umode_fxp);
    m_umode_fxp = m_svd_cfg.ant_num ;
    dump(fp_reg,&m_umode_fxp,1,m_umode_fxp);
    fp_reg << m_RecipracalCaliFlag << endl ;
    fp_reg << m_GWTxPowerFlag << endl;

    #endif
    	reset();
    #ifdef DUMP_RTL_tx_beamforming
    dump(fp_reg,&m_bw_overlop_w,1,m_bw_overlop_w);
    dump(fp_reg,&m_pow_diff3,1,m_pow_diff3);
    dump(fp_reg,&m_total_power ,1,m_total_power);
    dump(fp_reg, &m_mmse  ,1,m_mmse ) ;

    m_umode_fxp = m_umode_tx;
    dump(fp_umode_tx, &m_umode_fxp ,1,m_umode_fxp) ;

    #endif

}

template <class T>
void Cls_tx_beamforming<T>::reset()
{
    m_pow_diff3 = m_cfg.reg_pow_diff3;
    m_bw_overlop_w = 1.0;//m_cfg.reg_bw_overlop;
    m_BW_fxp = generate_BW(m_umode_tx);
    m_BW_tmp = m_bw_overlop_w * m_BW_fxp;
    m_mmse = m_cfg.reg_mmse;
    m_pinv_cnt = 1;
}

template <class T>
void Cls_tx_beamforming<T>::go()
{
    //get information
    get_input_msg();
    //Normalize Hhat
    Normalize_Hhat();
    //Gen Beamformer
    generate_Beamformer();
    //Power Allocate
    power_calculate();
    //output information
    get_out_msg();
}

template <class T>
void Cls_tx_beamforming<T>::get_input_msg()
{
    for (int ck0 = 0; ck0 < m_user_num; ck0++) {
        for (int ck1 = 0; ck1 < 8; ck1++) {
            m_ah_i_fxp = m_ah_i[ck1][ck0];//s19.13
            m_anoise_sqrt_i_fxp = m_anoise_sqrt_i[ck1];//s32.10
            m_GWRxAH_fxp = m_ah_i_fxp * m_anoise_sqrt_i_fxp;//s19.13*32.10=51.13
            m_GWRxAH[ck1][ck0] = m_GWRxAH_fxp;//s24.12
#ifdef DUMP_MATLAB_tx_beamforming
            m_file_h_anoise << m_GWRxAH_fxp.I << "\t" << m_GWRxAH_fxp.Q << endl;
            m_file_h_in << m_ah_i[ck1][ck0].I << "\t" << m_ah_i[ck1][ck0].Q << endl;
#endif
            //LOG2DEV << m_anoise_sqrt_i_fxp << endl;
#ifdef DUMP_RTL_tx_beamforming
            dump(fp_m_ah_i,&m_ah_i_fxp,1,m_ah_i_fxp.I);
            //dump(fp_anoise,&m_anoise_sqrt_i_fxp,1,m_anoise_sqrt_i_fxp);
#endif
        }
    }


#ifdef DUMP_RTL_tx_beamforming
        for (int ck1 = 0; ck1 < 8; ck1++) {
            m_anoise_sqrt_i_fxp = m_anoise_sqrt_i[ck1];//s32.10
            m_cali_factor_fxp = m_cali_factor[ck1];//s18.3

            dump(fp_anoise,&m_anoise_sqrt_i_fxp,1,m_anoise_sqrt_i_fxp);
            dump(fp_cali_factor,&m_cali_factor_fxp,1,m_cali_factor_fxp.I);
	}

#endif


    for (int ck0 = 0; ck0 < m_user_num; ck0++)
    {
        m_pilot_power_i_fxp = m_pilot_power_i[ck0];
        m_GWRxPilotPower[ck0] = m_pilot_power_i_fxp;
#ifdef DUMP_RTL_tx_beamforming
        dump(fp_pilot_power,&m_pilot_power_i_fxp,1,m_pilot_power_i_fxp);
#endif
    }

    if (m_RecipracalCaliFlag == 1)
    {
        for (int ck0 = 0; ck0 < m_user_num; ck0++)
        {
            for (int ck1 = 0; ck1 < 8; ck1++)
            {
                m_cali_factor_fxp = m_cali_factor[ck1];//s18.3
                m_power_tmp = m_cali_factor_fxp.power();//u21.6
                m_hat_tmp_fxp = m_GWRxAH[ck1][ck0] * m_cali_factor_fxp.conjug().todouble();//s21.12
                m_hat_fxp = m_hat_tmp_fxp / m_power_tmp;
                m_Hhat[ck1][ck0] = m_hat_fxp;//s24.12
#ifdef DUMP_MATLAB_tx_beamforming
                m_file_hat << m_hat_fxp.I << "\t" << m_hat_fxp.Q << endl;
#endif
#ifdef DUMP_RTL_tx_beamforming
               // dump(fp_cali_factor,&m_cali_factor_fxp,1,m_cali_factor_fxp.I);
	        dump(fp_Hhat,&m_hat_fxp,1,m_hat_fxp.I);
#endif
            }
        }
    }else{
        for (int ck0 = 0; ck0 < m_user_num; ck0++)
        {
            for (int ck1 = 0; ck1 < 8; ck1++)
            {
                m_hat_fxp = m_GWRxAH[ck1][ck0];
                m_Hhat[ck1][ck0] = m_hat_fxp;//24.12
#ifdef DUMP_RTL_tx_beamforming
	        dump(fp_Hhat,&m_hat_fxp,1,m_hat_fxp.I);
#endif
            }
        }
    }

    for (int ck0 = 0; ck0 < m_user_num; ck0++)
    {
        m_freq_fxp = m_freq_off[ck0];
        m_Freq[ck0] = m_freq_fxp;//s25.18
#ifdef DUMP_RTL_tx_beamforming
        dump(fp_freq,&m_freq_fxp,1,m_freq_fxp);
#endif
    }
}

template <class T>
void Cls_tx_beamforming<T>::Normalize_Hhat()
{
    double m_hat_root_sqrt_tmp = 0;
    for (int ck0 = 0; ck0 < m_user_num; ck0++)
    {
        m_h_hat_abs_sum = 0;
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            m_h_hat_pow = m_Hhat[ck1][ck0].power();//u26.21
            m_h_hat_abs_sum += m_h_hat_pow;//u26.23
        }
        m_sqrt_hat_fxp = sqrt_bisection(m_h_hat_abs_sum, DOUBLE_POW2[64 - 5]);
        //LOG2DEV << m_sqrt_hat_fxp << endl;
        m_hat_root_sqrt_tmp = m_sqrt_hat_fxp;
        m_hat_root_sqrt_fxp = 1.0 / m_hat_root_sqrt_tmp;//, DOUBLE_POW2[64 - 5]); //u12.-5
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            m_HNormal_fxp = m_Hhat[ck1][ck0] * m_hat_root_sqrt_fxp; //u24.12*u12.-5
            m_HNormal[ck1][ck0] = m_HNormal_fxp;//s18.4
            //LOG2DEV << m_HNormal_fxp.I << "\t" << m_HNormal_fxp.Q << endl;
#ifdef DUMP_MATLAB_tx_beamforming
            m_file_h_norm << m_HNormal[ck1][ck0].I << "\t" << m_HNormal[ck1][ck0].Q << endl;
#endif
#ifdef DUMP_RTL_tx_beamforming
            dump(fp_HNormal,&m_HNormal_fxp,1,m_HNormal_fxp.I);
#endif
        }

    }
}
template <class T>
void Cls_tx_beamforming<T>::generate_Beamformer()
{
    int NumUserBF, UserIndex;

    for (int ck0 = 0; ck0 < m_user_num; ck0++)
    {
        NumUserBF = 1;
        UserIndex = 1;

        for (int ck1 = 0; ck1 < m_user_num; ck1++)
        {
            m_freq_abs = abs(m_Freq[ck0] - m_Freq[ck1]);//u21.14 u24.17
            //LOG2DEV << m_freq_abs << endl;
            if(m_freq_abs < m_BW_tmp)
            {
                m_BFNumIndex[NumUserBF - 1] = ck1;
                if(ck0 == ck1)
                    UserIndex = NumUserBF;
                NumUserBF = NumUserBF + 1;
            }
        }

cout << NumUserBF <<"\t"<< UserIndex <<"\t"<<ck0<< endl;
        if ( (NumUserBF - 1) == 1)
        {
            for (int ck1 = 0; ck1 < 8; ck1++)
            {
                m_BeamformerMatrix[ck0][ck1] = m_HNormal[ck1][UserIndex - 1].conjug();
            }
        }else{
            for (int ck1 = 0; ck1 < NumUserBF - 1; ck1++)
            {
                for(int ck2 = 0; ck2 < 8; ck2++)
                    m_HNormalTemp[ck2][ck1] = m_HNormal[ck2][m_BFNumIndex[ck1]];
            }

            Complex<double> pinv_tmp;
            for (int ck1 = 0; ck1 < 8; ck1++)
            {
                for(int ck2 = 0; ck2 < 8; ck2++)
                {
                    pinv_tmp = 0;
                    for (int ck3 = 0; ck3 < NumUserBF - 1; ck3++)
                    {
                        pinv_tmp += m_HNormalTemp[ck1][ck3] * m_HNormalTemp[ck2][ck3].conjug();
                    }
                    m_pinv_fxp = pinv_tmp;//s28.15
                    m_pinv_in[ck1][ck2] = m_pinv_fxp;
                    if(ck1 == ck2) {
                        m_pinv_fxp = m_pinv_in[ck1][ck2].I + m_mmse;
                        m_pinv_in[ck1][ck2] = m_pinv_fxp;
                    }
                    m_pinv_in[ck1][ck2] = m_pinv_in[ck1][ck2];
#ifdef DUMP_RTL_tx_beamforming
            dump(fp_pinv_in_row ,&m_pinv_fxp,1,m_pinv_fxp.I);

             if(ck2 != 7)
                 dump(fp_pinv_in ,&m_pinv_fxp,1,m_pinv_fxp.I,0);
	     else
	         dump(fp_pinv_in ,&m_pinv_fxp,1,m_pinv_fxp.I);


	     fp_svd_in  << std::fixed<<setprecision(32) << m_pinv_in[ck1][ck2].I <<"\t" << m_pinv_in[ck1][ck2].Q <<endl;

#endif
                }
            }

            pinvFunction(m_pinv_in, m_MMSEBeamformerTmp);

            Complex<double> m_MMSEBeamformer_tmp;
            for (int ck1 = 0; ck1 < NumUserBF - 1; ck1++)
            {
                for (int ck2 = 0; ck2 < 8 ; ck2++)
                {
                    m_MMSEBeamformer_tmp = 0;
                    for (int ck3 = 0; ck3 < 8; ck3++)
                    {
                        m_MMSEBeamformer_tmp += m_HNormalTemp[ck3][ck1].conjug() * m_MMSEBeamformerTmp[ck3][ck2];
                        //LOG2DEV << m_HNormalTemp[ck3][ck1].conjug().I << "\t" << m_HNormalTemp[ck3][ck1].conjug().Q << "\t" << m_MMSEBeamformerTmp[ck3][ck2].I << "\t" <<m_MMSEBeamformerTmp[ck3][ck2].Q  <<endl;
                    }
                    m_MMSEBeamformer_tmp_fxp = m_MMSEBeamformer_tmp;
                    m_MMSEBeamformer[ck1][ck2] = m_MMSEBeamformer_tmp_fxp;
                }
            }

            for (int ck1 = 0; ck1 < 8; ck1++)
            {
                m_BeamformerMatrix[ck0][ck1] = m_MMSEBeamformer[UserIndex - 1][ck1];
                //LOG2DEV <<m_BeamformerMatrix[ck0][ck1].I << "\t" <<m_BeamformerMatrix[ck0][ck1].Q<<endl;
            }
        }
        m_BeamformerMatrix_tmp = 0;
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            //m_BeamformerMatrix_tmp = m_BeamformerMatrix[ck0][ck1] * m_BeamformerMatrix[ck0][ck1];
            //m_BeamformerMatrix_abs += m_BeamformerMatrix_tmp.abs();
            m_BeamformerMatrix_pow = m_BeamformerMatrix[ck0][ck1].power();//u23.7
            m_BeamformerMatrix_tmp += m_BeamformerMatrix_pow;
        }
        m_BeamformerMatrix_abs = m_BeamformerMatrix_tmp;//u20.8
        //LOG2DEV << m_BeamformerMatrix_tmp << endl;
        m_BeamformerMatrix_abs_sqrt_fxp = sqrt_bisection(m_BeamformerMatrix_abs, DOUBLE_POW2[64 - 5]);//u12.4
        m_BeamformerMatrix_abs_sqrt = 1.0 / m_BeamformerMatrix_abs_sqrt_fxp;//u15.6

        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            m_HNormal_fxp = m_BeamformerMatrix[ck0][ck1] * m_BeamformerMatrix_abs_sqrt;
            m_BeamformerMatrix[ck0][ck1] = m_HNormal_fxp;//s18.4 -> s25.5
            //LOG2DEV <<m_BeamformerMatrix[ck0][ck1].I << "\t" <<m_BeamformerMatrix[ck0][ck1].Q<<endl;
#ifdef DUMP_MATLAB_tx_beamforming
            m_file_bf_matrix << m_BeamformerMatrix[ck0][ck1].I << "\t" << m_BeamformerMatrix[ck0][ck1].Q << endl;
#endif
#ifdef DUMP_RTL_tx_beamforming
	        dump(bf_matrix ,&m_HNormal_fxp,1,m_HNormal_fxp.I);
#endif
        }
    }
}

template <class T>
int Cls_tx_beamforming<T>::generate_BW(int tx_mod)
{
    int GWTxMode[10] = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    int GWTxBW[10] = {2000, 2000, 2000, 4000, 8000, 16000, 32000, 64000, 128000, 128000};

    for (int ck0 = 0; ck0 < 10; ck0++)
    {
        if(tx_mod == GWTxMode[ck0])
            return GWTxBW[ck0];
    }
    return 0;
}

template <class T>
void Cls_tx_beamforming<T>::pinvFunction(Complex<double> **in, Complex<double> **out)
{
    m_svd.go(in);
    mem_copy(m_U_est, m_svd.m_U, 8, 8);
    mem_copy(m_V_est, m_svd.m_V, 8, 8);
    mem_copy(m_S_est, m_svd.m_S, 8, 8);
    m_svd.reset();

#ifdef DUMP_MATLAB_tx_beamforming
    LOG2DEV << "U_est" << endl;
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            LOG2DEV << m_U_est[ck0][ck1].I << "\t" ;
        }
        LOG2DEV << endl;
    }

    LOG2DEV << "V_est" << endl;
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            LOG2DEV << m_V_est[ck0][ck1].I << "\t" ;
        }
        LOG2DEV << endl;
    }
#endif
    int countNoZero = 0;
    for (int ck0 = 0; ck0 < 8; ck0++) {
        if (m_S_est[ck0][ck0] > 21 * DOUBLE_POW2[64 - 21]) {
            countNoZero += 1;
        }
    }

    double temp[8][8] = {0};
    for (int ck0 = 0; ck0 < countNoZero; ck0++)
    {
        m_s_est_frac = 1.0 / m_S_est[ck0][ck0];
        //LOG2DEV << m_S_est[ck0][ck0] << endl;
        temp[ck0][ck0] = m_s_est_frac;
#ifdef DUMP_RTL_tx_beamforming
            dump(fp_test ,&m_s_est_frac ,1,m_s_est_frac);
#endif
    }

    Complex<double> V_tmp[8][8] = {0};
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            m_v_tmp_fxp = m_V_est[ck0][ck1] * temp[ck1][ck1];
            V_tmp[ck0][ck1] = m_v_tmp_fxp;
        }
    }
    Complex<double> out_tmp_sum = 0;
    Complex<double> out_tmp = 0;
    for (int ck0 = 0; ck0 < 8; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            out_tmp = 0;
            for (int ck2 = 0; ck2 < 8; ck2++)
            {
                out_tmp_sum = V_tmp[ck0][ck2] * m_U_est[ck1][ck2].conjug();
                out_tmp +=  out_tmp_sum;
                //LOG2DEV << V_tmp[ck1][ck2].I << "\t" << V_tmp[ck1][ck2].Q << "\t" << m_U_est[ck1][ck2].I << "\t" <<m_U_est[ck1][ck2].Q << "\t" <<out_tmp_sum.I << "\t" << out_tmp_sum.Q <<endl;
#ifdef DUMP_RTL_tx_beamforming
            dump(fp_test,&V_tmp[ck0][ck2],1,m_test0);
            dump(fp_test,&m_U_est[ck1][ck2],1,m_test );
#endif
            }
            //LOG2DEV << out_tmp.I  << out_tmp.Q << endl;
            m_out_tmp_fxp = out_tmp;
            out[ck0][ck1] = m_out_tmp_fxp;
#ifdef DUMP_RTL_tx_beamforming
            dump(fp_pinv_out,&m_out_tmp_fxp,1,m_out_tmp_fxp.I);
#endif
        }
    }

    {
        std::ofstream trace("tx_svd_trace.txt", std::ios::app);
        trace << std::setprecision(17);
        for(int row=0;row<8;++row) for(int col=0;col<8;++col)
            trace << in[row][col].I << ' ' << in[row][col].Q << ' '
                  << m_U_est[row][col].I << ' ' << m_U_est[row][col].Q << ' '
                  << m_V_est[row][col].I << ' ' << m_V_est[row][col].Q << ' '
                  << m_S_est[row][col] << ' ' << out[row][col].I << ' ' << out[row][col].Q << '
';
    }
#ifdef DUMP_MATLAB_tx_beamforming
    dump_pinv(in, out);
#endif
}

template <class T>
void Cls_tx_beamforming<T>::power_calculate()
{
    if(m_GWTxPowerFlag == 0) {
        m_total_power = 0;
        for (int ck0 = 0; ck0 < m_user_num; ck0++) {
            m_pow_fxp = m_txdata_power[ck0];
            m_PowerPerUser[ck0] = m_pow_fxp;//u18.9
#ifdef DUMP_RTL_tx_beamforming
   dump(fp_power2,&m_pow_fxp,1,m_pow_fxp);//
   dump(fp_txdata_power,&m_pow_fxp,1,m_pow_fxp);
#endif
            m_total_power = m_total_power + (double)m_txdata_power[ck0];
        }
    } else if (m_GWTxPowerFlag == 1) {
        m_pow_fxp = m_total_power / (double)m_user_num;//u18.9
        for (int ck0 = 0; ck0 < m_user_num; ck0++) {
            m_PowerPerUser[ck0] = m_pow_fxp;
#ifdef DUMP_RTL_tx_beamforming
   dump(fp_power2,&m_pow_fxp,1,m_pow_fxp);
#endif
        }
    } else if (m_GWTxPowerFlag == 2) {
        power_allocation();
    }

    double PowerPerUser_sum = 0;
    for (int ck0 = 0; ck0 < m_user_num; ck0++) {
        PowerPerUser_sum +=  m_PowerPerUser[ck0];
    }
    for (int ck0 = 0; ck0 < m_user_num; ck0++) {
        m_PerPower1[ck0] = 10 * log10(m_PowerPerUser[ck0] / PowerPerUser_sum * m_total_power);
        //LOG2DEV << m_PerPower1[ck0] << endl;
    }
}

template <class T>
void Cls_tx_beamforming<T>::power_allocation()
{
    //double  PowerDiff3 = 0.01;
    double  *p_arr;
    double **pow_abs;
    double *pp;
    double *per;
    double m_pp_sum;

    p_arr = NULL;
    pow_abs = NULL;
    pp = NULL;
    per = NULL;
    p_arr = safe_new(p_arr, m_user_num);
    pow_abs = safe_new(pow_abs , m_user_num , m_user_num);
    pp = safe_new(pp , m_user_num);
    per = safe_new(per , m_user_num);

/*************************** first ***************************************/
    for (int ck0 = 0; ck0 < m_user_num; ck0++) {
        p_arr[ck0] = m_GWRxPilotPower[ck0];
    }

    m_pp_sum = 0;
    for (int ck0 = 0; ck0 < m_user_num; ck0++) {
        m_pp_fxp = 1.0 * m_BW_fxp / p_arr[ck0];
        pp[ck0] = m_pp_fxp;
        m_pp_sum = m_pp_sum + pp[ck0];
    }
    m_pp_sum_fxp = m_pp_sum;
    m_pp_sum_frac = 1.0 / m_pp_sum_fxp;
    for (int ck0 = 0; ck0 < m_user_num; ck0++) {
        m_per_fxp = pp[ck0] * m_pp_sum_frac;
        per[ck0] = m_per_fxp;
    }
/*************************** second ***************************************/
    int     TempFlag;
    for (int ck0 = 0; ck0 < m_user_num; ck0++)
    {
        m_TxPower0_fxp = m_total_power * per[ck0];
        m_TxPower0[ck0] = m_TxPower0_fxp;

#ifdef DUMP_RTL_tx_beamforming
   dump(fp_power0,&m_TxPower0_fxp,1,m_TxPower0_fxp);
#endif

    }

    for (int ck0 = 0; ck0 < m_user_num; ck0++)
    {
        zeros(m_TempTxPower , m_user_num);
        TempFlag = 0;
        for (int ck1 = 0; ck1 < m_user_num; ck1++)
        {
            m_bw_tmp = m_bw_overlop_w * (double)(m_BW_fxp + m_BW_fxp) ;
            m_fre_tmp = 2 * ( m_Freq[ck0] - m_Freq[ck1] );
            if(m_fre_tmp < 0)
                m_fre_tmp = -1.0*m_fre_tmp;
            if(ck0 == ck1)
            {
            }else{

                if( m_bw_tmp < m_fre_tmp) {
                } else {
                    if(m_TxPower0[ck1] > 10 * m_TxPower0[ck0]) {
                        TempFlag = 1;
                        m_TempTxPower[ck1] = m_TxPower0[ck1];
                    }
                }
            }
        }

        if(TempFlag == 1)
        {
            m_TxPower0[ck0] = m_TempTxPower[max(m_TempTxPower, m_user_num)];
            m_TxPower0_fxp = m_TxPower0[ck0] * 410 * DOUBLE_POW2[64 - 12];// 10.0;
            m_TxPower0[ck0] = m_TxPower0_fxp;
        }
    }

    m_TxPower0_sum = 0;
    for (int ck0 = 0; ck0 < m_user_num; ck0++)
    {
        m_TxPower0_sum += m_TxPower0[ck0];
    }
    m_TxPower0_sum_frac = 1.0 / m_TxPower0_sum;
    for (int ck0 = 0; ck0 < m_user_num; ck0++)
    {
        m_TxPowerPer_fxp = m_TxPower0[ck0] * m_TxPower0_sum_frac;
        m_TxPowerPer[ck0] = m_TxPowerPer_fxp;
        m_TxPower0_fxp = m_TxPowerPer[ck0] * m_total_power;
        m_TxPower1[ck0] =  m_TxPower0_fxp;//u20.10
#ifdef DUMP_RTL_tx_beamforming
   dump(fp_power1,&m_TxPower0_fxp,1,m_TxPower0_fxp);
#endif
    }

/*********************************third**********************************/
    double MaxTxPower;

    MaxTxPower = m_TxPower1[max(m_TxPower1 , m_user_num)];

    if((MaxTxPower * power2_diff[15] * DOUBLE_POW2[64 - 15]) < (m_pow_diff3 * m_total_power))
        m_PowerLimit = m_pow_diff3 * m_total_power;//u18.4
    else
        m_PowerLimit = MaxTxPower * power2_diff[15] * DOUBLE_POW2[64 - 15];

    int UserNum = 0;
    double *TxPower2;
    TxPower2 = NULL;
    TxPower2 = safe_new(TxPower2 , m_user_num);
    mem_copy(TxPower2 , m_TxPower1 , m_user_num);//u20.10

    for (int ck0 = 0; ck0 < m_user_num; ck0++)
    {
        if((m_TxPower1[ck0] < MaxTxPower * power2_diff[15] * DOUBLE_POW2[64 - 15]) || m_TxPower1[ck0] < m_pow_diff3 * m_total_power) {
            m_TxPowerPer[ck0] = 0;
            TxPower2[ck0] = m_PowerLimit;//u18.4
            UserNum += 1;
        }
    }

    for (int ck0 = 0; ck0 < m_user_num; ck0++)
    {
        m_TxPower2_sum += TxPower2[ck0];//u18.9
    }
    m_TxPower2_sum_frac = 1.0 / m_TxPower2_sum;//u12.-5
    for (int ck0 = 0; ck0 < m_user_num; ck0++)
    {
        m_pow_fxp = TxPower2[ck0] * m_TxPower2_sum_frac * m_total_power;
        m_PowerPerUser[ck0] = m_pow_fxp;//u18.9
#ifdef DUMP_MATLAB_tx_beamforming
        m_file_in << m_PowerPerUser[ck0] << endl;
#endif
#ifdef DUMP_RTL_tx_beamforming
   dump(fp_power2,&m_pow_fxp,1,m_pow_fxp);
#endif
    }

    free(pow_abs);
    free(pp);
    free(per);
    free(TxPower2);
    free(p_arr);
}

template <class T>
void Cls_tx_beamforming<T>::get_out_msg()
{
    for (int ck0 = 0; ck0 < m_user_num; ck0++)
    {
        for (int ck1 = 0; ck1 < 8; ck1++)
        {
            m_PowerPerUser_sqrt = sqrt_bisection(m_PowerPerUser[ck0], DOUBLE_POW2[64 - 5]);//u11.5
            m_BeamformerMatrix_fxp = m_BeamformerMatrix[ck0][ck1] * m_PowerPerUser_sqrt;//s18.4*u11.5
            m_BeamformerMatrix[ck0][ck1] = m_BeamformerMatrix_fxp;//s24.5
            m_beamforming_matrix_tx_o[ck0][ck1] = m_BeamformerMatrix[ck0][ck1];
#ifdef DUMP_RTL_tx_beamforming
   dump(fp_out ,&m_BeamformerMatrix_fxp,1,m_BeamformerMatrix_fxp.I);
#endif
#ifdef DUMP_MATLAB_tx_beamforming
            m_file_bf_matrix_o << m_beamforming_matrix_tx_o[ck0][ck1].I << "\t" << m_beamforming_matrix_tx_o[ck0][ck1].Q << endl;
#endif
            //LOG2DEV << m_PowerPerUser_sqrt << endl;
            //LOG2DEV<<ck0 <<"\t" << (double)m_BeamformerMatrix[ck0][ck1].I<< "\t" << m_BeamformerMatrix[ck0][ck1].Q  << endl;
            //m_file_in << m_BeamformerMatrix[ck0][ck1].I << "\t" << m_BeamformerMatrix[ck0][ck1].Q << endl;
        }
    }
}

template <class T>
double Cls_tx_beamforming<T>::sqrt_bisection(double in, double epsilon) {
    int times = 0;
    double a;
    double root;
    int times_int;

    if(in == 0) {
        root = 0;
        return root;
    }

    a = in;
    if(in >= 1024 && in <= 2048) {
        times = 0;
    } else if(a < 1024) {
        while(a < 1024) {
            times = times + 1;
            a = in * DOUBLE_POW2[64 + times];
        }
    } else if(a > 2048) {
        while(a > 2048) {
            times = times - 1;
            a = in * DOUBLE_POW2[64 + times];
        }
    }
    a = round(a);

    if (a < 0) {
        printf("Input must be non-negative\n");
        exit(1);
    } else if (a == 0 || a == 1) {
        root = a;
        return root;
    } else {
        double low = 0;
        double high = fmax(1, a);
        double mid = (low + high) / 2;

        for(int ck0 = 0; ck0 < 22; ck0++) {
            if(abs(mid * mid - a) > epsilon) {
                if (mid * mid > a) {
                    high = mid;
                } else {
                    low = mid;
                }
                mid = (low + high) / 2;
            } else {
                break;
            }
        }
        root = mid;
    }
    if(times > 0) {
        times_int = (int)(times / 2);
        root = root * DOUBLE_POW2[64 - times_int];
        if((times - 2 * times_int) == 1)
            root = root * 46341 * DOUBLE_POW2[64 - 16];// sqrt(2);
    } else if(times < 0) {
        times = times * -1;
        times_int = (int)(times / 2);
        root = root * DOUBLE_POW2[64 + times_int];
        if((times - 2 * times_int) == 1)
            root = root * 92682 * DOUBLE_POW2[64 - 16]; //sqrt(2);
    }
    return root;
}

template <class T>
void Cls_tx_beamforming<T>::dump_pinv(Complex<double> **in, Complex<double> **out) {
    //if(m_pinv_cnt == 2)
#ifdef DUMP_MATLAB_tx_beamforming
    {
        for(int ck0 = 0; ck0 < 8; ck0++) {
            for(int ck1 = 0; ck1 < 8; ck1++) {
                file_pinv_o<< setprecision(40) << in[ck0][ck1].I<< "\t" << in[ck0][ck1].Q << endl;
                file_pinv_out_o<< setprecision(40) << out[ck0][ck1].I<< "\t" << out[ck0][ck1].Q << endl;
            }
        }
    }
    m_pinv_cnt++;
#endif
}

#endif
