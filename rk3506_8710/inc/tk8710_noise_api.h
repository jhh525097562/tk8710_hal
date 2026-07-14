/**
 * @file tk8710_noise_api.h
 * @brief TK8710 噪底能量计算 API
 * @note 用于计算 8 天线的噪底能量 (dBm 格式)
 * 
 * 速率模式与 FFT 长度对应：
 *   mode5, 6, 7, 8  → FFT 长度 8192
 *   mode9            → FFT 长度 4096
 *   mode10           → FFT 长度 2048
 *   mode11, 18       → FFT 长度 1024
 * 
 * 使用方法：
 *   1. 先调用 TK8710DebugCtrl(TK8710_DBG_TYPE_CAPTURE_DATA) 采集数据
 *   2. 调用 tk8710_noise_process() 计算噪底
 * 
 * 示例:
 *   // 采集数据成功后
 *   tk8710_noise_process("8710CaptureData", 8, "ANoise.txt");
 * 
 */

#ifndef TK8710_NOISE_API_H
#define TK8710_NOISE_API_H

#include "driver/tk8710_platform.h"

#if defined(PLATFORM_TMS570)

#include <stdint.h>

typedef struct {
    float real;
    float imag;
} tk8710_complex_t;

typedef struct {
    int anoise_the1;
    int anoise_the2;
    float anoise_updata_per;
} tk8710_gwrx_para_t;

typedef struct {
    int rate_mode;
    int fft_len;
    int data_len;
    int num_antennas;
    tk8710_gwrx_para_t para;
} tk8710_fft_config_t;

static inline int tk8710_noise_process(const char* capture_dir, int rate_mode, const char* result_file)
{
    (void)capture_dir;
    (void)rate_mode;
    (void)result_file;
    return -1;
}

static inline int tk8710_calculate_sweep_noise(const char* capture_dir,
                                               const char* result_file,
                                               int rate_mode,
                                               int append_result)
{
    (void)capture_dir;
    (void)result_file;
    (void)rate_mode;
    (void)append_result;
    return -1;
}

static inline int tk8710_sweep_noise_process(const char* data_dir,
                                             int rate_mode,
                                             double frequency,
                                             int append_result)
{
    (void)data_dir;
    (void)rate_mode;
    (void)frequency;
    (void)append_result;
    return -1;
}

static inline const char* tk8710_noise_get_error_string(int error_code)
{
    (void)error_code;
    return "noise file processing disabled on PLATFORM_TMS570";
}

#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define TK8710_MAYBE_UNUSED __attribute__((unused))
#else
#define TK8710_MAYBE_UNUSED
#endif

/*============================================================================
 * 常量定义
 *============================================================================*/
#define TK8710_NUM_ANTENNAS       8       /* 天线数量 */

/* 速率模式定义 */
#define TK8710_RATE_MODE_5        5       /* FFT 长度 8192 */
#define TK8710_RATE_MODE_6        6       /* FFT 长度 8192 */
#define TK8710_RATE_MODE_7        7       /* FFT 长度 8192 */
#define TK8710_RATE_MODE_8        8       /* FFT 长度 8192 */
#define TK8710_RATE_MODE_9        9       /* FFT 长度 4096 */
#define TK8710_RATE_MODE_10       10      /* FFT 长度 2048 */
#define TK8710_RATE_MODE_11       11      /* FFT 长度 1024 */
#define TK8710_RATE_MODE_18       18      /* FFT 长度 1024 */

/* 噪底计算参数 */
#define TK8710_ANOISE_THE1        4       /* 噪底阈值 1 */
#define TK8710_ANOISE_THE2        5       /* 噪底阈值 2 */
#define TK8710_ANOISE_MIN_VAL     (1.0f / 2048.0f)
#define TK8710_ANOISE_OFFSET      -140.0f /* dBm/hz 转换偏移量 */

/*============================================================================
 * 数据结构
 *============================================================================*/
