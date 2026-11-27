#!/bin/sh
###
 # @Author: 359327604 359327604@qq.com
 # @Date: 2026-04-09 10:29:59
 # @LastEditors: 359327604 359327604@qq.com
 # @LastEditTime: 2026-04-09 11:22:54
 # @FilePath: \light\update_mqtt_state.sh
 # @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
### 

# 用于 MQTT 进程更新数据状态的 Mock 脚本 / SDK 示例
# 用法: ./update_mqtt_state.sh [connected | txrx | down]

LED_DIR="/run/led"
MQTT_STATE_FILE="${LED_DIR}/mqtt.state"
MQTT_TMP_FILE="${LED_DIR}/mqtt.tmp"

STATUS=${1:-"down"}
# buildroot 中的 busybox date 不支持 %3N，使用 awk 转换
TS_MS=$(awk 'BEGIN{srand();print srand()*1000}')

mkdir -p $LED_DIR

# 核心：写入临时文件后原子替换
echo "STATUS=${STATUS}" > "${MQTT_TMP_FILE}"
echo "TS_MS=${TS_MS}" >> "${MQTT_TMP_FILE}"

mv "${MQTT_TMP_FILE}" "${MQTT_STATE_FILE}"

echo "MQTT state updated to: ${STATUS} at TS_MS: ${TS_MS}"
