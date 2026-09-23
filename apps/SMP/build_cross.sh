#!/bin/bash
# -----------------------------------------------------------------------------
# Linux Cross-Compilation Script for RK3506B
#
# Usage:
#   ./build_cross.sh [CROSS_COMPILE_PREFIX]
#
# Examples:
#   ./build_cross.sh                        # Uses default: arm-linux-gnueabihf-
#   ./build_cross.sh aarch64-linux-gnu-     # Uses specific prefix
# -----------------------------------------------------------------------------

set -e

# Project root directory
PROJECT_ROOT=$(pwd)
BUILD_DIR="${PROJECT_ROOT}/build_linux_arm"
TOOLCHAIN_FILE="${PROJECT_ROOT}/cmake/toolchain_arm_linux.cmake"

# 1. Determine Cross-Compiler Prefix
# Default to arm-linux-gnueabihf- if not provided as argument
CROSS_COMPILE_PREFIX=${1:-"arm-linux-gnueabihf-"}

echo "------------------------------------------------------------"
echo "Starting Cross-Compilation"
echo "Target Architecture: ARM Linux"
echo "Cross-Compiler Prefix: ${CROSS_COMPILE_PREFIX}"
echo "Build Directory: ${BUILD_DIR}"
echo "------------------------------------------------------------"

# Check if CMake is installed
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake is not installed. Please install it first (e.g., sudo apt install cmake)."
    exit 1
fi

# 2. Create Build Directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# 3. Configure CMake
# We pass the toolchain file and the cross-compiler prefix
cmake -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
      -DCROSS_COMPILE_PREFIX="${CROSS_COMPILE_PREFIX}" \
      -DCMAKE_BUILD_TYPE=Release \
      "${PROJECT_ROOT}"

# 4. Build
echo "Building project with $(nproc) jobs..."
make -j$(nproc)

# 5. Output Results
if [ $? -eq 0 ]; then
    echo "------------------------------------------------------------"
    echo "Build SUCCESS!"
    echo "Artifacts:"
    echo "  Library: ${BUILD_DIR}/ipc/libipc_smp.so"
    echo "  Demo:    ${BUILD_DIR}/ipc/ipc_demo_pair"
    echo "------------------------------------------------------------"
else
    echo "Build FAILED!"
    exit 1
fi