/* 复数类型 */
typedef struct {
    float real;
    float imag;
} tk8710_complex_t;

/* GWRX 参数 */
typedef struct {
    int anoise_the1;       /* 噪底阈值 1 */
    int anoise_the2;       /* 噪底阈值 2 */
    float anoise_updata_per; /* 噪底更新系数 */
} tk8710_gwrx_para_t;

/* FFT 配置 */
typedef struct {
    int rate_mode;         /* 速率模式 */
    int fft_len;           /* FFT 长度 */
    int data_len;          /* 原始数据长度 */
    int num_antennas;      /* 天线数量 */
    tk8710_gwrx_para_t para; /* GWRX 参数 */
} tk8710_fft_config_t;

/*============================================================================
 * 工具函数
 *============================================================================*/
static inline tk8710_complex_t tk8710_complex_mul(tk8710_complex_t a, tk8710_complex_t b) {
    tk8710_complex_t r; 
    r.real = a.real * b.real - a.imag * b.imag; 
    r.imag = a.real * b.imag + a.imag * b.real; 
    return r;
}

static inline tk8710_complex_t tk8710_complex_add(tk8710_complex_t a, tk8710_complex_t b) {
    tk8710_complex_t r; 
    r.real = a.real + b.real; 
    r.imag = a.imag + b.imag; 
    return r;
}

static inline tk8710_complex_t tk8710_complex_sub(tk8710_complex_t a, tk8710_complex_t b) {
    tk8710_complex_t r; 
    r.real = a.real - b.real; 
    r.imag = a.imag - b.imag; 
    return r;
}

/*============================================================================
 * FFT 实现 - Radix-2
 *============================================================================*/
static void tk8710_bit_reverse(tk8710_complex_t* data, int n) {
    int j = 0;
    for (int i = 0; i < n - 1; i++) {
        if (i < j) {
            tk8710_complex_t t = data[i];
            data[i] = data[j];
            data[j] = t;
        }
        int k = n / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }
}

static void tk8710_fft_radix2(tk8710_complex_t* data, int n) {
    tk8710_bit_reverse(data, n);
    for (int step = 1; step < n; step *= 2) {
        for (int m = 0; m < step; m++) {
            float angle = -3.14159265358979f * m / step;
            tk8710_complex_t w = {cosf(angle), sinf(angle)};
            for (int k = m; k < n; k += step * 2) {
                int k2 = k + step;
                tk8710_complex_t t = tk8710_complex_mul(w, data[k2]);
                tk8710_complex_t u = data[k];
                data[k] = tk8710_complex_add(u, t);
                data[k2] = tk8710_complex_sub(u, t);
            }
        }
    }
}

/*============================================================================
 * 配置初始化
 *============================================================================*/
static void tk8710_fft_get_config(int rate_mode, tk8710_fft_config_t* config) {
    config->rate_mode = rate_mode;
    config->num_antennas = TK8710_NUM_ANTENNAS;
    config->para.anoise_the1 = TK8710_ANOISE_THE1;
    config->para.anoise_the2 = TK8710_ANOISE_THE2;
    config->para.anoise_updata_per = 0.1f;
    
    switch (rate_mode) {
        case TK8710_RATE_MODE_5:
        case TK8710_RATE_MODE_6:
        case TK8710_RATE_MODE_7:
        case TK8710_RATE_MODE_8:
            config->fft_len = 8192;
            config->data_len = 16384;
            break;
        case TK8710_RATE_MODE_9:
            config->fft_len = 4096;
            config->data_len = 8192;
            break;
        case TK8710_RATE_MODE_10:
            config->fft_len = 2048;
            config->data_len = 4096;
            break;
        case TK8710_RATE_MODE_11:
        case TK8710_RATE_MODE_18:
            config->fft_len = 1024;
            config->data_len = 2048;
            break;
        default:
            config->fft_len = 8192;
            config->data_len = 16384;
            break;
    }
}

/*============================================================================
 * 数据读取
 *============================================================================*/
