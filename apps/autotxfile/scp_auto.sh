#!/bin/bash
#==========================================================
# 内网 SCP 自动传输脚本 (sshpass 方案)
# 适用场景：内网设备间文件传输，自动密码交互
# 
# 用法: ./scp_auto.sh <源文件路径> [配置文件]
# 示例: ./scp_auto.sh /local/file.txt ./scp_config.ini
#       ./scp_auto.sh /home/user/data.log
#==========================================================

set -euo pipefail

# 参数处理
if [[ $# -lt 1 ]]; then
    echo "用法: $0 <源文件路径> [配置文件]"
    echo "示例: $0 /local/file.txt ./scp_config.ini"
    echo "默认配置文件: ./scp_config.ini"
    exit 1
fi

SOURCE_FILE="$1"
CONFIG_FILE="${2:-./scp_config.ini}"

# 检查配置文件
if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "错误: 配置文件不存在: $CONFIG_FILE"
    echo "请创建格式如下:"
    echo 'REMOTE_IP="192.168.1.100"'
    echo 'REMOTE_USER="root"'
    echo 'REMOTE_PASS="password"'
    echo 'REMOTE_DIR="/remote/path"'
    echo 'SSH_PORT="22"'
    echo 'TIMEOUT="10"'
    exit 1
fi

# 读取配置
source "$CONFIG_FILE"

# 参数校验
: "${REMOTE_IP:?未设置 REMOTE_IP}"
: "${REMOTE_USER:?未设置 REMOTE_USER}"
: "${REMOTE_PASS:?未设置 REMOTE_PASS}"
: "${REMOTE_DIR:?未设置 REMOTE_DIR}"
: "${SSH_PORT:=22}"
: "${TIMEOUT:=10}"

# 检查源文件
if [[ ! -f "$SOURCE_FILE" ]]; then
    echo "错误: 源文件不存在: $SOURCE_FILE"
    exit 1
fi

echo "	 正在传输: $(basename "$SOURCE_FILE")"
echo "   源路径: $SOURCE_FILE"
echo "   目标: $REMOTE_USER@$REMOTE_IP:$SSH_PORT -> $REMOTE_DIR"
# 执行传输（自动输入密码）
if timeout "${TIMEOUT}s" sshpass -p "$REMOTE_PASS" \
    scp -o StrictHostKeyChecking=no \
        -o ConnectTimeout="$TIMEOUT" \
        -P "$SSH_PORT" \
        "$SOURCE_FILE" "${REMOTE_USER}@${REMOTE_IP}:${REMOTE_DIR}"; then
    
    echo "传输成功！"
    exit 0
else
    echo "传输失败！请检查:"
    echo "   - 网络连通性 (ping $REMOTE_IP)"
    echo "   - 密码是否正确"
    echo "   - 目标目录是否存在/有写入权限"
    exit 1
fi
