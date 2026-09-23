#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <gpiod.h> // 引入 libgpiod 库
#include "file_logger.h"

#define LED_FILE_LOG "/var/log/led_agent.log"
#define printf(...) file_logger_printf(LED_FILE_LOG, "INFO", __VA_ARGS__)
#define perror(prefix) file_logger_perror(LED_FILE_LOG, prefix)

#define LED_DIR "/run/led"
#define MQTT_STATE_FILE "/run/led/mqtt.state"
#define NET_STATE_FILE "/run/led/net.state"

// 定义硬件LED控制节点 (使用 libgpiod)
// GPIO1_C7: 偏移量 = 2*8 + 7 = 23
// GPIO1_D0: 偏移量 = 3*8 + 0 = 24
#define GPIO_CHIP_NAME "gpiochip1"
#define RUN_LED_OFFSET 23
#define NET_LED_OFFSET 24

// 全局 GPIO 线句柄
struct gpiod_line *run_led_line = NULL;
struct gpiod_line *net_led_line = NULL;

// 灯状态枚举
typedef enum {
    LED_OFF = 0,
    LED_ON = 1
} LedState;

// 当前灯的物理状态缓存
LedState current_run_led = LED_OFF;
LedState current_net_led = LED_OFF;

// 初始化 GPIO
int init_gpio() {
    struct gpiod_chip *chip;

    chip = gpiod_chip_open_by_name(GPIO_CHIP_NAME);
    if (!chip) {
        perror("Open gpiochip failed");
        return -1;
    }

    // 获取 RUN_LED (GPIO1_C7)
    run_led_line = gpiod_chip_get_line(chip, RUN_LED_OFFSET);
    if (!run_led_line) {
        perror("Get RUN_LED line failed");
        gpiod_chip_close(chip);
        return -1;
    }

    // 获取 NET_LED (GPIO1_D0)
    net_led_line = gpiod_chip_get_line(chip, NET_LED_OFFSET);
    if (!net_led_line) {
        perror("Get NET_LED line failed");
        gpiod_chip_close(chip);
        return -1;
    }

    // 请求输出模式，默认初始状态为高电平 (高电平关闭，低电平打开)
    if (gpiod_line_request_output(run_led_line, "led_agent_run", 1) < 0) {
        perror("Request RUN_LED output failed");
        return -1;
    }
    current_run_led = LED_OFF; // 初始状态为灭 (高电平)

    if (gpiod_line_request_output(net_led_line, "led_agent_net", 1) < 0) {
        perror("Request NET_LED output failed");
        return -1;
    }
    current_net_led = LED_OFF; // 初始状态为灭 (高电平)

    return 0;
}

// 释放 GPIO
void release_gpio() {
    if (run_led_line) gpiod_line_release(run_led_line);
    if (net_led_line) gpiod_line_release(net_led_line);
}

// 设置LED状态 (libgpiod 底层驱动，低电平打开，高电平关闭)
void set_led(struct gpiod_line* line, LedState state, LedState* current_state) {
    if (!line) return;
    if (*current_state == state) return; // 状态未变，无需重复操作

    // LED_ON -> 0 (低电平), LED_OFF -> 1 (高电平)
    if (gpiod_line_set_value(line, state == LED_ON ? 0 : 1) == 0) {
        *current_state = state;
    } else {
        perror("Set LED value failed");
    }
}

// 获取当前毫秒级时间戳 (用于计算 <200ms)
unsigned long long get_current_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)(tv.tv_sec) * 1000 + (unsigned long long)(tv.tv_usec) / 1000;
}

// 读取状态文件 (将时间戳改为毫秒级别)
void read_state(const char* filepath, char* status_out, unsigned long long* ts_out) {
    status_out[0] = '\0';
    *ts_out = 0;

    FILE* fp = fopen(filepath, "r");
    if (!fp) return;

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "STATUS=", 7) == 0) {
            sscanf(line + 7, "%63s", status_out);
        } else if (strncmp(line, "TS_MS=", 6) == 0) {
            sscanf(line + 6, "%llu", ts_out);
        } else if (strncmp(line, "TS=", 3) == 0) {
            unsigned long long ts_val = 0;
            sscanf(line + 3, "%llu", &ts_val);
            // 自动兼容：如果是10位数的秒级时间戳，则转换为毫秒
            if (ts_val < 10000000000ULL) {
                *ts_out = ts_val * 1000;
            } else {
                *ts_out = ts_val;
            }
        }
    }
    fclose(fp);
}

