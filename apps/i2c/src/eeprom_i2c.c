#include "eeprom_i2c.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int eeprom_i2c_set_addr(int fd, uint8_t dev_addr)
{
    if (dev_addr < EEPROM_I2C_ADDR_FIRST || dev_addr > (EEPROM_I2C_ADDR_LAST + EEPROM_I2C_ID_ADDR_OFFSET)) {
        errno = EINVAL;
        return -1;
    }

    return ioctl(fd, I2C_SLAVE, dev_addr);
}

int eeprom_i2c_open(const char *dev_path)
{
    const char *path = dev_path ? dev_path : EEPROM_I2C_DEFAULT_DEV;

    return open(path, O_RDWR);
}

void eeprom_i2c_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

int eeprom_i2c_device_exists(int fd, uint8_t dev_addr)
{
    uint8_t offset = 0x00;
    uint8_t value = 0x00;

    if (eeprom_i2c_set_addr(fd, dev_addr) < 0) {
        return 0;
    }

    if (write(fd, &offset, 1) != 1) {
        return 0;
    }

    if (read(fd, &value, 1) != 1) {
        return 0;
    }

    return 1;
}

int eeprom_i2c_read(int fd, uint8_t dev_addr, uint8_t offset, uint8_t *buf, size_t len)
{
    if (!buf || len == 0) {
        errno = EINVAL;
        return -1;
    }

    if ((size_t)offset + len > EEPROM_I2C_SIZE) {
        errno = EINVAL;
        return -1;
    }

    if (!eeprom_i2c_device_exists(fd, dev_addr)) {
        errno = ENODEV;
        return -1;
    }

    if (eeprom_i2c_set_addr(fd, dev_addr) < 0) {
        return -1;
    }

    if (write(fd, &offset, 1) != 1) {
        return -1;
    }

    if (read(fd, buf, len) != (ssize_t)len) {
        return -1;
    }

    return 0;
}

int eeprom_i2c_write(int fd, uint8_t dev_addr, uint8_t offset, const uint8_t *buf, size_t len)
{
    size_t done = 0;

    if (!buf || len == 0) {
        errno = EINVAL;
        return -1;
    }

    if ((size_t)offset + len > EEPROM_I2C_SIZE) {
        errno = EINVAL;
        return -1;
    }

    if (!eeprom_i2c_device_exists(fd, dev_addr)) {
        errno = ENODEV;
        return -1;
    }

    while (done < len) {
        uint8_t packet[1 + EEPROM_I2C_PAGE_SIZE];
        uint8_t cur_offset = (uint8_t)(offset + done);
        size_t page_left = EEPROM_I2C_PAGE_SIZE - (cur_offset % EEPROM_I2C_PAGE_SIZE);
        size_t chunk = len - done;

        if (chunk > page_left) {
            chunk = page_left;
        }

        packet[0] = cur_offset;
        for (size_t i = 0; i < chunk; i++) {
            packet[1 + i] = buf[done + i];
        }

        if (eeprom_i2c_set_addr(fd, dev_addr) < 0) {
            return -1;
        }

        if (write(fd, packet, 1 + chunk) != (ssize_t)(1 + chunk)) {
            return -1;
        }

        usleep(EEPROM_I2C_WRITE_DELAY_US);
        done += chunk;
    }

    return 0;
}

int eeprom_i2c_read_id(int fd, uint8_t data_addr, uint8_t *id_buf, size_t id_len)
{
    uint8_t id_addr = data_addr + EEPROM_I2C_ID_ADDR_OFFSET;
    uint8_t offset = EEPROM_I2C_ID_WORD_ADDR;

    if (!id_buf || id_len == 0) {
        errno = EINVAL;
        return -1;
    }

    if (!eeprom_i2c_device_exists(fd, id_addr)) {
        errno = ENODEV;
        return -1;
    }

    if (eeprom_i2c_set_addr(fd, id_addr) < 0) {
        return -1;
    }

    if (write(fd, &offset, 1) != 1) {
        return -1;
    }

    if (read(fd, id_buf, id_len) != (ssize_t)id_len) {
        return -1;
    }

    return 0;
}

int eeprom_i2c_dump(int fd, uint8_t dev_addr, uint8_t *buf, size_t len)
{
    return eeprom_i2c_read(fd, dev_addr, 0x00, buf, len);
}

void eeprom_i2c_print_hex(const uint8_t *buf, size_t len, size_t base_offset)
{
    if (!buf) {
        return;
    }

    for (size_t i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            printf("%04zx:", base_offset + i);
        }

        printf(" %02x", buf[i]);

        if ((i % 16) == 15 || i + 1 == len) {
            printf("\n");
        }
    }
}
