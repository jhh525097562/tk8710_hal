<!--
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2026-03-13 09:33:17
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2026-03-13 11:52:35
 * @FilePath: \wlan0\README.md
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
-->
# Wi-Fi模式切换方案交付包

## 依赖检查
- 必要命令：hostapd、udhcpd、udhcpc、wpa_supplicant、wpa_cli、ifconfig、flock
- 选用命令：hostapd_cli或iw（用于自动切换时判断客户端关联）

## 一键安装
1. 复制脚本到/usr/local/sbin  
   - switch_to_sta.sh  
   - switch_to_ap.sh  
   - wifi_auto_toggle.sh  
2. 赋予执行权限  
   - chmod +x /usr/local/sbin/switch_to_sta.sh /usr/local/sbin/switch_to_ap.sh /usr/local/sbin/wifi_auto_toggle.sh  
3. 复制配置模板到/etc  
   - wpa_supplicant.conf  
   - hostapd.conf  
   - udhcpd.conf  

## 手动启动
- 切换到Station：/usr/local/sbin/switch_to_sta.sh
- 切换到AP：/usr/local/sbin/switch_to_ap.sh
- 自动切换守护：/usr/local/sbin/wifi_auto_toggle.sh

## 验证命令
### Station模式
- ping网关：ping -c 4 192.168.1.1
- 观察获取的IP：ifconfig wlan0

### AP模式
- 手机扫描并连接SSID：AP_SSID
- DHCP地址获取：手机端查看获取到的192.168.4.10–192.168.4.50
- AP成功判据：手机能连接且成功获取DHCP地址并可ping通192.168.4.1

### 自动切换
- 启动：/usr/local/sbin/wifi_auto_toggle.sh
- 日志：/var/log/wifi_auto.log

## 端口开放说明
- iptables示例：iptables -I INPUT -p tcp --dport 8822 -j ACCEPT
- nft示例：nft add rule inet filter input tcp dport 8822 accept

## 故障排查表
| 现象 | 可能原因 | 处理建议 | 日志位置 |
| --- | --- | --- | --- |
| STA无法获取IP | DHCP失败或信号弱 | 检查SSID/PSK与信号强度 | /var/log/wifi_sta.log |
| AP不可见 | hostapd启动失败 | 检查hostapd.conf国家码/信道 | /var/log/wifi_ap.log |
| 自动切换未触发 | 客户端未释放或无法检测 | 检查hostapd_cli/iw可用性 | /var/log/wifi_auto.log |
