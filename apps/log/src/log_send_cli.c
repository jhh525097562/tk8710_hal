#include "log_udp.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int level_from_string(const char *s, int *out_level) {
    if (s == NULL || *s == '\0' || out_level == NULL) {
        return -1;
    }

    if (isdigit((unsigned char)s[0])) {
        char *end = NULL;
        long v = strtol(s, &end, 10);
        if (end == s || *end != '\0' || v < 0 || v > 3) {
            return -1;
        }
        *out_level = (int)v;
        return 0;
    }

    char tmp[16];
    size_t n = strlen(s);
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
    for (size_t i = 0; i < n; i++) {
        tmp[i] = (char)toupper((unsigned char)s[i]);
    }
    tmp[n] = '\0';

    if (strcmp(tmp, "DEBUG") == 0) {
        *out_level = 0;
        return 0;
    }
    if (strcmp(tmp, "INFO") == 0) {
        *out_level = 1;
        return 0;
    }
    if (strcmp(tmp, "WARN") == 0 || strcmp(tmp, "WARNING") == 0) {
        *out_level = 2;
        return 0;
    }
    if (strcmp(tmp, "ERROR") == 0) {
        *out_level = 3;
        return 0;
    }

    return -1;
}

static void print_usage(FILE *fp) {
    fprintf(fp,
            "用法: log_send [选项] [消息...]\n"
            "  -l, --level LEVEL      日志级别: DEBUG/INFO/WARN/ERROR 或 0-3 (默认INFO)\n"
            "  -m, --msg MSG          日志内容\n"
            "  -a, --addr IP          守护进程IP (默认127.0.0.1)\n"
            "  -p, --port PORT        守护进程端口 (默认5514)\n"
            "  -t, --threshold BYTES  触发发送阈值(字节)，默认1保证“实时发送”\n"
            "  --proc NAME            覆盖进程名字段(可用于标记脚本名)\n"
            "  --no-flush             不主动flush，追求最小延迟(可能丢最后一条)\n"
            "  -h, --help             显示帮助\n"
            "\n"
            "示例:\n"
            "  log_send -l INFO \"script start\"\n"
            "  log_send --proc my_script.sh -l WARN -m \"something happened\"\n"
            "  echo \"hello\" | log_send -l INFO\n");
}

static int set_env_kv(const char *k, const char *v) {
    if (k == NULL || v == NULL) return -1;
    if (setenv(k, v, 1) != 0) return -1;
    return 0;
}

static char *join_args(int argc, char **argv, int start_idx) {
    size_t total = 0;
    for (int i = start_idx; i < argc; i++) {
        total += strlen(argv[i]) + 1;
    }
    if (total == 0) return NULL;

    char *out = (char *)malloc(total);
    if (!out) return NULL;

    size_t off = 0;
    for (int i = start_idx; i < argc; i++) {
        size_t n = strlen(argv[i]);
        memcpy(out + off, argv[i], n);
        off += n;
        if (i != argc - 1) {
            out[off++] = ' ';
        }
    }
    out[off] = '\0';
    return out;
}

static void trim_eol(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

int main(int argc, char **argv) {
    int level = 1;
    const char *msg = NULL;
    const char *addr = NULL;
    const char *port = NULL;
    const char *threshold = "1";
    const char *proc = NULL;
    int no_flush = 0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_usage(stdout);
            return 0;
        }
        if ((strcmp(a, "-l") == 0 || strcmp(a, "--level") == 0) && i + 1 < argc) {
            if (level_from_string(argv[i + 1], &level) != 0) {
                fprintf(stderr, "invalid level: %s\n", argv[i + 1]);
                return 2;
            }
            i++;
            continue;
        }
        if ((strcmp(a, "-m") == 0 || strcmp(a, "--msg") == 0) && i + 1 < argc) {
            msg = argv[i + 1];
            i++;
            continue;
        }
        if ((strcmp(a, "-a") == 0 || strcmp(a, "--addr") == 0) && i + 1 < argc) {
            addr = argv[i + 1];
            i++;
            continue;
        }
        if ((strcmp(a, "-p") == 0 || strcmp(a, "--port") == 0) && i + 1 < argc) {
            port = argv[i + 1];
            i++;
            continue;
        }
        if ((strcmp(a, "-t") == 0 || strcmp(a, "--threshold") == 0) && i + 1 < argc) {
            threshold = argv[i + 1];
            i++;
            continue;
        }
        if (strcmp(a, "--proc") == 0 && i + 1 < argc) {
            proc = argv[i + 1];
            i++;
            continue;
        }
        if (strcmp(a, "--no-flush") == 0) {
            no_flush = 1;
            continue;
        }
        if (strcmp(a, "--") == 0) {
            if (i + 1 < argc) {
                char *joined = join_args(argc, argv, i + 1);
                msg = joined;
            }
            break;
        }
        if (a[0] == '-') {
            fprintf(stderr, "unknown option: %s\n", a);
            print_usage(stderr);
            return 2;
        }

        char *joined = join_args(argc, argv, i);
        msg = joined;
        break;
    }

    if (addr && set_env_kv("LOG_UDP_ADDR", addr) != 0) {
        fprintf(stderr, "set LOG_UDP_ADDR failed: %s\n", strerror(errno));
        return 3;
    }
    if (port && set_env_kv("LOG_UDP_PORT", port) != 0) {
        fprintf(stderr, "set LOG_UDP_PORT failed: %s\n", strerror(errno));
        return 3;
    }
    if (threshold && set_env_kv("LOG_SEND_THRESHOLD", threshold) != 0) {
        fprintf(stderr, "set LOG_SEND_THRESHOLD failed: %s\n", strerror(errno));
        return 3;
    }
    if (proc && set_env_kv("LOG_PROC_NAME", proc) != 0) {
        fprintf(stderr, "set LOG_PROC_NAME failed: %s\n", strerror(errno));
        return 3;
    }

    int rc = 0;
    int sent_any = 0;

    if (msg != NULL) {
        rc = log_send(level, msg, (time_t)0);
        sent_any = 1;
        if (rc != LOG_OK) {
            fprintf(stderr, "log_send failed rc=%d\n", rc);
            log_close();
            return 4;
        }
    } else if (!isatty(fileno(stdin))) {
        char *line = NULL;
        size_t cap = 0;
        ssize_t n;
        while ((n = getline(&line, &cap, stdin)) >= 0) {
            (void)n;
            trim_eol(line);
            rc = log_send(level, line, (time_t)0);
            sent_any = 1;
            if (rc != LOG_OK) {
                fprintf(stderr, "log_send failed rc=%d\n", rc);
                free(line);
                log_close();
                return 4;
            }
        }
        free(line);
    } else {
        fprintf(stderr, "missing message\n");
        print_usage(stderr);
        return 2;
    }

    if (!no_flush && sent_any) {
        rc = log_flush();
        if (rc != LOG_OK) {
            fprintf(stderr, "log_flush failed rc=%d\n", rc);
            log_close();
            return 5;
        }
    }

    log_close();
    return 0;
}

