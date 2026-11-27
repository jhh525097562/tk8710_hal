# UDP轻量级日志通信系统部署文档

## 1. 目录说明

- `daemon/log_daemon.c`：守护进程源码
- `include/log_udp.h`：对外C接口头文件
- `src/log_udp.c`：接口实现
- `src/log_send_cli.c`：命令行可执行程序源码（脚本调用）
- `tests/test_log_udp.c`：单元测试
- `tests/stress_test.c`：压力测试
- `tests/test_log_send_cli.sh`：脚本调用集成测试

## 2. 编译

```bash
make all
```

或：

```bash
./build.sh
```

## 3. 启动守护进程

```bash
./bin/log_daemon 5514 ./logs 262144 500 10485760 0
```

参数顺序：

1. UDP端口（默认5514）
2. 日志输出目录（默认`./logs`）
3. 环形缓冲区大小（默认256KB）
4. 超时刷盘毫秒数（默认500ms）
5. 单文件最大字节（默认10MB）
6. 最多保留文件数（默认0表示不限制；>0 时自动删除最旧的 `log_*.dat`）

## 4. 业务进程接入

包含头文件并链接`src/log_udp.c`编译产物，调用接口：

```c
int rc = log_send(level, content, timestamp);
```

返回码：

- `0`：成功
- `-1`：网络异常
- `-2`：内容超长
- `-3`：参数非法

可选调用：

- `log_flush()`：主动刷发送缓存
- `log_close()`：进程退出前关闭

## 5. 运行配置

客户端支持环境变量：

- `LOG_UDP_ADDR`：守护进程IP，默认`127.0.0.1`
- `LOG_UDP_PORT`：守护进程端口，默认`5514`
- `LOG_SEND_THRESHOLD`：触发发送阈值（字节），默认`256`
- `LOG_PROC_NAME`：覆盖进程名字段（用于脚本/任务标记），默认读取`/proc/self/comm`

## 6. 脚本调用（log_send）

编译后会生成 `./bin/log_send`，用于脚本中直接调用并将消息发送到守护进程，由守护进程异步落盘（脚本侧无磁盘I/O阻塞）。

常用用法：

```bash
./bin/log_send -l INFO "script start"
./bin/log_send -l WARN -m "something happened"
LOG_PROC_NAME=my_script.sh ./bin/log_send -l INFO "step=1 ok"
echo "line1" | ./bin/log_send -l INFO
```

可选参数：

- `-l/--level`：DEBUG/INFO/WARN/ERROR 或 0-3，默认 INFO
- `-m/--msg`：日志内容（不填则从stdin逐行读取）
- `-a/--addr`、`-p/--port`：指定守护进程地址与端口
- `-t/--threshold`：触发发送阈值(字节)，默认 1 以保证“实时发送”
- `--proc`：覆盖进程名字段（等价设置 `LOG_PROC_NAME`）
- `--no-flush`：不主动flush（追求最小延迟，可能丢最后一条）

## 7. 文件落盘规则

- 文件命名：`log_YYYYMMDD_HHMMSS.dat`
- 达到单文件大小阈值后自动切分
- 若设置“最多保留文件数”，超出后删除最旧的日志文件
- 缓冲满或超时触发批量刷盘

## 8. 单元与压力测试

- 单元测试（默认地址/端口/阈值）：

```bash
./bin/test_log_udp
```

- 单元测试（指定地址/端口/阈值）：

```bash
./bin/test_log_udp 127.0.0.1 5515 64
```

或通过环境变量：

```bash
LOG_UDP_ADDR=127.0.0.1 LOG_UDP_PORT=5515 LOG_SEND_THRESHOLD=64 ./bin/test_log_udp
```

- 压力测试：

```bash
./bin/stress_test 20 60
```

或运行 24 小时脚本：

```bash
./tests/run_stress_24h.sh
```

脚本调用集成测试：

```bash
./tests/test_log_send_cli.sh
```

## 9. 接口变更说明

- log_send 的声明保持不变，但内部不再使用调用方传入的 timestamp。接口会在发送前自动生成格式为 YYYY-MM-DD HH:MM:SS.mmm 的时间戳，并自动采集当前进程名作为独立字段，将二者与日志等级、内容一起打包发送。
- 守护进程采用“透传模式”，不再为每条日志追加落盘时间戳；文件中的每一行即为客户端打好的“时间戳|进程名|等级|内容”。
- 调用方无需也不应再传入自定义时间戳；旧代码无需修改即可获得时间戳与进程名字段。
