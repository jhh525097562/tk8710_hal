# Linux 交叉编译指南 (Cross-Compilation Guide)

本文档说明如何在 Linux 开发环境中使用交叉编译工具链构建 RK3506B 的 IPC 库。

## 1. 准备工作

### 1.1 安装依赖
在你的 Linux 构建主机（如 Ubuntu 20.04/22.04）上安装必要的工具：

```bash
sudo apt update
sudo apt install build-essential cmake gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

*注意：根据你的开发板架构选择正确的交叉编译器包。RK3506B 通常是 ARMv7 (32-bit)，对应 `arm-linux-gnueabihf`。如果是 ARMv8 (64-bit)，请安装 `gcc-aarch64-linux-gnu`。*

### 1.2 确认交叉编译器路径
安装完成后，验证交叉编译器是否可用：

```bash
arm-linux-gnueabihf-gcc --version
```

## 2. 使用一键构建脚本

项目根目录下提供了 `build_cross.sh` 脚本，可自动完成 CMake 配置和编译。

### 2.1 赋予执行权限
首次使用前，请赋予脚本执行权限：

```bash
chmod +x build_cross.sh
```

### 2.2 执行构建
直接运行脚本即可使用默认配置（默认使用 `arm-linux-gnueabihf-`）：

```bash
./build_cross.sh
```

### 2.3 指定其他工具链（可选）
如果你使用的是其他工具链（例如 64位 ARM），可以将其前缀作为参数传递：

```bash
# 构建 ARM64 版本
./build_cross.sh aarch64-linux-gnu-
```

或者指定绝对路径：
```bash
./build_cross.sh /opt/gcc-linaro-7.5.0/bin/arm-linux-gnueabihf-
```

## 3. 手动构建步骤（高级）

如果你想手动控制构建过程，可以按照以下步骤操作：

1.  **创建构建目录**：
    ```bash
    mkdir -p build_arm
    cd build_arm
    ```

2.  **配置 CMake**：
    指定工具链文件和编译器前缀：
    ```bash
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_arm_linux.cmake \
          -DCROSS_COMPILE_PREFIX=arm-linux-gnueabihf- \
          ..
    ```

3.  **编译**：
    ```bash
    make -j4
    ```

## 4. 产物说明

构建成功后，生成的文件位于 `build_linux_arm/ipc/` 目录下：

*   `libipc_smp.so`: **动态库文件**。请将其拷贝到开发板的 `/usr/lib/` 或应用程序同级目录。
*   `ipc_demo_pair`: **测试程序**。可直接在开发板运行以验证通信功能。

## 5. 常见问题

**Q: CMake 报错 "The C compiler identification is unknown"**
A: 检查 `CROSS_COMPILE_PREFIX` 是否正确，以及编译器是否在系统 PATH 中。尝试使用绝对路径。

**Q: 编译时缺少头文件**
A: 确保你的交叉编译环境包含标准的 Linux 头文件。通常安装 `gcc-arm-linux-gnueabihf` 会自动包含。
