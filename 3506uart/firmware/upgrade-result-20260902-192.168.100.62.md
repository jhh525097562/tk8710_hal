# AG32 APP 升级记录

- 目标 RK3506：`192.168.100.62`
- 串口：`/dev/ttyS4`，115200 8N1
- 镜像：`ag32_gps_pps_app_20260902.bin`
- 文件大小：49,680 字节
- SHA-256：`848ae2b2f9eebce1dcf8ab4733c7a5d7bb7ae3d87243a290ec6cab8dc80a5720`
- 镜像版本参数：`0x20260902`
- 升级时间：2026-09-02 17:24（Asia/Shanghai）

## BOOT 能力检查

- BOOT：V1.0，build 2026-08-26 19:31
- 协议版本：1
- `support_image_mask=0x00000003`，支持 APP 和 CPLD
- APP 起始地址：`0x80008000`
- APP 分区大小：`0x0001E000`（122,880 字节）
- 最大分块：256 字节

## 烧录结果

- `START OK image_size=49680 chunk_size=256`
- 195/195 个分块全部写入
- `END OK image_crc32=0x1F248CB9 image_size=49680`
- `UPGRADE OK (reboot requested)`

## 重启验证

```text
OK APP_VER=V1.0 APP_BUILD=2026-09-02 16:21 BOOT_VER=V1.0 BOOT_BUILD=2026-08-26 19:31 CPLD_VER=V1.0 CPLD_BUILD=2026-08-28 13:27
OK STATUS STATE=IDLE PERIOD=1 OUT=0 WIDTH=100 PENDING=0 GPS=1 PPS=1 FIX=1 RMC=A ALIGN=0 HOLD=0 BAD=0 RESYNC=1 SATS=28 SNR=35 DEBUG=0 UTC=utc=2026-09-02 09:26:28.000
```

结论：APP Flash 写入、设备端 CRC32 校验、重启和新版本查询均通过。重启后的 `IDLE/PERIOD=1/OUT=0` 是尚未重新下发业务配置的状态。
