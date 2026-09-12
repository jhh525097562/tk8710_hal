#!/bin/bash

print_usage() {
    echo "Usage: $0 [all|TARGET ...]"
    echo "       $0 --list"
    echo ""
    echo "No target or 'all' builds every program."
    echo "One or more target names build only those programs and their common library."
}

list_targets() {
    {
        find test/DriverTest -maxdepth 1 -name "*.c" ! -name "TestJtool*.c" \
            -type f -exec basename {} .c \;
        for target in test8710main_3506 test_tk8710_pps_api test_tk8710_gw_gps test_tk8710_gw_gps_protocol test_tk8710_gw_gps_faults tk8710_gw_gps_device_test tk8710_pps_test TestTRMmain tk8710_gw tk8710_gw_gps_ns_test tk8710_gw_slave tk8710_gw_sat tk8710_gw_ground; do
            if [ "$target" = "tk8710_gw_gps_ns_test" ] && [ -f "test/example/tk8710_gw.c" ]; then
                echo "${target}"
            elif [ -f "test/example/${target}.c" ]; then
                echo "${target}"
            fi
        done
    } | sort -u
}

is_known_target() {
    case "$1" in
        TestJtool*) return 1 ;;
    esac

    if [ -f "test/DriverTest/$1.c" ]; then
        return 0
    fi

    if [ "$1" = "tk8710_gw_gps_ns_test" ]; then
        [ -f "test/example/tk8710_gw.c" ]
        return $?
    fi

    case "$1" in
        test8710main_3506|test_tk8710_pps_api|test_tk8710_gw_gps|test_tk8710_gw_gps_protocol|test_tk8710_gw_gps_faults|tk8710_gw_gps_device_test|tk8710_pps_test|TestTRMmain|tk8710_gw|tk8710_gw_gps_ns_test|tk8710_gw_slave|tk8710_gw_sat|tk8710_gw_ground)
            [ -f "test/example/$1.c" ]
            return $?
            ;;
    esac

    return 1
}

BUILD_ALL=0
REQUESTED_TARGETS=""

if [ $# -eq 0 ]; then
    BUILD_ALL=1
elif [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    print_usage
    exit 0
elif [ "$1" = "--list" ]; then
    list_targets
    exit 0
else
    for target in "$@"; do
        if [ "$target" = "all" ]; then
            BUILD_ALL=1
            continue
        fi
        if ! is_known_target "$target"; then
            echo "Error: unknown build target '$target'"
            echo "Available targets:"
            list_targets
            exit 2
        fi
        REQUESTED_TARGETS="${REQUESTED_TARGETS} ${target}"
    done
fi

should_build() {
    if [ "$BUILD_ALL" -eq 1 ]; then
        return 0
    fi

    case " ${REQUESTED_TARGETS} " in
        *" $1 "*) return 0 ;;
        *) return 1 ;;
    esac
}
# 完整编译脚本 - 编译所有可编译的模块

echo "TK8710 RK3506 完整编译脚本"
echo "=========================="
if [ "$BUILD_ALL" -eq 1 ]; then
    echo "Build targets: all"
else
    echo "Build targets:${REQUESTED_TARGETS}"
fi

# 检查交叉编译器
if ! command -v arm-buildroot-linux-gnueabihf-gcc &> /dev/null; then
    echo "错误: 未找到交叉编译器"
    echo "请运行: source ~/arm-buildroot-linux-gnueabihf_sdk-buildroot/environment-setup"
    exit 1
fi

echo "交叉编译器: $(arm-buildroot-linux-gnueabihf-gcc --version | head -n1)"

# 创建构建目录
BUILD_DIR="build_rk3506"
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}

# 编译选项
CFLAGS="-Wall -Wextra -Wno-unused-parameter -O2 -DPLATFORM_RK3506"
INCLUDES="-I./inc -I./inc/driver -I./inc/trm -I./inc/phy -I./port -I./port/rk3506 -I../../../核间通信/0323/0323/spi/inc -I../../../核间通信/0323/0323"

