# Web IPC 通信流程

## 1. 目标

本流程用于实现页面按钮触发业务进程启动采集任务，并在任务完成后由页面读取结果并展示。

本流程采用以下原则：

- 页面不直接访问 Unix Socket。
- 页面不直接访问状态文件和结果文件。
- `web_server.c` 负责 HTTP API 与 IPC Client。
- 业务进程负责 IPC Server、任务执行、状态文件与结果文件维护。

## 2. 总体流程

```text
页面按钮点击
  ↓
POST /api/data_collect_start
  ↓
web_server.c
  ↓
IPC Client 连接 /tmp/data_collect.sock
  ↓
业务进程 IPC Server
  ↓
收到 START 后立即返回 OK/BUSY/ERROR
  ↓
业务进程后台执行采集任务
  ↓
业务进程写状态文件 /tmp/data_collect_status.json
  ↓
业务进程写结果文件 /tmp/data_collect_result.json
  ↓
业务进程更新状态文件为 done
  ↓
页面轮询 GET /api/data_collect_status
  ↓
web_server.c 读取状态文件并返回 JSON
  ↓
页面发现状态为 done
  ↓
GET /api/data_collect_result
  ↓
web_server.c 读取结果文件并返回
  ↓
页面解析并显示数据
```

## 3. 角色职责

### 3.1 页面

- 用户点击按钮后调用启动接口。
- 启动成功后开始轮询状态接口。
- 当状态为 `done` 时停止轮询。
- 请求结果接口并显示结果。

### 3.2 web_server.c

- 提供启动接口、状态接口、结果接口。
- 通过 Unix Domain Socket 向业务进程发送 `START` 命令。
- 读取状态文件并转换成页面需要的 JSON。
- 读取结果文件并返回给页面。

### 3.3 业务进程

- 作为 IPC Server 监听 `/tmp/data_collect.sock`。
- 收到 `START` 后快速返回 `OK`、`BUSY` 或 `ERROR`。
- 在后台线程或后台任务中执行采集。
- 维护状态文件 `/tmp/data_collect_status.json`。
- 维护结果文件 `/tmp/data_collect_result.json`。

## 4. 接口流程说明

### 4.1 启动接口

页面调用：

```text
POST /api/data_collect_start
```

处理过程：

1. `web_server.c` 校验登录状态。
2. `web_server.c` 作为 IPC Client 连接 `/tmp/data_collect.sock`。
3. 发送命令：

```text
START\n
```

4. 业务进程快速返回：

```text
OK
BUSY
ERROR reason
```

5. `web_server.c` 转换为 JSON 响应给页面。

页面拿到 `success=true` 且 `state=running` 后开始轮询状态接口。

### 4.2 状态接口

页面轮询：

```text
GET /api/data_collect_status
```

处理过程：

1. 页面不是直接读文件，而是调用 Web API。
2. `web_server.c` 读取 `/tmp/data_collect_status.json`。
3. `web_server.c` 把文件内容转成 JSON 响应给页面。

推荐状态文件示例：

```json
{
  "state": "running",
  "message": "正在采集数据",
  "updated_at": 1710000000,
  "result_ready": false
}
```

状态值建议：

- `idle`
- `running`
- `done`
- `failed`

页面判断逻辑：

- `running`：继续轮询
- `done`：停止轮询并请求结果接口
- `failed`：停止轮询并显示错误

### 4.3 结果接口

页面调用：

```text
GET /api/data_collect_result
```

处理过程：

1. 页面检测到状态为 `done` 后调用该接口。
2. `web_server.c` 读取 `/tmp/data_collect_result.json`。
3. `web_server.c` 将结果返回给页面。
4. 页面解析并显示数据。

推荐结果文件示例：

```json
{
  "success": true,
  "items": [
    {
      "name": "example",
      "value": "123"
    }
  ]
}
```

## 5. 状态文件与结果文件

推荐路径：

```text
/tmp/data_collect_status.json
/tmp/data_collect_result.json
```

业务进程状态变更建议：

1. 收到 `START`，准备启动任务。
2. 先写状态文件为 `running`。
3. 后台执行采集任务。
4. 采集完成后先写结果文件。
5. 再写状态文件为 `done`。
6. 如果失败，写状态文件为 `failed`。

关键顺序要求：

- 必须先写结果文件，再写 `done` 状态。
- 这样页面看到 `done` 时，结果文件一定已经准备好。

## 6. 页面轮询的真实位置

页面轮询的不是 Unix Socket，也不是直接轮询本地文件。

页面轮询的是：

```text
GET /api/data_collect_status
```

`web_server.c` 内部读取的是：

```text
/tmp/data_collect_status.json
```

页面读取结果时调用的是：

```text
GET /api/data_collect_result
```

`web_server.c` 内部读取的是：

```text
/tmp/data_collect_result.json
```

## 7. 当前 demo 与本流程的关系

当前已实现 demo 的模式是：

- 已实现 Unix Socket IPC。
- 已实现客户端发送 `START`。
- 已实现服务端返回 `OK` / `BUSY` / `ERROR`。
- 已实现服务端后台模拟任务执行。

当前 demo 还没有实现的部分：

- 还没有写状态文件。
- 还没有写结果文件。
- 还没有实现页面轮询状态接口。
- 还没有实现页面读取结果接口。

因此，当前 demo 只实现了本流程中的“启动命令链路”，还没有完整实现“状态轮询 + 结果读取”链路。

## 8. 当前 demo 是否完全按本流程实现

答案是：

- 启动命令部分：是
- 状态文件与结果文件部分：否
- 页面轮询部分：否

更准确地说，当前 demo 是这个完整流程的第一阶段样例，主要用于验证：

- IPC Socket 是否连通
- `START` 命令是否可达
- `OK/BUSY/ERROR` 响应是否正确
- 服务端是否能启动后台任务

如果要完全符合本流程，下一步需要继续补充：

1. 服务端写 `/tmp/data_collect_status.json`
2. 服务端写 `/tmp/data_collect_result.json`
3. `web_server.c` 增加状态读取接口
4. `web_server.c` 增加结果读取接口
5. 页面增加轮询逻辑与结果展示逻辑

## 9. 一句话总结

本方案的核心是：

- Socket 只负责发起任务
- 状态文件负责表达任务进度
- 结果文件负责存放采集结果
- 页面通过 Web API 轮询状态
- 页面在状态为 `done` 后读取结果并显示
