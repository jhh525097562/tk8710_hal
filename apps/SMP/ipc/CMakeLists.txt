add_library(ipc_smp SHARED
    src/ipc_smp.c
    ../../../common/src/file_logger.c
)

target_include_directories(ipc_smp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/inc
)

target_link_libraries(ipc_smp
    PUBLIC
        rt
)

add_executable(ipc_demo_producer
    demo/ipc_demo_producer.c
)

target_include_directories(ipc_demo_producer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ipc_demo_producer
    PRIVATE
        ipc_smp
        rt
)

add_executable(ipc_demo_consumer
    demo/ipc_demo_consumer.c
)

target_include_directories(ipc_demo_consumer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ipc_demo_consumer
    PRIVATE
        ipc_smp
        rt
)
