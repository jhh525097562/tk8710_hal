#include "matlab_tx_inputs.h"
#include <iostream>
int main(int argc, char** argv) {
    if(argc!=2) return 2;
    MatlabTxInputs input;
    if(!input.load(argv[1],14)) return 3;
    std::cout << "VALID users=" << input.users.size() << '\n';
    return 0;
}
