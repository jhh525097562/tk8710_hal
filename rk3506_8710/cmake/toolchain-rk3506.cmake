# RK3506 ARM Linux 交叉编译工具链文件
# 使用方法: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-rk3506.cmake ..

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 交叉编译器前缀
set(CROSS_COMPILE_PREFIX "arm-buildroot-linux-gnueabihf-")

# 设置编译器
set(CMAKE_C_COMPILER ${CROSS_COMPILE_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE_PREFIX}g++)

# 设置查找路径模式
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mfpu=neon -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mfpu=neon -mfloat-abi=hard")

# 默认启用RK3506平台
set(PLATFORM_RK3506 ON CACHE BOOL "Build for RK3506 platform" FORCE)
set(BUILD_EXAMPLES ON CACHE BOOL "Build example programs" FORCE)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
