/* uart_demo.c - RK3506g UART 921600 示例
 * Linux 用户态串口测试程序
 * 功能：以 921600bps 的波特率打开指定串口，8N1，Raw 模式，非流控默认，
 * 通过 select 事件驱动实现双向数据传输：周期向串口发送测试数据，
 * 同时读取串口数据并统计吞吐量。支持 Ctrl+C 退出并输出统计信息。
 *
 * 编译：gcc -O2 -Wall -o uart_demo uart_demo.c
 * 运行：sudo ./uart_demo /dev/ttyS1 921600 [-v] [-f]
 *   -v: 启用接收数据输出和详细日志
 *   -f: 启用硬件流控 RTS/CTS
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/select.h>
#include <signal.h>
#include <time.h>

static volatile int keep_running = 1;
static unsigned long long total_tx = 0;
static unsigned long long total_rx = 0;
static struct timespec start_time;

/* 信号处理：Ctrl+C 时退出并输出统计 */
static void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

/* 将整数 baud 转换为 speed_t（仅当平台支持时） */
static speed_t baud_to_speed(int baud) {
    switch (baud) {
#if defined(B115200)
        case 115200: return B115200;
#endif
#if defined(B230400)
        case 230400: return B230400;
#endif
#if defined(B460800)
        case 460800: return B460800;
#endif
#if defined(B921600)
        case 921600: return B921600;
#endif
        default:
            fprintf(stderr, "Unsupported baud: %d\n", baud);
            exit(EXIT_FAILURE);
    }
}

/* 设置串口属性：波特率、数据位、停止位、无校验、原始模式、流控可选 */
static int configure_port(int fd, int baud, int enable_flow) {
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        return -1;
    }

    /* 原始模式，不进行行编辑处理 */
    cfmakeraw(&tty);

    speed_t speed = baud_to_speed(baud);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    /* 8N1，打开本地收发、关闭硬件流控默认关闭 */
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~PARENB;           /* 无奇偶校验 */
    tty.c_cflag &= ~CSTOPB;           /* 1 个停止位 */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;               /* 8 数据位 */

    if (enable_flow) {
        /* 启用硬件流控 RTS/CTS */
        tty.c_cflag |= CRTSCTS;
    } else {
        tty.c_cflag &= ~CRTSCTS;
    }

    /* 最小字符数/超时设置，非阻塞方式下准确性较好 */
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1; /* 0.1 秒 */

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        return -1;
    }

    /* 清空输入输出缓冲区 */
    tcflush(fd, TCIOFLUSH);
    return 0;
}

/* 简化打印统计信息已移除，吞吐输出在结束时统一输出。 */

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <device> <baud> [-v] [-f]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* dev = argv[1];
    int baud = atoi(argv[2]);
    int verbose = 0;
    int enable_flow = 0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) verbose = 1;
        else if (strcmp(argv[i], "-f") == 0) enable_flow = 1;
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
        }
    }

    int fd = open(dev, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    if (configure_port(fd, baud, enable_flow) != 0) {
        close(fd);
        return EXIT_FAILURE;
    }

    /* 注册信号处理以便优雅退出 */
    signal(SIGINT, handle_sigint);

    /* 测试数据块 */
    #define TX_BUF_SIZE 1024
    unsigned char txbuf[TX_BUF_SIZE];
    for (int i = 0; i < TX_BUF_SIZE; i++) {
        /* 填充可打印 ASCII 字符 (32-126: ' ' 到 '~') */
        txbuf[i] = (unsigned char)(32 + (i % 95));
    }

    /* 启动及统计信息 */
    total_tx = 0;
    total_rx = 0;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    /* 设置非阻塞，使用 select 机制实现事件驱动 */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    unsigned char rxbuf[4096];
    int wptr = 0, wlen = TX_BUF_SIZE; // 简单循环发送缓冲区指针
    int first_loop = 1;
    while (keep_running) {
        // 写数据：尽可能发送数据，直到发送完毕一个块
        if (wptr < wlen) {
            ssize_t w = write(fd, txbuf + wptr, wlen - wptr);
            if (w > 0) {
                wptr += (int)w;
                total_tx += (unsigned long long)w;
            }
        } else {
            // 一轮完成后，重置以持续发送
            wptr = 0;
        }

        // 读取数据
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000; // 0.2 秒
        int sel = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (sel > 0 && FD_ISSET(fd, &rfds)) {
            ssize_t r = read(fd, rxbuf, sizeof(rxbuf));
            if (r > 0) {
                total_rx += (unsigned long long)r;
                if (verbose) {
                    // 打印接收到的数据前 256 字节，避免大量输出
                    size_t show = (r > 256) ? 256 : (size_t)r;
                    fwrite(rxbuf, 1, show, stdout);
                    fflush(stdout);
                }
            } else if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("read");
            }
        } else if (sel < 0) {
            perror("select");
        }

        // 每秒打印一次统计信息
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - start_time.tv_sec) + (now.tv_nsec - start_time.tv_nsec) / 1e9;
        if (!first_loop && (int)elapsed > 0 && ((int)elapsed) % 1 == 0) {
            /* 仅简单地每秒刷新一次统计，避免重复输出 */
            // 空实现，避免在循环中频繁打印
        }
        first_loop = 0;
    }

    /* 退出前输出总统计信息 */
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = (now.tv_sec - start_time.tv_sec) + (now.tv_nsec - start_time.tv_nsec) / 1e9;
    double tx_rkb = (total_tx / 1024.0) / (elapsed > 0 ? elapsed : 1.0);
    double rx_rkb = (total_rx / 1024.0) / (elapsed > 0 ? elapsed : 1.0);
    printf("\n--- UART test finished ---\n");
    printf("Device: %s  Baud: %d  Elapsed: %.2f s\n", dev, baud, elapsed);
    printf("TX: %llu bytes  RX: %llu bytes  Throughput: %.2f KB/s (TX) / %.2f KB/s (RX)\n",
           total_tx, total_rx, tx_rkb, rx_rkb);

    close(fd);
    return EXIT_SUCCESS;
}
