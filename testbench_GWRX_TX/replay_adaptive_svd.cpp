#include "../Lib_io.h"
#include "../Cls_svd.h"
#include <fstream>
#include <iomanip>

template<class T> int replay(const char* path,const char* prefix,int maximum,int shift) {
    static Cls_svd<T> model;
    SVD_CFG cfg;
    cfg.max_sweeps=maximum;cfg.correlation_tol_shift=shift;
    model.configure(cfg);
    char folder[]="./";model.init(folder);
    std::ifstream input(path);
    std::ofstream status((std::string(prefix)+"_status.csv").c_str());
    std::ofstream matrices((std::string(prefix)+"_matrices.txt").c_str());
    if(!input || !status || !matrices) return 2;
    status << "index,sweeps,rotations,skipped,converged,hit_max,precision_limited\n";
    matrices << std::setprecision(17);
    int index=0;
    while(true) {
        Complex<double> a[8][8]={};Complex<double>* rows[8];
        input >> std::ws;if(input.eof()) break;
        for(int i=0;i<8;++i) {
            rows[i]=a[i];
            for(int j=0;j<8;++j) {
                double trace[9];
                for(int k=0;k<9;++k) if(!(input>>trace[k])) return 3;
                a[i][j].I=trace[0];a[i][j].Q=trace[1];
            }
        }
        model.go(rows);
        const SVD_STATUS& s=model.m_status;
        status << index++ << ',' << s.sweeps << ',' << s.rotations << ',' << s.skipped_pairs << ','
               << s.converged << ',' << s.hit_max_sweeps << ',' << s.precision_limited << '\n';
        if(s.rotations+s.skipped_pairs!=28*s.sweeps) return 4;
        if(s.converged && s.rotations_per_sweep[s.sweeps-1]!=0) return 5;
        for(int i=0;i<8;++i) for(int j=0;j<8;++j) {
            matrices << a[i][j].I << ' ' << a[i][j].Q << ' '
                     << model.m_U[i][j].I << ' ' << model.m_U[i][j].Q << ' '
                     << model.m_V[i][j].I << ' ' << model.m_V[i][j].Q << ' ' << model.m_S[i][j] << '\n';
        }
    }
    std::cout << "REPLAY_COMPLETE matrices=" << index << '\n';
    return index ? 0:6;
}

int main(int argc,char** argv) {
    if(argc!=6) return 2;
    try {
        if(std::string(argv[1])=="fixed") return replay<fxp>(argv[2],argv[3],std::atoi(argv[4]),std::atoi(argv[5]));
        if(std::string(argv[1])=="float") return replay<double>(argv[2],argv[3],std::atoi(argv[4]),std::atoi(argv[5]));
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 10;}
    return 2;
}