static int tk8710_read_antenna_file(const char* data_dir, int antenna_idx, 
                                    tk8710_complex_t* data, int fft_len, int data_len) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/AntennaData%d.bin", data_dir, antenna_idx + 1);
    
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("[NOISE] Cannot open file: %s\n", filename);
        return -1;
    }
    
    int16_t* raw_data = (int16_t*)malloc(data_len * sizeof(int16_t));
    if (!raw_data) {
        fclose(fp);
        return -1;
    }
    
    int count = fread(raw_data, sizeof(int16_t), data_len, fp);
    fclose(fp);
    
    if (count != data_len) {
        free(raw_data);
        printf("[NOISE] File read incomplete: %s\n", filename);
        return -1;
    }
    
    /* 转换为复数格式 */
    for (int i = 0; i < fft_len; i++) {
        data[i].real = (float)raw_data[i * 2] / 32768.0f;
        data[i].imag = (float)raw_data[i * 2 + 1] / 32768.0f;
        if(i >= fft_len-3){
            data[i].real = (float)0.0 / 32768.0f;
            data[i].imag = (float)0.0 / 32768.0f;
        }
    }
    
    free(raw_data);
    return 0;
}

static int tk8710_read_all_antenna_data(const char* data_dir, tk8710_complex_t** antenna_data,
                                        int num_antennas, int fft_len, int data_len) {
    *antenna_data = (tk8710_complex_t*)malloc(num_antennas * fft_len * sizeof(tk8710_complex_t));
    if (!*antenna_data) {
        return -1;
    }
    
    int success_count = 0;
    for (int ant = 0; ant < num_antennas; ant++) {
        if (tk8710_read_antenna_file(data_dir, ant, 
                                     &((*antenna_data)[ant * fft_len]), 
                                     fft_len, data_len) == 0) {
            success_count++;
        }
    }
    
    return (success_count == num_antennas) ? 0 : -1;
}

/*============================================================================
 * FFT 模块
 *============================================================================*/
static int tk8710_fft_process_with_noise(tk8710_complex_t* antenna_data, float* noise_floor,
                                          int num_antennas, int fft_len,
                                          tk8710_gwrx_para_t* para, int flag_cal_noise) {
    tk8710_complex_t* fft_data = (tk8710_complex_t*)malloc(num_antennas * fft_len * sizeof(tk8710_complex_t));
    if (!fft_data) {
        return -1;
    }
    
    /* FFT 变换 */
    for (int ant = 0; ant < num_antennas; ant++) {
        tk8710_complex_t* data_in = &antenna_data[ant * fft_len];
        tk8710_complex_t* data_out = &fft_data[ant * fft_len];
        
        memcpy(data_out, data_in, fft_len * sizeof(tk8710_complex_t));
        tk8710_fft_radix2(data_out, fft_len);
        data_out[0] = data_out[1];
    }
    
    /* 噪底计算 */
    if (flag_cal_noise) {
        float* power_spectrum = (float*)malloc(num_antennas * fft_len * sizeof(float));
        if (!power_spectrum) {
            free(fft_data);
            return -1;
        }
        
        /* 计算功率谱 */
        for (int ant = 0; ant < num_antennas; ant++) {
            tk8710_complex_t* fft_ant = &fft_data[ant * fft_len];
            float* power_ant = &power_spectrum[ant * fft_len];
            
            tk8710_complex_t data_cal[fft_len];
            memcpy(data_cal, fft_ant, fft_len * sizeof(tk8710_complex_t));
            data_cal[0] = data_cal[1];
            
            for (int i = 0; i < fft_len; i++) {
                power_ant[i] = data_cal[i].real * data_cal[i].real + 
                              data_cal[i].imag * data_cal[i].imag;
            }
        }
        
        /* 计算噪底 - 几何平均法 */
        for (int ant = 0; ant < num_antennas; ant++) {
            float* power_ant = &power_spectrum[ant * fft_len];
            
            float amax = power_ant[0];
            float amin = power_ant[0];
            
            for (int i = 1; i < fft_len; i++) {
                if (power_ant[i] > amax) amax = power_ant[i];
                if (power_ant[i] < amin) amin = power_ant[i];
            }
            
            if (amin < TK8710_ANOISE_MIN_VAL) amin = TK8710_ANOISE_MIN_VAL;
            if (amax < TK8710_ANOISE_MIN_VAL) amax = TK8710_ANOISE_MIN_VAL;
            
            if (amax == amin) {
                noise_floor[ant] = 1.0f;
                continue;
            }
            
            for (int iter = 0; iter < 7; iter++) {
                float threshold = sqrtf(amin * amax);
                int tmp_m = 0;
                
                for (int i = 0; i < fft_len; i++) {
                    if (power_ant[i] < threshold) {
                        tmp_m++;
                    }
                }
                
                if (tmp_m < fft_len / para->anoise_the1) {
                    amin = threshold;
                } else {
                    amax = threshold;
                }
                
                noise_floor[ant] = sqrtf(amin * amax);
                
                float tmpn = amax / amin;
                if (tmpn <= (float)para->anoise_the2 / (float)para->anoise_the1) {
                    break;
                }
            }
        }
        
        free(power_spectrum);
    }
    
    free(fft_data);
    return 0;
}

