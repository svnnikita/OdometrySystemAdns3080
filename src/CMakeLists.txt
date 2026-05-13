message(CHECK_START "\tSources")


add_executable(${CMAKE_PROJECT_NAME}
    main.cpp
    opencm3_freertos.c
    setup.cpp
    adns3080.cpp
    arch/sys_arch.c
    lan8720_config.cpp
    ethernetif.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    PRIVATE
    libopencm3
    freertos_kernel
    lwipcore
)

set_target_properties(${CMAKE_PROJECT_NAME}
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

# Копирование готовой прошивки в папку сборки проекта для отладчика cortex-debug
add_custom_command(TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND cmake -E copy ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME} ${CMAKE_BINARY_DIR}/firmware
)


message(CHECK_PASS "Done")