echo ""
echo "编译所有模块..."

# 编译src/driver目录下的所有C文件
echo "编译src/driver目录下的所有模块..."
driver_files=$(find src/driver -name "*.c" -type f)
driver_count=$(echo "$driver_files" | wc -l)
echo "发现 $driver_count 个C文件"

for file in $driver_files; do
    echo "编译 $file..."
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c $file -o ${BUILD_DIR}/$(basename $file .c).o
    if [ $? -eq 0 ]; then
        echo "✅ $file 编译成功"
    else
        echo "❌ $file 编译失败"
        exit 1
    fi
done

# 编译TRM模块
echo "编译TRM模块..."
arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
    -c src/trm/trm_core.c \
    -o ${BUILD_DIR}/trm_core.o

if [ $? -eq 0 ]; then
    echo "✅ src/trm/trm_core.c 编译成功"
else
    echo "❌ src/trm/trm_core.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
    -c src/trm/trm_beam.c \
    -o ${BUILD_DIR}/trm_beam.o

if [ $? -eq 0 ]; then
    echo "✅ src/trm/trm_beam.c 编译成功"
else
    echo "❌ src/trm/trm_beam.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
    -c src/trm/trm_data.c \
    -o ${BUILD_DIR}/trm_data.o

if [ $? -eq 0 ]; then
    echo "✅ src/trm/trm_data.c 编译成功"
else
    echo "❌ src/trm/trm_data.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
    -c src/trm/trm_satellite.c \
    -o ${BUILD_DIR}/trm_satellite.o

if [ $? -eq 0 ]; then
    echo "✅ src/trm/trm_satellite.c 编译成功"
else
    echo "❌ src/trm/trm_satellite.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
    -c src/trm/trm_log.c \
    -o ${BUILD_DIR}/trm_log.o

if [ $? -eq 0 ]; then
    echo "✅ src/trm/trm_log.c 编译成功"
else
    echo "❌ src/trm/trm_log.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
    -c src/trm/trm_mac_parser.c \
    -o ${BUILD_DIR}/trm_mac_parser.o

if [ $? -eq 0 ]; then
    echo "✅ src/trm/trm_mac_parser.c 编译成功"
else
    echo "❌ src/trm/trm_mac_parser.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
    -c src/trm/trm_slot.c \
    -o ${BUILD_DIR}/trm_slot.o

if [ $? -eq 0 ]; then
    echo "✅ src/trm/trm_slot.c 编译成功"
else
    echo "❌ src/trm/trm_slot.c 编译失败"
    exit 1
fi

# 尝试编译RK3506 port
echo "编译RK3506 port..."
arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c port/tk8710_rk3506.c -o ${BUILD_DIR}/tk8710_rk3506.o
if [ $? -eq 0 ]; then
    echo "✅ tk8710_rk3506.c 编译成功"
else
    echo "❌ tk8710_rk3506.c 编译失败，跳过..."
fi

# 编译HAL/PHY重构模块
echo "编译HAL/PHY重构模块..."

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c src/hal/hal_api.c -o ${BUILD_DIR}/hal_api.o
if [ $? -eq 0 ]; then
    echo "✅ src/hal/hal_api.c 编译成功"
else
    echo "❌ src/hal/hal_api.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c src/hal/hal_cb.c -o ${BUILD_DIR}/hal_cb.o
if [ $? -eq 0 ]; then
    echo "✅ src/hal/hal_cb.c 编译成功"
else
    echo "❌ src/hal/hal_cb.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c src/hal/hal_status.c -o ${BUILD_DIR}/hal_status.o
if [ $? -eq 0 ]; then
    echo "✅ src/hal/hal_status.c 编译成功"
else
    echo "❌ src/hal/hal_status.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c src/tk8710_pps_api.c -o ${BUILD_DIR}/tk8710_pps_api.o
if [ $? -eq 0 ]; then
    echo "✅ src/tk8710_pps_api.c 编译成功"
else
    echo "❌ src/tk8710_pps_api.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c src/phy/phy_api.c -o ${BUILD_DIR}/phy_api.o
