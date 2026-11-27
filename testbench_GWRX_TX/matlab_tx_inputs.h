#ifndef MATLAB_TX_INPUTS_H
#define MATLAB_TX_INPUTS_H
#include <array>
#include <vector>
#include <fstream>
#include <cmath>
#include <string>
#include <iomanip>
#include <algorithm>

struct MatlabTxUser {
    int id, mode;
    double frequency, pilot_power, tx_power;
    std::array<double,16> ah;
};

struct MatlabTxInputs {
    std::array<double,8> noise;
    std::array<double,16> calibration;
    std::vector<MatlabTxUser> users;

    static bool number(std::istream& in, double& value) {
        return bool(in >> value) && std::isfinite(value);
    }

    bool load(const std::string& path, int expected_mode) {
        std::ifstream in(path.c_str());
        std::string version;
        int count=0, antennas=0;
        if (!(in >> version >> count >> antennas) || version!="MATLAB_TX_V1" ||
            count<1 || count>128 || antennas!=8) return false;
        for (double& v:noise) if (!number(in,v) || v<0) return false;
        for (double& v:calibration) if (!number(in,v)) return false;
        users.clear();
        for (int i=0; i<count; ++i) {
            MatlabTxUser user;
            if (!(in >> user.id >> user.mode) || user.id<1 || user.id>128 || user.mode!=expected_mode)
                return false;
            for (const auto& old:users) if (old.id==user.id) return false;
            if (!number(in,user.frequency) || !number(in,user.pilot_power) || user.pilot_power<=0 ||
                !number(in,user.tx_power) || user.tx_power<=0) return false;
            for (double& v:user.ah) if (!number(in,v)) return false;
            users.push_back(user);
        }
        std::string extra;
        return !(in >> extra);
    }
};
#endif
