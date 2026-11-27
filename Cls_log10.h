#ifndef _LOG10_0D25
#define _LOG10_0D25

template <class T>
class Cls_log10
{
public:

    T run(T in);
};

template <class T>
T Cls_log10<T>::run(T in)
{
    T out;
    T out1=0;
    fix_set(out1, 9, 7, FXP_TC, FXP_RND, FXP_SAT_SYM);

    int for_RTL = 1;
    int power_wl = 27;

    if(in <= 0){
        out1 = 0;
        //std::cout << "the input to log10 must be positive" << std::endl;
        return out1;
        //exit(0);
    }
    if (sizeof(T)==sizeof(double)){
        return 10*log10(in);
    }

    double db3 = 3.0; // 3.01;
    double V;
    int wl;
    int tableSize;
    double tmpW;
    double tmp1;
    if(for_RTL == 1){
        out = db3 * (power_wl - 1);
        tmpW = in;
        V = double(1 << (power_wl - 1));
        wl = 32;
        tableSize = int(log2(wl));
        if(tmpW == 0)
            out = 0;
        else{
            while(tmpW < V){
                tmpW = tmpW * 2.0;
                out = out - db3;
            }
            tmp1 = floor(tmpW * 1.0 / (1 << (power_wl - tableSize - 1)) + 0.5);

            if(tmp1 < 34)
                out = out + 0.;
            else if(tmp1 < 35)
                out = out + 0.25;
            else if(tmp1 < 37)
                out = out + 0.5;
            else if(tmp1 < 40)
                out = out + 0.75;
            else if(tmp1 < 42)
                out = out + 1.0;
            else if(tmp1 < 44)
                out = out + 1.25;
            else if(tmp1 < 47)
                out = out + 1.5;
            else if(tmp1 < 49)
                out = out + 1.75;
            else if(tmp1 < 53)
                out = out + 2.0;
            else if(tmp1 < 56)
                out = out + 2.25;
            else if(tmp1 < 58)
                out = out + 2.5;
            else if(tmp1 < 62)
                out = out + 2.75;
            else
                out = out + 3.0;

//             for( int i = 0; i < wl; i++){
//                 if(tmp1 <= d[i])
//                     order = i + 1;
//                 break;
//             }
//
//             out = out + order * 3. / wl;
        }

        out = round(out * 4.);
    }else{
        wl = 16;
        tableSize = int(log2(wl));

        double snr[1000], a[1000], d[1000];
        double tmp;
        tmp = 1./wl;
        for(int i = 0; i < wl; i ++){
            snr[i] = tmp * 10*log10(2);
            a[i] = pow(10, snr[i]/10.);
            d[i] = round(a[i] * wl);

            tmp = tmp + 1./wl;
        }

        out = 3 * (power_wl - 1);
        tmpW = in;
        V = double(1 << (power_wl - 1));

        if(tmpW == 0)
            out = 0;
        else{
            int order;
            while(tmpW < V){
                tmpW = tmpW * 2.0;
                out = out - 3.;
            }

            tmp1 = floor(tmpW*1. / (1 << (power_wl - (tableSize + 1))) + 0.5);


            for( int i = 0; i < wl; i++){
                if(tmp1 <= d[i])
                    order = i + 1;
                break;
            }

            out = out + order * 3. / wl;
        }

        out = round(out * 4.);
    }
    out1 = out/4.;
    return(out1);
}
#endif
