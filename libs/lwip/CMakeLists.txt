# начинаем загрузку и настройку lwip
message(CHECK_START "\tLwIP stack")

# 
if(NOT LWIP_REPOSITORY)
  set(LWIP_REPOSITORY https://git.savannah.gnu.org/git/lwip.git)
  message(NOTICE "\t\tSelect default LwIP repository.")
endif()

if(NOT LWIP_TAG)
  set(LWIP_TAG STABLE-2_2_0_RELEASE)  # Рекомендуемая стабильная версия
  message(NOTICE "\t\tSelect default LwIP tag.")
endif()

message(NOTICE "\t\tUse repository: ${LWIP_REPOSITORY}")
message(NOTICE "\t\tUse tag: ${LWIP_TAG}")

include(FetchContent)

FetchContent_Declare(lwip_repo
  GIT_REPOSITORY ${LWIP_REPOSITORY}
  GIT_TAG ${LWIP_TAG}
)

FetchContent_MakeAvailable(lwip_repo)
FetchContent_GetProperties(lwip_repo)

if(NOT TARGET lwipcore)
    message(FATAL_ERROR "Target 'lwipcore' not found after fetching lwIP!")
endif()

target_link_libraries(lwipcore
    PUBLIC
        freertos_kernel      # Делает FreeRTOS доступной для всех, кто использует LwIP
    PRIVATE
        libopencm3           # Требуется только для сборки самого LwIP
)

target_include_directories(lwipcore
    PUBLIC
        ${lwip_repo_SOURCE_DIR}/src/include   # Основные заголовки LwIP
        ${CMAKE_SOURCE_DIR}/src               # Папка с вашим lwipopts.h
    PRIVATE
        ${lwip_repo_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/arch
)

target_compile_definitions(lwipcore PUBLIC NO_SYS=0)

target_sources(lwipcore PRIVATE ${CMAKE_SOURCE_DIR}/src/arch/sys_arch.c)

message(CHECK_PASS "Done")