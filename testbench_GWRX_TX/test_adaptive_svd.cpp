#include "../Lib_io.h"
#include "../Cls_svd.h"
#include <iostream>
int main() {
    static Cls_svd<double> model;
    SVD_CFG cfg = {};
    if(cfg.max_sweeps!=6) { std::cerr << "Default sweep cap is not 6\n"; return 10; }
    cfg.ant_num=8;
    model.configure(cfg);
    char folder[]="./";
    model.init(folder);
    Complex<double> a[8][8] = {};
    Complex<double>* rows[8];
    for(int i=0;i<8;++i) { rows[i]=a[i]; a[i][i]=1.0; }
    model.go(rows);
    double squared=0;
    for(int i=0;i<8;++i) for(int j=0;j<8;++j) {
        Complex<double> reconstructed=0;
        for(int k=0;k<8;++k)
            reconstructed+=model.m_U[i][k]*model.m_S[k][k]*model.m_V[j][k].conjug();
        squared+=(reconstructed-a[i][j]).power();
    }
    if(!std::isfinite(squared) || squared>1e-20) {
        std::cerr << "FAIL: orthogonal identity columns were changed; squared residual=" << squared << '\n';
        return 1;
    }
    std::cout << "PASS: identity columns preserved\n";
    if (!model.m_status.converged || model.m_status.sweeps!=1 || model.m_status.rotations!=0 ||
        model.m_status.skipped_pairs!=28 || model.m_status.hit_max_sweeps) return 2;
    model.reset();
    if (!model.m_status.converged || model.m_status.sweeps!=1) return 3;
    // Near-orthogonal pair falls below the configured relative threshold.
    a[0][1]=0.0001;
    model.go(rows);
    if (!model.m_status.converged || model.m_status.rotations!=0) return 4;
    // A correlated matrix with a one-sweep budget must report nonconvergence.
    a[0][1]=0.6;
    a[1][2]=0.4;
    cfg.max_sweeps=1;
    cfg.correlation_tol_shift=20;
    model.configure(cfg);
    model.go(rows);
    if (model.m_status.converged || !model.m_status.hit_max_sweeps ||
        model.m_status.sweeps!=1 || model.m_status.rotations==0) return 5;
    if(model.m_status.rotations+model.m_status.skipped_pairs!=28) return 6;
    // Repeated use clears the old nonconvergence status for a zero matrix.
    for(int i=0;i<8;++i) for(int j=0;j<8;++j) a[i][j]=0;
    model.go(rows);
    if(!model.m_status.converged || model.m_status.rotations || model.m_status.hit_max_sweeps) return 7;
    cfg.max_sweeps=0;
    bool rejected=false;
    try { model.configure(cfg); } catch(const std::invalid_argument&) { rejected=true; }
    if(!rejected) return 8;
    static Cls_svd<fxp> fixed;
    cfg=SVD_CFG();
    fixed.configure(cfg);fixed.init(folder);
    for(int i=0;i<8;++i) a[i][i]=1;
    fixed.go(rows);
    if(!fixed.m_status.converged || fixed.m_status.sweeps!=1 || fixed.m_status.rotations) return 9;
    std::cout << "PASS: double/fxp early exit, weak pair, sweep cap, counters, reset, invalid config\n";
    return 0;
}