if [ $? -eq 0 ]; then
    echo "✅ src/phy/phy_api.c 编译成功"
else
    echo "❌ src/phy/phy_api.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c src/phy/phy_irq.c -o ${BUILD_DIR}/phy_irq.o
if [ $? -eq 0 ]; then
    echo "✅ src/phy/phy_irq.c 编译成功"
else
    echo "❌ src/phy/phy_irq.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c src/phy/phy_regs.c -o ${BUILD_DIR}/phy_regs.o
if [ $? -eq 0 ]; then
    echo "✅ src/phy/phy_regs.c 编译成功"
else
    echo "❌ src/phy/phy_regs.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c src/phy/phy_log.c -o ${BUILD_DIR}/phy_log.o
if [ $? -eq 0 ]; then
    echo "✅ src/phy/phy_log.c 编译成功"
else
    echo "❌ src/phy/phy_log.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c src/trm/trm_queue.c -o ${BUILD_DIR}/trm_queue.o
if [ $? -eq 0 ]; then
    echo "✅ src/trm/trm_queue.c 编译成功"
else
    echo "❌ src/trm/trm_queue.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c src/trm/phy_data.c -o ${BUILD_DIR}/phy_data.o
if [ $? -eq 0 ]; then
    echo "✅ src/trm/phy_data.c 编译成功"
else
    echo "❌ src/trm/phy_data.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c src/trm/phy_cfg.c -o ${BUILD_DIR}/phy_cfg.o
if [ $? -eq 0 ]; then
    echo "✅ src/trm/phy_cfg.c 编译成功"
else
    echo "❌ src/trm/phy_cfg.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -c src/trm/phy_stat.c -o ${BUILD_DIR}/phy_stat.o
if [ $? -eq 0 ]; then
    echo "✅ src/trm/phy_stat.c 编译成功"
else
    echo "❌ src/trm/phy_stat.c 编译失败"
    exit 1
fi

# 编译核间通信模块
echo "编译核间通信模块..."
arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
    -c src/tk8710_ipc_comm.c \
    -o ${BUILD_DIR}/tk8710_ipc_comm.o
if [ $? -eq 0 ]; then
    echo "✅ tk8710_ipc_comm.c 编译成功"
else
    echo "❌ tk8710_ipc_comm.c 编译失败"
    exit 1
fi

# 编译Web命令IPC服务模块
printf "编译Web命令IPC服务模块...\n"
arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
    -c src/ipc_command_server.c \
    -o ${BUILD_DIR}/ipc_command_server.o
if [ $? -eq 0 ]; then
    echo "✅ ipc_command_server.c 编译成功"
else
    echo "❌ ipc_command_server.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
    -c src/tk8710_scan_service.c \
    -o ${BUILD_DIR}/tk8710_scan_service.o
if [ $? -eq 0 ]; then
    echo "✅ tk8710_scan_service.c 编译成功"
else
    echo "❌ tk8710_scan_service.c 编译失败"
    exit 1
fi

arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
    -c src/tk8710_scan_ipc_server.c \
    -o ${BUILD_DIR}/tk8710_scan_ipc_server.o
if [ $? -eq 0 ]; then
    echo "✅ tk8710_scan_ipc_server.c 编译成功"
else
    echo "❌ tk8710_scan_ipc_server.c 编译失败"
    exit 1
fi

