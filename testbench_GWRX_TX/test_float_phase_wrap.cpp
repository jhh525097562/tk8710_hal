#include "../Lib_io.h"
#include "../Cls_tx_us_2_1.h"
#include <vector>

static std::vector<Complex<double> > interpolate(double first, double second) {
    // Legacy constructor leaves some scratch pointers unspecified. Static
    // storage initializes them to zero without changing the algorithm under test.
    static Cls_tx_us_2_1<double> model;
    TX_US_2_1_Cfg cfg = {};
    cfg.tx_data_len_cfg = 2;
    cfg.data_times = 3;
    cfg.BlockNum = 1;
    cfg.tx_samplingrate = 125000;
    model.m_iblock = 0;
    model.m_freq_in = 0;
    model.config(cfg);
    char folder[] = "./";
    model.init(folder);
    std::vector<Complex<double> > out;
    for (int tick = 0; tick < 100; ++tick) {
        model.m_i_in_en = tick < 2;
        model.go(tick == 0 ? first : second);
        if (model.m_out_en || model.m_finished_o) out.push_back(model.m_out_iq.todouble());
        if (model.m_finished_o) return out;
    }
    return out;
}

int main() {
    const double pairs[][4] = {
        {-1.375, 2.8674174785275226, .625, .8674174785275226},
        {6.9, -4.9, .9, -.9},
        {-6.9, 4.9, -.9, .9},
        {1, 1.25, -1, -.75}
    };
    for (int test = 0; test < 4; ++test) {
        const std::vector<Complex<double> > out = interpolate(pairs[test][0], pairs[test][1]);
        if (out.size() != 17) { std::cerr << "Incorrect output length\n"; return 2; }
        double delta = pairs[test][3] - pairs[test][2];
        if (delta > 1) delta -= 2;
        if (delta < -1) delta += 2;
        for (int i = 0; i <= 8; ++i) {
            double phase = PI * (pairs[test][2] + delta*i/8.0);
            double error = std::hypot(out[i].I-std::cos(phase),out[i].Q-std::sin(phase));
            if (error > 1e-12) {
                std::cerr << "FAIL case=" << test << " sample=" << i << " complex_error=" << error << '\n';
                return 1;
            }
        }
    }
    std::cout << "PASS: 4 real 8x interpolation phase-wrap cases\n";
    return 0;
}
