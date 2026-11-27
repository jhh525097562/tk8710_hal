/****************************************************************************
**
**  Copyright  (c)  2006,  Amlogic, Inc.
**          All Rights Reserved.
**
**  Subsystem    :
**  File              : Lib_io.h
**  Created By    : Alan Niu
**  Created On    : 11/25/2019
**
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**
****************************************************************************/


#ifndef __Cls_io_H
#define __Cls_io_H

#include "Lib_fixpoint.h"
#include "Lib_utility.h"
#include "Lib_complex.h"
#include "Lib_dump.h"
#include "common_type.h"
#include <iostream>
#include <fstream>

typedef struct IO_config {
    int   io_fileOut_flag;// = 0;
    int   io_max_delay;// = 0;
    TX_RX tx_rx_flag;// = OTHER;
}IO_config;

IO_config        default_IO_config;

template <class T>
class Cls_io
{
    public:
        Cls_io(){
            m_buffer = NULL;
        }

        ~Cls_io(){
	     free(m_buffer);
            if (m_fileOut.is_open()) m_fileOut.close();
        }

        void   init(char *fileHandle,int fileOut_flag=0,int max_delay=1000,  short wlen=DEFAULT_WL, short iwlen=DEFAULT_IWL, short e_mode = FXP_TC, short q_mode=FXP_TRN, short o_mode=FXP_SAT);

        void   reset()                    {
                    m_read_cnt  = 0;
                    m_write_cnt = 0;
                    m_rvalue    = 0;
                    for (int ck0=0;ck0<m_max_delay;ck0++) m_buffer[ck0] = 0;
        }
        void   transact()                 {

                    m_buffer[m_write_cnt] = m_rvalue;

                    if (m_fileOut_flag==2)
                        print2file(read(0));
                    m_write_cnt ++;
                    m_write_cnt = (m_write_cnt==m_max_delay)?0:m_write_cnt;
        }
        T&     read(int delay = 0)        {
                    m_read_cnt = (m_write_cnt - 1 - delay);
                    m_read_cnt = (m_read_cnt<0)?m_read_cnt+m_max_delay:m_read_cnt;
                    return (m_buffer[m_read_cnt]);
        }
        T&     blocking_read()  {
                    return (m_rvalue);
        }

        Cls_io& operator = (T in)       {
                    m_rvalue  = in;
                    return *this;
        }
        Cls_io& operator = (Cls_io& x) {
                    m_rvalue  = x.m_rvalue;
                    return *this;
        }
        Cls_io& operator <= (Cls_io& x) {
                    m_rvalue  = x.read(0);
                    return *this;
        }

        operator bool()   { return bool(m_rvalue==1);}
        operator double() { return double(m_rvalue);}
        operator int()    { return int(m_rvalue);}

        T               m_rvalue ;// = 0;

    private:
        char            m_fileHandle[300];// = {0};
        int             m_read_cnt;// = 0;
        int             m_write_cnt;// = 0;
        int             m_max_delay;// = 0;
        T             * m_buffer;
        std::ofstream   m_fileOut;
        int             m_fileOut_flag;// = 0;
        void            print2file(double in){m_fileOut << m_buffer[m_write_cnt] << "\n";}
        void            print2file(fxp    in){m_buffer[m_write_cnt].write(m_fileOut) << "\n";}

};


template <class T>
void Cls_io<T>::init(char *fileHandle,int fileOut_flag,int max_delay,  short wlen, short iwlen, short e_mode, short q_mode, short o_mode)
{
    m_max_delay    = max_delay;
    m_fileOut_flag = fileOut_flag;

    if (fileOut_flag==2)
    {
        LOG2DEV << "Module's IO " << fileHandle << " is being logged to file by the same name.\n " ;
        strcpy(m_fileHandle, g_log_folder);
        strcat(m_fileHandle, fileHandle);
        m_fileOut.open(m_fileHandle);
    }
    m_buffer = safe_new(m_buffer, m_max_delay);

    for (int ck0=0;ck0<m_max_delay;ck0++)
        fix_set(m_buffer[ck0],wlen,iwlen,e_mode,q_mode,o_mode);

    fix_set(m_rvalue,wlen,iwlen,e_mode,q_mode,o_mode);

    reset();

}
#endif