/*============================================================================
 * 保存结果
 *============================================================================*/
static int tk8710_save_noise_floor(float* noise_floor, int num_antennas, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        printf("[NOISE] Cannot create output file: %s\n", filename);
        return -1;
    }
    
    for (int ant = 0; ant < num_antennas; ant++) {
        float noise_dbm = 10.0f * log10f(noise_floor[ant]) + TK8710_ANOISE_OFFSET;
        fprintf(fp, "%.4f\n", noise_dbm);
    }
    
    fclose(fp);
    return 0;
}

static int tk8710_ensure_directory(const char* directory) {
#ifdef _WIN32
    if (_mkdir(directory) == 0 || errno == EEXIST) {
        return 0;
    }
#else
    if (mkdir(directory, 0755) == 0 || errno == EEXIST) {
        return 0;
    }
#endif
    printf("[NOISE] Cannot create directory: %s\n", directory);
    return -1;
}

static int tk8710_save_sweep_noise_floor(float* noise_floor, int num_antennas,
                                         uint32_t frequency, uint8_t append_result) {
    const char* result_dir = "SweepFreqResult";
    const char* result_file = "SweepFreqResult/Result.txt";

    if (tk8710_ensure_directory(result_dir) != 0) {
        return -1;
    }

    FILE* fp = fopen(result_file, append_result ? "a" : "w");
    if (!fp) {
        printf("[NOISE] Cannot create output file: %s\n", result_file);
        return -1;
    }

    fprintf(fp, "%u\n", frequency);
    for (int ant = 0; ant < num_antennas; ant++) {
        float noise_dbm = 10.0f * log10f(noise_floor[ant]) + TK8710_ANOISE_OFFSET;
        fprintf(fp, "%.4f\n", noise_dbm);
    }

    fclose(fp);
    return 0;
}

/*============================================================================
 * API 接口
 *============================================================================*/

/**
 * @brief 噪底处理主函数
 * @param data_dir 数据目录路径 (如 "8710CaptureData")
 * @param rate_mode 速率模式 (5-11, 18)
 * @param output_file 输出文件名 (如 "ANoise.txt")
 * @return 0 成功, -1 失败
 */
