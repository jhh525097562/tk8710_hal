# ADR-002：以tk8710app作为SDK发布来源

## 状态

Accepted

## 日期

2026-09-20

## 背景

旧流程从独立HAL工程的build_rk3506目录手工复制单个tk8710_gw，容易漏掉配套测试程序和class3数据，也无法由统一SDK入口复现。

## 决策

Windows的rk3506app/tk8710app作为正式发布目录。build_apps.sh --stage自动将其全部内容预置到userdata根目录，并保留SDK内后备副本。脚本验证ARM架构、文件类型、权限和逐文件内容。

## 影响

一次SDK预置命令即可得到完整目录；独立HAL工程的编译输出必须先由发布人员整理进tk8710app。三App SWU仍保持窄范围，只携带tk8710_gw；完整发布目录由userdata/update.img承载。