# 创建静态库（不包含IPC通信模块，避免链接冲突）
echo ""
echo "创建静态库..."
ar rcs ${BUILD_DIR}/libtk8710_hal_complete.a \
    ${BUILD_DIR}/tk8710_core.o \
    ${BUILD_DIR}/tk8710_config.o \
    ${BUILD_DIR}/tk8710_irq.o \
    ${BUILD_DIR}/tk8710_log.o \
    ${BUILD_DIR}/tk8710_rk3506.o \
    ${BUILD_DIR}/hal_api.o \
    ${BUILD_DIR}/hal_cb.o \
    ${BUILD_DIR}/hal_status.o \
    ${BUILD_DIR}/tk8710_pps_api.o \
    ${BUILD_DIR}/phy_api.o \
    ${BUILD_DIR}/phy_irq.o \
    ${BUILD_DIR}/phy_regs.o \
    ${BUILD_DIR}/phy_log.o \
    ${BUILD_DIR}/trm_core.o \
    ${BUILD_DIR}/trm_beam.o \
    ${BUILD_DIR}/trm_data.o \
    ${BUILD_DIR}/trm_satellite.o \
    ${BUILD_DIR}/trm_queue.o \
    ${BUILD_DIR}/trm_log.o \
    ${BUILD_DIR}/trm_mac_parser.o \
    ${BUILD_DIR}/trm_slot.o \
    ${BUILD_DIR}/phy_data.o \
    ${BUILD_DIR}/phy_cfg.o \
    ${BUILD_DIR}/phy_stat.o \
    ${BUILD_DIR}/ipc_command_server.o \
    ${BUILD_DIR}/tk8710_scan_service.o \
    ${BUILD_DIR}/tk8710_scan_ipc_server.o

if [ $? -eq 0 ]; then
    echo "✅ 静态库创建成功"
else
    echo "❌ 静态库创建失败"
    exit 1
fi

# 创建静态库后，编译test/DriverTest目录下的所有C文件并立即链接
echo ""
echo "编译test/DriverTest目录下的所有测试程序..."

# 先编译验证器模块（所有测试程序都需要）
echo "编译验证器模块..."
arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
    -c test/example/trm_tx_validator.c \
    -o ${BUILD_DIR}/trm_tx_validator.o

if [ $? -eq 0 ]; then
    echo "✅ trm_tx_validator.c 编译成功"
else
    echo "❌ trm_tx_validator.c 编译失败"
    exit 1
fi

# 编译并链接每个测试程序
test_files=$(find test/DriverTest -name "*.c" ! -name "TestJtool*.c" -type f)
test_count=$(echo "$test_files" | wc -l)
echo "发现 $test_count 个测试C文件"

for file in $test_files; do
    basename_file=$(basename "$file" .c)
    if ! should_build "$basename_file"; then
        continue
    fi
    echo ""
    echo "编译并链接 $file..."
    
    # 编译为 .o 文件
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port -I./port/rk3506 -I./test/example -c "$file" -o ${BUILD_DIR}/${basename_file}.o
    if [ $? -ne 0 ]; then
        echo "❌ $file 编译失败"
        exit 1
    fi
    
    # 立即链接为可执行文件
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port -I./test/example \
        ${BUILD_DIR}/${basename_file}.o \
        ${BUILD_DIR}/trm_tx_validator.o \
        ${BUILD_DIR}/tk8710_ipc_comm.o \
        -L${BUILD_DIR} -ltk8710_hal_complete \
        -L./lib -lipc_smp \
        -Wl,-rpath,./lib \
        -lpthread -lgpiod -lm \
        -o ${BUILD_DIR}/${basename_file}
    
    if [ $? -eq 0 ]; then
        echo "✅ ${basename_file} 创建成功"
    else
        echo "❌ ${basename_file} 创建失败"
    fi
done

# 创建示例程序
echo ""
echo "创建示例程序..."

if [ -f "test/example/test_tk8710_pps_api.c" ] && should_build "test_tk8710_pps_api"; then
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} \
        test/example/test_tk8710_pps_api.c \
        -L${BUILD_DIR} -ltk8710_hal_complete -lgpiod -lpthread -lm \
        -o ${BUILD_DIR}/test_tk8710_pps_api
    if [ $? -eq 0 ]; then
        echo "✅ test_tk8710_pps_api 创建成功"
    else
        echo "❌ test_tk8710_pps_api 创建失败"
        exit 1
    fi
fi

