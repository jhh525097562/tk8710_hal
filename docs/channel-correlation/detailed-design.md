# 终端信道相关性详细设计

## 算法

1. 校验回调列表和 `users` 指针。
2. 对每条用户数据计算 `terminal_id = userId >> 8`；若已存在则覆盖 AH，否则追加。
3. 按 `terminal_id` 升序排序。
4. 对每个 `i < j`，把 `ahData[2k]`、`ahData[2k+1]` 解释为第 k 根天线的 I、Q。
5. 累加 `h1 * conj(h2)` 的实部和虚部，以及两个向量的能量。
6. 能量非零时计算 `hypot(real, imag) / sqrt(energy1 * energy2)` 并钳制到 1；否则标记零范数。
7. 第一行和第一列输出终端 ID；对角线输出 `1.000000`，上三角输出相关系数或 `N/A`，下三角输出空单元格。各列使用固定 12 字符宽度，不依赖制表符宽度。

## 日志与错误

日志目录使用 `mkdir("/userdata/8710log", 0755)`，`EEXIST` 视为成功；文件以 `a` 模式打开。正常结果仅写文件，控制台只报告目录、打开、写入或关闭错误。文件错误不返回到回调调用者，也不阻止 `IpcSendUplinkData`。

## 兼容性

仅在 `test/example/tk8710_gw.c` 内增加私有类型和函数。不改变 `TRM_RxUserData`、`TRM_BeamInfo` 或 HAL API。
