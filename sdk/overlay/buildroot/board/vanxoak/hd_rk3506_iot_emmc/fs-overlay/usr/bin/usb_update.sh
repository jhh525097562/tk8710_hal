#!/bin/bash
#set -x
# --- 配置区 ---
# 公钥在开发板上的固定存放路径（建议存放在只读分区）
PUB_KEY="/etc/public.pem"
# U 盘挂载路径（根据实际挂载点修改，通常是 /mnt/udisk ）
USB_PATH="/mnt/udisk"
# 定义日志文件输出到 U 盘，方便查阅产测结果
LOG_FILE="/var/run/usb_update_result.log"
exec > >(tee -a "$LOG_FILE" /dev/console) 2>&1
# --- 逻辑区 ---
echo "--- USB 升级检测任务启动 1---"
MAX_WAIT=15  # 最大等待15秒，适配不同U盘/系统
WAIT_INTERVAL=1
WAIT_COUNT=0
TARGET_DEV="${1:-}"
MOUNT_TABLE="/proc/mounts"
DEBUG_MOUNT=1

debug_print_mounts() {
    [ "$DEBUG_MOUNT" != "1" ] && return 0
    echo "[调试] 当前挂载快照（$MOUNT_TABLE）:"
    while read -r d m t o r1 r2; do
        echo "[调试] dev=$d mnt=$m fs=$t"
    done < "$MOUNT_TABLE"
}

get_mount_point_by_dev() {
    local dev_name="$1"
    local mount_dev mount_point _rest parent_name

    [ -z "$dev_name" ] && return 1
    dev_name=$(echo "$dev_name" | tr -d '\r\n[:space:]')

    # 1) 优先直接匹配触发设备（/dev/sda、/dev/sda1、/dev/mmcblk0p1 等）
    while read -r mount_dev mount_point _rest; do
        if [ "$mount_dev" = "/dev/$dev_name" ]; then
            echo "$mount_point"
            return 0
        fi
    done < "$MOUNT_TABLE"

    # 2) 若触发的是分区，回退检查其父盘（例如 sda1 -> sda，mmcblk0p1 -> mmcblk0）
    parent_name="$dev_name"
    parent_name=$(echo "$parent_name" | sed -E 's/p?[0-9]+$//')
    if [ -n "$parent_name" ] && [ "$parent_name" != "$dev_name" ]; then
        while read -r mount_dev mount_point _rest; do
            if [ "$mount_dev" = "/dev/$parent_name" ]; then
                echo "$mount_point"
                return 0
            fi
        done < "$MOUNT_TABLE"
    fi

    # 3) 若触发的是整盘，补查其分区挂载（例如 sda -> sda1/sda2，mmcblk0 -> mmcblk0p1）
    while read -r mount_dev mount_point _rest; do
        case "$mount_dev" in
            "/dev/${dev_name}"[0-9]*|"/dev/${dev_name}p"[0-9]*|"/dev/${parent_name}"[0-9]*|"/dev/${parent_name}p"[0-9]*)
                echo "$mount_point"
                return 0
                ;;
        esac
    done < "$MOUNT_TABLE"

    return 1
}

# 兼容：sda / sda1 都能识别
if [ -n "$TARGET_DEV" ]; then
    PART_NAME=$(basename "$TARGET_DEV" | tr -d '\r\n[:space:]')
    echo "[等待] 精准等待设备 $PART_NAME 挂载（支持整盘/分区）"
else
    echo "[等待] 全局等待U盘挂载..."
fi

# 循环等待挂载
while [ $WAIT_COUNT -lt $MAX_WAIT ]; do
    if [ -n "$PART_NAME" ]; then
        MOUNT_POINT=$(get_mount_point_by_dev "$PART_NAME")
        if [ -n "$MOUNT_POINT" ]; then
            echo "[成功] 设备 /dev/$PART_NAME 已挂载到 $MOUNT_POINT"
            break
        fi
        # 兜底：精准匹配失败时，只要检测到 U 盘常见挂载目录，也继续进入后续全局扫描
        if awk '$2 ~ /^\/media|^\/mnt|^\/udisk|^\/run\/media/ {found=1; exit} END{exit found?0:1}' "$MOUNT_TABLE"; then
            echo "[提示] 精准匹配未命中，但已检测到U盘挂载目录，转入全局扫描"
            debug_print_mounts
            break
        fi
    else
        if awk '$2 ~ /^\/media|^\/mnt|^\/udisk|^\/run\/media/ {found=1; exit} END{exit found?0:1}' "$MOUNT_TABLE"; then
            echo "[成功] 检测到U盘挂载目录"
            break
        fi
    fi

    if [ "$DEBUG_MOUNT" = "1" ]; then
        echo "[调试] 本轮未匹配，TARGET_DEV=$TARGET_DEV PART_NAME=$PART_NAME"
        debug_print_mounts
    fi
    echo "[等待] 挂载未完成，等待${WAIT_COUNT}/${MAX_WAIT}秒..."
    sleep $WAIT_INTERVAL
    WAIT_COUNT=$((WAIT_COUNT + 1))
