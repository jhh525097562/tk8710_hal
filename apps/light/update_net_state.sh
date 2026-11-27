#!/bin/sh
# 用于网络守护进程更新网络状态的 Mock 脚本 / SDK 示例
# 用法: ./update_net_state.sh [L3_OK | L3_DOWN | RECOVERING]

LED_DIR="/run/led"
NET_STATE_FILE="${LED_DIR}/net.state"
NET_TMP_FILE="${LED_DIR}/net.tmp"

STATUS=${1:-"L3_DOWN"}
# buildroot 中的 busybox date 不支持 %3N，使用 awk 转换
TS_MS=$(awk 'BEGIN{srand();print srand()*1000}')

# 确保目录存在（防呆设计）
mkdir -p $LED_DIR

# 核心：写入临时文件后原子替换
echo "STATUS=${STATUS}" > "${NET_TMP_FILE}"
echo "TS_MS=${TS_MS}" >> "${NET_TMP_FILE}"

mv "${NET_TMP_FILE}" "${NET_STATE_FILE}"

echo "Net state updated to: ${STATUS} at TS_MS: ${TS_MS}"
