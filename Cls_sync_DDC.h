#pragma once
#include "Lib_io.h"
#include "common_type.h"

//#define RTL_DUMP
typedef struct SYNC_DDC_Cfg
{
    IO_config       io_config;
    Module_config   module_config;
    double          ang_init ;
    double          ang_delta ;
}SYNC_DDC_Cfg;

template <class T>
class Cls_sync_DDC
{
    public:
        Cls_sync_DDC() {

        }
        ~Cls_sync_DDC() {
        }
        void init();
        void reset();
        void ang_delta(double delta, int type);
        void ang_accum();
        Complex<double> rot(Complex<double> input);
        void config(SYNC_DDC_Cfg parm);
    private:
        SYNC_DDC_Cfg    m_cfg;
        int             m_test;
        T               m_delta;
        T               m_angle;
        T               m_angle_accum;
        T               m_ang_f;
        T               m_normal;
        T               m_CCFO;
        T               m_FCFO;
        T               m_track;
        T               m_cos;
        T               m_sin;
        Complex<T>      m_cossin;
        T               m_sync;
        Complex<T>      m_in;
        Complex<T>      m_out;
        std::ofstream   file_in;
};

template <class T>
void Cls_sync_DDC<T>::ang_accum()
{
    m_angle = m_angle_accum + m_sync;
    //m_sync +=
    m_angle_accum += m_delta;
};

template <class T>
void Cls_sync_DDC<T>::reset()
{
    m_angle = m_cfg.ang_init;
    m_normal = m_cfg.ang_delta;
    m_angle_accum = m_angle;
    m_CCFO = 0;
    m_sync = 0;
    m_FCFO = 0;
    m_track = 0;
    m_cos = 0;
    m_sin = 0;
    m_test = 100;
    m_delta = m_normal + m_CCFO + m_FCFO + m_track;
};

template <class T>
void Cls_sync_DDC<T>::config(SYNC_DDC_Cfg param)
{
    m_cfg.ang_delta = param.ang_delta;
    m_cfg.ang_init = param.ang_init;
};

template <class T>
void Cls_sync_DDC<T>::init()
{
    //char filename[300] = {};
    fix_set(m_ang_f, 10, 1, FXP_TC, FXP_RND, FXP_WRAP);
    fix_set(m_out, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_in, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_cossin, 10, 1, FXP_TC, FXP_RND, FXP_SAT_SYM);
    fix_set(m_angle, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
    fix_set(m_normal, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
    fix_set(m_angle_accum, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
    fix_set(m_sync, 25, 1, FXP_TC, FXP_RND, FXP_WRAP);
    #ifdef RTL_DUMP
//    if(m_test!=100)
        file_in.open("ddc_in.dat");
    #endif
    reset();
};

template <class T>
void Cls_sync_DDC<T>::ang_delta(double delta, int type)
{
    switch (type)
    {
    case 0:m_normal += delta;
        break;
    case 1:m_CCFO += delta;
        break;
    case 2:m_FCFO += delta;
        break;
    case 3:m_track += delta;
        break;
    case 4:m_sync = delta;
        break;
    default:
        m_normal += delta;
        break;
    }
    m_delta = m_normal + m_CCFO + m_FCFO + m_track;
};

template <class T>
Complex<double> Cls_sync_DDC<T>::rot(Complex<double> input)
{
    Complex<double> output;
    ang_accum();
    m_ang_f = m_angle;
    //std::cout<<"phase_new = "<<m_angle / 2 <<std::endl;
    m_cossin.I = cos(m_ang_f * PI);
    m_cossin.Q = sin(m_ang_f * PI);
    //std::cout<<m_cossin.I<<"\t"<<m_cossin.Q <<std::endl;
    m_in = input;
    //m_out.I = m_in.I * m_cos - m_in.Q * m_sin;
    //m_out.Q = m_in.Q * m_cos + m_in.I * m_sin;
    m_out =  m_in * m_cossin;
    output.I = (double)m_out.I;
    output.Q = (double)m_out.Q;

    #ifdef RTL_DUMP
//    if(m_test==100)
        dump(file_in, &m_in, 1, m_in.I);
	dump(file_in, &m_sync, 1, m_sync);
	dump(file_in, &m_angle_accum, 1, m_angle_accum);
	dump(file_in, &m_angle, 1, m_angle);
	dump(file_in, &output, 1, m_in.I);
    #endif
    return output;
};



