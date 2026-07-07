# Build jtool

**## 使用方法**

**```bash**

**# 清理构建目录**

mingw32-make -f cmake/Makefile.jtool clean

**# 编译静态库**

mingw32-make -f cmake/Makefile.jtool lib

**# 编译动态库（需要修复port层）**

mingw32-make -f cmake/Makefile.jtool dll

**# 编译示例程序**

mingw32-make -f cmake/Makefile.jtool clean driver_tests

# Build 3506

在 WSL 中执行
在wsl中，选择ubuntu22.04
然后执行如下指令
source ~/arm-buildroot-linux-gnueabihf_sdk-buildroot/environment-setup

chmod +x ./cmake/build_rk3506.sh
./cmake/build_rk3506.sh
