# UTC 与周期修改后的对齐说明

本文供上位机软件开发者使用，说明修改 PPS_OUT 周期后如何判断对齐时间。

## 1. UTC 来源

CPU 使用 GPS 提供的有效 UTC 进行周期对齐，不使用 PC 本地时间。

## 2. 修改周期

```text
SET PERIOD <周期秒数>
```

示例：

```text
SET PERIOD 3
```

## 3. 对齐规则

CPU 自动选择的目标对齐 UTC 必须满足：

```text
UTC % 周期 == 0
```

例如周期为 3 秒时，只有能被 3 整除的 UTC 秒点才是有效对齐点。

CPU 会自动选择一个满足上述规则的未来 UTC 秒点。

## 4. 如何确认完成对齐

周期修改后，等待 CPU 主动打印：

```text
EVENT CPLD_ALIGNED UTC=<utc> PERIOD=<周期> ...
```

收到该事件表示 CPLD 已在对应 UTC 的 PPS 点完成对齐，新周期已经生效。

其中 `UTC` 是实际完成对齐的 UTC 时间，`PERIOD` 是已经生效的输出周期。

> `GET OUT` 中的 `NEXT` 表示下一次 PPS_OUT 输出时间，不表示重新对齐目标时间。