if [ -f "test/example/test_tk8710_gw_gps.c" ] && should_build "test_tk8710_gw_gps"; then
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./test/example \
        test/example/test_tk8710_gw_gps.c test/example/tk8710_gw_gps.c \
        -L${BUILD_DIR} -ltk8710_hal_complete -lpthread -lgpiod \
        -o ${BUILD_DIR}/test_tk8710_gw_gps
    if [ $? -eq 0 ]; then
        echo "test_tk8710_gw_gps created successfully"
    else
        echo "test_tk8710_gw_gps build failed"
        exit 1
    fi
fi

if [ -f "test/example/test_tk8710_gw_gps_protocol.c" ] && should_build "test_tk8710_gw_gps_protocol"; then
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./test/example \
        test/example/test_tk8710_gw_gps_protocol.c test/example/tk8710_gw_gps.c \
        -L${BUILD_DIR} -ltk8710_hal_complete -lpthread -lgpiod \
        -o ${BUILD_DIR}/test_tk8710_gw_gps_protocol
    if [ $? -eq 0 ]; then
        echo "test_tk8710_gw_gps_protocol created successfully"
    else
        echo "test_tk8710_gw_gps_protocol build failed"
        exit 1
    fi
fi

if [ -f "test/example/test_tk8710_gw_gps_faults.c" ] && should_build "test_tk8710_gw_gps_faults"; then
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./test/example \
        -DTK8710_GPS_TEST_HOOKS \
        test/example/test_tk8710_gw_gps_faults.c test/example/tk8710_gw_gps.c \
        -L${BUILD_DIR} -ltk8710_hal_complete -lpthread -lgpiod \
        -o ${BUILD_DIR}/test_tk8710_gw_gps_faults
    if [ $? -eq 0 ]; then
        echo "test_tk8710_gw_gps_faults created successfully"
    else
        echo "test_tk8710_gw_gps_faults build failed"
        exit 1
    fi
fi

if [ -f "test/example/tk8710_gw_gps_device_test.c" ] && should_build "tk8710_gw_gps_device_test"; then
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./test/example \
        test/example/tk8710_gw_gps_device_test.c test/example/tk8710_gw_gps.c \
        -L${BUILD_DIR} -ltk8710_hal_complete -lpthread -lgpiod \
        -o ${BUILD_DIR}/tk8710_gw_gps_device_test
    if [ $? -eq 0 ]; then
        echo "tk8710_gw_gps_device_test created successfully"
    else
        echo "tk8710_gw_gps_device_test build failed"
        exit 1
    fi
fi

if [ -f "test/example/tk8710_pps_test.c" ] && should_build "tk8710_pps_test"; then
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
        test/example/tk8710_pps_test.c \
        -L${BUILD_DIR} -ltk8710_hal_complete -lgpiod -lpthread -lm \
        -o ${BUILD_DIR}/tk8710_pps_test
    if [ $? -eq 0 ]; then
        echo "✅ tk8710_pps_test 创建成功"
    else
        echo "❌ tk8710_pps_test 创建失败"
        exit 1
    fi
fi

# 创建 test8710main_3506
if [ -f "test/example/test8710main_3506.c" ] && should_build "test8710main_3506"; then
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
        test/example/test8710main_3506.c \
        ${BUILD_DIR}/tk8710_ipc_comm.o \
        -L${BUILD_DIR} -ltk8710_hal_complete \
        -L./lib -lipc_smp \
        -Wl,-rpath,./lib \
        -lpthread -lgpiod -lm \
        -o ${BUILD_DIR}/test8710main_3506
    
    if [ $? -eq 0 ]; then
        echo "✅ test8710main_3506 创建成功"
    else
        echo "❌ test8710main_3506 创建失败"
    fi
elif should_build "test8710main_3506"; then
    echo "⚠️  test8710main_3506 源文件不存在"
fi

