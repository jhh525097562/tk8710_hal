cmake_minimum_required(VERSION 3.10)
project(tk8710_hal VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT MSVC)
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

option(PLATFORM_JTOOL "Build for JTOOL platform (Windows)" OFF)
option(PLATFORM_RK3506 "Build for RK3506 platform (Linux ARM)" OFF)
option(BUILD_EXAMPLES "Build example programs" ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/driver
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/trm
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/phy
    ${CMAKE_CURRENT_SOURCE_DIR}/../port
    ${CMAKE_CURRENT_SOURCE_DIR}/../port/jtool
)

set(PHY_SOURCES
    ../src/phy/phy_api.c
    ../src/phy/phy_irq.c
    ../src/phy/phy_regs.c
    ../src/phy/phy_log.c
    ../src/phy/phy_test.c
)

set(HAL_SOURCES
    ../src/hal/hal_api.c
    ../src/hal/hal_cb.c
    ../src/hal/hal_status.c
)

set(TRM_V02_SOURCES
    ../src/trm/trm_queue.c
    ../src/trm/phy_data.c
    ../src/trm/phy_cfg.c
    ../src/trm/phy_stat.c
)

set(LEGACY_SOURCES
    ../src/driver/tk8710_log.c
    ../src/driver/tk8710_core.c
    ../src/driver/tk8710_config.c
    ../src/driver/tk8710_irq.c
    ../src/trm/trm_core.c
    ../src/trm/trm_beam.c
    ../src/trm/trm_data.c
    ../src/trm/trm_slot.c
    ../src/trm/trm_freq.c
    ../src/trm/trm_power.c
    ../src/trm/trm_log.c
)

if(PLATFORM_JTOOL)
    set(PORT_SOURCES ../port/jtool/port_jtool.c)
    add_definitions(-DPLATFORM_JTOOL)
elseif(PLATFORM_RK3506)
    set(PORT_SOURCES ../port/tk8710_rk3506.c)
    add_definitions(-DPLATFORM_RK3506)
endif()

add_library(tk8710_hal STATIC
    ${PHY_SOURCES}
    ${HAL_SOURCES}
    ${TRM_V02_SOURCES}
    ${LEGACY_SOURCES}
    ${PORT_SOURCES}
)

target_include_directories(tk8710_hal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../inc)

if(BUILD_EXAMPLES)
    add_executable(test_hal_phy_api ../test/example/test_hal_phy_api.c)
    target_link_libraries(test_hal_phy_api tk8710_hal)
endif()