done

if [ $WAIT_COUNT -eq $MAX_WAIT ]; then
    echo "[超时] 等待U盘挂载超时" | tee -a "$LOG_FILE"
    debug_print_mounts
    exit 0
fi


# ==============================
# 自动扫描 已自动挂载 的 U 盘（不执行挂载，安全版）
# 适配：sda1 / sdb1 / sda2 / mmcblk 等任意盘符
# ==============================
find_usb_with_swu() {
    # 遍历 /proc/mounts 里所有已挂载的分区
    while read -r DEV MOUNT FSTYPE OPTS; do
        # 只处理 U 盘常用的自动挂载目录（过滤系统盘）
        if echo "$MOUNT" | grep -qE '^/media|^/mnt|^/udisk|^/run/media'; then
            # 查找该目录下是否有 .swu 升级包
            SWU_PATH=$(find "$MOUNT" -maxdepth 1 -type f -name "*.swu" 2>/dev/null | head -n 1)
            
            if [ -n "$SWU_PATH" ]; then
                # 找到了！返回路径
                USB_PATH="$MOUNT"
                SWU_FILE="$SWU_PATH"
                SIG_FILE="${SWU_FILE}.sig"
                echo "[自动扫描] 发现已挂载U盘：$USB_PATH"
                return 0
            fi
        fi
    done < "$MOUNT_TABLE"

    # 没找到
    return 1
}

# 执行扫描
if ! find_usb_with_swu; then
    echo "[提示] 未在任何已挂载U盘中找到 .swu 升级文件" | tee -a "$LOG_FILE"
    exit 0
fi

echo "[信息] 发现升级包: $(basename $SWU_FILE)" | tee -a $LOG_FILE

# 3. 签名检测 (核心安全环节)
if [ ! -f "$SIG_FILE" ]; then
	    echo "[拒绝] 错误：找不到签名文件 (.sig)，为了安全拒绝升级！" | tee -a $LOG_FILE
	        # 这里可以加入 LED 红灯闪烁逻辑
		    exit 1
fi

echo "[正在校验] 使用 $PUB_KEY 进行签名验证..."
# 使用 openssl 校验整个 .swu 文件
openssl dgst -sha256 -verify "$PUB_KEY" -signature "$SIG_FILE" "$SWU_FILE" > /dev/null 2>&1

if [ $? -eq 0 ]; then
	    echo "[成功] 签名验证通过！合法固件。" | tee -a $LOG_FILE
    else
	        echo "[报警] 签名验证失败！固件非法或已被篡改。" | tee -a $LOG_FILE
		    exit 1
fi

# 4. 执行升级
echo "[执行] 启动 SWUpdate 升级流程..." | tee -a $LOG_FILE
# -v 开启详细日志，-i 指定文件
swupdate -v -i "$SWU_FILE" 2>&1 | tee -a $LOG_FILE

UPGRADE_RESULT=$?  # 保存升级执行结果状态码

# 5. 升级后处理
if [ $UPGRADE_RESULT -eq 0 ]; then
    # 升级成功结果打印
    echo -e "\n==================== 升级结果 ====================" | tee -a $LOG_FILE
    echo "升级状态：成功" | tee -a $LOG_FILE
    echo "升级固件：$(basename $SWU_FILE)" | tee -a $LOG_FILE
    echo "升级时间：$(date '+%Y-%m-%d %H:%M:%S')" | tee -a $LOG_FILE
    echo "==================================================" | tee -a $LOG_FILE
    echo "[完成] 升级成功。系统即将重启或等待 U 盘拔出。" | tee -a $LOG_FILE
    # sync 确保日志写入 U 盘
    sync
    # 根据需求决定是否 reboot
    # reboot
    echo "升级完成"
else
    # 升级失败结果打印
    echo -e "\n==================== 升级结果 ====================" | tee -a $LOG_FILE
    echo "升级状态：失败" | tee -a $LOG_FILE
    echo "升级固件：$(basename $SWU_FILE)" | tee -a $LOG_FILE
    echo "失败原因：SWUpdate 执行过程中出错（状态码：$UPGRADE_RESULT）" | tee -a $LOG_FILE
    echo "升级时间：$(date '+%Y-%m-%d %H:%M:%S')" | tee -a $LOG_FILE
    echo "==================================================" | tee -a $LOG_FILE
    echo "[错误] SWUpdate 执行过程中出错，请检查日志。" | tee -a $LOG_FILE
fi

# 自动卸载U盘逻辑
echo "[执行] 准备卸载U盘：$USB_PATH" | tee -a $LOG_FILE
# 先同步所有未写入的数据，确保日志/结果完整写入U盘
sync
# 卸载U盘（-l 参数避免符号链接问题，-f 强制卸载，增加容错）
umount -lf "$USB_PATH" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "[成功] U盘已成功卸载：$USB_PATH" | tee -a $LOG_FILE
else
    echo "[警告] U盘卸载失败，可能已被占用（路径：$USB_PATH）" | tee -a $LOG_FILE
fi
