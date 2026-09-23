#ifndef EEPROM_I2C_H
#define EEPROM_I2C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EEPROM_I2C_DEFAULT_DEV "/dev/i2c-0"
#define EEPROM_I2C_ADDR_FIRST 0x50
#define EEPROM_I2C_ADDR_LAST 0x57
#define EEPROM_I2C_ID_ADDR_OFFSET 0x08
#define EEPROM_I2C_ID_WORD_ADDR 0x80
#define EEPROM_I2C_ID_LEN 16
#define EEPROM_I2C_SIZE 256
#define EEPROM_I2C_PAGE_SIZE 8
#define EEPROM_I2C_WRITE_DELAY_US 100000

int eeprom_i2c_open(const char *dev_path);
void eeprom_i2c_close(int fd);
int eeprom_i2c_device_exists(int fd, uint8_t dev_addr);
int eeprom_i2c_read(int fd, uint8_t dev_addr, uint8_t offset, uint8_t *buf, size_t len);
int eeprom_i2c_write(int fd, uint8_t dev_addr, uint8_t offset, const uint8_t *buf, size_t len);
int eeprom_i2c_read_id(int fd, uint8_t data_addr, uint8_t *id_buf, size_t id_len);
int eeprom_i2c_dump(int fd, uint8_t dev_addr, uint8_t *buf, size_t len);
void eeprom_i2c_print_hex(const uint8_t *buf, size_t len, size_t base_offset);

#ifdef __cplusplus
}
#endif

#endif