# 创建 TestTRMmain
if [ -f "test/example/TestTRMmain.c" ] && should_build "TestTRMmain"; then
    # 先编译验证器模块
    echo "编译验证器模块..."
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
        -c test/example/trm_tx_validator.c \
        -o ${BUILD_DIR}/trm_tx_validator.o
    
    if [ $? -eq 0 ]; then
        echo "✅ trm_tx_validator.c 编译成功"
    else
        echo "❌ trm_tx_validator.c 编译失败"
        exit 1
    fi
    
    # 编译测试程序并链接验证器
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
        test/example/TestTRMmain.c \
        ${BUILD_DIR}/trm_tx_validator.o \
        ${BUILD_DIR}/tk8710_ipc_comm.o \
        -L${BUILD_DIR} -ltk8710_hal_complete \
        -L./lib -lipc_smp \
        -Wl,-rpath,./lib \
        -lpthread -lgpiod -lm \
        -o ${BUILD_DIR}/TestTRMmain
    
    if [ $? -eq 0 ]; then
        echo "✅ TestTRMmain 创建成功"
    else
        echo "❌ TestTRMmain 创建失败"
    fi
elif should_build "TestTRMmain"; then
    echo "⚠️  TestTRMmain 源文件不存在"
fi

# 创建 tk8710_gw
if [ -f "test/example/tk8710_gw.c" ] && should_build "tk8710_gw"; then
    # 先编译验证器模块
    echo "编译验证器模块..."
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
        -c test/example/trm_tx_validator.c \
        -o ${BUILD_DIR}/trm_tx_validator.o
    
    if [ $? -eq 0 ]; then
        echo "✅ trm_tx_validator.c 编译成功"
    else
        echo "❌ trm_tx_validator.c 编译失败"
        exit 1
    fi
    
    # 编译测试程序并链接验证器（单独包含IPC对象文件）
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
        test/example/tk8710_gw.c \
        test/example/tk8710_gw_gps.c \
        ${BUILD_DIR}/trm_tx_validator.o \
        ${BUILD_DIR}/tk8710_ipc_comm.o \
        -L${BUILD_DIR} -ltk8710_hal_complete \
        -L./lib -lipc_smp \
        -Wl,-rpath,./lib \
        -lpthread -lgpiod -lm \
        -o ${BUILD_DIR}/tk8710_gw
    
    if [ $? -eq 0 ]; then
        echo "✅ tk8710_gw 创建成功"
    else
        echo "❌ tk8710_gw 创建失败"
    fi
elif should_build "tk8710_gw"; then
    echo "⚠️  tk8710_gw 源文件不存在"
fi

if [ -f "test/example/tk8710_gw.c" ] && should_build "tk8710_gw_gps_ns_test"; then
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
        -DTK8710_GPS_TEST_HOOKS \
        test/example/tk8710_gw.c test/example/tk8710_gw_gps.c \
        test/example/trm_tx_validator.c ${BUILD_DIR}/tk8710_ipc_comm.o \
        -L${BUILD_DIR} -ltk8710_hal_complete -L./lib -lipc_smp \
        -Wl,-rpath,./lib -lpthread -lgpiod -lm \
        -o ${BUILD_DIR}/tk8710_gw_gps_ns_test
    if [ $? -eq 0 ]; then
        echo "tk8710_gw_gps_ns_test created successfully"
    else
        echo "tk8710_gw_gps_ns_test build failed"
        exit 1
    fi
fi

# 创建 tk8710_gw_slave
if [ -f "test/example/tk8710_gw_slave.c" ] && should_build "tk8710_gw_slave"; then
    # 先编译验证器模块
    echo "编译验证器模块..."
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
        -c test/example/trm_tx_validator.c \
        -o ${BUILD_DIR}/trm_tx_validator.o
    
    if [ $? -eq 0 ]; then
        echo "✅ trm_tx_validator.c 编译成功"
    else
        echo "❌ trm_tx_validator.c 编译失败"
        exit 1
    fi
    
    # 编译测试程序并链接验证器（单独包含IPC对象文件）
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
        test/example/tk8710_gw_slave.c \
        ${BUILD_DIR}/trm_tx_validator.o \
        ${BUILD_DIR}/tk8710_ipc_comm.o \
        -L${BUILD_DIR} -ltk8710_hal_complete \
        -L./lib -lipc_smp \
        -Wl,-rpath,./lib \
        -lpthread -lgpiod -lm \
        -o ${BUILD_DIR}/tk8710_gw_slave

    if [ $? -eq 0 ]; then
        echo "✅ tk8710_gw_slave 创建成功"
    else
        echo "❌ tk8710_gw_slave 创建失败"
    fi
