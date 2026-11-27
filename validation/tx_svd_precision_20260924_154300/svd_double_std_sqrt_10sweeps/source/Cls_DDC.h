#pragma once
#include "Lib_io.h"
#include "common_type.h"

typedef struct DDC_Cfg
{
    IO_config       io_config;
    Module_config   module_config;
    double          ang_init;// = 0;
    double          ang_delta;// = 0;
}DDC_Cfg;

template <class T>
class Cls_DDC
{
    public:
        Cls_DDC() {

        }
        ~Cls_DDC() {
        }
        void init(char* module_name);
        void reset();
        void ang_delta(double delta, int type);
        void ang_accum();
        Complex<double> rot(Complex<double> input);
        void config(DDC_Cfg parm);
    private:
        DDC_Cfg     m_cfg;
        T           m_delta;// = 0;
        T           m_angle;// = 0;
        T           m_ang_f;// = 0;
        T           m_normal;// = 0;
        T           m_CCFO;// = 0;
        T           m_FCFO;// = 0;
        T           m_track;// = 0;
        T           m_cos;// = 0;
        T           m_sin;// = 0;
        Complex<T>  m_in;
        Complex<T>  m_out;
};

template <class T>
void Cls_DDC<T>::ang_accum()
{
    m_angle += m_delta;
};

template <class T>
void Cls_DDC<T>::reset()
{
    m_angle = m_cfg.ang_init;
    m_normal = m_cfg.ang_delta;
    m_CCFO = 0;
    m_FCFO = 0;
    m_track = 0;
    m_cos = 0;
    m_sin = 0;
    m_delta = m_normal + m_CCFO + m_FCFO + m_track;
};

template <class T>
void Cls_DDC<T>::config(DDC_Cfg param)
{
    m_cfg.ang_delta = param.ang_delta;
    m_cfg.ang_init = param.ang_init;
};

template <class T>
void Cls_DDC<T>::init(char* module_name)
{
    reset();
};

template <class T>
void Cls_DDC<T>::ang_delta(double delta, int type)
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
    default:
        m_normal += delta;
        break;
    }
    m_delta = m_normal + m_CCFO + m_FCFO + m_track;
};

template <class T>
Complex<double> Cls_DDC<T>::rot(Complex<double> input)
{
    Complex<double> output;
    m_in = input;
    m_out.I = m_in.I * m_cos - m_in.Q * m_sin;
    m_out.Q = m_in.Q * m_cos + m_in.I * m_sin;

    m_ang_f = m_angle;
    m_cos = cos(m_ang_f * PI);
    m_sin = sin(m_ang_f * PI);

    output.I = (double)m_out.I;
    output.Q = (double)m_out.Q;
    ang_accum();
    return output;
};



