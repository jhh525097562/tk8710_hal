# TK8710应用目录SDK预置说明

更新日期：2026-09-20。

## 来源与目标

TK8710应用发布目录的主来源固定为：

```text
D:\CodexWorkSpace\RK3506资料\rk3506\rk3506app\tk8710app
```

WSL路径为：

```text
/mnt/d/CodexWorkSpace/RK3506资料/rk3506/rk3506app/tk8710app
```

运行SDK根目录下的：

```sh
bash external/rk3506app/build_apps.sh --stage
```

会调用`external/rk3506app/stage_tk8710app.sh`，把来源目录的**内容**复制到：

```text
device/rockchip/common/extra-parts/userdata/normal/
```

生成userdata镜像并烧录后，对应板端路径为：

```text
/userdata/tk8710_gw
/userdata/Test8710Loopback
/userdata/Test8710RFTxTone
/userdata/Test8710RxSensitivity
/userdata/Test8710W_R_Reg
/userdata/readme.txt
/userdata/class3/...
```

是复制`tk8710app`目录内部内容，不会在板上额外创建`/userdata/tk8710app/`。

## 校验和权限

脚本要求`tk8710_gw`、4个`Test8710*`程序均为ARM 32-bit ELF，要求`class3`目录存在，拒绝符号链接和特殊文件。临时副本中目录权限统一为755，普通数据文件为644，5个ARM程序为755。复制后对全部来源文件逐个执行内容比较，任何缺失/差异都会使`build_apps.sh --stage`失败。

覆盖前，userdata预置目录中已经存在的同名顶层文件/目录会备份到：

```text
output/tk8710-userdata-backup-XXXXXX/
```

脚本只覆盖来源中存在的内容，不删除userdata里的MQTT、Web、FRP、GPS、RF状态、校准和其他现场预置文件。`class3`中来源已删除的历史文件目前不会自动清理；这样避免递归删除边界过宽，但发布目录维护者需要避免复用旧userdata预置目录留下同名废弃数据。

SDK的`external/rk3506app/tk8710app`保留一份逐文件一致的后备副本。默认优先读取Windows主来源；该路径不可用时才使用SDK副本。可用环境变量显式指定其他测试来源：

```sh
TK8710_APP_SOURCE=/absolute/test/bundle \
  bash external/rk3506app/build_apps.sh --stage
```

正式发布不要设置该变量，避免误用测试产物。

## 验证结果

本次主来源包含1161个文件、129个子目录，总大小约4.5MiB；其中1156个文本文件和5个ARM程序。完整`build_apps.sh --stage`执行成功，输出来源路径、1161个已预置文件和备份目录。

`tk8710_gw`来源与SDK userdata预置文件SHA256均为：

```text
2271e1e1d70ba2f66eb69bd9449fc64f53cd9e7fffd29bb018686539b7dcd09f
```

SDK后备副本与Windows主来源的1161个文件已逐个进行SHA256比较，全部一致。验证日志：`output/tk8710-stage-validation-20260920.log`。

本次只更新了SDK预置输入，没有运行`./build.sh extra-parts`、`./build.sh updateimg`或烧录板卡。因此现有update.img和板上userdata不会自动变化。

## 与SWU的边界

现有三App SWU固定更新MQTT、Web和`tk8710_gw`，不会携带4个测试程序、readme或class3数据。需要完整目录时，使用重新生成的userdata镜像/完整update.img。PowerShell旧升级脚本的`tk8710_gw`来源已经改到新目录，但其包范围仍是单个程序。