elif should_build "tk8710_gw_slave"; then
    echo "⚠️  tk8710_gw_slave 源文件不存在"
fi

# 创建 tk8710_gw_sat
if [ -f "test/example/tk8710_gw_sat.c" ] && should_build "tk8710_gw_sat"; then
    # 先编译验证器模块
    echo "编译验证器模块..."
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
        -c test/example/trm_tx_validator.c \
        -o ${BUILD_DIR}/trm_tx_validator.o
    
    if [ $? -eq 0 ]; then
        echo "✅ trm_tx_validator.c 编译成功"
    else
        echo "❌ trm_tx_validator.c 编译失败"
        exit 1
    fi
    
    # 编译测试程序并链接验证器（单独包含IPC对象文件）
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
        test/example/tk8710_gw_sat.c \
        ${BUILD_DIR}/trm_tx_validator.o \
        ${BUILD_DIR}/tk8710_ipc_comm.o \
        -L${BUILD_DIR} -ltk8710_hal_complete \
        -L./lib -lipc_smp \
        -Wl,-rpath,./lib \
        -lpthread -lgpiod -lm \
        -o ${BUILD_DIR}/tk8710_gw_sat
    
    if [ $? -eq 0 ]; then
        echo "✅ tk8710_gw_sat 创建成功"
    else
        echo "❌ tk8710_gw_sat 创建失败"
    fi
elif should_build "tk8710_gw_sat"; then
    echo "⚠️  tk8710_gw_sat 源文件不存在"
fi


# 创建 tk8710_gw_ground
if [ -f "test/example/tk8710_gw_ground.c" ] && should_build "tk8710_gw_ground"; then
    # 先编译验证器模块
    echo "编译验证器模块..."
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
        -c test/example/trm_tx_validator.c \
        -o ${BUILD_DIR}/trm_tx_validator.o
    
    if [ $? -eq 0 ]; then
        echo "✅ trm_tx_validator.c 编译成功"
    else
        echo "❌ trm_tx_validator.c 编译失败"
        exit 1
    fi
    
    # 编译测试程序并链接验证器（单独包含IPC对象文件）
    arm-buildroot-linux-gnueabihf-gcc ${CFLAGS} ${INCLUDES} -I./port \
        test/example/tk8710_gw_ground.c \
        ${BUILD_DIR}/trm_tx_validator.o \
        ${BUILD_DIR}/tk8710_ipc_comm.o \
        -L${BUILD_DIR} -ltk8710_hal_complete \
        -L./lib -lipc_smp \
        -Wl,-rpath,./lib \
        -lpthread -lgpiod -lm \
        -o ${BUILD_DIR}/tk8710_gw_ground
    
    if [ $? -eq 0 ]; then
        echo "✅ tk8710_gw_ground 创建成功"
    else
        echo "❌ tk8710_gw_ground 创建失败"
    fi
elif should_build "tk8710_gw_ground"; then
    echo "⚠️  tk8710_gw_ground 源文件不存在"
fi

# 显示结果
echo ""
echo "=========================="
echo "编译完成！"
echo "输出文件:"
ls -la ${BUILD_DIR}/
echo ""
echo "库文件信息:"
if [ -f "${BUILD_DIR}/libtk8710_hal_complete.a" ]; then
    echo "静态库大小: $(stat -c%s ${BUILD_DIR}/libtk8710_hal_complete.a) 字节"
    echo "包含的目标文件:"
    arm-buildroot-linux-gnueabihf-ar t ${BUILD_DIR}/libtk8710_hal_complete.a
fi
echo "=========================="

# 检查是否可以运行（在ARM设备上）
echo ""
echo "注意：生成的程序适用于ARM平台，需要在RK3506设备上运行。"
