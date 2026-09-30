# 网关应用与运行辅助模块

本目录保存正式网关入口及运行模块，原路径为 `test/example/`。

| 文件 | 用途 |
| --- | --- |
| `tk8710_gw.c` | 主网关应用入口，NS配置、GPS/PPS、RF状态及生命周期编排 |
| `tk8710_gw_slave.c` | 从站网关应用入口 |
| `tk8710_gw_sat.c` | 卫星载荷应用入口 |
| `tk8710_gw_ground.c` | 地面站应用入口 |
| `tk8710_gw_gps.c/.h` | GPS探测、配置、健康监控、位置文件及诊断 |
| `tk8710_gw_rf_status.h` | RF状态文件原子保存辅助函数 |

四个入口分别构建，不能将本目录全部 `.c` 一次性链接到同一可执行文件。
GPS模块保留编译宏 `TK8710_GPS_TEST_HOOKS` 控制的故障注入，正式构建不启用。
头文件与实现放在同一目录，使用者需加入 `-Isrc/gateway`。CMake及Shell构建已同步。
现有目标名和可执行文件名不变：

```bash
source ~/arm-buildroot-linux-gnueabihf_sdk-buildroot/environment-setup
bash cmake/build_rk3506.sh tk8710_gw
# 其他应用：tk8710_gw_slave、tk8710_gw_sat、tk8710_gw_ground
```

`test/example`继续保留单元测试、协议测试、设备测试、示例程序及频差专项测试。
`tk8710_gw_freq_runtime.c/.h`专供频差测试，不是主网关运行模块；
`trm_tx_validator.c/.h`属于发送验证工具，也保留在测试目录。为兼容既有验证工具引用，
Shell构建保留 `-Itest/example`。

GPS日志统一使用TRM日志接口：

- INFO：版本/测试注入信息、搜星状态、配置命令、对齐和运行轮询；
- WARN：查询失败、异常诊断及诊断详情；
- ERROR：状态文件写入失败、测试标记安全校验/创建失败。

运行时使用 `--trm-log-level info` 可查看正常GPS过程；WARN级仅输出告警和错误。
`GPSinfo.txt`的fprintf以及snprintf属于数据文件写入/字符串构造，仍保留，不作为日志替换。
历史验证文档中记载的旧源码路径表示当时版本，当前路径以本说明为准。
