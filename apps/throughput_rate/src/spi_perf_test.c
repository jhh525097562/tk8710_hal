/*******************************************************************************
 * SPI Performance Test Tool
 * Description: Tests SPI throughput with 1MB data transfer at 16MHz
 * Features: CPU affinity binding, CPU usage statistics, precise timing
 ******************************************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <time.h>
#include <sched.h>
#include <sys/time.h>
#include <errno.h>

#define SPI_DEVICE "/dev/spidev0.0"
#define BUFFER_SIZE (1024 * 1024)  // 1MB
#define SPI_SPEED 16000000        // 16MHz
#define SPI_BITS_PER_WORD 8
#define SPI_MODE 0

// 32KB chunks for optimal performance with updated driver
#define CHUNK_SIZE (32 * 1024)
#define NUM_CHUNKS (BUFFER_SIZE / CHUNK_SIZE)

/*******************************************************************************
 * Function: set_cpu_affinity
 * Description: Bind current process to specified CPU core
 ******************************************************************************/
int set_cpu_affinity(int cpu_core) {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(cpu_core, &cpu_set);
    
    if (sched_setaffinity(0, sizeof(cpu_set), &cpu_set) < 0) {
        perror("Failed to set CPU affinity");
        return -1;
    }
    
    printf("Process bound to CPU core %d\n", cpu_core);
    return 0;
}

/*******************************************************************************
 * Function: get_cpu_time_stats
 * Description: Get CPU usage statistics for current process
 ******************************************************************************/
void get_cpu_time_stats(clock_t start_time, struct timeval *start_tv) {
    clock_t end_time = clock();
    struct timeval end_tv;
    gettimeofday(&end_tv, NULL);
    
    // Calculate CPU time (user + system time)
    double cpu_time_used = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    // Calculate wall clock time
    double wall_time = (end_tv.tv_sec - start_tv->tv_sec) + 
                      (end_tv.tv_usec - start_tv->tv_usec) / 1000000.0;
    
    // Calculate CPU usage percentage
    double cpu_usage = (cpu_time_used / wall_time) * 100.0;
    
    printf("CPU Statistics:\n");
    printf("  Total Time: %.3f seconds\n", wall_time);
    printf("  CPU Time: %.3f seconds\n", cpu_time_used);
    printf("  CPU Usage: %.1f%%\n", cpu_usage);
    printf("  Throughput: %.2f MB/s\n", (BUFFER_SIZE / (1024.0 * 1024.0)) / wall_time);
}

/*******************************************************************************
 * Function: generate_test_data
 * Description: Generate pseudo-random test data
 ******************************************************************************/
void generate_test_data(unsigned char *buffer, size_t size) {
    unsigned int seed = 0x12345678;
    for (size_t i = 0; i < size; i++) {
        seed = seed * 1103515245 + 12345;
        buffer[i] = (unsigned char)(seed >> 16);
    }
}

/*******************************************************************************
 * Function: verify_data
 * Description: Verify received data matches transmitted data
 ******************************************************************************/
int verify_data(unsigned char *tx_buffer, unsigned char *rx_buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (tx_buffer[i] != rx_buffer[i]) {
            printf("Data mismatch at byte %zu: TX=0x%02X, RX=0x%02X\n", 
                   i, tx_buffer[i], rx_buffer[i]);
            return -1;
        }
    }
    return 0;
}

/*******************************************************************************
 * Function: spi_transfer_chunked
 * Description: Perform SPI transfer using looped ioctl calls due to bufsiz limit
 ******************************************************************************/
int spi_transfer_chunked(int fd, unsigned char *tx_buffer, unsigned char *rx_buffer) {
    struct spi_ioc_transfer tr;
    
    // Loop through chunks
    // Note: Since kernel bufsiz is 32KB, we cannot send 1MB in a single ioctl message.
    // We must break it down into multiple ioctl calls, one per chunk.
    for (int i = 0; i < NUM_CHUNKS; i++) {
        memset(&tr, 0, sizeof(tr));
        tr.tx_buf = (unsigned long)(tx_buffer + i * CHUNK_SIZE);
        tr.rx_buf = (unsigned long)(rx_buffer + i * CHUNK_SIZE);
        tr.len = CHUNK_SIZE;
        tr.speed_hz = SPI_SPEED;
        tr.bits_per_word = SPI_BITS_PER_WORD;
        tr.cs_change = 0; // CS will deassert between ioctl calls unless driver has special handling

        // Perform the transfer for this chunk
        if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
            perror("SPI transfer failed");
            return -1;
        }
    }
    
    return 0;
}

/*******************************************************************************
 * Function: main
 * Description: Main program entry point
 ******************************************************************************/
int main(int argc, char *argv[]) {
    int fd;
    unsigned char *tx_buffer, *rx_buffer;
    clock_t start_clock;
    struct timeval start_time;
    
    printf("============================================\n");
    printf("SPI Performance Test Tool\n");
    printf("Device: %s\n", SPI_DEVICE);
    printf("Speed: %d Hz (%.1f MHz)\n", SPI_SPEED, SPI_SPEED / 1000000.0);
    printf("Buffer Size: %d MB\n", BUFFER_SIZE / (1024 * 1024));
    printf("Chunk Size: %d KB\n", CHUNK_SIZE / 1024);
    printf("============================================\n");
    
    // Set CPU affinity to core 2
    if (set_cpu_affinity(2) < 0) {
        return 1;
    }
    
    // Allocate buffers
    tx_buffer = malloc(BUFFER_SIZE);
    rx_buffer = malloc(BUFFER_SIZE);
    if (!tx_buffer || !rx_buffer) {
        perror("Failed to allocate buffers");
        return 1;
    }
    
    // Generate test data
    printf("Generating test data...\n");
    generate_test_data(tx_buffer, BUFFER_SIZE);
    memset(rx_buffer, 0, BUFFER_SIZE);
    
    // Open SPI device
    fd = open(SPI_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open SPI device");
        free(tx_buffer);
        free(rx_buffer);
        return 1;
    }
    
    // Configure SPI
    uint8_t mode = SPI_MODE;
    uint8_t bits = SPI_BITS_PER_WORD;
    uint32_t speed = SPI_SPEED;
    
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("Failed to configure SPI");
        close(fd);
        free(tx_buffer);
        free(rx_buffer);
        return 1;
    }
    
    printf("Starting SPI transfer...\n");
    
    // Start timing
    start_clock = clock();
    gettimeofday(&start_time, NULL);
    
    // Perform SPI transfer
    if (spi_transfer_chunked(fd, tx_buffer, rx_buffer) < 0) {
        close(fd);
        free(tx_buffer);
        free(rx_buffer);
        return 1;
    }
    
    // Get final statistics
    get_cpu_time_stats(start_clock, &start_time);
    
    // Verify data integrity
    printf("Verifying data integrity...\n");
    if (verify_data(tx_buffer, rx_buffer, BUFFER_SIZE) == 0) {
        printf("Data Integrity: PASS\n");
    } else {
        printf("Data Integrity: FAIL\n");
    }
    
    // Cleanup
    close(fd);
    free(tx_buffer);
    free(rx_buffer);
    
    printf("Test Complete.\n");
    return 0;
}