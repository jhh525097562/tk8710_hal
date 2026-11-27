#ifndef GWRXTX_TESTBENCH_OPTIONS_H
#define GWRXTX_TESTBENCH_OPTIONS_H

#include <cerrno>
#include <cmath>
#include <climits>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#ifdef _WIN32
#include <direct.h>
#define TB_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define TB_MKDIR(path) mkdir(path, 0777)
#endif

struct TestbenchOptions {
    std::string input_root = "/home/data/K2301/K2301PHY_CaseList_Data";
    std::string output_root = "/home/data/K2301/K2301PHY_CaseList_Data";
    int frames = 100;
    std::string tx_bits_file;
    std::string tx_user_ids_file;
    std::string tx_reference_file;

    bool parse(int argc, char** argv) {
        if (argc < 92 || (argc - 92) % 2 != 0) {
            std::cerr << "Usage: GWRXTX <91 case parameters> [--input-root path] "
                         "[--output-root path] [--frames n] [--tx-bits-file path] [--tx-user-ids-file path] [--tx-reference-file path]\n";
            return false;
        }
        for (int i = 1; i <= 91; ++i) {
            char* end = NULL;
            errno = 0;
            double value = std::strtod(argv[i], &end);
            if (errno || end == argv[i] || *end || !std::isfinite(value)) {
                std::cerr << "Invalid numeric parameter " << i << '\n';
                return false;
            }
            bool floating = i == 6 || i == 7 || i == 10 || i == 11 || i == 24 ||
                i == 26 || i == 27 || i == 30 || i == 31 || i == 37 ||
                (i >= 41 && i <= 59) || i == 62 || i == 67 || i == 68 ||
                i == 69 || i == 72;
            if (!floating && (value < INT_MIN || value > INT_MAX || value != std::floor(value))) {
                std::cerr << "Invalid integer parameter " << i << '\n';
                return false;
            }
        }
        for (int i = 92; i < argc; i += 2) {
            std::string key(argv[i]);
            if (key == "--input-root") input_root = argv[i + 1];
            else if (key == "--output-root") output_root = argv[i + 1];
            else if (key == "--tx-bits-file") tx_bits_file = argv[i + 1];
            else if (key == "--tx-user-ids-file") tx_user_ids_file = argv[i + 1];
            else if (key == "--tx-reference-file") tx_reference_file = argv[i + 1];
            else if (key == "--frames") {
                char* end = NULL;
                errno = 0;
                long n = std::strtol(argv[i + 1], &end, 10);
                if (errno || end == argv[i + 1] || *end || n < 1 || n > 100000) return false;
                frames = static_cast<int>(n);
            } else {
                std::cerr << "Unknown option: " << key << '\n';
                return false;
            }
        }
        return !input_root.empty() && !output_root.empty() &&
            (tx_user_ids_file.empty() || !tx_bits_file.empty()) &&
            (tx_reference_file.empty() || (frames==1 && !tx_bits_file.empty() && !tx_user_ids_file.empty()));
    }

    template <size_t N>
    void case_prefix(char (&out)[N], bool input, const char* module) const {
        const std::string& root = input ? input_root : output_root;
        int n = std::snprintf(out, N, "%s/%s/case", root.c_str(), module);
        // Existing callers append case number, slash and a filename.
        if (n < 0 || static_cast<size_t>(n) + 64 >= N) {
            std::cerr << "Data root is too long\n";
            std::exit(2);
        }
    }
};
#endif
