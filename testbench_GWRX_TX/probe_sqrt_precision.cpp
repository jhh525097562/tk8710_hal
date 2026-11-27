#include "../Lib_io.h"
#include "../Cls_sqrt.h"
#include <iomanip>
int main() {
    Cls_sqrt<double> root;
    SQRT_CFG cfg;
    cfg.bit_len=22;
    root.configure(cfg);
    char folder[]="./";
    root.init(folder);
    const double inputs[]={0.25,0.3,0.5,0.9,1.01,2.0,4.0,1e-8,1e8};
    std::cout << "input,custom_sqrt,std_sqrt,relative_error_percent\n" << std::setprecision(17);
    for(double x:inputs) {
        const double exact=std::sqrt(x),actual=root.sqrt(x);
        std::cout << x << ',' << actual << ',' << exact << ',' << (actual-exact)/exact*100 << '\n';
    }
    return 0;
}
