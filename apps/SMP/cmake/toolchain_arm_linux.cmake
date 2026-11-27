# CMake Toolchain File for ARM Linux Cross-Compilation
# Set the target system name and processor
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Specify the cross-compiler prefix
# You can set this variable via command line: -DCROSS_COMPILE_PREFIX=arm-linux-gnueabihf-
if(NOT DEFINED CROSS_COMPILE_PREFIX)
    set(CROSS_COMPILE_PREFIX "arm-linux-gnueabihf-")
endif()

# Specify the cross-compilers
set(CMAKE_C_COMPILER ${CROSS_COMPILE_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE_PREFIX}g++)

# Set the search mode for libraries and include files
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Optional: Add sysroot if needed
# set(CMAKE_SYSROOT /path/to/sysroot)