int main() {
    printf("LED Agent Started.\n");

    // 初始化 libgpiod
    if (init_gpio() < 0) {
        printf("GPIO Init failed, mock mode fallback.\n");
        // 实际使用中这里可以决定是否退出
        // return -1;
    }

    // 确保 /run/led 目录存在 (通常由 SysV 脚本创建，这里加一层保险)
    mkdir(LED_DIR, 0755);

    char mqtt_status[64] = {0};
    unsigned long long mqtt_ts = 0;

    char net_status[64] = {0};
    unsigned long long net_ts = 0;

    // 主循环 tick (每 100ms 一次)
    unsigned long long tick = 0;
    
    // 用于快闪的状态标志
    LedState run_blink_state = LED_OFF;
    LedState net_blink_state = LED_OFF;

    while (1) {
        unsigned long long now = get_current_ms();

        // 1. 读取业务状态
        read_state(MQTT_STATE_FILE, mqtt_status, &mqtt_ts);
        read_state(NET_STATE_FILE, net_status, &net_ts);

        // 2. 更新闪烁状态
        // 100ms tick，5Hz闪烁 = 100ms亮/100ms灭 -> 每次 tick 翻转
        run_blink_state = (run_blink_state == LED_ON) ? LED_OFF : LED_ON;
        
        // 2Hz闪烁 = 250ms亮/250ms灭 -> 大约每 2~3 个 tick 翻转 (这里简化为200ms翻转)
        if (tick % 2 == 0) {
            net_blink_state = (net_blink_state == LED_ON) ? LED_OFF : LED_ON;
        }

        // 3. 仲裁 RUN 灯 (健康与业务数据)
        // 需求：当上下行数据时间少于200ms，不变更运行灯状态
        // 增加看门狗逻辑：如果 MQTT 进程超过 3 秒没有更新时间戳，视为异常，熄灭运行灯
        unsigned long long mqtt_diff = now > mqtt_ts ? (now - mqtt_ts) : 0;
        
        if (mqtt_status[0] == '\0') {
            // 如果状态字段为空 (可能文件被清空或者尚未写入)，熄灭 RUN 灯
            set_led(run_led_line, LED_OFF, &current_run_led);
        } else if (mqtt_diff > 3000) {
            // 超过 3 秒没更新，认为业务进程挂了，熄灭 RUN 灯
            set_led(run_led_line, LED_OFF, &current_run_led);
        } else if (strcmp(mqtt_status, "txrx") == 0) {
            if (mqtt_diff < 200) {
                // 如果上下行数据时间间隔 < 200ms，不变更当前 RUN 灯的状态
                // 直接跳过 set_led 操作
            } else if (mqtt_diff <= 1000) {
                // > 200ms 且 <= 1秒，执行快闪
                set_led(run_led_line, run_blink_state, &current_run_led);
            } else {
                // > 1秒 且 <= 3秒，恢复常亮 (空闲存活状态)
                set_led(run_led_line, LED_ON, &current_run_led);
            }
        } else {
            // 系统运行正常且在3秒心跳内 (比如状态为 connected)，默认常亮
            set_led(run_led_line, LED_ON, &current_run_led);
        }

        // 4. 仲裁 NET 灯 (网络L3状态)
        // L3正常 -> 常亮； 恢复中 -> 2Hz闪烁； 其它/异常 -> 灭
        // 若状态超时(超过15秒没更新)，视为网络守护进程异常，灯灭
        if ((now > net_ts) && (now - net_ts) > 15000) {
            set_led(net_led_line, LED_OFF, &current_net_led);
        } else if (strcmp(net_status, "L3_OK") == 0) {
            set_led(net_led_line, LED_ON, &current_net_led);
        } else if (strcmp(net_status, "RECOVERING") == 0) {
            set_led(net_led_line, net_blink_state, &current_net_led);
        } else { // L3_DOWN 或其他
            set_led(net_led_line, LED_OFF, &current_net_led);
        }

        tick++;
        usleep(100 * 1000); // 休眠 100ms
    }

    release_gpio();
    return 0;
}