static TK8710_MAYBE_UNUSED int tk8710_noise_process(const char* data_dir, int rate_mode, const char* output_file) {
    tk8710_fft_config_t config;
    tk8710_complex_t* antenna_data = NULL;
    float noise_floor[TK8710_NUM_ANTENNAS];
    
    /* 获取配置 */
    tk8710_fft_get_config(rate_mode, &config);
    
    printf("[NOISE] FFT config:\n");
    printf("[NOISE]   Rate mode: %d\n", config.rate_mode);
    printf("[NOISE]   FFT length: %d\n", config.fft_len);
    printf("[NOISE]   Raw data length: %d\n", config.data_len);
    printf("[NOISE]   Antenna count: %d\n", config.num_antennas);
    printf("[NOISE]   Noise threshold 1: %d\n", config.para.anoise_the1);
    printf("[NOISE]   Noise threshold 2: %d\n", config.para.anoise_the2);
    
    /* 读取数据 */
    printf("[NOISE] Reading antenna data...\n");
    if (tk8710_read_all_antenna_data(data_dir, &antenna_data, 
                                      config.num_antennas, 
                                      config.fft_len, 
                                      config.data_len) != 0) {
        printf("[NOISE] Error: data read failed\n");
        return -1;
    }
    printf("[NOISE] Data read succeeded\n");
    
    /* FFT 处理 */
    printf("[NOISE] Processing FFT...\n");
    if (tk8710_fft_process_with_noise(antenna_data, noise_floor,
                                       config.num_antennas, config.fft_len,
                                       &config.para, 1) != 0) {
        printf("[NOISE] Error: FFT processing failed\n");
        free(antenna_data);
        return -1;
    }
    printf("[NOISE] FFT processing completed\n");
    
    free(antenna_data);
    
    /* 保存结果 */
    printf("[NOISE] Save result to: %s\n", output_file);
    if (tk8710_save_noise_floor(noise_floor, config.num_antennas, output_file) != 0) {
        printf("[NOISE] Error: save result failed\n");
        return -1;
    }
    
    /* 打印结果 (dBm 格式) */
    printf("[NOISE] === Noise floor result (dBm) ===\n");
    for (int ant = 0; ant < config.num_antennas; ant++) {
        float noise_dbm = 10.0f * log10f(noise_floor[ant]) + TK8710_ANOISE_OFFSET;
        printf("[NOISE]   Antenna %d: %.4f dBm\n", ant + 1, noise_dbm);
    }
    printf("[NOISE] ========================\n");
    
    return 0;
}

/**
 * @brief 获取噪底计算结果 (仅计算，不保存)
 * @param data_dir 数据目录路径
 * @param rate_mode 速率模式
 * @param noise_floor 输出: 噪底数组 (大小为 TK8710_NUM_ANTENNAS)
 * @return 0 成功, -1 失败
 */
static TK8710_MAYBE_UNUSED int tk8710_get_noise_floor(const char* data_dir, int rate_mode, float* noise_floor) {
    tk8710_fft_config_t config;
    tk8710_complex_t* antenna_data = NULL;
    
    tk8710_fft_get_config(rate_mode, &config);
    
    if (tk8710_read_all_antenna_data(data_dir, &antenna_data,
                                      config.num_antennas,
                                      config.fft_len,
                                      config.data_len) != 0) {
        return -1;
    }
    
    int result = tk8710_fft_process_with_noise(antenna_data, noise_floor,
                                               config.num_antennas, config.fft_len,
                                               &config.para, 1);
    
    free(antenna_data);
    return result;
}

static TK8710_MAYBE_UNUSED int tk8710_sweep_noise_process(const char* data_dir, int rate_mode,
                                      uint32_t frequency, uint8_t append_result) {
    float noise_floor[TK8710_NUM_ANTENNAS];

    if (tk8710_get_noise_floor(data_dir, rate_mode, noise_floor) != 0) {
        printf("[NOISE] Error: sweep noise calculation failed\n");
        return -1;
    }

    printf("[NOISE] Save sweep result to: SweepFreqResult/Result.txt\n");
    if (tk8710_save_sweep_noise_floor(noise_floor, TK8710_NUM_ANTENNAS,
                                      frequency, append_result) != 0) {
        printf("[NOISE] Error: save sweep result failed\n");
        return -1;
    }

    return 0;
}

#endif /* PLATFORM_TMS570 */

#endif /* TK8710_NOISE_API_H */
