# ARM Third-Party Dependencies

This directory contains ARM dependencies used by the RK3506 application Makefiles when the toolchain sysroot does not provide them.

Default Makefile lookup path:

```sh
DEPS_ROOT ?= $(CURDIR)/../third_party/$(ARCH)
```

For `ARCH=arm`, the default path is `app/third_party/arm`.

## Contents

| Dependency | Version / ABI | Used by | Notes |
| --- | --- | --- | --- |
| json-c | `libjson-c.so.5.3.0` | `Discovery`, `mqtt`, `webserver` | Built with the RK3506 ARM toolchain from json-c 0.17. |
| mosquitto | `libmosquitto.so.1` | `mqtt` | Copied from the existing ARM milestone library set; headers from mosquitto 2.0.18. |
| mbedTLS crypto | `libmbedcrypto.so.7` | `mqtt` | Copied from the existing ARM milestone library set; headers from mbedTLS 2.28.7. |
| Mongoose | 7.0 | `webserver` | Built as `libmongoose.so` with the RK3506 ARM toolchain. Newer Mongoose 7.14 does not match the current web server API usage. |

## Runtime Notes

The target root filesystem must include the shared libraries needed by each executable, either in a standard loader path or in the application deployment directory configured on the device.

The SDK sysroot is not modified by this directory.
