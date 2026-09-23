# Wi-Fi模式切换测试报告模板

## 测试信息
- 设备型号：
- 固件版本：
- 测试人员：
- 测试日期：
- 测试环境：

## Station模式验证
| 项目 | 期望 | 结果 | 时间 | 日志路径 | 备注/截图 |
| --- | --- | --- | --- | --- | --- |
| 连接指定SSID | 成功 |  |  | /var/log/wifi_sta.log |  |
| DHCP获取IP | 成功 |  |  | /var/log/wifi_sta.log |  |
| ping网关 | 4/4 |  |  | /var/log/wifi_sta.log |  |

## AP模式验证
| 项目 | 期望 | 结果 | 时间 | 日志路径 | 备注/截图 |
| --- | --- | --- | --- | --- | --- |
| 手机扫描并连接 | 成功 |  |  | /var/log/wifi_ap.log |  |
| DHCP获取地址 | 192.168.4.10–192.168.4.50 |  |  | /var/log/wifi_ap.log |  |
| ping 192.168.4.1 | 成功 |  |  | /var/log/wifi_ap.log |  |

## 自动切换验证
| 项目 | 期望 | 结果 | 时间 | 日志路径 | 备注/截图 |
| --- | --- | --- | --- | --- | --- |
| 10min±30s内AP→STA | 成功 |  |  | /var/log/wifi_auto.log |  |
| 切换耗时<15s | 成功 |  |  | /var/log/wifi_auto.log |  |
| 连续3次100%通过 | 成功 |  |  | /var/log/wifi_auto.log |  |

## 签字
- 测试人员签字：
- 审核签字：
