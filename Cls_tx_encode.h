/****************************************************************************
**
**  Copyright  (c)  2023-2028,  All Rights Reserved.
**
**  File          : Cls_bf.h
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
#ifndef __Cls_tx_encode
#define __Cls_tx_encode

#define DUMP_MATLAB_tx_encode

#include "common_type.h"
#include <cmath>
#include "Lib_complex.h"
#include "Lib_fixpoint.h"
#include "Lib_random.h"
#include "Lib_utility.h"
using namespace std;

unsigned int ScodeIn[12] = {0x4e431080,0x2765ec35,0x26fcb50a,0x99593f69,0xa58a6301,0xd35c68bf,
                         0xd60bbe3c,0x57d68376,0xdd3d4a05,0xebc97381,0xf439848a,0xf0f70b36};
int InterLvIndex[24] = {22,10,16,4,19,7,13,1,21,9,15,3,18,6,12,0,2,14,8,20,5,17,11,23};
typedef struct TX_ENCODE_CFG
{
    int len_tx;
    double CR;
    int Blocknum;
    int crc_en;
}TX_ENCODE_CFG;

template <class T>
class Cls_tx_encode
{
	public:
		Cls_tx_encode() {
            m_in = NULL;
            m_crc_in = NULL;
            m_crc_o = NULL;
            m_dqpsk_cco = NULL;
            m_fsk_cco = NULL;
            m_InterLv_out_fsk = NULL;
            m_InterLv_out_dqpsk = NULL;
		}
		~Cls_tx_encode() {
            free(m_in);
            free(m_crc_in);
            free(m_crc_o);
            free(m_dqpsk_cco);
            free(m_fsk_cco);
            free(m_InterLv_out_fsk);
            free(m_InterLv_out_dqpsk);
#ifdef DUMP_MATLAB_tx_encode
        fp_mat_dqpsk.close();
        fp_mat_fsk.close();
        encode_fsk.close();
        encode_dqpsk.close();
#endif
		}
		void			init(char* module_name);
		void			go(int *in, int *dqpsk, int *fsk);
		void 			reset();
		void			configure(TX_ENCODE_CFG cfg);
        int             m_len_out;
	private:
        TX_ENCODE_CFG          m_cfg;
        int                    m_BlockNum;
        int                    m_in_len;
        int                    m_crc_in_len;
        int                    m_crc_outlen;
        int                    m_crclen;
        int                    m_dqpsk_ccinlen;
        int                    m_fsk_ccinlen;
        int                    m_dqpsk_ccoutlen;
        int                    m_fsk_ccoutlen;
        int                    m_dqpsk_InterPunoutlen;
        int                    m_fsk_InterPunoutlen;
        int                    m_crc_en;

        int                    *m_in;
        int                    *m_crc_in;
        int                    *m_crc_o;
        int                    *m_dqpsk_cco;
        int                    *m_fsk_cco;
        int                    *m_InterLv_out_fsk;
        int                    *m_InterLv_out_dqpsk;
        void    crc_encode();
        void    cc_encode(int *in, int in_len, int *out);
        void    get_input_msg(int *msg);
        void    get_output_msg(int *dqpsk, int *fsk);
        void    InterLvPunch(int *in, int in_len, int *out);
#ifdef DUMP_MATLAB_tx_encode
        ofstream       fp_mat_dqpsk;
        ofstream        fp_mat_fsk;
        ofstream        encode_fsk;
        ofstream       encode_dqpsk;
#endif
};

template <class T>
void Cls_tx_encode<T>::configure(TX_ENCODE_CFG cfg)
{
	m_cfg = cfg;
}

template <class T>
void Cls_tx_encode<T>::init(char* module_name)
{
    m_BlockNum = m_cfg.Blocknum;
    m_crc_en = m_cfg.crc_en;
    m_in_len = m_cfg.len_tx * 8;

    m_crclen = 14;
    m_crc_in_len = m_in_len - m_crclen;
    m_crc_outlen = m_in_len;
    m_dqpsk_ccinlen = m_crc_outlen / 2;
    m_fsk_ccinlen = m_dqpsk_ccinlen;
    m_dqpsk_ccoutlen = m_dqpsk_ccinlen * 2;
    m_fsk_ccoutlen = m_dqpsk_ccoutlen;
    m_dqpsk_InterPunoutlen = m_cfg.len_tx * 4 / m_cfg.CR;
    m_fsk_InterPunoutlen = m_dqpsk_InterPunoutlen;

    m_in = safe_new(m_in, m_in_len);
    m_crc_in = safe_new(m_crc_in , m_crc_in_len);
    m_crc_o = safe_new(m_crc_o, m_crc_outlen);
    m_dqpsk_cco = safe_new(m_dqpsk_cco, m_dqpsk_ccoutlen);
    m_fsk_cco = safe_new(m_fsk_cco, m_fsk_ccoutlen);
    m_InterLv_out_dqpsk = safe_new(m_InterLv_out_dqpsk, m_dqpsk_InterPunoutlen);
    m_InterLv_out_fsk = safe_new(m_InterLv_out_fsk, m_fsk_InterPunoutlen);
#ifdef DUMP_MATLAB_tx_encode
    char filename_mat[300] = {};
    strcpy(filename_mat, module_name); strcat(filename_mat, "dqpsk.txt");       fp_mat_dqpsk.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "fsk.txt");       fp_mat_fsk.open(filename_mat);
        strcpy(filename_mat, module_name); strcat(filename_mat, "encode_dqpsk.txt");       encode_dqpsk.open(filename_mat);
    strcpy(filename_mat, module_name); strcat(filename_mat, "encode_fsk.txt");       encode_fsk.open(filename_mat);
#endif
	reset();
}

template <class T>
void Cls_tx_encode<T>::reset()
{
    zeros(m_in, m_in_len);
    zeros(m_crc_in,m_crc_in_len);
    zeros(m_crc_o,m_crc_outlen);
    zeros(m_dqpsk_cco,m_dqpsk_ccoutlen);
    zeros(m_fsk_cco,m_fsk_ccoutlen);
    zeros(m_InterLv_out_dqpsk,m_dqpsk_InterPunoutlen);
    zeros(m_InterLv_out_fsk,m_fsk_InterPunoutlen);
}

template <class T>
void Cls_tx_encode<T>::go(int *in, int *dqpsk,int *fsk)
{
    get_input_msg(in);

    crc_encode();

    cc_encode(&m_crc_o[0], m_fsk_ccinlen, m_fsk_cco);
    cc_encode(&m_crc_o[m_fsk_ccinlen], m_dqpsk_ccinlen, m_dqpsk_cco);

    for(int ck0 = 0;ck0 < m_fsk_ccinlen*2;ck0++)
    {
        encode_dqpsk << m_dqpsk_cco[ck0] << endl;
        encode_fsk << m_fsk_cco[ck0] << endl;
    }

    InterLvPunch(m_fsk_cco, m_fsk_ccoutlen, m_InterLv_out_fsk);
    InterLvPunch(m_dqpsk_cco, m_dqpsk_ccoutlen, m_InterLv_out_dqpsk);

    get_output_msg(dqpsk, fsk);
}

template<class T>
void Cls_tx_encode<T>::get_input_msg(int *in)
{
    int ck0;
    for (ck0 = 0; ck0 < m_in_len; ck0++)
        m_in[ck0] = in[ck0];
    memcpy(m_crc_in, m_in, sizeof(int)*m_crc_in_len);
}

template<class T>
void Cls_tx_encode<T>::crc_encode()
{
    int poly14 = 18147;
    int poly = 0;
    unsigned state = 0;
    if(m_crc_en == 1)
    {
    for (int i = 0; i < m_crc_in_len; ++i) {
        m_crc_o[i] = m_crc_in[i];
    }
    for (int i = 0; i < m_crclen; ++i) {
        m_crc_o[i + m_crc_in_len] = 0;
    }

    poly = poly14;
    for (int i = 0; i < m_crclen; ++i) {
        state += m_crc_in[i] << (m_crclen - 1 - i);
    }

    for (int i = m_crclen; i < m_crc_in_len + m_crclen; ++i) {
        state = (state << 1) + m_crc_o[i];
        state = state >= (1 << m_crclen) ? state ^ poly : state;
    }

    for (int i = 0; i < m_crclen; ++i) {
        m_crc_o[i + m_crc_in_len] = (state >> (m_crclen - 1 - i)) & 0x1;
    }
    }else{
        mem_copy(m_crc_o, m_in, m_crc_outlen);

    }

}

template<class T>
void Cls_tx_encode<T>::get_output_msg(int *dqpsk, int *fsk)
{
    for (int ck0 = 0; ck0 < m_dqpsk_InterPunoutlen; ck0++)
    {
        dqpsk[ck0] = m_InterLv_out_dqpsk[ck0];
        fsk[ck0] = m_InterLv_out_fsk[ck0];
    }
for (int ck0 = 0; ck0 < m_dqpsk_InterPunoutlen; ck0++)
{
        fp_mat_fsk << fsk[ck0] << endl;
        fp_mat_dqpsk << dqpsk[ck0] << endl;
}
}

template<class T>
void Cls_tx_encode<T>::InterLvPunch(int *in, int in_len, int *out)
{
    unsigned int Scode[24];
    int ScodeInlen = 12;
    int *Inxor = NULL;
    int i,j,ii;
    int col = 24;
    int row;
    int count_tmp;
    int tempIndex;
    int tmp;
    int *out_tmp = NULL;

    out_tmp = safe_new(out_tmp, in_len);

    Inxor = safe_new(Inxor, in_len);
    memcpy(Scode, ScodeIn, sizeof(unsigned int) * ScodeInlen);
    memcpy(Scode + ScodeInlen, ScodeIn, sizeof(unsigned int) * ScodeInlen);

    for(i = 0;i < in_len;i++)
    {
        tmp = (Scode[i >> 5] >> (i & 31) & (0x1));
        Inxor[i] = in[i] ^ tmp;
    }

    row = (in_len + col - 1) / col;
    count_tmp = 0;
    for(i = 0;i < col;i++)
    {
        ii = InterLvIndex[i];
        for(j = 0;j < row;j++)
        {
            tempIndex = j * col + ii;
            if (tempIndex < in_len)
            {
                out_tmp[count_tmp] = Inxor[tempIndex];
                count_tmp += 1;
            }

        }
    }

    memcpy(out, out_tmp, sizeof(int) * m_fsk_InterPunoutlen);
    free(Inxor);
    free(out_tmp);
}

template<class T>
void Cls_tx_encode<T>::cc_encode(int *in, int in_len, int * out)
{
    char convol_outs[2][64] = {{0,3,1,2,0,3,1,2,3,0,2,1,3,0,2,1,3,0,2,1,3,0,2,1,0,3,1,2,0,3,1,2,2,1,3,0,2,1,3,0,1,2,0,3,1,2,0,3,1,2,0,3,1,2,0,3,2,1,3,0,2,1,3,0},
                               {3,0,2,1,3,0,2,1,0,3,1,2,0,3,1,2,0,3,1,2,0,3,1,2,3,0,2,1,3,0,2,1,1,2,0,3,1,2,0,3,2,1,3,0,2,1,3,0,2,1,3,0,2,1,3,0,1,2,0,3,1,2,0,3}};
    int i;
    int state = (in[in_len - 1] << 5) + (in[in_len - 2] << 4) + (in[in_len - 3] << 3)
              + (in[in_len - 4] << 2) + (in[in_len - 5] << 1) + in[in_len - 6];
    int val;

    for (i = 0; i < 6; ++i) {
        state = (state >> 1) + in[in_len - 6 + i] * 32;
    }

    for (i = 0; i < in_len; i ++){
        val = convol_outs[in[i]][state];
        out[i * 2      ] = val >> 1;
        out[i * 2 + 1] = val & 0x1;
        state = (state >> 1) + in[i] * 32;
    }

}
#endif
