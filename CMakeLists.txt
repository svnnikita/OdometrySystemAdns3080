#================================================================================
cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE cmake/arm-none-eabi-gcc.cmake)


#================================================================================
project(freertos-proj LANGUAGES CXX C ASM)
set(DEVICE stm32f407vg)


#================================================================================
add_subdirectory(libs/libopencm3)

add_subdirectory(libs/freertos)

add_subdirectory(libs/lwip)

add_subdirectory(src)

add_subdirectory(flash_n_debug)

add_subdirectory(doc)
