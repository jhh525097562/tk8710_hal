#include "eeprom_i2c.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static void print_usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s [i2c-dev]\n", prog);
    printf("  %s write <addr> <offset> <hex...> [i2c-dev]\n", prog);
    printf("\n");
    printf("Examples:\n");
    printf("  %s /dev/i2c-0\n", prog);
    printf("  %s write 0x50 0xf0 0xa5 0x5a /dev/i2c-0\n", prog);
}

static int parse_u8(const char *text, uint8_t *value)
{
    unsigned int tmp = 0;
    char tail = '\0';

    if (!text || !value) {
        return -1;
    }

    if (sscanf(text, "%i%c", &tmp, &tail) != 1 || tmp > 0xff) {
        return -1;
    }

    *value = (uint8_t)tmp;
    return 0;
}

static int scan_and_dump(const char *dev_path)
{
    int fd = eeprom_i2c_open(dev_path);
    int found = 0;

    if (fd < 0) {
        printf("open %s failed: %s\n", dev_path, strerror(errno));
        return 1;
    }

    printf("I2C EEPROM scan on %s\n", dev_path);
    printf("EEPROM data address: 0x%02x-0x%02x\n", EEPROM_I2C_ADDR_FIRST, EEPROM_I2C_ADDR_LAST);
    printf("Linux I2C uses 7-bit address, do not shift address or append read/write bit in user code.\n");

    for (uint8_t addr = EEPROM_I2C_ADDR_FIRST; addr <= EEPROM_I2C_ADDR_LAST; addr++) {
        uint8_t id[EEPROM_I2C_ID_LEN];
        uint8_t data[EEPROM_I2C_SIZE];

        printf("\nDevice 0x%02x:\n", addr);

        if (!eeprom_i2c_device_exists(fd, addr)) {
            printf("  device not found\n");
            continue;
        }

        found++;
        printf("  device found\n");

        if (eeprom_i2c_read_id(fd, addr, id, sizeof(id)) == 0) {
            printf("  unique id address 0x%02x, word address 0x%02x:\n", addr + EEPROM_I2C_ID_ADDR_OFFSET, EEPROM_I2C_ID_WORD_ADDR);
            eeprom_i2c_print_hex(id, sizeof(id), 0);
        } else {
            printf("  read unique id failed: %s\n", strerror(errno));
        }

        if (eeprom_i2c_dump(fd, addr, data, sizeof(data)) == 0) {
            printf("  eeprom data 0x00-0xff:\n");
            eeprom_i2c_print_hex(data, sizeof(data), 0);
        } else {
            printf("  read eeprom data failed: %s\n", strerror(errno));
        }
    }

    eeprom_i2c_close(fd);

    printf("\nScan done, found %d device(s).\n", found);
    return found > 0 ? 0 : 1;
}

static int write_data(int argc, char *argv[])
{
    const char *dev_path = EEPROM_I2C_DEFAULT_DEV;
    uint8_t addr = 0;
    uint8_t offset = 0;
    uint8_t data[EEPROM_I2C_SIZE];
    int data_argc = argc - 4;
    int fd = -1;

    if (argc < 5) {
        print_usage(argv[0]);
        return 1;
    }

    if (parse_u8(argv[2], &addr) < 0 || parse_u8(argv[3], &offset) < 0) {
        printf("invalid addr or offset\n");
        return 1;
    }

    if (addr < EEPROM_I2C_ADDR_FIRST || addr > EEPROM_I2C_ADDR_LAST) {
        printf("addr 0x%02x out of range, valid range is 0x%02x-0x%02x\n", addr, EEPROM_I2C_ADDR_FIRST, EEPROM_I2C_ADDR_LAST);
        return 1;
    }

    if (data_argc > EEPROM_I2C_SIZE) {
        printf("too many data bytes\n");
        return 1;
    }

    if (argc > 5) {
        uint8_t maybe_dev = 0;
        if (parse_u8(argv[argc - 1], &maybe_dev) < 0) {
            dev_path = argv[argc - 1];
            data_argc--;
        }
    }

    if (data_argc <= 0) {
        printf("no data to write\n");
        return 1;
    }

    if ((size_t)offset + (size_t)data_argc > EEPROM_I2C_SIZE) {
        printf("write range out of eeprom size\n");
        return 1;
    }

    for (int i = 0; i < data_argc; i++) {
        if (parse_u8(argv[4 + i], &data[i]) < 0) {
            printf("invalid data byte: %s\n", argv[4 + i]);
            return 1;
        }
    }

    fd = eeprom_i2c_open(dev_path);
    if (fd < 0) {
        printf("open %s failed: %s\n", dev_path, strerror(errno));
        return 1;
    }

    if (!eeprom_i2c_device_exists(fd, addr)) {
        printf("device 0x%02x not found\n", addr);
        eeprom_i2c_close(fd);
        return 1;
    }

    if (eeprom_i2c_write(fd, addr, offset, data, (size_t)data_argc) < 0) {
        printf("write device 0x%02x offset 0x%02x failed: %s\n", addr, offset, strerror(errno));
        eeprom_i2c_close(fd);
        return 1;
    }

    printf("write device 0x%02x offset 0x%02x len %d success\n", addr, offset, data_argc);
    eeprom_i2c_close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc >= 2 && strcmp(argv[1], "write") == 0) {
        return write_data(argc, argv);
    }

    if (argc > 2) {
        print_usage(argv[0]);
        return 1;
    }

    return scan_and_dump(argc == 2 ? argv[1] : EEPROM_I2C_DEFAULT_DEV);
}
