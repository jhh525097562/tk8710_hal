#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define UART_DEVICE "/dev/ttyS4"
#define RX_BUFFER_SIZE 4096

static int uart_open(const char *device)
{
    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n",
                device, strerror(errno));
        return -1;
    }

    struct termios tio;
    memset(&tio, 0, sizeof(tio));

    if (tcgetattr(fd, &tio) != 0) {
        fprintf(stderr, "tcgetattr failed: %s\n",
                strerror(errno));
        close(fd);
        return -1;
    }

    /* 原始数据模式，不进行终端字符转换 */
    cfmakeraw(&tio);

    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);

    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;           /* 8数据位 */
    tio.c_cflag &= ~PARENB;       /* 无校验 */
    tio.c_cflag &= ~CSTOPB;       /* 1停止位 */
    tio.c_cflag |= CLOCAL | CREAD;

#ifdef CRTSCTS
    tio.c_cflag &= ~CRTSCTS;      /* 关闭硬件流控 */
#endif

    tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        fprintf(stderr, "tcsetattr failed: %s\n",
                strerror(errno));
        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);
    return fd;
}

static int uart_write_all(int fd, const void *data, size_t length)
{
    const uint8_t *position = data;
    size_t remaining = length;

    while (remaining > 0) {
        ssize_t written = write(fd, position, remaining);

        if (written > 0) {
            position += written;
            remaining -= (size_t)written;
            continue;
        }

        if (written < 0 &&
            (errno == EINTR ||
             errno == EAGAIN ||
             errno == EWOULDBLOCK)) {
            usleep(1000);
            continue;
        }

        fprintf(stderr, "UART write failed: %s\n",
                strerror(errno));
        return -1;
    }

    /* 等待数据真正从UART发送完成 */
    if (tcdrain(fd) != 0) {
        fprintf(stderr, "tcdrain failed: %s\n",
                strerror(errno));
        return -1;
    }

    return 0;
}

static ssize_t uart_read_response(
    int fd,
    char *buffer,
    size_t buffer_size,
    int timeout_ms)
{
    size_t received = 0;

    while (received + 1 < buffer_size) {
        struct pollfd pfd = {
            .fd = fd,
            .events = POLLIN
        };

        int result = poll(&pfd, 1, timeout_ms);

        if (result == 0) {
            /* 超时：有数据时视为本次响应结束 */
            break;
        }

        if (result < 0) {
            if (errno == EINTR)
                continue;

            fprintf(stderr, "poll failed: %s\n",
                    strerror(errno));
            return -1;
        }

        if (pfd.revents & POLLIN) {
            ssize_t size = read(
                fd,
                buffer + received,
                buffer_size - received - 1);

            if (size > 0) {
                received += (size_t)size;

                /*
                 * 收到第一批数据后将等待时间缩短，
                 * 连续200ms无数据认为响应结束。
                 */
                timeout_ms = 200;
                continue;
            }

            if (size < 0 &&
                errno != EAGAIN &&
                errno != EWOULDBLOCK &&
                errno != EINTR) {
                fprintf(stderr, "read failed: %s\n",
                        strerror(errno));
                return -1;
            }
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            break;
    }

    buffer[received] = '\0';
    return (ssize_t)received;
}

static int uart_send_command(
    int fd,
    const char *command,
    char *response,
    size_t response_size)
{
    char tx_buffer[256];

    /*
     * 设备命令区分大小写，并要求LF结束。
     * command参数中不要自己添加换行符。
     */
    int length = snprintf(
        tx_buffer,
        sizeof(tx_buffer),
        "%s\n",
        command);

    if (length <= 0 ||
        (size_t)length >= sizeof(tx_buffer)) {
        fprintf(stderr, "command too long\n");
        return -1;
    }

    tcflush(fd, TCIFLUSH);

    if (uart_write_all(
            fd,
            tx_buffer,
            (size_t)length) != 0) {
        return -1;
    }

    ssize_t received = uart_read_response(
        fd,
        response,
        response_size,
        2000);

    if (received < 0)
        return -1;

    if (received == 0) {
        fprintf(stderr, "UART response timeout\n");
        return -1;
    }

    return 0;
}

int main(void)
{
    int fd = uart_open(UART_DEVICE);
    if (fd < 0)
        return 1;

    char response[RX_BUFFER_SIZE];

    if (uart_send_command(
            fd,
            "HELP",
            response,
            sizeof(response)) != 0) {
        close(fd);
        return 1;
    }

    printf("UART response:\n%s", response);

    close(fd);
    return 0;
}